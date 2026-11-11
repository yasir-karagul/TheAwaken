#pragma once

#include "CoreMinimal.h"
#include "TimeFramework/00Enums/E_TimeFramework.h"
#include "WeatherFramework/00Enums/E_WeatherFramework.h"

// What the environment manager reads from Time (R12) and Weather (B14). Two small wrapper functions in one file, so a stub is a one-file change.

struct FS_EnvironmentTimeInput{
    // True once the clock exists on this machine: initialised on the host, the carrier present on a client.
    bool isReady = false;

    // Hours, 0 to 24 (UTimeSubsystem::getCurrentTimeOfDay).
    float timeOfDayHours = 12.0f;

    E_DayNightPhase phase = E_DayNightPhase::Day;
    E_Season season = E_Season::Spring;
    bool isSnowyBlock = false;
    bool isGroupSleepSkipRunning = false;

    // The jump counter that R12's clock sync carries (gap G9). It changes on every clock jump, also a dev command skip.
    bool hasSkipSerial = false;
    int32 skipSerial = 0;
};

struct FS_EnvironmentWeatherInput{
    bool isReady = false;
    E_WeatherType type = E_WeatherType::Clear;

    // 0 to 100.
    float rainStrength = 0.0f;
    float snowStrength = 0.0f;

    // 0 to 1.
    float snowLevel = 0.0f;
};

namespace EnvironmentSources{
    FS_EnvironmentTimeInput readTime(const UObject* worldContext);
    FS_EnvironmentWeatherInput readWeather(const UObject* worldContext);
}
