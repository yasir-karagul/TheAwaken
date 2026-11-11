#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/PlayerState.h"
#include "StructUtils/InstancedStruct.h"
#include "SaveFramework/00Enums/E_SaveFramework.h"
#include "S_SaveFramework.generated.h"   // always the last include

USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_SaveResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Save") bool success = false;
    UPROPERTY(BlueprintReadOnly, Category = "Save") FGameplayTag failReasonTag;
    UPROPERTY(BlueprintReadOnly, Category = "Save") int32 saveCounter = 0;

    static FS_SaveResult ok(int32 inCounter = 0){
        FS_SaveResult r;
        r.success = true;
        r.saveCounter = inCounter;
        return r;
    }
    static FS_SaveResult fail(const FGameplayTag& reason){
        FS_SaveResult r;
        r.failReasonTag = reason;
        return r;
    }
};

// what a framework hands Save; Save never opens data
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_SnapshotRecord{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadOnly) FName saveKey;   // stable forever (Doc02 19.3)
    UPROPERTY(SaveGame, BlueprintReadOnly) int32 snapshotVersion = 1;
    UPROPERTY(SaveGame, BlueprintReadOnly) FInstancedStruct data;   // the framework's own S_...SaveRecord
};

// Doc R13 section 4; owner is a CHARACTER, not a player
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_BagRecord{
    GENERATED_BODY()

    UPROPERTY(SaveGame, BlueprintReadWrite) FGuid bagID;
    UPROPERTY(SaveGame, BlueprintReadWrite) FGuid ownerCharacterID;
    UPROPERTY(SaveGame, BlueprintReadWrite) FVector location = FVector::ZeroVector;
    /** Doc R01's snapshot: contents, hotbar bindings, equipped gear. Not worn clothes, no reservations (Index A110). */
    UPROPERTY(SaveGame, BlueprintReadWrite) FInstancedStruct contents;
};

// for the slot-selection screen, Doc B17
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_SaveSlotHeader{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FString slotName;
    /** False = a slot that was never saved. The UI shows the R10 key UI.SlotList.NeverSaved ("New, not saved"). */
    UPROPERTY(BlueprintReadWrite) bool hasFiles = false;
    /** Empty when the header was read. Otherwise why a slot WITH files could not be read: SchemaTooNew or MigrationMissing (a newer or older game wrote it; not damaged)
     *  or another reason. With hasFiles true the UI must not show "New, not saved" and must not offer a new game over it. */
    UPROPERTY(BlueprintReadWrite) FGameplayTag unreadableReason;
    UPROPERTY(BlueprintReadWrite) FDateTime savedAtUtc;
    UPROPERTY(BlueprintReadWrite) int32 schemaVersion = 0;
    UPROPERTY(BlueprintReadWrite) int32 dayCount = 0;
    UPROPERTY(BlueprintReadWrite) FGameplayTag seasonTag;
    UPROPERTY(BlueprintReadWrite) FGameplayTag difficultyTag;
};

// Gameplay Message payload for Save.Started / Completed / Failed
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_SaveMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) TObjectPtr<APlayerState> player = nullptr;   // UI shows the notice only for its own player
    UPROPERTY(BlueprintReadWrite) FGameplayTag failReasonTag;   // empty unless Save.Failed
};

// ---- Step 7a: file header (plain struct, hand-serialized) ----
struct THEAWAKEN_API FS_SaveFileHeader{
    E_SaveScope scope = E_SaveScope::World;
    int32 envelopeVersion = 0;   // read from the envelope prefix (7b) and copied here, so operator<< can branch on it; not stored inside the header
    int32 schemaVersion = 0;
    FString buildVersion;
    FDateTime writtenAtUtc;
    FGuid worldID;   // changes only on Start Again or a handover copy (Doc R13 3.10)
    int32 saveCounter = 0;   // rises by one per committed save; compared only within one worldID
    int32 payloadSize = 0;
    uint32 payloadCrc = 0;

    // Character files
    FGuid characterID;
    FString playerIdentity;
    TArray<FString> linkedIds;
    FGuid heldBagID;   // invalid = no bag (a Character save never says Bagless, Index A85)

