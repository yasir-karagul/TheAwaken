#include "ItemInventoryFramework/UBFL_InventoryBag.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "ItemInventoryFramework/03Interfaces/BPI_BagActorService.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
// Tests only: 1 or 2 makes that step of detachBag fail, to prove that nothing moves (R01 3.8). 0 = off.
static TAutoConsoleVariable<int32> cvarFailBagStep(TEXT("Awaken.Inventory.FailBagStep"), 0, TEXT("Make step 1 (create the bag) or step 2 (release worn gear) of detachBag fail. Tests only. 0 = off."), ECVF_Cheat);
#endif

bool UBFL_InventoryBag::shouldFailStep(int32 step){
#if !UE_BUILD_SHIPPING
    return cvarFailBagStep.GetValueOnGameThread() == step;
#else
    return false;
#endif
}

// ---------- queries ----------

bool UBFL_InventoryBag::hasBag(const APlayerState* player){
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    return inventory != nullptr && inventory->hasBag();
}

int32 UBFL_InventoryBag::getUsableHotbarSlotCount(const APlayerState* player){
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    return inventory != nullptr ? inventory->getUsableHotbarSlotCount() : 0;
}

// ---------- S9.4 detachBag ----------

FS_BagDetachResult UBFL_InventoryBag::detachBag(APlayerState* player, const FVector& location){
    FS_BagDetachResult result;
    if(!UBFL_Inventory::isServerWorld(player)){
        result.failReason = T_Inventory_FailReason_NotServer;
        return result;
    }
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    UObject* bagService = services != nullptr ? services->getBagService() : nullptr;
    if(inventory == nullptr || bagService == nullptr){
        result.failReason = T_Inventory_FailReason_NotFound;
        return result;
    }
    if(!inventory->hasBag()){
        result.failReason = T_Inventory_FailReason_NoBag;   // dying Bagless: Doc B02 calls discardBaglessItems instead
        return result;
    }

    // Before anything moves, the staged reservations are released, so the record never holds a reserved quantity
    // (Index A103, S4-F28). This is safe even if a later step fails: the units are simply available again.
    UBFL_Inventory::releaseAllStaged(player);

    // Step 1: the bag record and its actor are created first (Doc B02 3.6).
    const FGuid bagId = FGuid::NewGuid();
    if(shouldFailStep(1) || !IBPI_BagActorService::Execute_createBag(bagService, bagId, inventory->getCharacterId(), location)){
        IBPI_BagActorService::Execute_deleteBag(bagService, bagId);
        result.failReason = T_Inventory_FailReason_BagCreateFailed;
        return result;
    }

    // Step 2: worn armor goes back to the bag record (Doc R05 3.3). The injected failure happens BEFORE the call,
    // because a real release empties the worn stack and could not be undone.
    FS_GearReleaseResult gear;
    gear.succeeded = false;
    if(!shouldFailStep(2)){
        gear = services->releaseEquippedGear(player);
    }
    if(!gear.succeeded){
        IBPI_BagActorService::Execute_deleteBag(bagService, bagId);   // the record and the actor created in step 1 are deleted
        result.failReason = T_Inventory_FailReason_GearReleaseFailed;
        return result;
    }

    // Step 3: the move. Nothing below can fail: it is memory only, in one server step.
    FS_BagContents contents;
    contents.hotbar = inventory->getHotbarBindings();                 // the snapshot that recovery puts back exactly
    contents.gear = gear.gearSlots;
    const FS_HeldItem held = UBFL_Inventory::getHeldState(player);    // read before the bag leaves and the hand empties
    contents.wielded.itemId = held.itemId;
    contents.wielded.instanceId = held.instanceId;

    UInventoryData* bag = inventory->releaseInventory();              // the player is Bagless from here; the hand is empty
    for(const FS_InventoryEntry& piece : gear.gearEntries){
        bag->addInstanceForced(piece);                                // limits are ignored so no piece can be lost
    }
    bag->setDeathBag(true);                                           // nobody takes from it or stores into it (R01 3.4)
    contents.inventoryId = bag->getInventoryId();
    inventory->setHotbarBindings(TArray<FS_HotbarBinding>());         // the Bagless holding mode has no bindings
    IBPI_BagActorService::Execute_setBagContents(bagService, bagId, contents);

    result.bagId = bagId;
    INVENTORY_LOG(Log, player, "Bag %s of %s detached at %s with %d entries and %d worn piece(s); the player is Bagless", *bagId.ToString(), *GetNameSafe(player), *location.ToCompactString(), bag->getEntries().Num(), gear.gearSlots.Num());
    return result;
}

