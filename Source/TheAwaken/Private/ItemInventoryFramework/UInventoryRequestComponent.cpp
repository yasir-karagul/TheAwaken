#include "ItemInventoryFramework/UInventoryRequestComponent.h"
#include "ItemInventoryFramework/ALooseItemActor.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UBFL_InventoryBag.h"
#include "ItemInventoryFramework/UContainerComponent.h"
#include "ItemInventoryFramework/UContainerViewComponent.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

UInventoryRequestComponent::UInventoryRequestComponent(){
    SetIsReplicatedByDefault(true);
}

void UInventoryRequestComponent::BeginPlay(){
    Super::BeginPlay();
    requestTokens = UInventorySettings::get()->requestsPerSecond;
    lastTokenTime = FPlatformTime::Seconds();
}

UInventoryRequestComponent* UInventoryRequestComponent::findFor(const APlayerState* player){
    return player != nullptr ? player->FindComponentByClass<UInventoryRequestComponent>() : nullptr;
}

APlayerState* UInventoryRequestComponent::getOwnerPlayerState() const{
    return Cast<APlayerState>(GetOwner());
}

// ---------- S12.3 the gate ----------

FGameplayTag UInventoryRequestComponent::gate(bool needsHands, bool allowWhenIncapacitated){
    // A token bucket on real time (a rate limit is a short timer, Doc02 §19.4). It refills at requestsPerSecond
    // and holds at most one second's worth.
    const float ratePerSecond = UInventorySettings::get()->requestsPerSecond;
    const double now = FPlatformTime::Seconds();
    requestTokens = FMath::Min(ratePerSecond, requestTokens + static_cast<float>(now - lastTokenTime) * ratePerSecond);
    lastTokenTime = now;
    if(requestTokens < 1.0f){
        return T_Inventory_FailReason_RateLimited;
    }
    requestTokens -= 1.0f;

    const UInventoryServices* services = UInventoryServices::get(this);
    const APlayerState* player = getOwnerPlayerState();
    if(services == nullptr || player == nullptr){
        return T_Inventory_FailReason_NotFound;
    }
    if(!allowWhenIncapacitated && services->isIncapacitated(player)){
        return T_Interaction_FailReason_NotAllowedInState;
    }
    if(needsHands && services->isCarryActive(player)){
        return T_Interaction_FailReason_HandsFull;
    }
    return FGameplayTag();
}

bool UInventoryRequestComponent::isInReach(const APlayerState* player, const AActor* target, float range) const{
    const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
    if(pawn == nullptr || !IsValid(target)){
        return false;
    }
    const FVector eye = pawn->GetPawnViewLocation();
    const FVector targetLocation = target->GetActorLocation();
    if(FVector::Dist(eye, targetLocation) > range){   // inclusive: exactly at the limit is allowed (Doc02 §19.9)
        return false;
    }
    FHitResult hit;
    FCollisionQueryParams parameters(SCENE_QUERY_STAT(InventoryReach), false, pawn);
    const bool isBlocked = GetWorld()->LineTraceSingleByChannel(hit, eye, targetLocation, ECC_Visibility, parameters);
    return !isBlocked || hit.GetActor() == target;
}

bool UInventoryRequestComponent::isContainerReachable(APlayerState* player, const FGuid& containerId) const{
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    if(registry == nullptr || !containerId.IsValid()){
        return false;
    }
    const UContainerComponent* host = registry->findHost(containerId);
    if(host != nullptr){
        return isInReach(player, host->GetOwner(), UInventorySettings::get()->containerRange);
    }
    // Not a loaded world container: it is reachable only as the contents of a Portable Container in the player's own bag.
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    if(bag == nullptr){
        return false;
    }
    return bag->getEntries().ContainsByPredicate([&containerId](const FS_InventoryEntry& entry){
        return entry.state.containedInventoryId == containerId;
    });
}

void UInventoryRequestComponent::refuse(const FGameplayTag& reason, const TCHAR* operation){
    INVENTORY_LOG(Log, this, "%s of %s refused: %s", operation, *GetNameSafe(GetOwner()), *reason.ToString());
    clientRefused(reason);
}

void UInventoryRequestComponent::clientRefused_Implementation(FGameplayTag reason){
    onRequestRefused.Broadcast(reason);
}

void UInventoryRequestComponent::reply(const FS_InventoryResult& result, const TCHAR* operation){
    if(!result.succeeded()){
        refuse(result.failReason, operation);
    }
}

// ---------- S12.4 hotbar, hand, consumables, drop, pickup ----------

