// Private/GameFrameworkClassTable/CutTreesCarrierComponent.cpp
#include "GameFrameworkClassTable/CutTreesCarrierComponent.h"

UCutTreesCarrierComponent::UCutTreesCarrierComponent(){
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}
