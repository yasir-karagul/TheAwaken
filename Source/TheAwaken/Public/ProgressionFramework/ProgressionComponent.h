#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "ProgressionFramework/01Structs/S_ProgressionFramework.h"
#include "ProgressionFramework/DA_StartingUnlocks.h"
#include "ProgressionComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAwakenProgression, Log, All);

// The shared unlocked set (Doc R06). Lives on the GameState, so every machine reads the same set.
UCLASS(ClassGroup = (Progression), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API UProgressionComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UProgressionComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ---- Queries: never change anything, work on every machine (Doc02 section 4) ----
    bool isUnlocked(const FGameplayTag& tag) const;
    FGameplayTagContainer getAllUnlocked() const;
    bool isReady() const;
    FGameplayTagContainer getEveryUnlockTag() const;

    // ---- Commands: host only ----
    FS_ProgressionResult grantUnlock(const FGameplayTag& tag);
    bool seedNewWorld();

    // ---- Save contract (Doc02 section 9, Doc R13) ----
    FS_ProgressionSaveRecord getSnapshotForSave() const;
    bool restoreFromSnapshot(const FS_ProgressionSaveRecord& record);

    // C++ callers (volumes, dialogue, building, crafting) find the component here.
    // Returns nullptr when there is no world or no GameState yet (a client that is still joining).
    static UProgressionComponent* findProgression(const UObject* worldContextObject);

protected:
    virtual void BeginPlay() override;

private:
    // ---- State every machine receives (Doc02 section 19.1) ----
    UPROPERTY(ReplicatedUsing = OnRep_unlockedTags)
    FGameplayTagContainer unlockedTags;

    // False until the host has finished filling the set (new world, load). See Step 7.
    UPROPERTY(ReplicatedUsing = OnRep_isSeeded)
    bool isSeeded = false;

    // ---- Assets, assigned on the component in the GameState Blueprint ----
    UPROPERTY(EditDefaultsOnly, Category = "Progression")
    TSoftObjectPtr<UDataTable> storyChainTable;

    UPROPERTY(EditDefaultsOnly, Category = "Progression")
    TSoftObjectPtr<UDA_StartingUnlocks> startingUnlocks;

    // The loaded copies. Host only, filled on first use.
    UPROPERTY(Transient)
    TObjectPtr<UDataTable> loadedStoryChain;

    UPROPERTY(Transient)
    TObjectPtr<UDA_StartingUnlocks> loadedStartingUnlocks;

    // ---- Bookkeeping for the client side of the message (Step 7). Not replicated, not saved. ----
    FGameplayTagContainer knownTags;
    bool hasSynced = false;

    UFUNCTION()
    void OnRep_unlockedTags();

    UFUNCTION()
    void OnRep_isSeeded();

    bool isServer() const;
    const TCHAR* roleName() const;
    bool isGrantableTag(const FGameplayTag& tag) const;
    void ensureAssetsLoaded();
    const FS_StoryChainRow* findStoryRow(const FGameplayTag& tag) const;
    FGameplayTag checkStoryGrant(const FGameplayTag& tag, bool& grantsComplete) const;
    void validateStoryChain() const;
    int32 readDaysPassed() const;
    bool readUnlockEverything() const;
    bool readQuestsEnabled() const;
    int32 addStartingTags();
    void announce(const FGameplayTag& tag) const;
    void handleReplicatedChange();
    void finishSeeding();
};
