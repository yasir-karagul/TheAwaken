#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

UHeldItemComponent::UHeldItemComponent(){
    SetIsReplicatedByDefault(true);
}

void UHeldItemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UHeldItemComponent, held);   // COND_None
}

UHeldItemComponent* UHeldItemComponent::findFor(const APlayerState* player){
    const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
    return pawn != nullptr ? pawn->FindComponentByClass<UHeldItemComponent>() : nullptr;
}

bool UHeldItemComponent::isServerSide() const{
    return GetOwner() != nullptr && GetOwner()->HasAuthority();
}

APlayerState* UHeldItemComponent::getPlayerState() const{
    const APawn* pawn = Cast<APawn>(GetOwner());
    return pawn != nullptr ? pawn->GetPlayerState() : nullptr;
}

// ---------- queries ----------

FS_HeldItem UHeldItemComponent::getHeldState() const{
    return held;
}

const UDA_ItemDefinition* UHeldItemComponent::getHeldDefinition() const{
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    if(catalog == nullptr){
        return nullptr;
    }
    const UDA_ItemDefinition* wielded = held.isWielded ? catalog->getDefinition(held.itemId) : nullptr;
    return wielded != nullptr ? wielded : catalog->getUnarmedDefinition();   // the fallback of R01 3.10
}

const UDA_ItemDefinition* UHeldItemComponent::getShownDefinition() const{
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    return (catalog != nullptr && !held.isEmpty()) ? catalog->getDefinition(held.itemId) : nullptr;
}

bool UHeldItemComponent::isUnarmed() const{
    return !held.isWielded;
}

// ---------- commands ----------

FS_InventoryResult UHeldItemComponent::selectHotbarSlot(int32 slotIndex){
    if(!isServerSide()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotServer);
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getPlayerState());
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    if(inventory == nullptr || catalog == nullptr){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    if(!inventory->isHotbarSlotUsable(slotIndex)){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_InvalidSlot);
    }
    const FS_ResolvedHotbarSlot resolved = inventory->resolveHotbarSlot(slotIndex);
    const UDA_ItemDefinition* definition = resolved.isUsable ? catalog->getDefinition(resolved.itemId) : nullptr;
    if(definition == nullptr){
        putAway();   // an empty slot, or a slot whose pool is gone: the hand is empty
        return FS_InventoryResult();
    }
    FS_HeldItem newHeld;
    newHeld.itemId = resolved.itemId;
    newHeld.instanceId = resolved.instanceId;
    newHeld.sourceSlot = slotIndex;
    // Only a weapon held by a player who has the bag is wielded (I10; a Bagless player fights Unarmed, R01 3.8).
    newHeld.isWielded = inventory->hasBag() && definition->isWeapon();
    setHeld(newHeld);
    return FS_InventoryResult();
}

FS_InventoryResult UHeldItemComponent::holdFromBag(const FS_ItemRef& ref){
    if(!isServerSide()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotServer);
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getPlayerState());
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    if(inventory == nullptr || catalog == nullptr){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    const UInventoryData* bag = inventory->getPersonalInventory();
    if(bag == nullptr){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NoBag);
    }
    const FS_InventoryEntry* instance = ref.isInstance() ? bag->findInstance(ref.instanceId) : nullptr;
    const FName itemId = instance != nullptr ? instance->itemId : ref.itemId;
    const UDA_ItemDefinition* definition = catalog->getDefinition(itemId);
    if(definition == nullptr || (ref.isInstance() && instance == nullptr)){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    FS_HeldItem newHeld;
    newHeld.itemId = itemId;
    if(definition->isInstanced()){
        newHeld.instanceId = instance != nullptr ? instance->instanceId : inventory->findUsableInstance(itemId);
        const FS_InventoryEntry* chosen = bag->findInstance(newHeld.instanceId);
        if(chosen == nullptr){
            return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
        }
        if(chosen->reservedQuantity > 0){
            return UBFL_Inventory::refusal(T_Inventory_FailReason_ReservedQuantity);   // it is staged for a craft
        }
    }else if(bag->getAvailableQuantity(itemId) < 1){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_InsufficientQuantity);
    }
    newHeld.isWielded = definition->isWeapon();
    setHeld(newHeld);
    return FS_InventoryResult();
}

void UHeldItemComponent::putAway(){
    if(isServerSide()){
        setHeld(FS_HeldItem());
    }
}

void UHeldItemComponent::refreshFromInventory(){
    if(!isServerSide() || held.isEmpty()){
        return;
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getPlayerState());
    const UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    if(bag == nullptr){
        // Bagless: the hand shows a hold slot. Keep it only while that slot still holds the same item.
        const FS_ResolvedHotbarSlot shown = inventory != nullptr ? inventory->resolveHotbarSlot(held.sourceSlot) : FS_ResolvedHotbarSlot();
        if(!shown.isUsable || shown.itemId != held.itemId || shown.instanceId != held.instanceId || held.isWielded){
            putAway();
        }
        return;
    }
    if(held.instanceId.IsValid()){
        if(bag->findInstance(held.instanceId) != nullptr){
            return;   // still there (its durability may have changed; that is not a change of what is held)
        }
        // Broken, thrown, stored or given away: the next most recently used copy takes over (R01 3.5, PROPOSED there).
        const FGuid next = inventory->findUsableInstance(held.itemId);
        if(next.IsValid()){
            FS_HeldItem newHeld = held;
            newHeld.instanceId = next;
            setHeld(newHeld);
            return;
        }
        putAway();
        return;
    }
    if(bag->getAvailableQuantity(held.itemId) < 1){
        putAway();   // the last unit of the pool was used or staged
    }
}

void UHeldItemComponent::setHeld(const FS_HeldItem& newHeld){
    if(newHeld == held){
        return;
    }
    held = newHeld;
    APlayerState* player = getPlayerState();
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(inventory != nullptr){
        inventory->noteInstanceUsed(held.instanceId);
    }
    // A weapon swap cancels a reload in progress (Doc R04 3.3, R04-R08-IG17). Server only; Hotbar.Changed is not the trigger.
    const UInventoryServices* services = UInventoryServices::get(this);
    if(services != nullptr && player != nullptr){
        services->cancelAction(player, T_Action_Reload);
    }
    INVENTORY_LOG(Log, this, "%s now holds %s%s", *GetNameSafe(player), held.isEmpty() ? TEXT("nothing") : *held.itemId.ToString(), held.isWielded ? TEXT(" (wielded)") : TEXT(""));
    onHeldItemChanged.Broadcast();
}

void UHeldItemComponent::OnRep_held(){
    onHeldItemChanged.Broadcast();
}
