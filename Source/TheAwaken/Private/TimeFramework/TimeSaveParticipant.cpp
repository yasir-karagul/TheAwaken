#include "TimeFramework/TimeSaveParticipant.h"
#include "TimeFramework/TimeSubsystem.h"
#include "TimeFramework/02Tags/T_TimeFramework.h"

bool UTimeSaveParticipant::getSnapshotForSave_Implementation(FInstancedStruct& snapshot) const{
    const UTimeSubsystem* time = UTimeSubsystem::get(this);
    if(!time){
        return false;   // nothing new to say: Save keeps the previous record
    }
    snapshot = FInstancedStruct::Make(time->getSnapshotForSave());
    return true;
}

bool UTimeSaveParticipant::restoreFromSnapshot_Implementation(const FInstancedStruct& snapshot, int32 savedVersion){
    const FS_TimeSaveRecord* record = snapshot.GetPtr<FS_TimeSaveRecord>();
    UTimeSubsystem* time = UTimeSubsystem::get(this);
    if(!record || !time){
        return false;
    }
    time->restoreFromSnapshot(*record);   // first simulation step: every later timer reads this clock
    return true;
}

bool UTimeSaveParticipant::contributeToSlotHeader_Implementation(const FS_SaveSlotHeader& display, FS_SaveSlotHeader& contribution) const{
    const UTimeSubsystem* time = UTimeSubsystem::get(this);
    if(!time){
        return false;
    }
    contribution.dayCount = static_cast<int32>(time->getDaysPassed());
    switch(time->getCurrentSeason()){
        case E_Season::Spring:
            contribution.seasonTag = T_Time_Season_Spring.GetTag();
            break;
        case E_Season::Summer:
            contribution.seasonTag = T_Time_Season_Summer.GetTag();
            break;
        case E_Season::Fall:
            contribution.seasonTag = T_Time_Season_Fall.GetTag();
            break;
        case E_Season::Winter:
            contribution.seasonTag = T_Time_Season_Winter.GetTag();
            break;
    }
    return true;
}
