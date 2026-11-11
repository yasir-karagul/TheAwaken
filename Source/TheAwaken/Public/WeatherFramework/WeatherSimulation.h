#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "WeatherFramework/00Enums/E_WeatherFramework.h"
#include "WeatherFramework/01Structs/S_WeatherFramework.h"

class UDA_WeatherTuning;

// One in-game hour in game seconds. GameTime counts game seconds (Doc R12 3.8, Doc02 19.4).
static constexpr int64 gameSecondsPerHour = 3600;

// Plain C++. No world, no Time, no carrier. A sleep skip and the same time passing normally run
// the same code here (W-IG4, W-IG22).
class THEAWAKEN_API FWeatherSimulation{
public:
    // ---- data (host working state) ----
    const UDA_WeatherTuning* tuning = nullptr;
    TMap<E_WeatherSeasonKey, TArray<FS_SeasonWeatherRow>> rowsBySeason;
    FS_WeatherState state;
    TArray<FS_WaterBodyState> bodies;
    TMap<int64, float> rainHoursByDay;
    float pendingFillUnits = 0.0f;
    int64 lastAdvanceGameTime = 0;
    E_WeatherSeasonKey activeSeasonKey = E_WeatherSeasonKey::Spring;
    int64 currentDayNumber = 0;
    bool needsCalendarRederive = false;
    FRandomStream random;

    // ---- commands ----
    void setRows(const TArray<FS_SeasonWeatherRow>& allRows);
    void advanceTo(int64 targetGameTime);
    void startNextPeriod(int64 startGameTime);
    void startForcedPeriod(E_WeatherType type, float strength, int64 startGameTime);

    // ---- queries ----
    bool isTypeAllowed(E_WeatherSeasonKey key, E_WeatherType type) const;
    bool isMoving() const;

private:
    FS_WeatherState makeNextPeriod(int64 startGameTime);
    void integrateSegment(float hours);
};