// ---------- S9.5 reattachBag ----------

FS_InventoryResult UBFL_InventoryBag::reattachBag(APlayerState* player, const FGuid& bagId){
    if(!UBFL_Inventory::isServerWorld(player)){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotServer);
    }
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(player);
    UObject* bagService = services != nullptr ? services->getBagService() : nullptr;
    if(inventory == nullptr || registry == nullptr || bagService == nullptr){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    if(inventory->hasBag()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_AlreadyHasBag);
    }
    const FS_BagInfo info = IBPI_BagActorService::Execute_findBag(bagService, bagId);
    if(!info.isValid){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    if(info.ownerCharacterId != inventory->getCharacterId()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotBagOwner);   // a bag belongs to a character (Index A93)
    }
    UInventoryData* bag = registry->findInventory(info.contents.inventoryId);
    if(bag == nullptr){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }

    // Everything checked. All four steps below happen in this one server step, or none did (R01 3.8).
    // (1) The Personal Inventory is back.
    TArray<FS_InventoryEntry> heldItems;
    for(int32 holdIndex = 0; holdIndex < inventory->getHoldSlots().Num(); holdIndex++){
        FS_InventoryEntry heldEntry;
        if(inventory->takeFromHoldSlot(holdIndex, heldEntry)){
            heldItems.Add(heldEntry);   // taken out first: the slots become bindings again in step 3
        }
    }
    bag->setDeathBag(false);
    inventory->attachInventory(bag);

    // (2) Each held item is added to the bag; what does not fit drops next to the bag. A stack that fits in part goes in in part.
    int32 droppedQuantity = 0;
    for(const FS_InventoryEntry& heldEntry : heldItems){
        droppedQuantity += UBFL_Inventory::grantEntryOrDrop(player, heldEntry).quantityDropped;
    }

    // (3) The hotbar bindings return exactly as they were at death: a sword that was on key 1 is on key 1 again.
    inventory->setHotbarBindings(info.contents.hotbar);

    // (4) The worn gear is re-equipped in the places it occupied (Doc B02); Equipment takes each piece out of the bag again.
    services->reEquipGear(player, info.contents.gear);
    UHeldItemComponent* heldItem = UHeldItemComponent::findFor(player);
    if(heldItem != nullptr && !info.contents.wielded.itemId.IsNone()){
        heldItem->holdFromBag(info.contents.wielded);   // the weapon that was in the hand is in the hand again (I18)
    }

    IBPI_BagActorService::Execute_deleteBag(bagService, bagId);   // the record and the actor end; the inventory lives on as the bag
    FS_InventoryResult result;
    result.quantity = droppedQuantity;
    INVENTORY_LOG(Log, player, "Bag %s recovered by %s: %d held item(s) merged, %d unit(s) dropped beside it, %d piece(s) re-equipped", *bagId.ToString(), *GetNameSafe(player), heldItems.Num(), droppedQuantity, info.contents.gear.Num());
    return result;
}

// ---------- S9.6 Bagless helpers ----------

int32 UBFL_InventoryBag::discardBaglessItems(APlayerState* player){
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(!UBFL_Inventory::isServerWorld(player) || inventory == nullptr || inventory->hasBag()){
        return 0;
    }
    const int32 destroyed = inventory->clearHoldSlots();
    INVENTORY_LOG(Log, player, "%s died Bagless: %d held item(s) destroyed", *GetNameSafe(player), destroyed);
    return destroyed;
}

