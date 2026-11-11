#include "AbilityActionFramework/GE_Cost_Dodge.h"
#include "AbilityActionFramework/MMC_StaminaCost.h"
#include "StatsFramework/AS_CoreVitals.h"

UGE_Cost_Dodge::UGE_Cost_Dodge(){
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FCustomCalculationBasedFloat custom;
    custom.CalculationClassMagnitude = UMMC_StaminaCost::StaticClass();
    FGameplayModifierInfo modifier;
    modifier.Attribute = UAS_CoreVitals::currentStaminaAttribute();
    modifier.ModifierOp = EGameplayModOp::Additive;
    modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(custom);
    Modifiers.Add(modifier);
}