    // World file, for the slot list
    int32 dayCount = 0;
    FName seasonTag;
    FName difficultyTag;

    friend THEAWAKEN_API FArchive& operator<<(FArchive& ar, FS_SaveFileHeader& header);
};

// ---- Step 7c: manifest (JSON, strings and ints only) ----
USTRUCT()
struct THEAWAKEN_API FS_SaveManifestEntry{
    GENERATED_BODY()
    UPROPERTY() FString fileName;
    UPROPERTY() int32 scope = 0;   // E_SaveScope
    UPROPERTY() int32 size = 0;
    UPROPERTY() int64 payloadCrc = 0;
    UPROPERTY() FString playerIdentity;   // Character files only
};

USTRUCT()
struct THEAWAKEN_API FS_SaveManifest{
    GENERATED_BODY()
    UPROPERTY() int32 schemaVersion = 0;
    UPROPERTY() FString slotName;
    UPROPERTY() FString worldId;
    UPROPERTY() int32 saveCounter = 0;
    UPROPERTY() TArray<FS_SaveManifestEntry> files;   // every file of the set, including carried-over ones
};

// ---- Step 7d: slot paths and the write job (plain structs, no UObject inside) ----
/** Every folder a slot uses. Built ONCE on the game thread (it reads the settings), then handed to the worker as plain strings. */
struct FS_SaveSlotPaths{
    FString root;   // <Saved>/SaveGames/<RootFolder>/<Slot>
    FString current;   // the committed set
    FString backup;   // the set before it
    FString retired;   // Current, only while a commit is in progress
    FString refused;   // one refused set per slot, kept
    FString refusedCharacters;   // single Character files that failed their check
    FString staging;   // Staging_<n>: the set being written
};

/** Everything a save needs on disk, captured as bytes. No UObject inside: safe on a worker thread. */
struct FS_SaveWriteJob{
    struct FS_OutFile{
        FString fileName;
        TArray<uint8> bytes;
        FS_SaveFileHeader header;
    };

    FS_SaveSlotPaths paths;
    int32 counter = 0;
    int32 minSchema = 1;
    bool keepBackup = true;
    FS_SaveManifest manifest;   // written + carried-over files
    TArray<FS_OutFile> filesToWrite;
    TArray<FS_SaveManifestEntry> filesToCarryOver;   // players not rewritten this save: checked, then copied from Current

    // Set by the game thread when the job is built; finishSave applies them only if this job committed.
    TArray<FString> honouredRekeys;   // newIdentity keys whose old file this set leaves out
    TArray<FString> honouredDeletes;   // identities removed by deleteCharacter that this set leaves out
    // Set by the worker.
    TArray<FString> skippedIdentities;   // carried files that failed their check and were set aside
};

// ---- Step 8: restore plan ----
/** What B06 needs to restore one player (Doc B06 P2 and P4). Ids only: no object, nothing applied. */
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_CharacterRestorePlan{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Save") E_SaveRestoreSource source = E_SaveRestoreSource::None;
    UPROPERTY(BlueprintReadOnly, Category = "Save") FGuid characterId;   // from the file, or a fresh id for a new character
    UPROPERTY(BlueprintReadOnly, Category = "Save") FString sourceIdentity;   // the identity the source is stored under (differs from the player's when the file is re-keyed)
    UPROPERTY(BlueprintReadOnly, Category = "Save") E_SaveBagOutcome bagOutcome = E_SaveBagOutcome::NoConflict;
    UPROPERTY(BlueprintReadOnly, Category = "Save") FGuid worldBagToRemove;
    UPROPERTY(BlueprintReadOnly, Category = "Save") FGameplayTag noticeTag;   // Save.WorldOlderThanCharacter, or empty
};

// ---- Step 10: bag rule result ----
struct FS_SaveBagDecision{
    E_SaveBagOutcome outcome = E_SaveBagOutcome::NoConflict;
    FGuid worldBagToRemove;   // valid only for SavedStateWinsRemoveWorldBag
};