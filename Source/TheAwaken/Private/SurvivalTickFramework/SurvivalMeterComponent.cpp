// Private/SurvivalTickFramework/SurvivalMeterComponent.cpp (TEMPORARY)
#include "SurvivalTickFramework/SurvivalMeterComponent.h"

USurvivalMeterComponent::USurvivalMeterComponent(){
    PrimaryComponentTick.bCanEverTick = false;
}

bool USurvivalMeterComponent::isHungryOrThirsty() const{
    return false;
}

bool USurvivalMeterComponent::hasBlood() const{
    return false;
}

bool USurvivalMeterComponent::isIll() const{
    return false;
}
