#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "UObject/StrongObjectPtr.h"
#include "Async/Future.h"
#include "SaveFramework/00Enums/E_SaveFramework.h"
#include "SaveFramework/01Structs/S_SaveFramework.h"
#include "SaveFramework/SaveSlotStore.h"
#include "SaveFramework/AwakenSaveGame.h"       // TStrongObjectPtr<UAwakenCharacterSave> needs the complete type
#include "SessionHandshakeFramework/03Interfaces/BPI_CharacterLookupSource.h"
#include "SaveSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAwakenSave, Log, All);   // Doc B22 3.9 names the category LogAwakenSave

/** Blueprint mirror of the Gameplay Messages. Raised LOCALLY, and only for the local player (Doc02 19.2).
 *  Declared here, not in 04Delegates: UHT only reads headers that contain UCLASS, USTRUCT or similar, so a header holding just a delegate is not seen. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FD_SaveEvent, FGameplayTag, eventTag, FGameplayTag, failReasonTag);

class APlayerState;
class APlayerController;
class UGameplayMessageSubsystem;

UCLASS()
class THEAWAKEN_API USaveSubsystem : public UGameInstanceSubsystem, public IBPI_CharacterLookupSource{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;

    UPROPERTY(BlueprintAssignable, Category = "Save") FD_SaveEvent onSaveEvent;

    /** IBPI_CharacterLookupSource (Session Handshake guide, step 13). Registered with the handshake in Initialize. Reads only.
     *  ReturningPlayer when a held state inside its hold, or a Character file of the active world, exists for any of the ids. */
    virtual E_ReturningLookup lookupCharacter_Implementation(const TArray<FString>& lookupIds) const override;

    /** Real-time clock for the 15-minute hold. Tests set this to move time; empty = FPlatformTime::Seconds(). */
    TFunction<double()> realTimeSource;

    // ---- Registration. Server only (a call on a client does nothing). ----
    UFUNCTION(BlueprintCallable, Category = "Save") void registerParticipant(UObject* participant);
    UFUNCTION(BlueprintCallable, Category = "Save") void unregisterParticipant(UObject* participant);
    UFUNCTION(BlueprintCallable, Category = "Save") void registerGateSource(UObject* gateSource);
    UFUNCTION(BlueprintCallable, Category = "Save") void unregisterGateSource(UObject* gateSource);
    UFUNCTION(BlueprintCallable, Category = "Save") void setBagProvider(UObject* provider);

    // ---- Restore marks. B06 calls these (Doc B06 P5; Doc B16 gate Loading.Gate.SaveRestored). Server only. ----
    UFUNCTION(BlueprintCallable, Category = "Save") void markWorldRestored(bool restored);
    UFUNCTION(BlueprintCallable, Category = "Save") void markPlayerRestored(APlayerState* player);
    UFUNCTION(BlueprintCallable, Category = "Save") void clearPlayerRestored(APlayerState* player);
    /** PlayerStates are recreated on level travel, and these marks live longer than a level: B06 calls this on EVERY level change. */
    UFUNCTION(BlueprintCallable, Category = "Save") void clearAllRestoreMarks();

    // ---- Commands. Server only: a client gets NotAuthority or nothing. ----
    /** Doc R13 section 4: the one entry point. A client's hold is a request the Save Point forwards here. */
    UFUNCTION(BlueprintCallable, Category = "Save") FS_SaveResult requestSaveSession(APlayerState* requester);   // Step 9
    UFUNCTION(BlueprintCallable, Category = "Save") void cancelPendingRequests();   // Step 9
    /** The four slot changes below refuse with SaveInProgress while a write runs: cancelPendingRequests, then wait for hasPendingSaves() to be false. */
    UFUNCTION(BlueprintCallable, Category = "Save") FS_SaveResult loadSlot(const FString& slotName);   // Step 9
    UFUNCTION(BlueprintCallable, Category = "Save") FS_SaveResult beginNewWorld(const FString& slotName);   // Step 9: SlotNotEmpty if the slot has files (B06 deletes it first, after its confirmation)
    UFUNCTION(BlueprintCallable, Category = "Save") void unloadSlot();   // Step 9: B06's failLoad calls it
    UFUNCTION(BlueprintCallable, Category = "Save") FS_SaveResult deleteSlot(const FString& slotName);   // Step 14
    UFUNCTION(BlueprintCallable, Category = "Save") FS_SaveResult startAgain();   // Step 14
    UFUNCTION(BlueprintCallable, Category = "Save") void deleteCharacter(APlayerState* player);   // Step 14
    UFUNCTION(BlueprintCallable, Category = "Save") void storeHeldCharacter(APlayerState* player);   // Step 9, B20 calls on drop
    UFUNCTION(BlueprintCallable, Category = "Save") void discardHeldCharacter(const FString& playerIdentity);   // Step 9

    /** B06 restore primitives: it owns the ORDER, Save owns migrating and applying one snapshot (Step 14). */
    UFUNCTION(BlueprintCallable, Category = "Save") FS_SaveResult restoreWorldParticipant(FName saveKey);
    UFUNCTION(BlueprintCallable, Category = "Save") FS_SaveResult restoreWorldBags();
    /** READ ONLY: which source this player restores from and what the bag rule says. Changes nothing, safe to call twice. A new character's id is made here, so pass the SAME plan to Begin and Finish. */
    UFUNCTION(BlueprintCallable, Category = "Save") FS_CharacterRestorePlan previewCharacterRestore(APlayerState* player);
    /** COMMAND: puts the characterID on the PlayerState, removes the duplicate world bag, records a re-key. Call before the player's participants restore. It also fixes the source they restore from, so a hold that runs out half way changes nothing. RestoreFailed = the plan is stale (preview again); SchemaTooNew = FileUnsupported. */
    UFUNCTION(BlueprintCallable, Category = "Save") FS_SaveResult beginCharacterRestore(APlayerState* player, const FS_CharacterRestorePlan& plan);
    UFUNCTION(BlueprintCallable, Category = "Save") FS_SaveResult restorePlayerParticipant(APlayerState* player, FName saveKey);
    /** succeeded false = the player stays NOT restored: the gate refuses them and their previous file is carried untouched. */
    UFUNCTION(BlueprintCallable, Category = "Save") void finishCharacterRestore(APlayerState* player, const FS_CharacterRestorePlan& plan, bool succeeded);

    /** Local feedback. localController is the controller that shows it: the delegate fires only if it is local (Step 12). */
    void notifyLocalFeedback(FGameplayTag eventTag, APlayerState* player, FGameplayTag failReasonTag, const APlayerController* localController);

    // ---- Queries (never mutate) ----
    /** Invalid tag = allowed. Otherwise the refusal reason (Doc02 sec. 6). Not BlueprintPure: it runs Blueprint gate code, and a pure node would run it once per pin. */
    UFUNCTION(BlueprintCallable, Category = "Save") FGameplayTag evaluateSaveGate(APlayerState* requester) const;
    UFUNCTION(BlueprintPure, Category = "Save") bool hasPendingSaves() const;
    UFUNCTION(BlueprintPure, Category = "Save")
    bool canChangeSlot() const{
        return !hasPendingSaves();
    }
    UFUNCTION(BlueprintPure, Category = "Save") bool slotHasFiles(const FString& slotName) const;
    UFUNCTION(BlueprintPure, Category = "Save")
    FString getActiveSlotName() const{
        return activeSlotName;
    }
    UFUNCTION(BlueprintPure, Category = "Save")
    FGuid getActiveWorldID() const{
        return activeWorldID;
    }
    UFUNCTION(BlueprintPure, Category = "Save")
    int32 getWorldSaveCounter() const{
        return committedCounter;
    }
    UFUNCTION(BlueprintPure, Category = "Save")
    bool didLastLoadUseBackup() const{
        return lastLoadUsedBackup;
    }
    /** Identities whose Character file failed its check at load. B06 and B20 show the host's choice for each (Step 14). */
    UFUNCTION(BlueprintPure, Category = "Save") TArray<FString> getRefusedCharacterIdentities() const;
    UFUNCTION(BlueprintCallable, Category = "Save") void listSlotHeaders(TArray<FS_SaveSlotHeader>& headers) const;   // Step 14

