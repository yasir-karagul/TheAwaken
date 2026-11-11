#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Abilities/GameplayAbility.h"
#include "DA_ActionDefinition.generated.h"

// A2 step 1. One per action. A field an action does not use stays at zero or empty.
// Every number is tuning data and a placeholder (Doc00 2.7).
UCLASS(BlueprintType)
class THEAWAKEN_API UDA_ActionDefinition : public UPrimaryDataAsset{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "Action"))
    FGameplayTag actionTag;
    UPROPERTY(EditDefaultsOnly)
    TSoftClassPtr<UGameplayAbility> abilityClass;
    UPROPERTY(EditDefaultsOnly)
    float staminaCost = 0.0f;
    UPROPERTY(EditDefaultsOnly)
    float cooldownSeconds = 0.0f;
    UPROPERTY(EditDefaultsOnly)
    float drainPerSecond = 0.0f;
    // Sprint: stamina must rise above this value before the exhausted latch clears.
    UPROPERTY(EditDefaultsOnly)
    float restartThreshold = 0.0f;
    UPROPERTY(EditDefaultsOnly)
    float invulnerabilitySeconds = 0.0f;
    UPROPERTY(EditDefaultsOnly)
    float durationSeconds = 0.0f;
    // Dodge only. The movement component (Doc R07) reads Action.Dodge and moves this far (A7 step 5).
    UPROPERTY(EditDefaultsOnly)
    float dashDistance = 0.0f;
    // Under the GameplayCue. root. Empty for Sprint and Crouch (A12).
    UPROPERTY(EditDefaultsOnly, meta = (Categories = "GameplayCue"))
    FGameplayTag cueTag;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& context) const override;
#endif
};
