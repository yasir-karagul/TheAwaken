#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TimeFramework/00Enums/E_TimeFramework.h"
#include "WeatherFramework/00Enums/E_WeatherFramework.h"
#include "WeatherFramework/01Structs/S_WeatherFramework.h"
#include "WeatherMathLibrary.generated.h"

class UDA_WeatherTuning;

UCLASS()
class THEAWAKEN_API UWeatherMathLibrary : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // Query
    static E_WeatherSeasonKey makeSeasonKey(E_Season season, bool snowyBlockActive);
    static bool isRainType(E_WeatherType type);
    static bool isSnowType(E_WeatherType type);
    static E_WeatherStrengthBand getStrengthBand(float strength, const UDA_WeatherTuning& tuning);
    static float getRiverTarget(E_WeatherType type, float strength, const UDA_WeatherTuning& tuning);
    static float stepRiverStrength(float current, float target, float hours, const UDA_WeatherTuning& tuning);
    static float stepIceLevel(float current, E_WeatherSeasonKey key, float hours, const UDA_WeatherTuning& tuning);
    static bool updateIceSolid(bool wasSolid, float iceLevel, const UDA_WeatherTuning& tuning);
    static float stepSnowLevel(float current, E_WeatherType type, float strength, E_WeatherSeasonKey key, float hours, const UDA_WeatherTuning& tuning);
    static float deriveCalendarLevel(E_WeatherSeasonKey key, int32 dayInSeason, int32 snowyStartDay, int32 snowyLengthDays, float winterDays, float snowyDays, float thawDays);
    static const FS_SeasonWeatherRow* pickWeightedRow(const TArray<FS_SeasonWeatherRow>& rows, float roll01);
    static float moveToward(float current, float target, float maxDelta);

    // The cold exposure rate for one player, from plain numbers (Weather guide, Step 14). Pure, so it can be tested
    // without a world. UWeatherSubsystem::getColdExposureRate gathers the inputs and calls this.
    static float getColdRate(float baseRate, float rain01, float snowLevel, const FS_ShelterState& shelter, const UDA_WeatherTuning& tuning);
};
