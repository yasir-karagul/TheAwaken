#pragma once
#include "StructUtils/InstancedStruct.h"
#include "SaveFramework/SaveParticipantComponent.h"
#include "TimeSaveParticipant.generated.h"

UCLASS(ClassGroup = (Save), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API UTimeSaveParticipant : public USaveParticipantComponent{
    GENERATED_BODY()
public:
    UTimeSaveParticipant(){
        saveKey = TEXT("Time");   // never rename after release (Doc02 19.3)
        scope = E_SaveScope::World;
        snapshotVersion = 1;
    }

    virtual bool getSnapshotForSave_Implementation(FInstancedStruct& snapshot) const override;
    virtual bool restoreFromSnapshot_Implementation(const FInstancedStruct& snapshot, int32 savedVersion) override;
    // Fills the day and the season of the slot list (the Save framework reads them from the World participants).
    virtual bool contributeToSlotHeader_Implementation(const FS_SaveSlotHeader& display, FS_SaveSlotHeader& contribution) const override;
};
