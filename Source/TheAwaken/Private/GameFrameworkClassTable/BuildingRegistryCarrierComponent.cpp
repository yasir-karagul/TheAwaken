// Private/GameFrameworkClassTable/BuildingRegistryCarrierComponent.cpp
#include "GameFrameworkClassTable/BuildingRegistryCarrierComponent.h"

UBuildingRegistryCarrierComponent::UBuildingRegistryCarrierComponent(){
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}
