// Private/ItemInventoryFramework/HeldItemComponent.cpp (TEMPORARY)
#include "ItemInventoryFramework/HeldItemComponent.h"
#include "AbilityActionFramework/AbilityLibrary.h"
#include "StatsFramework/StatsLibrary.h"

UHeldItemComponent::UHeldItemComponent(){
    PrimaryComponentTick.bCanEverTick = false;
}

UObject* UHeldItemComponent::getHeldItem() const{
    return nullptr;
}

void UHeldItemComponent::applyReload(AActor* player, UObject* weaponInstance){
    UE_LOG(LogAwakenAbility, Log, TEXT("[%s] TEMPORARY HeldItem.applyReload"), *UStatsLibrary::getRoleName(this));
}
