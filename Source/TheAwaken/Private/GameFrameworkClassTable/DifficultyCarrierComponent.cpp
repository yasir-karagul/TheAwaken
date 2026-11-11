// Private/GameFrameworkClassTable/DifficultyCarrierComponent.cpp
#include "GameFrameworkClassTable/DifficultyCarrierComponent.h"

UDifficultyCarrierComponent::UDifficultyCarrierComponent(){
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}
