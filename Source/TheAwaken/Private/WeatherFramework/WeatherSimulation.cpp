#include "WeatherFramework/WeatherSimulation.h"
#include "WeatherFramework/WeatherLog.h"
#include "WeatherFramework/WeatherMathLibrary.h"
#include "WeatherFramework/DA_WeatherTuning.h"

void FWeatherSimulation::setRows(const TArray<FS_SeasonWeatherRow>& allRows){
    rowsBySeason.Empty();
    for(const FS_SeasonWeatherRow& row : allRows){
        if(row.weight > 0.0f){
            rowsBySeason.FindOrAdd(row.seasonKey).Add(row);
        }
    }
}

// A type is allowed in a season when the table has a row for it with weight above 0 (B14 3.7: a running
// period ends at a season or Snowy change only if its type is not allowed in the new one, Index A106 S3-F14).
bool FWeatherSimulation::isTypeAllowed(E_WeatherSeasonKey key, E_WeatherType type) const{
    const TArray<FS_SeasonWeatherRow>* rows = rowsBySeason.Find(key);
    if(rows == nullptr){
        return false;
    }
    for(const FS_SeasonWeatherRow& row : *rows){
        if(row.weatherType == type){
            return true;
        }
    }
    return false;
}

FS_WeatherState FWeatherSimulation::makeNextPeriod(int64 startGameTime){
    FS_WeatherState next = state;
    FS_SeasonWeatherRow chosen;
    const TArray<FS_SeasonWeatherRow>* rows = rowsBySeason.Find(activeSeasonKey);
    if(rows != nullptr){
        const FS_SeasonWeatherRow* picked = UWeatherMathLibrary::pickWeightedRow(*rows, random.FRand());
        if(picked != nullptr){
            chosen = *picked;
        }
    }else{
        UE_LOG(LogAwakenWeather, Error, TEXT("No weather rows for season key %d, using Clear"), static_cast<int32>(activeSeasonKey));
    }

    next.type = chosen.weatherType;
    next.strength = random.FRandRange(chosen.strengthMin, FMath::Max(chosen.strengthMin, chosen.strengthMax));

    const float minHours = (chosen.durationMinHours > 0.0f) ? chosen.durationMinHours : tuning->defaultPeriodMinHours;
    const float maxHours = (chosen.durationMaxHours > 0.0f) ? chosen.durationMaxHours : tuning->defaultPeriodMaxHours;
    const float hours = random.FRandRange(minHours, FMath::Max(minHours, maxHours));
    next.periodEndGameTime = startGameTime + static_cast<int64>(hours * static_cast<float>(gameSecondsPerHour));
    next.periodIndex = state.periodIndex + 1;
    return next;
}

void FWeatherSimulation::startNextPeriod(int64 startGameTime){
    state = makeNextPeriod(startGameTime);
}

void FWeatherSimulation::startForcedPeriod(E_WeatherType type, float strength, int64 startGameTime){
    state.type = type;
    state.strength = strength;
    const float hours = random.FRandRange(tuning->defaultPeriodMinHours, FMath::Max(tuning->defaultPeriodMinHours, tuning->defaultPeriodMaxHours));
    state.periodEndGameTime = startGameTime + static_cast<int64>(hours * static_cast<float>(gameSecondsPerHour));
    state.periodIndex++;
}

static constexpr int64 rederiveSpanSeconds = 24 * gameSecondsPerHour;
static constexpr int32 maxSegmentsPerAdvance = 256;

void FWeatherSimulation::advanceTo(int64 targetGameTime){
    if(tuning == nullptr || targetGameTime <= lastAdvanceGameTime){
        return;
    }
    if(targetGameTime - lastAdvanceGameTime > rederiveSpanSeconds){
        // Only a dev time command can jump this far. Re-derive after the rollover. [W-IG16]
        lastAdvanceGameTime = targetGameTime;
        needsCalendarRederive = true;
        return;
    }

    int64 cursor = lastAdvanceGameTime;
    int32 segments = 0;
    while(cursor < targetGameTime && segments < maxSegmentsPerAdvance){
        segments++;
        if(state.periodEndGameTime <= cursor){
            startNextPeriod(cursor);
            continue;
        }
        const int64 segmentEnd = FMath::Min(targetGameTime, state.periodEndGameTime);
        integrateSegment(static_cast<float>(segmentEnd - cursor) / static_cast<float>(gameSecondsPerHour));
        cursor = segmentEnd;
        if(cursor >= state.periodEndGameTime){
            startNextPeriod(cursor);
        }
    }
    lastAdvanceGameTime = targetGameTime;
}

void FWeatherSimulation::integrateSegment(float hours){
    if(UWeatherMathLibrary::isRainType(state.type)){
        rainHoursByDay.FindOrAdd(currentDayNumber) += hours;
        pendingFillUnits += tuning->rainFillUnitsPerHourAtFull * (state.strength / 100.0f) * hours;
    }

    const float riverTarget = UWeatherMathLibrary::getRiverTarget(state.type, state.strength, *tuning);
    for(FS_WaterBodyState& body : bodies){
        if(body.hasCurrent){
            body.riverStrength = UWeatherMathLibrary::stepRiverStrength(body.riverStrength, riverTarget, hours, *tuning);
        }
        if(body.canFreeze){
            body.iceLevel = UWeatherMathLibrary::stepIceLevel(body.iceLevel, activeSeasonKey, hours, *tuning);
            body.iceSolid = UWeatherMathLibrary::updateIceSolid(body.iceSolid, body.iceLevel, *tuning);
        }
    }

    state.snowLevel = UWeatherMathLibrary::stepSnowLevel(state.snowLevel, state.type, state.strength, activeSeasonKey, hours, *tuning);
}

bool FWeatherSimulation::isMoving() const{
    if(tuning == nullptr){
        return false;
    }
    // Rain fills containers and counts rain hours; snow builds the level.
    if(UWeatherMathLibrary::isRainType(state.type) || UWeatherMathLibrary::isSnowType(state.type)){
        return true;
    }
    const bool isFreezingSeason = activeSeasonKey == E_WeatherSeasonKey::Winter || activeSeasonKey == E_WeatherSeasonKey::Snowy;
    if(!isFreezingSeason && state.snowLevel > 0.0f && tuning->snowMeltPerHour > 0.0f){
        return true;
    }
    for(const FS_WaterBodyState& body : bodies){
        // With no rain the river target is the normal level.
        if(body.hasCurrent && !FMath::IsNearlyEqual(body.riverStrength, tuning->riverNormalLevel)){
            return true;
        }
        if(body.canFreeze && !FMath::IsNearlyEqual(body.iceLevel, isFreezingSeason ? 1.0f : 0.0f)){
            return true;
        }
    }
    return false;
}
