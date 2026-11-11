#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SaveFramework/01Structs/S_SaveFramework.h"
#include "AwakenSaveGame.generated.h"

UCLASS(BlueprintType)
class THEAWAKEN_API UAwakenSaveGameBase : public USaveGame{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) TArray<FS_SnapshotRecord> snapshots;

	void setSnapshot(FName key, int32 version, const FInstancedStruct& data);
	const FS_SnapshotRecord* findSnapshot(FName key) const;

	/** Copies every record of Previous that this save did not handle: a framework that is not registered right now, or whose
	 *  getSnapshotForSave returned false. A record whose struct type no longer exists cannot be copied (it would be written empty). */
	void carryForward(const UAwakenSaveGameBase* previous, const TSet<FName>& handledKeys);
};

/** One per slot. Owned by the host. Time, buildings, unlocks, bags lying in the world... (Doc R13 3.4). */
UCLASS(BlueprintType)
class THEAWAKEN_API UAwakenWorldSave : public UAwakenSaveGameBase{
	GENERATED_BODY()
public:
	UPROPERTY(SaveGame) TArray<FS_BagRecord> bags;
};

/** One per player identity, held on the host. Bag, meters, worn gear, position... */
UCLASS(BlueprintType)
class THEAWAKEN_API UAwakenCharacterSave : public UAwakenSaveGameBase{
	GENERATED_BODY()
};