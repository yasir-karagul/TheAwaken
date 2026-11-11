#include "StatsFramework/MMC_StaminaDelta.h"
#include "GameplayEffect.h"
#include "StatsFramework/AS_CoreVitals.h"

UMMC_StaminaDelta::UMMC_StaminaDelta(){
    drainDefinition.AttributeToCapture = UAS_CoreVitals::staminaDrainMultiplierAttribute();
    drainDefinition.AttributeSource = EGameplayEffectAttributeCaptureSource::Target;
    drainDefinition.bSnapshot = false;
    regenDefinition.AttributeToCapture = UAS_CoreVitals::staminaRegenMultiplierAttribute();
    regenDefinition.AttributeSource = EGameplayEffectAttributeCaptureSource::Target;
    regenDefinition.bSnapshot = false;
    RelevantAttributesToCapture.Add(drainDefinition);
    RelevantAttributesToCapture.Add(regenDefinition);
}

float UMMC_StaminaDelta::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& spec) const{
    const float delta = spec.GetSetByCallerMagnitude(FName("delta"), false, 0.0f);
    FAggregatorEvaluateParameters params;
    params.SourceTags = spec.CapturedSourceTags.GetAggregatedTags();
    params.TargetTags = spec.CapturedTargetTags.GetAggregatedTags();
    float drainMultiplier = 1.0f;
    float regenMultiplier = 1.0f;
    GetCapturedAttributeMagnitude(drainDefinition, spec, params, drainMultiplier);
    GetCapturedAttributeMagnitude(regenDefinition, spec, params, regenMultiplier);
    if(delta < 0.0f){
        return delta * drainMultiplier;
    }
    return delta * regenMultiplier;
}
