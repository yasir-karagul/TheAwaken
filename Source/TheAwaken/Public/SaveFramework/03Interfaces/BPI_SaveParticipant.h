#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameFramework/PlayerState.h"
#include "StructUtils/InstancedStruct.h"
#include "SaveFramework/00Enums/E_SaveFramework.h"
#include "SaveFramework/01Structs/S_SaveFramework.h"
#include "BPI_SaveParticipant.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UBPI_SaveParticipant : public UInterface{
    GENERATED_BODY()
};

class THEAWAKEN_API IBPI_SaveParticipant{
    GENERATED_BODY()
public:
    /** Written in the file. Never rename or reuse after release (Doc02 19.3, Extension rule 5). */
    UFUNCTION(BlueprintNativeEvent, Category = "Save") FName getSaveKey() const;
    UFUNCTION(BlueprintNativeEvent, Category = "Save") E_SaveScope getSaveScope() const;
    /** Current layout version of this framework's snapshot struct. Starts at 1. */
    UFUNCTION(BlueprintNativeEvent, Category = "Save") int32 getSnapshotVersion() const;
    /** Character scope only: the PlayerState this participant belongs to. World scope returns null. */
    UFUNCTION(BlueprintNativeEvent, Category = "Save") APlayerState* getSaveOwner() const;

    /** QUERY (Doc02 §4): never mutates. True with a valid struct = write it. False = KEEP the record the file already has for this key.
     *  To delete a record on purpose, return true with an empty (invalid) FInstancedStruct. */
    UFUNCTION(BlueprintNativeEvent, Category = "Save") bool getSnapshotForSave(FInstancedStruct& snapshot) const;

    /** COMMAND: rebuild state from a snapshot already migrated to the current version. Skip unknown item ids and log one line (Doc R13 3.11). */
    UFUNCTION(BlueprintNativeEvent, Category = "Save") bool restoreFromSnapshot(const FInstancedStruct& snapshot, int32 savedVersion);

    /** One step from fromVersion to fromVersion + 1, written as data. Return false when no step exists: the set is refused. */
    UFUNCTION(BlueprintNativeEvent, Category = "Save") bool migrateSnapshotStep(int32 fromVersion, const FInstancedStruct& oldSnapshot, FInstancedStruct& newSnapshot) const;

    /** World scope only, optional: fill the slot-list fields (day, season, difficulty). Return true if you changed contribution. */
    UFUNCTION(BlueprintNativeEvent, Category = "Save") bool contributeToSlotHeader(const FS_SaveSlotHeader& display, FS_SaveSlotHeader& contribution) const;
};