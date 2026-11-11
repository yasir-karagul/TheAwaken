#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/ALooseItemActor.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/InventoryTime.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UContainerViewComponent.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

// ---------- helpers ----------

bool UBFL_Inventory::isServerWorld(const UObject* worldContext){
    const UWorld* world = (GEngine != nullptr && worldContext != nullptr) ? GEngine->GetWorldFromContextObject(worldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    return world != nullptr && world->GetNetMode() != NM_Client;
}

FVector UBFL_Inventory::getDropLocation(const APlayerState* player){
    const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
    if(pawn == nullptr){
        // UE 5.8 hides APlayerState::GetActorLocation; a PlayerState has no root, so this is the actor's own (zero) location.
        return player != nullptr ? static_cast<const AActor*>(player)->GetActorLocation() : FVector::ZeroVector;
    }
    return pawn->GetActorLocation() + pawn->GetActorForwardVector() * UInventorySettings::get()->dropForwardDistance;
}

FS_InventoryEntry UBFL_Inventory::makeFreshInstance(const UObject* worldContext, const UDA_ItemDefinition& definition){
    FS_InventoryEntry entry;
    entry.itemId = definition.itemId;
    entry.totalQuantity = 1;
    entry.instanceId = FGuid::NewGuid();              // created with the instance; it never changes (Doc02 19.3)
    entry.state.durability = definition.durabilityMax;   // -1 stays -1: the unbreakable Axe
    if(definition.isPortableContainer()){
        entry.state.containedInventoryId = FGuid::NewGuid();   // the inventory itself is created when first used
    }
    return entry;
}

FS_InventoryEntry UBFL_Inventory::makeEntry(const UObject* worldContext, const UDA_ItemDefinition& definition, int32 quantity, int64 spoilAtTime){
    if(definition.isInstanced()){
        return makeFreshInstance(worldContext, definition);
    }
    FS_InventoryEntry entry;
    entry.itemId = definition.itemId;
    entry.totalQuantity = quantity;
    entry.spoilAtTime = spoilAtTime;
    return entry;
}

bool UBFL_Inventory::canFit(const UInventoryData* inventory, const FS_ItemStack& stack){
    return inventory != nullptr && stack.quantity > 0 && inventory->canFit(stack) == stack.quantity;
}

FS_InventoryResult UBFL_Inventory::refusal(const FGameplayTag& reason){
    FS_InventoryResult result;
    result.failReason = reason;
    return result;
}

void UBFL_Inventory::reportDropped(APlayerState* player, FName itemId, int32 quantity, const FVector& location){
    // Inventory.GrantDropped: server only, then local to the owning player (R01 §6).
    FS_GrantDroppedMessage message;
    message.player = player;
    message.itemId = itemId;
    message.quantity = quantity;
    UGameplayMessageSubsystem::Get(player).BroadcastMessage(T_Inventory_GrantDropped, message);
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(inventory != nullptr){
        inventory->clientGrantDropped(itemId, quantity);
    }
    const UInventoryServices* services = UInventoryServices::get(player);
    if(services != nullptr){
        services->raiseItemDroppedCosmetic(player, location);   // Cosmetic.Event.Interaction.ItemDropped (Index A89)
    }
}

// ---------- S5.4 grantOrDrop ----------

FS_GrantResult UBFL_Inventory::grantOrDrop(APlayerState* player, const FS_ItemStack& stack, int64 spoilStartTime){
    FS_GrantResult result;
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(stack.itemId) : nullptr;
    if(!isServerWorld(player) || services == nullptr || definition == nullptr || stack.quantity < 1){
        INVENTORY_LOG(Warning, player, "grantOrDrop did nothing: not the server, no player, unknown item '%s' or quantity %d", *stack.itemId.ToString(), stack.quantity);
        return result;
    }
    const int64 now = services->getGameTime();
    // The spoil clock starts at the given time, and a start in the future is clamped to now (R01 amendment).
    // The same spoil time is used for the part that is stored and for the part that is dropped.
    const int64 startTime = spoilStartTime < 0 ? now : FMath::Min(spoilStartTime, now);
    const int64 spoilAtTime = definition->canSpoil ? startTime + InventoryTime::hoursToGameSeconds(definition->spoilTimeHours) : 0;
    const FVector dropLocation = getDropLocation(player);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;   // null while Bagless

    if(definition->isHeavy()){
        // Step 0: an item with a carry cost never enters the bag. A free carry slot, otherwise loose beside the player.
        for(int32 unit = 0; unit < stack.quantity; unit++){
            const FS_InventoryEntry entry = makeEntry(player, *definition, 1, spoilAtTime);
            if(services->beginCarry(player, entry)){
                result.quantityCarried++;
            }else{
                services->spawnLooseItem(player, entry, dropLocation);
                result.quantityDropped++;
            }
        }
    }else if(definition->isStackable()){
        // Step 1: store as much as the type's limit allows. Step 2: the rest drops. Nothing is destroyed.
        if(bag != nullptr){
            bag->evaluateSpoilage(now);   // so fresh units are never merged into a stack that is already past its time
            result.quantityStored = bag->addStack(stack.itemId, stack.quantity, spoilAtTime).quantity;
        }
        result.quantityDropped = stack.quantity - result.quantityStored;
        if(result.quantityDropped > 0){
            services->spawnLooseItem(player, makeEntry(player, *definition, result.quantityDropped, spoilAtTime), dropLocation);
        }
    }else{
        for(int32 unit = 0; unit < stack.quantity; unit++){
            const FS_InventoryEntry entry = makeFreshInstance(player, *definition);
            if(bag != nullptr && bag->addInstance(entry).succeeded()){
                result.quantityStored++;
            }else{
                services->spawnLooseItem(player, entry, dropLocation);
                result.quantityDropped++;
            }
        }
    }

    // Step 3: say what happened.
    if(result.quantityDropped > 0){
        reportDropped(player, stack.itemId, result.quantityDropped, dropLocation);
    }
    INVENTORY_LOG(Log, player, "Granted %s x%d to %s: stored %d, carried %d, dropped %d", *stack.itemId.ToString(), stack.quantity, *GetNameSafe(player), result.quantityStored, result.quantityCarried, result.quantityDropped);
    return result;
}

FS_GrantResult UBFL_Inventory::grantEntryOrDrop(APlayerState* player, const FS_InventoryEntry& entry){
    FS_GrantResult result;
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(entry.itemId) : nullptr;
    if(!isServerWorld(player) || services == nullptr || definition == nullptr || entry.totalQuantity < 1){
        return result;
    }
    const FVector dropLocation = getDropLocation(player);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;

    if(definition->isHeavy()){
        if(services->beginCarry(player, entry)){
            result.quantityCarried = entry.totalQuantity;
            return result;
        }
    }else if(bag != nullptr){
        bag->evaluateSpoilage(services->getGameTime());
        if(entry.isInstanced()){
            result.quantityStored = bag->addInstance(entry).succeeded() ? 1 : 0;
        }else{
            result.quantityStored = bag->addStack(entry.itemId, entry.totalQuantity, entry.spoilAtTime).quantity;   // a partly fitting stack goes in in part
        }
    }
    result.quantityDropped = entry.totalQuantity - result.quantityStored;
    if(result.quantityDropped > 0){
        FS_InventoryEntry rest = entry;
        rest.totalQuantity = result.quantityDropped;
        services->spawnLooseItem(player, rest, dropLocation);
        reportDropped(player, entry.itemId, result.quantityDropped, dropLocation);
    }
    return result;
}

// ---------- S5.5 pickup and drop ----------

FS_InventoryResult UBFL_Inventory::pickUpLooseItem(APlayerState* player, ALooseItemActor* looseItem){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(catalog == nullptr || services == nullptr || inventory == nullptr || !IsValid(looseItem)){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    const FS_InventoryEntry entry = looseItem->getEntry();
    const UDA_ItemDefinition* definition = catalog->getDefinition(entry.itemId);
    if(definition == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    FS_InventoryResult result;

    // A heavy item goes to the hands only (R01 3.11).
    if(definition->isHeavy()){
        FS_InventoryEntry unit = entry;
        unit.totalQuantity = 1;
        if(!services->beginCarry(player, unit)){
            return refusal(T_Interaction_FailReason_HandsFull);
        }
        looseItem->reduceQuantity(1);
        result.quantity = 1;
        return result;
    }

    if(!inventory->hasBag()){
        // One item into a free hold slot; a slot never takes a stack, so the rest of a loose stack stays (R01 3.8, 3.11).
        UPlayerInventoryComponent* holder = UPlayerInventoryComponent::findFor(player);
        const int32 holdIndex = holder->findFreeHoldSlot();
        if(holdIndex == INDEX_NONE){
            return refusal(T_Interaction_FailReason_HandsFull);   // a fourth pickup is refused with a short reason (I8)
        }
        FS_InventoryEntry unit = entry;
        unit.totalQuantity = 1;
        holder->putInHoldSlot(holdIndex, unit);
        looseItem->reduceQuantity(1);
        result.quantity = 1;
        result.instanceId = unit.instanceId;
        return result;
    }

    UInventoryData* bag = inventory->getPersonalInventory();
    bag->evaluateSpoilage(services->getGameTime());
    FS_ItemStack wanted;
    wanted.itemId = entry.itemId;
    wanted.quantity = entry.totalQuantity;
    const int32 fits = bag->canFit(wanted);
    if(fits < 1){
        // Refused, and nothing is dropped: the item stays where it lies (R01 3.11, Index A93).
        return refusal(T_Interaction_FailReason_BagFull);
    }
    const FS_InventoryResult added = entry.isInstanced() ? bag->addInstance(entry) : bag->addStack(entry.itemId, fits, entry.spoilAtTime);
    if(!added.succeeded()){
        return added;
    }
    looseItem->reduceQuantity(added.quantity);   // a partly fitting stack: the rest stays (Index A98)
    INVENTORY_LOG(Log, player, "%s picked up %s x%d", *GetNameSafe(player), *entry.itemId.ToString(), added.quantity);
    return added;
}

FS_InventoryResult UBFL_Inventory::takeOutOfBag(APlayerState* player, const FS_ItemRef& ref, int32 quantity, FS_InventoryEntry& outEntry){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(player);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(catalog == nullptr || services == nullptr || registry == nullptr || inventory == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(!inventory->hasBag()){
        return refusal(T_Inventory_FailReason_NoBag);
    }
    UInventoryData* bag = inventory->getPersonalInventory();
    bag->evaluateSpoilage(services->getGameTime());

    if(ref.isInstance()){
        const FS_InventoryEntry* entry = bag->findInstance(ref.instanceId);
        const UDA_ItemDefinition* definition = entry != nullptr ? catalog->getDefinition(entry->itemId) : nullptr;
        if(definition == nullptr){
            return refusal(T_Inventory_FailReason_NotFound);
        }
        if(definition->isPermanent()){
            return refusal(T_Inventory_FailReason_Permanent);          // the Axe is never thrown or dropped (R01 3.10)
        }
        if(!registry->isContainerItemEmpty(*entry)){
            return refusal(T_Inventory_FailReason_ContainerNotEmpty);  // empty it first (R01 3.4a)
        }
        outEntry = *entry;   // copied before the entry is removed
        return bag->removeInstance(ref.instanceId);                    // refuses a staged instance (R01 3.6)
    }

    const UDA_ItemDefinition* definition = catalog->getDefinition(ref.itemId);
    if(definition == nullptr || !definition->isStackable()){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(definition->isPermanent()){
        return refusal(T_Inventory_FailReason_Permanent);              // the map
    }
    const FS_InventoryEntry* stack = bag->findStack(ref.itemId);
    const int64 spoilAtTime = stack != nullptr ? stack->spoilAtTime : 0;   // a split part keeps the spoil time of its stack
    const FS_InventoryResult removed = bag->removeStack(ref.itemId, quantity);   // only the available part can leave
    if(removed.succeeded()){
        outEntry = FS_InventoryEntry();
        outEntry.itemId = ref.itemId;
        outEntry.totalQuantity = removed.quantity;
        outEntry.spoilAtTime = spoilAtTime;
    }
    return removed;
}

FS_InventoryResult UBFL_Inventory::dropFromBag(APlayerState* player, const FS_ItemRef& ref, int32 quantity){
    FS_InventoryEntry taken;
    const FS_InventoryResult result = takeOutOfBag(player, ref, quantity, taken);
    if(result.succeeded()){
        const UInventoryServices* services = UInventoryServices::get(player);
        services->spawnLooseItem(player, taken, getDropLocation(player));
        INVENTORY_LOG(Log, player, "%s dropped %s x%d from the bag", *GetNameSafe(player), *taken.itemId.ToString(), taken.totalQuantity);
    }
    return result;
}

// ---------- S5.6 transfer ----------

FS_InventoryResult UBFL_Inventory::transferEntry(UInventoryData* source, UInventoryData* destination, const FS_ItemRef& ref, int32 quantity){
    if(source == nullptr || destination == nullptr || source == destination){
        return refusal(T_Inventory_FailReason_InvalidRequest);
    }
    if(!isServerWorld(source)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    if(source->isDeathBag() || destination->isDeathBag()){
        return refusal(T_Inventory_FailReason_DeathBagLocked);   // one interaction only: its owner's pick-up hold (R01 3.4)
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(source);
    const UInventoryServices* services = UInventoryServices::get(source);
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(source);
    if(catalog == nullptr || services == nullptr || registry == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    const int64 now = services->getGameTime();
    source->evaluateSpoilage(now);        // a transfer is a request on the stack (R01 3.2a)
    destination->evaluateSpoilage(now);

    const FS_InventoryEntry* instance = ref.isInstance() ? source->findInstance(ref.instanceId) : nullptr;
    const FName itemId = instance != nullptr ? instance->itemId : ref.itemId;
    const UDA_ItemDefinition* definition = catalog->getDefinition(itemId);
    if(definition == nullptr || ref.isInstance() != definition->isInstanced() || (ref.isInstance() && instance == nullptr)){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    const bool intoBag = destination->getInventoryType() == E_InventoryType::Personal;
    if(definition->isPermanent() && !intoBag){
        return refusal(T_Inventory_FailReason_Permanent);        // never moved into another container (R01 3.10)
    }
    if(definition->isPortableContainer()){
        if(destination->getInventoryType() == E_InventoryType::PortableContainer){
            return refusal(T_Inventory_FailReason_NestedContainer);   // never one inside another (R01 3.4a)
        }
        if(!intoBag && !registry->isContainerItemEmpty(*instance)){
            return refusal(T_Inventory_FailReason_ContainerNotEmpty); // one that holds something only goes into a bag
        }
    }
    const FGameplayTag blocked = destination->checkAccepts(*definition);
    if(blocked.IsValid()){
        return refusal(blocked);
    }

    if(instance != nullptr){
        if(instance->reservedQuantity > 0){
            return refusal(T_Inventory_FailReason_ReservedQuantity);
        }
        const FS_InventoryEntry moved = *instance;   // the same instance id and state: it is the same physical item
        const FS_InventoryResult added = destination->addInstance(moved);
        if(!added.succeeded()){
            return added;
        }
        source->removeInstance(moved.instanceId);
        return added;
    }

    const int32 wanted = FMath::Min(quantity, source->getAvailableQuantity(itemId));   // staged units cannot be moved away (Index A103)
    if(wanted < 1){
        return refusal(T_Inventory_FailReason_InsufficientQuantity);
    }
    FS_ItemStack stack;
    stack.itemId = itemId;
    stack.quantity = wanted;
    const int32 fits = destination->canFit(stack);
    if(fits < 1){
        return refusal(destination->getFullReason());
    }
    const int64 spoilAtTime = source->findStack(itemId)->spoilAtTime;
    source->removeStack(itemId, fits);
    return destination->addStack(itemId, fits, spoilAtTime);   // the leftover stays in the source (R01 3.7)
}

// ---------- Section 6: held item, durability, reload ----------

const UDA_ItemDefinition* UBFL_Inventory::getHeldItem(const APlayerState* player){
    const UHeldItemComponent* heldItem = UHeldItemComponent::findFor(player);
    if(heldItem != nullptr){
        return heldItem->getHeldDefinition();
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    return catalog != nullptr ? catalog->getUnarmedDefinition() : nullptr;   // no pawn: still never "no weapon"
}

FS_HeldItem UBFL_Inventory::getHeldState(const APlayerState* player){
    const UHeldItemComponent* heldItem = UHeldItemComponent::findFor(player);
    return heldItem != nullptr ? heldItem->getHeldState() : FS_HeldItem();
}

FS_InventoryResult UBFL_Inventory::applyDurabilityLoss(APlayerState* player, const FGuid& instanceId, float amount){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    const FS_InventoryEntry* entry = bag != nullptr ? bag->findInstance(instanceId) : nullptr;
    const UDA_ItemDefinition* definition = (entry != nullptr && catalog != nullptr) ? catalog->getDefinition(entry->itemId) : nullptr;
    if(definition == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    FS_InventoryResult result;
    result.instanceId = instanceId;
    if(!definition->hasDurability() || amount <= 0.0f){
        return result;   // the unbreakable Axe, or an item with no durability: nothing to do
    }
    FS_ItemInstanceState state = entry->state;
    state.durability = FMath::Max(0.0f, state.durability - amount);
    if(state.durability > 0.0f){
        bag->setInstanceState(instanceId, state);
        return result;
    }
    // Zero durability: the item breaks and disappears. The hand moves to the next copy by itself (refreshFromInventory).
    const FName itemId = entry->itemId;
    const FS_InventoryResult removed = bag->removeInstance(instanceId);
    if(removed.succeeded()){
        result.quantity = 1;
        INVENTORY_LOG(Log, player, "%s of %s broke and is destroyed", *itemId.ToString(), *GetNameSafe(player));
    }
    return result;
}

FS_InventoryResult UBFL_Inventory::applyReload(APlayerState* player, const FGuid& weaponInstanceId){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    if(bag == nullptr){
        return refusal(T_Inventory_FailReason_NoBag);
    }
    const FS_InventoryEntry* weapon = bag->findInstance(weaponInstanceId);
    const UDA_ItemDefinition* definition = (weapon != nullptr && catalog != nullptr) ? catalog->getDefinition(weapon->itemId) : nullptr;
    if(definition == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    FS_ItemInstanceState state = weapon->state;
    int32 wanted = 0;
    if(definition->weaponType == E_WeaponType::Crossbow){
        wanted = state.isLoaded ? 0 : 1;                                   // one arrow
    }else if(definition->weaponType == E_WeaponType::Firearm){
        wanted = definition->magazineSize - state.loadedRounds;            // up to MagazineSize
    }
    if(wanted < 1){
        return refusal(T_Inventory_FailReason_CannotReload);
    }
    const int32 loaded = FMath::Min(wanted, bag->getAvailableQuantity(definition->ammoItemId));
    if(loaded < 1){
        return refusal(T_Inventory_FailReason_NoAmmo);
    }
    // Everything checked: spend the ammunition, then write the loaded state. One bullet per round loaded.
    const FS_InventoryResult spent = bag->removeStack(definition->ammoItemId, loaded);
    if(!spent.succeeded()){
        return spent;
    }
    if(definition->weaponType == E_WeaponType::Crossbow){
        state.isLoaded = true;
    }else{
        state.loadedRounds += loaded;
    }
    bag->setInstanceState(weaponInstanceId, state);
    FS_InventoryResult result;
    result.quantity = loaded;
    result.instanceId = weaponInstanceId;
    INVENTORY_LOG(Log, player, "%s reloaded %s with %d", *GetNameSafe(player), *definition->itemId.ToString(), loaded);
    return result;
}

FS_InventoryResult UBFL_Inventory::spendLoadedShot(APlayerState* player, const FGuid& weaponInstanceId){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    const FS_InventoryEntry* weapon = bag != nullptr ? bag->findInstance(weaponInstanceId) : nullptr;
    if(weapon == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    FS_ItemInstanceState state = weapon->state;
    if(state.isLoaded){
        state.isLoaded = false;
    }else if(state.loadedRounds > 0){
        state.loadedRounds--;
    }else{
        return refusal(T_Inventory_FailReason_NoAmmo);   // nothing is loaded
    }
    bag->setInstanceState(weaponInstanceId, state);
    FS_InventoryResult result;
    result.quantity = 1;
    result.instanceId = weaponInstanceId;
    return result;
}

// ---------- Section 7: craft staging ----------

FS_InventoryResult UBFL_Inventory::reserveForStaging(APlayerState* player, const FS_ItemRef& ref, int32 quantity, const FGuid& stationId){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(player);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(catalog == nullptr || services == nullptr || registry == nullptr || inventory == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    UInventoryData* bag = inventory->getPersonalInventory();
    if(bag == nullptr){
        return refusal(T_Inventory_FailReason_NoBag);   // staging draws only from the bag; a Bagless player cannot craft (R01 3.8)
    }
    bag->evaluateSpoilage(services->getGameTime());

    const FS_InventoryEntry* named = ref.isInstance() ? bag->findInstance(ref.instanceId) : nullptr;
    const FName itemId = named != nullptr ? named->itemId : ref.itemId;
    const UDA_ItemDefinition* definition = catalog->getDefinition(itemId);
    if(definition == nullptr || (ref.isInstance() && named == nullptr)){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(definition->isPermanent()){
        return refusal(T_Inventory_FailReason_Permanent);   // the Axe and the map are never staged (R01 3.10)
    }
    if(definition->isStackable()){
        return bag->reserveStack(itemId, quantity, stationId);
    }

    const FGuid wieldedId = getHeldState(player).instanceId;
    FGuid chosenId = ref.instanceId;
    if(!chosenId.IsValid()){
        chosenId = bag->findMostWornInstance(itemId, wieldedId);   // the most worn one first (Doc R02 3.10)
        if(!chosenId.IsValid()){
            // No free copy. Say why: the only one left is in the hand, or there is none.
            return refusal(bag->findInstance(wieldedId) != nullptr && bag->findInstance(wieldedId)->itemId == itemId ? T_Inventory_FailReason_ItemWielded : T_Inventory_FailReason_InsufficientQuantity);
        }
    }else if(chosenId == wieldedId){
        return refusal(T_Inventory_FailReason_ItemWielded);   // an equipped weapon cannot be staged
    }
    if(!registry->isContainerItemEmpty(*bag->findInstance(chosenId))){
        return refusal(T_Inventory_FailReason_ContainerNotEmpty);   // never a crafting ingredient while it holds something (R01 3.4a)
    }
    return bag->reserveInstance(chosenId, stationId);
}

FS_InventoryResult UBFL_Inventory::releaseStagedItem(APlayerState* player, const FS_ItemRef& ref, int32 quantity){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    if(bag == nullptr){
        return refusal(T_Inventory_FailReason_NoBag);
    }
    FS_InventoryResult result;
    result.quantity = bag->releaseItem(ref, quantity);
    if(result.quantity < 1){
        result.failReason = T_Inventory_FailReason_NotFound;   // nothing of that item was staged
    }
    return result;
}

int32 UBFL_Inventory::releaseStagedReservation(APlayerState* player, const FGuid& stationId){
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = (isServerWorld(player) && inventory != nullptr) ? inventory->getPersonalInventory() : nullptr;
    return bag != nullptr ? bag->releaseStation(stationId) : 0;
}

int32 UBFL_Inventory::releaseAllStaged(APlayerState* player){
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = (isServerWorld(player) && inventory != nullptr) ? inventory->getPersonalInventory() : nullptr;
    const int32 released = bag != nullptr ? bag->releaseAll() : 0;
    if(released > 0){
        INVENTORY_LOG(Log, player, "Released %d staged unit(s) of %s", released, *GetNameSafe(player));
    }
    return released;
}

FS_InventoryResult UBFL_Inventory::consumeStaged(APlayerState* player, const FGuid& stationId, const FS_ItemRef& ref, int32 quantity){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    if(bag == nullptr){
        return refusal(T_Inventory_FailReason_NoBag);
    }
    return bag->consumeReserved(stationId, ref, quantity);
}

void UBFL_Inventory::handlePlayerLeft(APlayerState* player){
    releaseAllStaged(player);
    UContainerViewComponent* view = UContainerViewComponent::findFor(player);
    if(view != nullptr){
        view->closeContainer();
    }
}

// ---------- Section 8: consumables ----------

void UBFL_Inventory::buildConsumeEffects(const UObject* worldContext, const UDA_ItemDefinition& definition, TArray<FS_ConsumeEffect>& outEffects){
    outEffects.Reset();
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(worldContext);
    if(catalog == nullptr){
        return;
    }
    // A variant derives its meter values from a base item (R01 3.2a, 3.4):
    //  - unclean water names its base itself and scales by its own uncleanWaterEffectiveness;
    //  - spoiled food is found through the fresh item that names it, and scales by that item's spoiledEffectiveness.
    const UDA_ItemDefinition* base = nullptr;
    float scale = 1.0f;
    if(!definition.effectBaseItemId.IsNone()){
        base = catalog->getDefinition(definition.effectBaseItemId);
        scale = definition.uncleanWaterEffectiveness;
    }else{
        base = catalog->findFreshItemFor(definition.itemId);
        scale = base != nullptr ? base->spoiledEffectiveness : 1.0f;
    }
    if(base != nullptr){
        for(const FS_ConsumeEffect& effect : base->consumeEffects){
            if(effect.kind == E_ConsumeEffectKind::ModifyMeter){
                FS_ConsumeEffect scaled = effect;
                scaled.amount *= scale;   // fresh meat gives 100 hunger, spoiled meat gives 50 (the designer's rule)
                outEffects.Add(scaled);
            }
        }
    }
    outEffects.Append(definition.consumeEffects);   // the item's own entries: for a variant, the illness chance and the HP loss
}

void UBFL_Inventory::applyConsumeEffects(APlayerState* player, const UDA_ItemDefinition& definition){
    const UInventoryServices* services = UInventoryServices::get(player);
    if(services == nullptr){
        return;
    }
    TArray<FS_ConsumeEffect> effects;
    buildConsumeEffects(player, definition, effects);
    const bool isIllnessActive = services->isIllnessActive(player);
    for(const FS_ConsumeEffect& effect : effects){
        switch(effect.kind){
            case E_ConsumeEffectKind::ModifyMeter:{
                services->modifyMeter(player, effect.targetTag, effect.amount, definition.categoryTag);
                break;
            }
            case E_ConsumeEffectKind::IllnessChance:{
                // The chance decides whether the illness source is registered (Doc B03 3.4, Index A96).
                if(isIllnessActive && services->rollChance(effect.chance)){
                    services->registerIllnessSource(player, effect.targetTag, effect.sourceDurationHours);
                }
                break;
            }
            case E_ConsumeEffectKind::HealthLoss:{
                // Every time, not by the chance. Through the normal damage path, which applies the Peaceful floor
                // and refuses during a dialogue (R01 3.2a, Doc R03 3.9).
                if(isIllnessActive && effect.amount > 0.0f){
                    services->damagePlayer(player, effect.amount, T_DamageType_Illness);
                }
                break;
            }
        }
    }
    // Medicine (R01 3.9): the amounts are data on the definition.
    if(definition.healthRestoreAmount > 0.0f){
        services->healPlayer(player, definition.healthRestoreAmount, T_Stats_Source_Medicine);
    }
    if(definition.illnessReductionAmount > 0.0f){
        services->modifyMeter(player, T_Survival_Meter_Illness, -definition.illnessReductionAmount, T_Stats_Source_Medicine);
    }
}

FS_InventoryResult UBFL_Inventory::useConsumable(APlayerState* player, FName itemId){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(catalog == nullptr || services == nullptr || inventory == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    UInventoryData* bag = inventory->getPersonalInventory();
    if(bag == nullptr){
        return refusal(T_Inventory_FailReason_NoBag);   // a Bagless player eats from a hold slot (Section 9)
    }
    // FIRST: a stack past its spoil time is the spoiled item from now on. So an eat at the exact boundary
    // uses the server's answer, and a key bound to the fresh item never eats the spoiled one (R01 3.2a, 3.5).
    bag->evaluateSpoilage(services->getGameTime());

    const UDA_ItemDefinition* definition = catalog->getDefinition(itemId);
    if(definition == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(!definition->isConsumable() || !definition->isStackable()){
        return refusal(T_Inventory_FailReason_NotConsumable);
    }
    const FS_InventoryResult removed = bag->removeStack(itemId, 1);   // only an available unit can be consumed
    if(!removed.succeeded()){
        return removed;
    }
    applyConsumeEffects(player, *definition);
    INVENTORY_LOG(Log, player, "%s consumed %s", *GetNameSafe(player), *itemId.ToString());
    return removed;
}
