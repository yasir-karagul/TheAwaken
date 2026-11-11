#include "AbilityActionFramework/MMC_StaminaCost.h"
#include "GameplayEffect.h"
#include "AbilityActionFramework/DA_ActionDefinition.h"
#include "StatsFramework/AS_CoreVitals.h"

UMMC_StaminaCost::UMMC_StaminaCost(){
    drainDefinition.AttributeToCapture = UAS_CoreVitals::staminaDrainMultiplierAttribute();
    // Source, not Target: the engine's cost check (CanApplyAttributeModifiers) evaluates this effect before any target is
    // captured, so a Target capture fails and the cost reads 0. A cost is always applied by the ability's owner to itself,
    // so the source and the target are the same ASC and the value is the same.
    drainDefinition.AttributeSource = EGameplayEffectAttributeCaptureSource::Source;
    drainDefinition.bSnapshot = false;
    RelevantAttributesToCapture.Add(drainDefinition);
}

float UMMC_StaminaCost::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& spec) const{
    const UDA_ActionDefinition* definition = Cast<UDA_ActionDefinition>(spec.GetContext().GetSourceObject());
    if(definition == nullptr){
        return 0.0f;
    }
    float baseCost = definition->staminaCost;
    if(spec.GetPeriod() > 0.0f){
        baseCost = definition->drainPerSecond * spec.GetPeriod();
    }
    FAggregatorEvaluateParameters params;
    params.SourceTags = spec.CapturedSourceTags.GetAggregatedTags();
    params.TargetTags = spec.CapturedTargetTags.GetAggregatedTags();
    float drainMultiplier = 1.0f;
    GetCapturedAttributeMagnitude(drainDefinition, spec, params, drainMultiplier);
    return -baseCost * drainMultiplier;
}