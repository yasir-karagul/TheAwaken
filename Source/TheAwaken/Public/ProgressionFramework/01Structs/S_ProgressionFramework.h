#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "S_ProgressionFramework.generated.h"

// Result of a command (Doc02 section 6: explained rejection, never a bare false).
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_ProgressionResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Progression")
    bool success = false;

    // Empty on success. A Progression.FailReason.* tag otherwise.
    UPROPERTY(BlueprintReadOnly, Category = "Progression")
    FGameplayTag failReason;
};

// One row of DT_StoryChain (Doc R06 3.1a). Defined in C++, never as a Blueprint struct.
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_StoryChainRow : public FTableRowBase{
    GENERATED_BODY()

    // The story part this row describes, for example Progression.Story.Part2.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression", meta = (Categories = "Progression.Story"))
    FGameplayTag partTag;

    // The part that must already be unlocked. Leave empty for the first part (there is no Part 0).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression", meta = (Categories = "Progression.Story"))
    FGameplayTag prerequisiteTag;

    // Smallest DaysPassed (Doc R12) at the moment of the grant. 0 means no minimum.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression", meta = (ClampMin = "0"))
    int32 minimumDays = 0;

    // True on the final story part. Granting it also grants Progression.Story.Complete.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
    bool isLastPart = false;
};

// Payload of the Progression.UnlockGranted message (Step 7).
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_ProgressionUnlockMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Progression")
    FGameplayTag unlockedTag;
};

// What Save stores (Doc R13, Step 9). Tag NAMES, so a tag that no longer exists can be skipped and named in the log.
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_ProgressionSaveRecord{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadOnly, Category = "Progression")
    TArray<FName> unlockedTags;
};