void UInventoryRequestComponent::serverBindHotbarSlot_Implementation(int32 slotIndex, FName itemId){
    const FGameplayTag blocked = gate(false, false);
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getOwnerPlayerState());
    if(blocked.IsValid() || inventory == nullptr){
        refuse(blocked.IsValid() ? blocked : T_Inventory_FailReason_NotFound.GetTag(), TEXT("bindHotbarSlot"));
        return;
    }
    reply(inventory->bindHotbarSlot(slotIndex, itemId), TEXT("bindHotbarSlot"));
}

void UInventoryRequestComponent::serverSelectHotbarSlot_Implementation(int32 slotIndex){
    const FGameplayTag blocked = gate(false, false);
    UHeldItemComponent* heldItem = UHeldItemComponent::findFor(getOwnerPlayerState());
    if(blocked.IsValid() || heldItem == nullptr){
        refuse(blocked.IsValid() ? blocked : T_Inventory_FailReason_NotFound.GetTag(), TEXT("selectHotbarSlot"));
        return;
    }
    reply(heldItem->selectHotbarSlot(slotIndex), TEXT("selectHotbarSlot"));
}

void UInventoryRequestComponent::serverHoldFromBag_Implementation(const FS_ItemRef& ref){
    const FGameplayTag blocked = gate(false, false);
    UHeldItemComponent* heldItem = UHeldItemComponent::findFor(getOwnerPlayerState());
    if(blocked.IsValid() || heldItem == nullptr){
        refuse(blocked.IsValid() ? blocked : T_Inventory_FailReason_NotFound.GetTag(), TEXT("holdFromBag"));
        return;
    }
    reply(heldItem->holdFromBag(ref), TEXT("holdFromBag"));
}

void UInventoryRequestComponent::serverPutAway_Implementation(){
    const FGameplayTag blocked = gate(false, false);
    UHeldItemComponent* heldItem = UHeldItemComponent::findFor(getOwnerPlayerState());
    if(blocked.IsValid()){
        refuse(blocked, TEXT("putAway"));
        return;
    }
    if(heldItem != nullptr){
        heldItem->putAway();
    }
}

void UInventoryRequestComponent::serverUseConsumable_Implementation(FName itemId){
    const FGameplayTag blocked = gate(true, false);   // busy hands cannot eat (Index A103, S4-F36)
    if(blocked.IsValid()){
        refuse(blocked, TEXT("useConsumable"));
        return;
    }
    reply(UBFL_Inventory::useConsumable(getOwnerPlayerState(), itemId), TEXT("useConsumable"));
}

void UInventoryRequestComponent::serverUseHoldSlot_Implementation(int32 slotIndex){
    const FGameplayTag blocked = gate(true, false);
    if(blocked.IsValid()){
        refuse(blocked, TEXT("useHoldSlot"));
        return;
    }
    reply(UBFL_InventoryBag::useConsumableFromHoldSlot(getOwnerPlayerState(), slotIndex - 1), TEXT("useHoldSlot"));
}

void UInventoryRequestComponent::serverDropHoldSlot_Implementation(int32 slotIndex){
    const FGameplayTag blocked = gate(false, false);
    if(blocked.IsValid()){
        refuse(blocked, TEXT("dropHoldSlot"));
        return;
    }
    reply(UBFL_InventoryBag::dropHoldSlot(getOwnerPlayerState(), slotIndex - 1), TEXT("dropHoldSlot"));
}

void UInventoryRequestComponent::serverDropFromBag_Implementation(const FS_ItemRef& ref, int32 quantity){
    const FGameplayTag blocked = gate(false, false);
    if(blocked.IsValid()){
        refuse(blocked, TEXT("dropFromBag"));
        return;
    }
    reply(UBFL_Inventory::dropFromBag(getOwnerPlayerState(), ref, quantity), TEXT("dropFromBag"));
}

void UInventoryRequestComponent::serverPickUp_Implementation(ALooseItemActor* looseItem){
    APlayerState* player = getOwnerPlayerState();
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UDA_ItemDefinition* definition = (IsValid(looseItem) && catalog != nullptr) ? catalog->getDefinition(looseItem->getEntry().itemId) : nullptr;
    // A heavy item goes TO the hands, so carrying does not block the request itself: Carry answers whether a slot is free.
    const bool needsHands = definition == nullptr || !definition->isHeavy();
    const FGameplayTag blocked = gate(needsHands, false);
    if(blocked.IsValid()){
        refuse(blocked, TEXT("pickUp"));
        return;
    }
    if(!isInReach(player, looseItem, UInventorySettings::get()->pickupRange)){
        refuse(T_Inventory_FailReason_OutOfRange, TEXT("pickUp"));
        return;
    }
    reply(UBFL_Inventory::pickUpLooseItem(player, looseItem), TEXT("pickUp"));
}

