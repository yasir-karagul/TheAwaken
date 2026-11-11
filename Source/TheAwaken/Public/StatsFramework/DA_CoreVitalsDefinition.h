#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DA_CoreVitalsDefinition.generated.h"

// S1 step 5. Base values and percentages are data (Doc00 2.7). Numbers here are placeholders to tune.
UCLASS(BlueprintType)
class THEAWAKEN_API UDA_CoreVitalsDefinition : public UPrimaryDataAsset{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, Category = "Maximums")
    float maxHP = 100.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Maximums")
    float maxStamina = 100.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Regeneration")
    float staminaRegenPerSecond = 15.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Regeneration")
    float healthRegenPerSecond = 1.0f;
    // Seconds between regeneration ticks (server timer, Doc02 5).
    UPROPERTY(EditDefaultsOnly, Category = "Regeneration")
    float regenTickSeconds = 0.25f;

    UPROPERTY(EditDefaultsOnly, Category = "Regeneration cap percent of maxHP")
    float naturalRegenCapPercent = 50.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Regeneration cap percent of maxHP")
    float hungryThirstyRegenCapPercent = 30.0f;
    UPROPERTY(EditDefaultsOnly, Category = "Regeneration cap percent of maxHP")
    float bloodRegenCapPercent = 20.0f;

    UPROPERTY(EditDefaultsOnly, Category = "Low health")
    float lowHealthPercent = 20.0f;
};
