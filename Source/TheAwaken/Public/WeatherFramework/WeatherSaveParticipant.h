#pragma once
#include "StructUtils/InstancedStruct.h"
#include "SaveFramework/SaveParticipantComponent.h"
#include "WeatherSaveParticipant.generated.h"

UCLASS(ClassGroup = (Save), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API UWeatherSaveParticipant : public USaveParticipantComponent{
    GENERATED_BODY()
public:
    UWeatherSaveParticipant(){
        saveKey = TEXT("Weather");   // never rename after release (Doc02 19.3)
        scope = E_SaveScope::World;
        snapshotVersion = 1;
    }

    virtual bool getSnapshotForSave_Implementation(FInstancedStruct& snapshot) const override;
    virtual bool restoreFromSnapshot_Implementation(const FInstancedStruct& snapshot, int32 savedVersion) override;
};
