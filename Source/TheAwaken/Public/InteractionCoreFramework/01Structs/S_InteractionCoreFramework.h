// Public/InteractionCoreFramework/01Structs/S_InteractionCoreFramework.h
// Doc B18 3.3, 3.9, 6. None of these are saved (B18 section 8). Text is stored as localization keys, never sentences (Doc R10 3.3).
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "GameFramework/Actor.h"
#include "InteractionCoreFramework/00Enums/E_InteractionCoreFramework.h"
#include "S_InteractionCoreFramework.generated.h"

// What the UI is given (B18 3.3). Doc B17 3.7 renders it. B18 writes the field bHold; Doc02 20.1 forbids the b prefix.
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_InteractionPrompt{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Interaction")
    FName verbText;

    UPROPERTY(BlueprintReadWrite, Category = "Interaction")
    FName objectName;

    UPROPERTY(BlueprintReadWrite, Category = "Interaction")
    FKey key;

    UPROPERTY(BlueprintReadWrite, Category = "Interaction")
    bool isHold = false;

    UPROPERTY(BlueprintReadWrite, Category = "Interaction")
    float holdProgress = 0.0f;

    // Empty, or the reason canInteract gave. The UI shows the prompt only when this is empty.
    UPROPERTY(BlueprintReadWrite, Category = "Interaction")
    FGameplayTag blockedReason;
};

// How loud an interaction is (B18 3.9). Both fields are data on the object.
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_InteractionNoise{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Interaction")
    float loudness = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Interaction")
    FGameplayTag noiseTag;
};

// ---- Message payloads (B18 section 6). Gameplay Messages are local to the machine that sends them (Doc02 19.2). ----

// Interaction.FocusChanged. Local to the owning player. Also re-sent when only the prompt changed; the UI re-reads getFocusedPrompt().
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_InteractionFocusMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Interaction")
    TObjectPtr<AActor> player;

    UPROPERTY(BlueprintReadOnly, Category = "Interaction")
    TObjectPtr<AActor> target;
};

// Interaction.HoldStarted and Interaction.HoldCancelled. cancelReason is only meaningful for HoldCancelled.
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_InteractionHoldMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Interaction")
    TObjectPtr<AActor> player;

    UPROPERTY(BlueprintReadOnly, Category = "Interaction")
    TObjectPtr<AActor> target;

    UPROPERTY(BlueprintReadOnly, Category = "Interaction")
    E_InteractionHoldCancelReason cancelReason = E_InteractionHoldCancelReason::Released;
};

// Interaction.HoldToActivateCompleted. Server only. Doc B03 (Sleep) and Doc B08 (Harvest) listen on the host.
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_InteractionCompletedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Interaction")
    FGameplayTag interactionTag;

    UPROPERTY(BlueprintReadOnly, Category = "Interaction")
    TObjectPtr<AActor> instigator;
};

// ---- Runtime record (B18 3.6, guide Step 7). A player runs one hold at a time. Never saved. ----
USTRUCT()
struct FS_ActiveHold{
    GENERATED_BODY()

    TWeakObjectPtr<AActor> target;
    FGameplayTag interactionTag;
    double endTime = 0.0;
    bool isDue = false;
};
