#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SaveFramework/03Interfaces/BPI_SaveParticipant.h"
#include "SaveParticipantComponent.generated.h"

UCLASS(Abstract, Blueprintable, ClassGroup = (Save), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API USaveParticipantComponent : public UActorComponent, public IBPI_SaveParticipant{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save") FName saveKey;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save") E_SaveScope scope = E_SaveScope::World;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Save", meta = (ClampMin = "1")) int32 snapshotVersion = 1;

	virtual FName getSaveKey_Implementation() const override{
		return saveKey;
	}
	virtual E_SaveScope getSaveScope_Implementation() const override{
		return scope;
	}
	virtual int32 getSnapshotVersion_Implementation() const override{
		return snapshotVersion;
	}
	virtual APlayerState* getSaveOwner_Implementation() const override;

	// Step 15: safe defaults. A Blueprint subclass that overrides only some events must never reach an undefined native function.
	// write nothing: the previous record is kept
	virtual bool getSnapshotForSave_Implementation(FInstancedStruct& snapshot) const override{
		return false;
	}
	virtual bool restoreFromSnapshot_Implementation(const FInstancedStruct& snapshot, int32 savedVersion) override{
		return true;
	}
	// no step: the set is refused
	virtual bool migrateSnapshotStep_Implementation(int32 fromVersion, const FInstancedStruct& oldSnapshot, FInstancedStruct& newSnapshot) const override{
		return false;
	}
	virtual bool contributeToSlotHeader_Implementation(const FS_SaveSlotHeader& display, FS_SaveSlotHeader& contribution) const override{
		return false;
	}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;
};