FS_InventoryResult UBFL_InventoryBag::useConsumableFromHoldSlot(APlayerState* player, int32 holdIndex){
    if(!UBFL_Inventory::isServerWorld(player)){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotServer);
    }
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    if(inventory == nullptr || catalog == nullptr || services == nullptr){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    const TArray<FS_BaglessHoldSlot> slots = inventory->getHoldSlots();
    if(inventory->hasBag() || !slots.IsValidIndex(holdIndex) || !slots[holdIndex].isOccupied){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_InvalidSlot);
    }
    const UDA_ItemDefinition* definition = catalog->getDefinition(slots[holdIndex].entry.itemId);
    if(definition == nullptr || !definition->isConsumable()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotConsumable);
    }
    FS_InventoryEntry eaten;
    inventory->takeFromHoldSlot(holdIndex, eaten);   // the item leaves the slot; its spoil time is rebuilt from the clock
    // A ration whose time ran out before it was picked up is the spoiled item, never a fresh one (R01 amendment).
    if(definition->canSpoil && eaten.spoilAtTime > 0 && services->getGameTime() >= eaten.spoilAtTime){
        const UDA_ItemDefinition* spoiled = catalog->getDefinition(definition->spoiledItemId);
        definition = spoiled != nullptr ? spoiled : definition;
    }
    UBFL_Inventory::applyConsumeEffects(player, *definition);
    INVENTORY_LOG(Log, player, "%s consumed %s from hold slot %d", *GetNameSafe(player), *definition->itemId.ToString(), holdIndex);
    FS_InventoryResult result;
    result.quantity = 1;
    return result;
}

FS_InventoryResult UBFL_InventoryBag::dropHoldSlot(APlayerState* player, int32 holdIndex){
    if(!UBFL_Inventory::isServerWorld(player)){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotServer);
    }
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    FS_InventoryEntry dropped;
    if(inventory == nullptr || services == nullptr || inventory->hasBag() || !inventory->takeFromHoldSlot(holdIndex, dropped)){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_InvalidSlot);
    }
    services->spawnLooseItem(player, dropped, UBFL_Inventory::getDropLocation(player));
    FS_InventoryResult result;
    result.quantity = 1;
    return result;
}

// ---------- S9.7 starting kit ----------

FS_InventoryResult UBFL_InventoryBag::grantStartingKit(APlayerState* player){
    if(!UBFL_Inventory::isServerWorld(player)){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotServer);
    }
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(inventory == nullptr){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    if(inventory->isKitGranted()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_KitAlreadyGranted);   // granted once, when the character is created
    }
    inventory->createFreshBag();   // the kit is the bag, with the Axe and the map inside it; does nothing if a bag exists
    FS_InventoryResult result;
    for(const FS_ItemStack& stack : UInventorySettings::get()->startingKit){
        result.quantity += UBFL_Inventory::grantOrDrop(player, stack).quantityStored;
    }
    if(result.quantity < 1){
        // Marked as granted only when at least one item arrived, so a broken data setup can be fixed and tried again.
        INVENTORY_LOG(Error, player, "The starting kit of %s is empty or names items that do not exist; check Project Settings -> Awaken Inventory", *GetNameSafe(player));
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    inventory->setKitGranted(true);
    INVENTORY_LOG(Log, player, "Starting kit granted to %s: %d item(s)", *GetNameSafe(player), result.quantity);
    return result;
}

// ---------- S9.8 fresh character ----------

void UBFL_InventoryBag::startNewCharacter(APlayerState* player, const FGuid& newCharacterId){
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(player);
    if(!UBFL_Inventory::isServerWorld(player) || inventory == nullptr || services == nullptr || registry == nullptr){
        return;
    }
    UBFL_Inventory::releaseAllStaged(player);

    // The old character's bags are removed completely: nobody else can pick them up and the new character
    // is not their owner (designer-confirmed, Index A93).
    UObject* bagService = services->getBagService();
    const FGuid oldCharacterId = inventory->getCharacterId();
    if(bagService != nullptr && oldCharacterId.IsValid()){
        for(const FGuid& inventoryId : IBPI_BagActorService::Execute_removeBagsOwnedBy(bagService, oldCharacterId)){
            registry->destroyInventory(inventoryId);
        }
    }
    UInventoryData* carriedBag = inventory->releaseInventory();
    if(carriedBag != nullptr){
        registry->destroyInventory(carriedBag->getInventoryId());
    }
    inventory->clearHoldSlots();
    inventory->setHotbarBindings(TArray<FS_HotbarBinding>());

    inventory->setCharacterId(newCharacterId);
    inventory->setKitGranted(false);
    grantStartingKit(player);
    INVENTORY_LOG(Log, player, "%s starts a new character %s", *GetNameSafe(player), *newCharacterId.ToString());
}
