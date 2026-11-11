#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "S_AbilityActionFramework.generated.h"

class UDA_ActionDefinition;
class UInputAction;

// A2 step 2.
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_AbilityLoadoutEntry{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Action"))
    FGameplayTag actionTag;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UDA_ActionDefinition> definition;
    // Empty: granted at spawn. Set: granted when Progression (Doc R06) grants this unlock.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGameplayTag unlockTag;
};

// A5 step 2.
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_ActionInputBinding{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<const UInputAction> inputAction;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Action"))
    FGameplayTag actionTag;
};

// Payload of Action.Started and Action.Ended. "cancelled" is added so listeners can tell the difference (A4 step 4).
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_ActionMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FGameplayTag actionTag;
    UPROPERTY(BlueprintReadWrite)
    TObjectPtr<AActor> instigator;
    UPROPERTY(BlueprintReadWrite)
    bool cancelled = false;
};

// A11 step 2. Stable ids only, never pointers (Doc02 19.8). effectId is the effect class path.
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_TimedEffectSaveRecord{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString effectId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float timeLeft = 0.0f;
};