// ---------- S12.5 containers ----------

void UInventoryRequestComponent::serverOpenContainer_Implementation(AActor* containerActor){
    APlayerState* player = getOwnerPlayerState();
    const FGameplayTag blocked = gate(true, false);
    if(blocked.IsValid()){
        refuse(blocked, TEXT("openContainer"));
        return;
    }
    const UContainerComponent* container = IsValid(containerActor) ? containerActor->FindComponentByClass<UContainerComponent>() : nullptr;
    UContainerViewComponent* view = UContainerViewComponent::findFor(player);
    if(container == nullptr || view == nullptr){
        refuse(T_Inventory_FailReason_NotFound, TEXT("openContainer"));
        return;
    }
    if(!UBFL_InventoryBag::hasBag(player)){
        refuse(T_Inventory_FailReason_NoBag, TEXT("openContainer"));   // a Bagless player cannot use container storage (R01 3.8)
        return;
    }
    if(!isContainerReachable(player, container->getContainerId())){
        refuse(T_Inventory_FailReason_OutOfRange, TEXT("openContainer"));
        return;
    }
    reply(view->openContainer(container->getContainerId()), TEXT("openContainer"));
}

void UInventoryRequestComponent::serverOpenPortableContainer_Implementation(FGuid instanceId){
    APlayerState* player = getOwnerPlayerState();
    const FGameplayTag blocked = gate(true, false);
    if(blocked.IsValid()){
        refuse(blocked, TEXT("openPortableContainer"));
        return;
    }
    UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UContainerViewComponent* view = UContainerViewComponent::findFor(player);
    const UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    if(bag == nullptr){
        refuse(T_Inventory_FailReason_NoBag, TEXT("openPortableContainer"));
        return;
    }
    const FS_InventoryEntry* entry = bag->findInstance(instanceId);
    // The contents are created the first time the container is opened, with the rules of its item definition.
    const UInventoryData* contained = (entry != nullptr && registry != nullptr) ? registry->getContainedInventory(*entry, true) : nullptr;
    if(contained == nullptr || view == nullptr){
        refuse(T_Inventory_FailReason_NotFound, TEXT("openPortableContainer"));
        return;
    }
    reply(view->openContainer(contained->getInventoryId()), TEXT("openPortableContainer"));
}

void UInventoryRequestComponent::serverCloseContainer_Implementation(){
    UContainerViewComponent* view = UContainerViewComponent::findFor(getOwnerPlayerState());
    if(view != nullptr){
        view->closeContainer();
    }
}

void UInventoryRequestComponent::transferWith(const FGuid& containerId, const FS_ItemRef& ref, int32 quantity, bool toContainer, const TCHAR* operation){
    APlayerState* player = getOwnerPlayerState();
    const FGameplayTag blocked = gate(true, false);
    if(blocked.IsValid()){
        refuse(blocked, operation);
        return;
    }
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    UInventoryData* container = registry != nullptr ? registry->findInventory(containerId) : nullptr;
    if(bag == nullptr){
        refuse(T_Inventory_FailReason_NoBag, operation);
        return;
    }
    if(container == nullptr){
        refuse(T_Inventory_FailReason_NotFound, operation);
        return;
    }
    // Checked again on every move: the player may have walked away since the container was opened (Doc02 §19.6).
    if(!isContainerReachable(player, containerId)){
        refuse(T_Inventory_FailReason_OutOfRange, operation);
        return;
    }
    reply(UBFL_Inventory::transferEntry(toContainer ? bag : container, toContainer ? container : bag, ref, quantity), operation);
}

void UInventoryRequestComponent::serverTransfer_Implementation(const FS_ItemRef& ref, int32 quantity, bool toContainer){
    const UContainerViewComponent* view = UContainerViewComponent::findFor(getOwnerPlayerState());
    transferWith(view != nullptr ? view->getViewedContainerId() : FGuid(), ref, quantity, toContainer, TEXT("transfer"));
}

void UInventoryRequestComponent::serverQuickTransfer_Implementation(AActor* containerActor, const FS_ItemRef& ref, int32 quantity, bool toContainer){
    const UContainerComponent* container = IsValid(containerActor) ? containerActor->FindComponentByClass<UContainerComponent>() : nullptr;
    transferWith(container != nullptr ? container->getContainerId() : FGuid(), ref, quantity, toContainer, TEXT("quickTransfer"));
}
