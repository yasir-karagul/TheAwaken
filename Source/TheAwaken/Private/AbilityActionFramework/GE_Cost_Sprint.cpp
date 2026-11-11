#include "AbilityActionFramework/GE_Cost_Sprint.h"
#include "AbilityActionFramework/MMC_StaminaCost.h"
#include "StatsFramework/AS_CoreVitals.h"

namespace{
    // Seconds between Sprint drain ticks. The magnitude is drainPerSecond times this period (A7 step 1).
    constexpr float sprintDrainPeriodSeconds = 0.25f;
}

UGE_Cost_Sprint::UGE_Cost_Sprint(){
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    Period = FScalableFloat(sprintDrainPeriodSeconds);
    FCustomCalculationBasedFloat custom;
    custom.CalculationClassMagnitude = UMMC_StaminaCost::StaticClass();
    FGameplayModifierInfo modifier;
    modifier.Attribute = UAS_CoreVitals::currentStaminaAttribute();
    modifier.ModifierOp = EGameplayModOp::Additive;
    modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(custom);
    Modifiers.Add(modifier);
}
