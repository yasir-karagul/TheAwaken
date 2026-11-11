#include "WeatherFramework/WeatherSaveParticipant.h"
#include "WeatherFramework/WeatherSubsystem.h"

bool UWeatherSaveParticipant::getSnapshotForSave_Implementation(FInstancedStruct& snapshot) const{
    const UWeatherSubsystem* weather = UWeatherSubsystem::get(this);
    if(!weather || !weather->isWeatherReady()){
        return false;   // nothing new to say: Save keeps the previous record
    }
    snapshot = FInstancedStruct::Make(weather->getSnapshotForSave());
    return true;
}

bool UWeatherSaveParticipant::restoreFromSnapshot_Implementation(const FInstancedStruct& snapshot, int32 savedVersion){
    const FS_WeatherSaveRecord* record = snapshot.GetPtr<FS_WeatherSaveRecord>();
    UWeatherSubsystem* weather = UWeatherSubsystem::get(this);
    if(!record || !weather){
        return false;
    }
    weather->restoreFromSnapshot(*record);   // Time has been restored before this (Doc B06 order: step 3 Time, step 4 Weather)
    return true;
}
