#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AbilityActionFramework/01Structs/S_AbilityActionFramework.h"
#include "TimedEffectLibrary.generated.h"

class UAbilitySystemComponent;

// A11. Save, restore, sleep skip and respawn rules for timed effects. Doc R13 aggregates the snapshot;
// this framework never writes files (Doc02 9). Illness is owned and saved by Doc B03 (gap 10).
UCLASS()
class THEAWAKEN_API UTimedEffectLibrary : public UBlueprintFunctionLibrary{
    GENERATED_BODY()
public:
    // Only a long effect marked with the asset tag Effect.Saved. Empty until one is authored.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Ability")
    static TArray<FS_TimedEffectSaveRecord> getSnapshotForSave(AActor* target);

    // Applies the effect again, then sets its remaining duration to timeLeft. Server only.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Ability")
    static void restoreFromSnapshot(AActor* target, const TArray<FS_TimedEffectSaveRecord>& records);

    // Handler for Time.SkipAnnounced (R12 3.6 AdvanceTo). Timed effects are frozen across a skip, so
    // remaining time is left unchanged. Verify in 5.8 that effect durations run on world time (gap 22).
    UFUNCTION(BlueprintCallable, Category = "Awaken|Ability")
    static void advanceTo(AActor* target, float newGameTime);

    // PROPOSED, called by Doc B02 at a co-op respawn. Removes every duration effect except tier debuffs
    // (Survival.Tier.Debuff.*) and the Action.* state effects, which A3 already releases.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Ability")
    static void clearTimedEffects(AActor* target);
};
