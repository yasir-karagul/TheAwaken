#include "ItemInventoryFramework/UContainerViewComponent.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UInventoryRequestComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UContainerViewComponent::UContainerViewComponent(){
    SetIsReplicatedByDefault(true);
}

void UContainerViewComponent::BeginPlay(){
    Super::BeginPlay();
    if(!isServerSide()){
        clientView = NewObject<UInventoryData>(this);
        clientView->setup(viewedContainerId, E_InventoryType::Container, FS_ContainerRules());
        replicatedView.onReceived.BindUObject(this, &UContainerViewComponent::handleMirrorReceived);
    }
}

void UContainerViewComponent::EndPlay(const EEndPlayReason::Type EndPlayReason){
    closeContainer();
    Super::EndPlay(EndPlayReason);
}

void UContainerViewComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UContainerViewComponent, viewedContainerId, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UContainerViewComponent, replicatedView, COND_OwnerOnly);
}

UContainerViewComponent* UContainerViewComponent::findFor(const APlayerState* player){
    return player != nullptr ? player->FindComponentByClass<UContainerViewComponent>() : nullptr;
}

bool UContainerViewComponent::isServerSide() const{
    return GetOwner() != nullptr && GetOwner()->HasAuthority();
}

FGuid UContainerViewComponent::getViewedContainerId() const{
    return viewedContainerId;
}

UInventoryData* UContainerViewComponent::getViewedInventory() const{
    if(!viewedContainerId.IsValid()){
        return nullptr;
    }
    return isServerSide() ? viewedInventory.Get() : clientView.Get();
}

TArray<FS_InventoryEntry> UContainerViewComponent::getViewedEntries() const{
    const UInventoryData* inventory = getViewedInventory();
    return inventory != nullptr ? inventory->getEntries() : TArray<FS_InventoryEntry>();
}

FS_InventoryResult UContainerViewComponent::openContainer(const FGuid& containerId){
    if(!isServerSide()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotServer);
    }
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    const UInventoryServices* services = UInventoryServices::get(this);
    UInventoryData* inventory = registry != nullptr ? registry->findInventory(containerId) : nullptr;
    if(inventory == nullptr || services == nullptr){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    if(inventory->isDeathBag()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_DeathBagLocked);   // nobody opens a death bag (R01 3.4)
    }
    closeContainer();   // one view at a time
    viewedInventory = inventory;
    viewedContainerId = containerId;
    inventory->evaluateSpoilage(services->getGameTime());   // opening is a request on the stacks (R01 3.2a)
    changedHandle = inventory->onChanged.AddUObject(this, &UContainerViewComponent::handleViewedChanged);
    handleViewedChanged(inventory);
    GetWorld()->GetTimerManager().SetTimer(reachTimer, this, &UContainerViewComponent::checkReach, 0.5f, true);
    onViewChanged.Broadcast(viewedContainerId);
    INVENTORY_LOG(Log, this, "%s opened container %s", *GetNameSafe(GetOwner()), *containerId.ToString());
    return FS_InventoryResult();
}

void UContainerViewComponent::closeContainer(){
    if(!isServerSide() || !viewedContainerId.IsValid()){
        return;
    }
    if(viewedInventory != nullptr){
        viewedInventory->onChanged.Remove(changedHandle);
    }
    changedHandle.Reset();
    if(GetWorld() != nullptr){
        GetWorld()->GetTimerManager().ClearTimer(reachTimer);
    }
    viewedInventory = nullptr;
    viewedContainerId.Invalidate();
    replicatedView.syncFrom(TArray<FS_InventoryEntry>());
    GetOwner()->ForceNetUpdate();
    onViewChanged.Broadcast(viewedContainerId);
}

void UContainerViewComponent::handleViewedChanged(UInventoryData* inventory){
    replicatedView.syncFrom(inventory->getEntries());
    GetOwner()->ForceNetUpdate();
}

void UContainerViewComponent::handleMirrorReceived(){
    if(clientView != nullptr){
        clientView->setup(viewedContainerId, E_InventoryType::Container, FS_ContainerRules());
        clientView->replaceEntries(replicatedView.toEntries());   // raises Inventory.Changed with the container's id, on this machine
    }
}

void UContainerViewComponent::OnRep_viewedContainerId(){
    handleMirrorReceived();
    onViewChanged.Broadcast(viewedContainerId);
}

void UContainerViewComponent::checkReach(){
    APlayerState* player = Cast<APlayerState>(GetOwner());
    const UInventoryRequestComponent* requests = UInventoryRequestComponent::findFor(player);
    if(requests == nullptr || !requests->isContainerReachable(player, viewedContainerId)){
        closeContainer();   // too far away, or the container is gone (Doc B17 3.2)
    }
}
