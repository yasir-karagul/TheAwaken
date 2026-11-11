#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_StaminaDelta.generated.h"

// S3 step 1. Takes the set-by-caller "delta" and multiplies it by the drain multiplier when negative,
// by the regeneration multiplier when positive. MMC_StaminaCost (Ability framework) handles action costs.
UCLASS()
class THEAWAKEN_API UMMC_StaminaDelta : public UGameplayModMagnitudeCalculation{
    GENERATED_BODY()
public:
    UMMC_StaminaDelta();
    virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& spec) const override;

private:
    FGameplayEffectAttributeCaptureDefinition drainDefinition;
    FGameplayEffectAttributeCaptureDefinition regenDefinition;
};
