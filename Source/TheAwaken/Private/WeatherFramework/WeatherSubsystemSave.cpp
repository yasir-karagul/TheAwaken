#include "WeatherFramework/WeatherSubsystem.h"
#include "WeatherFramework/WeatherLog.h"
#include "WeatherFramework/DA_WeatherTuning.h"
#include "TimeFramework/TimeSubsystem.h"

FS_WeatherSaveRecord UWeatherSubsystem::getSnapshotForSave() const{
    FS_WeatherSaveRecord record;
    record.version = 1;
    record.type = simulation.state.type;
    record.strength = simulation.state.strength;
    record.periodEndGameTime = simulation.state.periodEndGameTime;
    record.snowLevel = simulation.state.snowLevel;
    for(const FS_WaterBodyState& body : simulation.bodies){
        FS_WaterBodySaveRecord saved;
        saved.bodyId = body.bodyId;
        saved.iceLevel = body.iceLevel;
        saved.riverStrength = body.riverStrength;
        saved.iceSolid = body.iceSolid;
        saved.hasCurrent = body.hasCurrent;
        saved.canFreeze = body.canFreeze;
        record.waterBodies.Add(saved);
    }
    for(const TPair<int64, float>& pair : simulation.rainHoursByDay){
        FS_RainDayRecord day;
        day.dayNumber = pair.Key;
        day.rainHours = pair.Value;
        record.rainDays.Add(day);
    }
    return record;
}

void UWeatherSubsystem::restoreFromSnapshot(const FS_WeatherSaveRecord& record){
    UTimeSubsystem* time = getTime();
    if(!isAuthority() || !tuning || !time){
        return;
    }
    if(!time->isClockReady()){
        UE_LOG(LogAwakenWeather, Warning, TEXT("[%s] Weather restore ran before Time was restored (Doc B06 order: Time first)"), *roleName());
    }
    simulation.activeSeasonKey = getCurrentSeasonKey();

    FS_WeatherState restored;
    restored.type = record.type;
    restored.strength = record.strength;
    restored.periodEndGameTime = record.periodEndGameTime;
    restored.snowLevel = record.snowLevel;
    // Always above the published value, so clients raise Weather.Changed after a reload. [W-IG5]
    restored.periodIndex = publishedState.periodIndex + 1;
    simulation.state = restored;

    for(const FS_WaterBodySaveRecord& saved : record.waterBodies){
        FS_WaterBodyState* body = simulation.bodies.FindByPredicate([&saved](const FS_WaterBodyState& candidate){
            return candidate.bodyId == saved.bodyId;
        });
        if(!body){
            simulation.bodies.AddDefaulted();
            body = &simulation.bodies.Last();
            body->bodyId = saved.bodyId;
            body->hasCurrent = saved.hasCurrent;
            body->canFreeze = saved.canFreeze;
        }
        body->iceLevel = saved.iceLevel;
        body->riverStrength = saved.riverStrength;
        body->iceSolid = body->canFreeze && saved.iceSolid;
        pendingDeriveIds.Remove(saved.bodyId);
    }

    simulation.rainHoursByDay.Reset();
    for(const FS_RainDayRecord& day : record.rainDays){
        simulation.rainHoursByDay.Add(day.dayNumber, day.rainHours);
    }

    // A body that loaded before this restore and has no saved record gets the calendar values.
    isInitialized = true;
    for(FS_WaterBodyState& body : simulation.bodies){
        if(pendingDeriveIds.Contains(body.bodyId)){
            resetBodyFromCalendar(body);
        }
    }
    pendingDeriveIds.Reset();

    simulation.pendingFillUnits = 0.0f;
    simulation.lastAdvanceGameTime = time->getGameTime();
    simulation.needsCalendarRederive = false;
    reschedulePeriodTimer();
    publishState();
    UE_LOG(LogAwakenWeather, Log, TEXT("[%s] Weather restored: type %d strength %.0f, %d water bodies, %d rain days"), *roleName(), static_cast<int32>(simulation.state.type), simulation.state.strength, simulation.bodies.Num(), simulation.rainHoursByDay.Num());
}
