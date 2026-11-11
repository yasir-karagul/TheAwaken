#include "ItemInventoryFramework/UContainerComponent.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/InventoryLog.h"

void UContainerComponent::BeginPlay(){
    Super::BeginPlay();
    if(GetOwner() == nullptr || !GetOwner()->HasAuthority()){
        return;
    }
    UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    if(registry == nullptr){
        return;
    }
    if(!containerId.IsValid()){
        containerId = FGuid::NewGuid();
        INVENTORY_LOG(Log, this, "Container %s had no id and received %s. A designer-placed container must have its id set in the editor, or it will not find its contents after a load", *GetNameSafe(GetOwner()), *containerId.ToString());
    }
    // Found again when the area streams back in: the data never left (Doc02 18.1).
    UInventoryData* inventory = registry->findOrCreateInventory(E_InventoryType::Container, containerId, rules);
    if(inventory != nullptr){
        inventory->setup(containerId, E_InventoryType::Container, rules);   // the rules always come from the loaded actor
    }
    registry->registerHost(containerId, this);
}

void UContainerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason){
    UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    if(registry != nullptr && GetOwner() != nullptr && GetOwner()->HasAuthority()){
        registry->unregisterHost(containerId);   // the inventory stays in the registry on purpose
    }
    Super::EndPlay(EndPlayReason);
}

FGuid UContainerComponent::getContainerId() const{
    return containerId;
}

UInventoryData* UContainerComponent::getInventory() const{
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    return registry != nullptr ? registry->findInventory(containerId) : nullptr;
}
