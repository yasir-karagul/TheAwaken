#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "S_StatsFramework.generated.h"

// S9: only current values are saved. Maximum health is a fixed 100 and maxStamina is derived.
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_StatsSaveRecord{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float currentHP = 100.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float currentStamina = 0.0f;
};

// S10. playerId is PROPOSED (gap 28). sourceTag is empty on a client.
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_HPChangedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString playerId;
    UPROPERTY(BlueprintReadWrite)
    float newValue = 0.0f;
    UPROPERTY(BlueprintReadWrite)
    float maxValue = 0.0f;
    UPROPERTY(BlueprintReadWrite)
    FGameplayTag sourceTag;
};

USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_StaminaChangedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString playerId;
    UPROPERTY(BlueprintReadWrite)
    float newValue = 0.0f;
    UPROPERTY(BlueprintReadWrite)
    float maxValue = 0.0f;
};
