// Public/SaveFramework/SaveSmokeTest.h
//
// DEV-ONLY. Delete this file and SaveSmokeTest.cpp once your real frameworks are in and tested.
//
// This header holds the small "fake" pieces the smoke test needs, so that Save can be tested without any real framework:
//   - two snapshot structs (what a framework would hand to Save)
//   - a fake participant (stands in for e.g. the Time or Survival framework)
//   - a fake gate source (stands in for the rules of B02 / B12: "refuse this save")
//   - a fake PlayerState (stands in for a connected player: identity + CharacterID)
// The test itself (the console command Awaken.Save.SmokeTest) is in SaveSmokeTest.cpp.
#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/PlayerState.h"
#include "StructUtils/InstancedStruct.h"
#include "SaveFramework/00Enums/E_SaveFramework.h"
#include "SaveFramework/01Structs/S_SaveFramework.h"
#include "SaveFramework/03Interfaces/BPI_SaveParticipant.h"
#include "SaveFramework/03Interfaces/BPI_SavePlayerSubject.h"
#include "SaveFramework/03Interfaces/BPI_SaveGateSource.h"
#include "SaveSmokeTest.generated.h"   // always the last include

// ---- Snapshot structs: what a framework hands to Save ---------------------------------------------------------------------

USTRUCT()
struct THEAWAKEN_API FS_SmokeItem{
    GENERATED_BODY()

    UPROPERTY(SaveGame) FName itemId;
    UPROPERTY(SaveGame) int32 count = 0;
};

/** A fake World-scope snapshot. It uses the field types a real framework will use: a 64-bit int, an FString, an array, an FVector. */
USTRUCT()
struct THEAWAKEN_API FS_SmokeWorldRecord{
    GENERATED_BODY()

    UPROPERTY(SaveGame) int64 gameTimeSeconds = 0;   // bigger than 32 bits on purpose
    UPROPERTY(SaveGame) int32 dayCount = 0;
    UPROPERTY(SaveGame) FString note;
    UPROPERTY(SaveGame) TArray<int32> numbers;
    UPROPERTY(SaveGame) FVector location = FVector::ZeroVector;

    /** Marked Transient on purpose. The first test run showed that a field with only a missing SaveGame flag IS still written
     *  inside an FInstancedStruct, so Transient is the way to keep a field out of the file. The test proves it. */
    UPROPERTY(Transient) int32 notSaved = 0;
};

/** A fake Character-scope snapshot, with a struct array inside (a nested struct must carry SaveGame on its own fields too). */
USTRUCT()
struct THEAWAKEN_API FS_SmokeCharacterRecord{
    GENERATED_BODY()

    UPROPERTY(SaveGame) int32 level = 0;
    UPROPERTY(SaveGame) float health = 0.0f;
    UPROPERTY(SaveGame) FString displayName;
    UPROPERTY(SaveGame) TArray<FS_SmokeItem> items;
    UPROPERTY(SaveGame) FVector position = FVector::ZeroVector;
    UPROPERTY(SaveGame) FGuid someId;

    UPROPERTY(Transient) int32 notSaved = 0;   // Transient, see above
};

// ---- A fake participant -----------------------------------------------------------------------------------------------------

/** Implements the same IBPI_SaveParticipant contract as USaveParticipantComponent, but as a plain object the test controls.
 *  Every interface function is written out: an interface function that is not overridden hits UHT's "do not call directly" default. */
UCLASS()
class THEAWAKEN_API USaveSmokeTestParticipant : public UObject, public IBPI_SaveParticipant{
    GENERATED_BODY()
public:
    FName key;
    E_SaveScope scope = E_SaveScope::World;
    int32 version = 1;   // the snapshot version this participant claims to be at

    /** Character scope only: the fake player this participant belongs to. */
    UPROPERTY() TObjectPtr<APlayerState> ownerPlayer = nullptr;

    // The data the participant "owns". Save reads it at capture, and writes it back at restore.
    FS_SmokeWorldRecord worldData;
    FS_SmokeCharacterRecord characterData;

    // What the test can inspect afterwards.
    int32 restoreCalls = 0;
    int32 lastSavedVersion = 0;
    mutable int32 migrateCalls = 0;   // mutable: migrateSnapshotStep is const in the interface

    virtual FName getSaveKey_Implementation() const override;
    virtual E_SaveScope getSaveScope_Implementation() const override;
    virtual int32 getSnapshotVersion_Implementation() const override;
    virtual APlayerState* getSaveOwner_Implementation() const override;
    virtual bool getSnapshotForSave_Implementation(FInstancedStruct& snapshot) const override;
    virtual bool restoreFromSnapshot_Implementation(const FInstancedStruct& snapshot, int32 savedVersion) override;
    virtual bool migrateSnapshotStep_Implementation(int32 fromVersion, const FInstancedStruct& oldSnapshot, FInstancedStruct& newSnapshot) const override;
    virtual bool contributeToSlotHeader_Implementation(const FS_SaveSlotHeader& display, FS_SaveSlotHeader& contribution) const override;
};

// ---- A fake gate source -----------------------------------------------------------------------------------------------------

/** Stands in for the rule providers (B02, B12). Invalid refuseWith = allow the save. */
UCLASS()
class THEAWAKEN_API USaveSmokeTestGate : public UObject, public IBPI_SaveGateSource{
    GENERATED_BODY()
public:
    FGameplayTag refuseWith;
    bool keepPreviousFile = false;

    virtual FGameplayTag getSaveRefusalReason_Implementation(APlayerState* requester) const override;
    virtual bool shouldKeepPreviousCharacterFile_Implementation(APlayerState* player) const override;
};

// ---- A fake player ----------------------------------------------------------------------------------------------------------

/** A PlayerState with the identity and CharacterID that IBPI_SavePlayerSubject asks for. It has no controller, which is why the
 *  Save Feedback Component is not covered by this test. */
UCLASS()
class THEAWAKEN_API ASaveSmokeTestPlayerState : public APlayerState, public IBPI_SavePlayerSubject{
    GENERATED_BODY()
public:
    FString identity;
    TArray<FString> linkedPlayerIds;
    FGuid characterId;
    FGuid heldBagId;

    virtual FString getPlayerIdentity_Implementation() const override;
    virtual void getLinkedPlayerIds_Implementation(TArray<FString>& linkedIds) const override;
    virtual FGuid getCharacterId_Implementation() const override;
    virtual FGuid getHeldBagId_Implementation() const override;
    virtual void setCharacterId_Implementation(FGuid newCharacterId) override;
};