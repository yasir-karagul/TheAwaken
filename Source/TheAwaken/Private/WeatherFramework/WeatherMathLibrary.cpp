#include "WeatherFramework/WeatherMathLibrary.h"
#include "WeatherFramework/DA_WeatherTuning.h"

E_WeatherSeasonKey UWeatherMathLibrary::makeSeasonKey(E_Season season, bool snowyBlockActive){
    switch(season){
        case E_Season::Spring:{
            return E_WeatherSeasonKey::Spring;
        }
        case E_Season::Summer:{
            return E_WeatherSeasonKey::Summer;
        }
        case E_Season::Fall:{
            return E_WeatherSeasonKey::Fall;
        }
        case E_Season::Winter:{
            if(snowyBlockActive){
                return E_WeatherSeasonKey::Snowy;
            }
            return E_WeatherSeasonKey::Winter;
        }
    }
    return E_WeatherSeasonKey::Spring;
}

bool UWeatherMathLibrary::isRainType(E_WeatherType type){
    return type == E_WeatherType::Rain || type == E_WeatherType::Storm;
}

bool UWeatherMathLibrary::isSnowType(E_WeatherType type){
    return type == E_WeatherType::Snow || type == E_WeatherType::Blizzard;
}

E_WeatherStrengthBand UWeatherMathLibrary::getStrengthBand(float strength, const UDA_WeatherTuning& tuning){
    if(strength < tuning.lightBelow){
        return E_WeatherStrengthBand::Light;
    }
    if(strength > tuning.heavyAbove){
        return E_WeatherStrengthBand::Heavy;
    }
    return E_WeatherStrengthBand::Normal;
}

float UWeatherMathLibrary::getRiverTarget(E_WeatherType type, float strength, const UDA_WeatherTuning& tuning){
    if(!isRainType(type)){
        return tuning.riverNormalLevel;
    }
    if(type == E_WeatherType::Storm){
        return tuning.riverTargetHeavyRain;
    }
    switch(getStrengthBand(strength, tuning)){
        case E_WeatherStrengthBand::Light:{
            return tuning.riverTargetLightRain;
        }
        case E_WeatherStrengthBand::Heavy:{
            return tuning.riverTargetHeavyRain;
        }
        case E_WeatherStrengthBand::Normal:{
            if(tuning.riverTargetNormalRain > 0.0f){
                return tuning.riverTargetNormalRain;
            }
            return (tuning.riverTargetLightRain + tuning.riverTargetHeavyRain) * 0.5f;
        }
    }
    return tuning.riverNormalLevel;
}

float UWeatherMathLibrary::moveToward(float current, float target, float maxDelta){
    if(target > current){
        return FMath::Min(current + maxDelta, target);
    }
    return FMath::Max(current - maxDelta, target);
}

float UWeatherMathLibrary::stepRiverStrength(float current, float target, float hours, const UDA_WeatherTuning& tuning){
    if(target > current){
        // Same time to reach any target from normal, so a smaller target means a slower rise.
        const float riseRate = (target - tuning.riverNormalLevel) / FMath::Max(tuning.riverRiseHours, 0.01f);
        return moveToward(current, target, riseRate * hours);
    }
    const float fallRate = (tuning.riverMaxLevel - tuning.riverNormalLevel) / FMath::Max(tuning.riverFallHours, 0.01f);
    return moveToward(current, target, fallRate * hours);
}

float UWeatherMathLibrary::stepIceLevel(float current, E_WeatherSeasonKey key, float hours, const UDA_WeatherTuning& tuning){
    float target = 0.0f;
    float hoursToMove = tuning.iceHoursToThaw;
    if(key == E_WeatherSeasonKey::Snowy){
        target = 1.0f;
        hoursToMove = tuning.iceHoursToFreezeSnowy;
    }else if(key == E_WeatherSeasonKey::Winter){
        target = 1.0f;
        hoursToMove = tuning.iceHoursToFreezeWinter;
    }
    const float ratePerHour = 1.0f / FMath::Max(hoursToMove, 0.01f);
    return moveToward(current, target, ratePerHour * hours);
}

