#include "StatsFramework/GE_ModifyStamina.h"
#include "StatsFramework/AS_CoreVitals.h"
#include "StatsFramework/MMC_StaminaDelta.h"

UGE_ModifyStamina::UGE_ModifyStamina(){
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FCustomCalculationBasedFloat custom;
    custom.CalculationClassMagnitude = UMMC_StaminaDelta::StaticClass();
    FGameplayModifierInfo modifier;
    modifier.Attribute = UAS_CoreVitals::currentStaminaAttribute();
    modifier.ModifierOp = EGameplayModOp::Additive;
    modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(custom);
    Modifiers.Add(modifier);
}
