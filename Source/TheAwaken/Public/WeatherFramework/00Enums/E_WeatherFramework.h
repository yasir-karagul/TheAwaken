#pragma once

#include "CoreMinimal.h"
#include "E_WeatherFramework.generated.h"

UENUM(BlueprintType)
enum class E_WeatherType : uint8{
    Clear,
    Cloudy,
    Rain,
    Storm,
    Fog,
    Snow,
    Blizzard
};

// The key of DT_SeasonWeather. R12's E_Season has four values; Snowy is a flag there.
UENUM(BlueprintType)
enum class E_WeatherSeasonKey : uint8{
    Spring,
    Summer,
    Fall,
    Winter,
    Snowy
};

// Below 35 light, 35 to 70 normal, above 70 heavy (B14 3.7). The limits are data.
UENUM(BlueprintType)
enum class E_WeatherStrengthBand : uint8{
    Light,
    Normal,
    Heavy
};
