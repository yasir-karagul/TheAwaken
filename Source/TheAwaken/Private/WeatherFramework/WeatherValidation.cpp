#include "WeatherFramework/WeatherValidation.h"
#include "WeatherFramework/DA_WeatherTuning.h"
#include "WeatherFramework/01Structs/S_WeatherFramework.h"
#include "Engine/DataTable.h"

namespace WeatherValidation{

    bool validateWeatherData(const UDataTable* seasonTable, const UDA_WeatherTuning* tuning, TArray<FString>& errors, TArray<FString>& warnings){
        if(seasonTable == nullptr){
            errors.Add(TEXT("DT_SeasonWeather is not set"));
        }
        if(tuning == nullptr){
            errors.Add(TEXT("DA_WeatherTuning is not set"));
        }
        if(seasonTable == nullptr || tuning == nullptr){
            return false;
        }

        TMap<E_WeatherSeasonKey, float> weightBySeason;

        seasonTable->ForeachRow<FS_SeasonWeatherRow>(TEXT("WeatherValidation"), [&](const FName& rowName, const FS_SeasonWeatherRow& row){
            const FString name = rowName.ToString();
            if(row.weight < 0.0f){
                errors.Add(FString::Printf(TEXT("Row %s: weight is below 0"), *name));
            }
            if(row.strengthMin > row.strengthMax){
                errors.Add(FString::Printf(TEXT("Row %s: strengthMin is above strengthMax"), *name));
            }
            if(row.strengthMin < 0.0f || row.strengthMax > 100.0f){
                errors.Add(FString::Printf(TEXT("Row %s: strength is outside 0 to 100"), *name));
            }
            if(row.durationMinHours > 0.0f && row.durationMaxHours > 0.0f && row.durationMinHours > row.durationMaxHours){
                errors.Add(FString::Printf(TEXT("Row %s: durationMinHours is above durationMaxHours"), *name));
            }
            const bool needsStrength = row.weatherType == E_WeatherType::Rain || row.weatherType == E_WeatherType::Storm || row.weatherType == E_WeatherType::Snow || row.weatherType == E_WeatherType::Blizzard;
            if(needsStrength && row.strengthMax <= 0.0f){
                errors.Add(FString::Printf(TEXT("Row %s: a rain or snow type needs a strength above 0"), *name));
            }
            if(!needsStrength && row.strengthMax > 0.0f){
                warnings.Add(FString::Printf(TEXT("Row %s: Clear, Cloudy and Fog ignore strength"), *name));
            }
            if(row.seasonKey == E_WeatherSeasonKey::Snowy && row.weatherType == E_WeatherType::Clear && row.weight > 0.0f){
                errors.Add(FString::Printf(TEXT("Row %s: the Snowy block has no Clear weather (B14 3.7)"), *name));
            }
            weightBySeason.FindOrAdd(row.seasonKey) += row.weight;
        });

        const E_WeatherSeasonKey allKeys[] = {E_WeatherSeasonKey::Spring, E_WeatherSeasonKey::Summer, E_WeatherSeasonKey::Fall, E_WeatherSeasonKey::Winter, E_WeatherSeasonKey::Snowy};
        for(const E_WeatherSeasonKey key : allKeys){
            const float* total = weightBySeason.Find(key);
            if(total == nullptr || *total <= 0.0f){
                errors.Add(FString::Printf(TEXT("Season key %d has no weather rows"), static_cast<int32>(key)));
            }else if(!FMath::IsNearlyEqual(*total, 100.0f, 0.01f)){
                warnings.Add(FString::Printf(TEXT("Season key %d: weights sum to %.2f, B14 3.7 uses 100"), static_cast<int32>(key), *total));
            }
        }

        // Tuning
        if(tuning->defaultPeriodMinHours <= 0.0f || tuning->defaultPeriodMinHours > tuning->defaultPeriodMaxHours){
            errors.Add(TEXT("Default period hours: min must be above 0 and not above max"));
        }
        if(tuning->lightBelow >= tuning->heavyAbove){
            errors.Add(TEXT("lightBelow must be below heavyAbove"));
        }
        if(tuning->weatherStepSeconds <= 0.0f || tuning->shelterIntervalSeconds <= 0.0f){
            errors.Add(TEXT("weatherStepSeconds and shelterIntervalSeconds must be above 0"));
        }
        if(tuning->riverNormalLevel >= tuning->riverMaxLevel || tuning->riverRiseHours <= 0.0f || tuning->riverFallHours <= 0.0f){
            errors.Add(TEXT("River: normal must be below max, rise and fall hours above 0"));
        }
        if(tuning->riverTargetLightRain < tuning->riverNormalLevel || tuning->riverTargetHeavyRain > tuning->riverMaxLevel || tuning->riverTargetLightRain > tuning->riverTargetHeavyRain){
            errors.Add(TEXT("River targets must satisfy normal <= light <= heavy <= max"));
        }
        if(tuning->strongCurrentThreshold <= tuning->riverNormalLevel || tuning->strongCurrentThreshold > tuning->riverMaxLevel){
            errors.Add(TEXT("strongCurrentThreshold must be above the normal level and not above the max"));
        }
        if(tuning->iceHoursToFreezeWinter <= 0.0f || tuning->iceHoursToFreezeSnowy <= 0.0f || tuning->iceHoursToThaw <= 0.0f){
            errors.Add(TEXT("Ice hours must be above 0"));
        }
        if(tuning->iceThawMargin < 0.0f || tuning->iceThawMargin >= 0.5f){
            errors.Add(TEXT("iceThawMargin must be at least 0 and below 0.5"));
        }
        if(tuning->shelterUpTraceCount < 1 || tuning->shelterSideTraceCount < tuning->indoorsWallHitCount){
            errors.Add(TEXT("Shelter traces: at least 1 upward trace, and sideways traces at least the indoors hit count"));
        }
        if(tuning->shelteredThreshold <= 0.0f || tuning->shelteredThreshold > 1.0f){
            errors.Add(TEXT("shelteredThreshold must be above 0 and not above 1"));
        }
        if(tuning->snowBucketCount < 1 || tuning->rainHistoryDays < 1){
            errors.Add(TEXT("snowBucketCount and rainHistoryDays must be at least 1"));
        }

        // Values nobody has written yet (section 7 of this guide). A warning, not a failure.
        bool allColdZero = true;
        for(const TPair<E_WeatherSeasonKey, float>& pair : tuning->coldRateBySeason){
            if(pair.Value > 0.0f){
                allColdZero = false;
            }
        }
        if(allColdZero){
            warnings.Add(TEXT("All cold base rates are 0: cold exposure is inactive until the designer sets them"));
        }
        if(tuning->snowAccumulatePerHourAtFull <= 0.0f || tuning->snowMeltPerHour <= 0.0f){
            warnings.Add(TEXT("Snow build or melt rate is 0: snow level will not change in play"));
        }
        if(tuning->rainFillUnitsPerHourAtFull <= 0.0f){
            warnings.Add(TEXT("rainFillUnitsPerHourAtFull is 0: rain fills no container"));
        }
        if(tuning->fireSearchRadius <= 0.0f){
            warnings.Add(TEXT("fireSearchRadius is 0: a lit fire does not lower cold exposure"));
        }
        if(tuning->rainHistoryDays < 120){
            warnings.Add(TEXT("rainHistoryDays is below 120: a crop planted late in Fall can lose its pre-winter rain by Spring (B10 3.5)"));
        }

        return errors.Num() == 0;
    }

}
