#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_StaminaCost.generated.h"

// A7 step 1. Reads staminaCost (or drainPerSecond times the period) from the definition on the effect
// context's source object, negates it and multiplies by staminaDrainMultiplier captured from the source (the owner
// that applies the cost to itself; a Target capture is missing during the engine's cost check).
UCLASS()
class THEAWAKEN_API UMMC_StaminaCost : public UGameplayModMagnitudeCalculation{
    GENERATED_BODY()
public:
    UMMC_StaminaCost();
    virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& spec) const override;

private:
    FGameplayEffectAttributeCaptureDefinition drainDefinition;
};