private:
    struct FS_CharacterFileInfo{
        FS_SaveFileHeader header;
        FString fileName;
        int32 fileSize = 0;
    };
    // another game version wrote it: not damaged, carried as opaque bytes
    struct FS_UnsupportedCharacterFile{
        FS_SaveManifestEntry entry;
        FGameplayTag reason;
    };
    // what one player restores from, fixed at beginCharacterRestore (null = a new character)
    struct FS_PinnedRestore{
        TStrongObjectPtr<UAwakenCharacterSave> source;
    };
    struct FS_HeldCharacter{
        FS_SaveFileHeader header;
        TStrongObjectPtr<UAwakenCharacterSave> save;   // keeps the object alive across frames
        double expiresAtRealTime = 0.0;   // real time, not game time (Doc B20 3.8)
        bool keepPreviousFile = false;   // dropped Bagless, Downed or Restrained: restore from it, never write it
    };

    bool isHostOrOffline() const;
    double nowSeconds() const{
        return realTimeSource ? realTimeSource() : FPlatformTime::Seconds();
    }
    UGameplayMessageSubsystem* getMessages() const;
    bool shouldKeepPreviousFile(APlayerState* player) const;
    void resetLoadedState();
    void purgeExpiredHeldStates();
    void deliverToRequester(APlayerState* requester, FGameplayTag eventTag, FGameplayTag failReasonTag);

    // Step 9
    void pumpQueue();
    void startSave();
    FGameplayTag captureSet(FS_SaveWriteJob& job, int32 counter);   // invalid tag = captured
    bool captureCharacterFile(APlayerState* player, int32 counter, FS_SaveFileHeader& header, UAwakenCharacterSave*& save) const;
    bool addFileToJob(FS_SaveWriteJob& job, FS_SaveFileHeader& header, USaveGame* save, const FString& fileName) const;
    void finishSave(FGameplayTag failReason);

    // Step 14
    FS_SaveResult applySnapshot(UObject* participant, const FS_SnapshotRecord& record) const;
    bool resolveCharacterSource(APlayerState* player, FS_CharacterRestorePlan& plan, FS_SaveFileHeader& header, UAwakenCharacterSave*& source) const;   // reads only

    // Participants and rules. Weak: a framework can be destroyed without telling us.
    UPROPERTY() TArray<TWeakObjectPtr<UObject>> participants;
    UPROPERTY() TArray<TWeakObjectPtr<UObject>> gateSources;
    UPROPERTY() TWeakObjectPtr<UObject> bagProvider;

    // Restore marks
    bool worldRestored = false;
    TSet<TWeakObjectPtr<APlayerState>> restoredPlayers;
    TMap<TWeakObjectPtr<APlayerState>, FS_PinnedRestore> pinnedRestores;   // Begin to Finish: a restore never looks its source up again half way

    // The loaded slot
    FString activeSlotName;
    FGuid activeWorldID;
    int32 committedCounter = 0;
    bool lastLoadUsedBackup = false;
    FS_SaveFileHeader loadedWorldHeader;
    UPROPERTY() TObjectPtr<UAwakenWorldSave> loadedWorld;   // the World file as it was last loaded or saved: carry-forward copies from it
    UPROPERTY() TMap<FString, TObjectPtr<UAwakenCharacterSave>> loadedCharacterSaves;   // by the identity the file is stored under (same key as characterIndex): restore reads it, capture carries unclaimed records from it
    TMap<FString, FS_CharacterFileInfo> characterIndex;   // by playerIdentity, headers of the Current set (valid files only)
    TArray<FS_UnsupportedCharacterFile> unsupportedCharacterFiles;   // their players cannot restore (FileUnsupported); the files are carried untouched
    TArray<FS_SaveManifestEntry> refusedCharacterFiles;   // failed their check at load, set aside
    TMap<FString, FString> pendingRekeys;   // newIdentity -> oldIdentity: the old file is dropped once the new one is in a committed set
    TSet<FString> deletedIdentities;   // deleteCharacter only
    TMap<FString, FS_HeldCharacter> heldStates;   // by playerIdentity

    // The save queue
    TArray<TWeakObjectPtr<APlayerState>> pending;
    TArray<TWeakObjectPtr<APlayerState>> inFlightRequesters;
    TSharedPtr<FS_SaveWriteJob, ESPMode::ThreadSafe> inFlightJob;
    UPROPERTY() TObjectPtr<UAwakenWorldSave> inFlightWorldSave;   // what the running write captured; becomes loadedWorld on commit
    UPROPERTY() TMap<FString, TObjectPtr<UAwakenCharacterSave>> inFlightCharacterSaves;
    TFuture<void> inFlightFuture;
    bool writeInFlight = false;
};