bool UWeatherMathLibrary::updateIceSolid(bool wasSolid, float iceLevel, const UDA_WeatherTuning& tuning){
    if(wasSolid){
        // Switches off only below 1 minus the margin (Index A97, A99).
        return iceLevel >= 1.0f - tuning.iceThawMargin;
    }
    return iceLevel >= 1.0f;
}

float UWeatherMathLibrary::stepSnowLevel(float current, E_WeatherType type, float strength, E_WeatherSeasonKey key, float hours, const UDA_WeatherTuning& tuning){
    float level = current;
    if(isSnowType(type)){
        level += tuning.snowAccumulatePerHourAtFull * (strength / 100.0f) * hours;
    }else if(key != E_WeatherSeasonKey::Winter && key != E_WeatherSeasonKey::Snowy){
        level -= tuning.snowMeltPerHour * hours;
    }
    return FMath::Clamp(level, 0.0f, 1.0f);
}

// Used at world creation and after a long dev skip. Same formula for ice and snow (W-IG15):
// in Winter, min(1, plain days / winterDays + snowy days / snowyDays); in Spring, the thaw runs down from 1.
float UWeatherMathLibrary::deriveCalendarLevel(E_WeatherSeasonKey key, int32 dayInSeason, int32 snowyStartDay, int32 snowyLengthDays, float winterDays, float snowyDays, float thawDays){
    switch(key){
        case E_WeatherSeasonKey::Spring:{
            const float level = 1.0f - static_cast<float>(dayInSeason) / FMath::Max(thawDays, 0.01f);
            return FMath::Clamp(level, 0.0f, 1.0f);
        }
        case E_WeatherSeasonKey::Winter:
        case E_WeatherSeasonKey::Snowy:{
            int32 snowyDaysSoFar = 0;
            if(snowyLengthDays > 0){
                snowyDaysSoFar = FMath::Clamp(dayInSeason - snowyStartDay, 0, snowyLengthDays);
            }
            const int32 plainDaysSoFar = FMath::Max(0, dayInSeason - snowyDaysSoFar);
            const float level = static_cast<float>(plainDaysSoFar) / FMath::Max(winterDays, 0.01f) + static_cast<float>(snowyDaysSoFar) / FMath::Max(snowyDays, 0.01f);
            return FMath::Clamp(level, 0.0f, 1.0f);
        }
        default:{
            return 0.0f;
        }
    }
}

const FS_SeasonWeatherRow* UWeatherMathLibrary::pickWeightedRow(const TArray<FS_SeasonWeatherRow>& rows, float roll01){
    float total = 0.0f;
    for(const FS_SeasonWeatherRow& row : rows){
        total += row.weight;
    }
    if(total <= 0.0f){
        return nullptr;
    }
    const float target = FMath::Clamp(roll01, 0.0f, 0.99999f) * total;
    float running = 0.0f;
    for(const FS_SeasonWeatherRow& row : rows){
        running += row.weight;
        if(target < running){
            return &row;
        }
    }
    return &rows.Last();
}

float UWeatherMathLibrary::getColdRate(float baseRate, float rain01, float snowLevel, const FS_ShelterState& shelter, const UDA_WeatherTuning& tuning){
    float rate = baseRate;
    rate += tuning.rainColdAddendAtFull * rain01 * (1.0f - shelter.shelterValue);
    rate *= 1.0f + tuning.snowDepthColdFactor * snowLevel;

    if(shelter.isIndoors){
        rate *= tuning.indoorsColdMultiplier;
    }else{
        rate *= FMath::Lerp(1.0f, tuning.shelteredColdMultiplier, shelter.shelterValue);
    }
    if(shelter.nearFire){
        rate *= tuning.fireColdMultiplier;
    }
    return FMath::Max(0.0f, rate);
}
