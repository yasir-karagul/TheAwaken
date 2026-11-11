#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "WeatherFramework/00Enums/E_WeatherFramework.h"
#include "DA_WeatherTuning.generated.h"

UCLASS(BlueprintType)
class THEAWAKEN_API UDA_WeatherTuning : public UPrimaryDataAsset{
    GENERATED_BODY()

public:
    UDA_WeatherTuning();

    float getColdBaseRate(E_WeatherSeasonKey key) const;

    // Registers this asset under the Primary Asset Type "WeatherTuning" (W-IG24), like DA_TimeSettings does for "TimeSettings".
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    // ---- Data ----
    // The season table of 6.2. A soft pointer, loaded by the subsystem in Initialize.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Data")
    TSoftObjectPtr<UDataTable> seasonWeatherTable;

    // ---- Periods [SPEC B14 3.7] ----
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Periods")
    float defaultPeriodMinHours = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Periods")
    float defaultPeriodMaxHours = 12.0f;

    // Below this a strength is Light. [SPEC]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Periods")
    float lightBelow = 35.0f;

    // Above this a strength is Heavy. [SPEC]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Periods")
    float heavyAbove = 70.0f;

    // Real seconds between host steps. Engineering value, not a design value. [W-IG3]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Periods")
    float weatherStepSeconds = 5.0f;

    // ---- River [SPEC B14 3.7, A100] ----
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="River")
    float riverNormalLevel = 30.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="River")
    float riverMaxLevel = 100.0f;

    // Light rain raises a river "slowly to about 60". [SPEC]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="River")
    float riverTargetLightRain = 60.0f;

    // Heavy rain or a storm takes it to 100. [SPEC]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="River")
    float riverTargetHeavyRain = 100.0f;

    // [UNWRITTEN] Normal rain. 0 means the middle of the light and heavy targets.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="River")
    float riverTargetNormalRain = 0.0f;

    // In-game hours to reach a target from the normal level. [SPEC: about 12]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="River")
    float riverRiseHours = 12.0f;

    // In-game hours to fall from the maximum back to normal. [SPEC: about 24]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="River")
    float riverFallHours = 24.0f;

    // Above this, players and NPCs move very slowly (R07 3.15). Read through the subsystem. [SPEC, W-IG19]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="River")
    float strongCurrentThreshold = 85.0f;

    // ---- Ice [SPEC B14 3.7 and 8] ----
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ice")
    float iceHoursToFreezeWinter = 72.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ice")
    float iceHoursToFreezeSnowy = 24.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ice")
    float iceHoursToThaw = 48.0f;

    // The name is approved (Index A99). [UNWRITTEN] the value; 0.02 keeps the collision from flickering.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Ice")
    float iceThawMargin = 0.02f;

    // ---- Snow [UNWRITTEN] ----
    // Levels per in-game hour at strength 100; scaled by strength. 0 = snow never builds.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Snow")
    float snowAccumulatePerHourAtFull = 0.0f;

    // Levels per in-game hour lost outside Winter and the Snowy block. 0 = snow never melts.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Snow")
    float snowMeltPerHour = 0.0f;

    // Weather.SnowLevelChanged fires when the level crosses one of this many equal steps. [W-IG17]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Snow")
    int32 snowBucketCount = 10;

    // ---- Cold exposure [UNWRITTEN: "set in playtest", Index A100] ----
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cold")
    TMap<E_WeatherSeasonKey, float> coldRateBySeason;

    // Added at rain strength 100, scaled down by the shelter value.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cold")
    float rainColdAddendAtFull = 0.0f;

    // The rate is multiplied by 1 + this * snowLevel ("deeper snow raises the rate", B14 3.6).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cold")
    float snowDepthColdFactor = 0.0f;

    // Multipliers at full shelter, indoors and near a fire. 1 = no reduction.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cold")
    float shelteredColdMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cold")
    float indoorsColdMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cold")
    float fireColdMultiplier = 1.0f;

    // 0 = the fire check is off. Each source also reports its own radius. [UNWRITTEN, B14 section 8]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cold")
    float fireSearchRadius = 0.0f;

    // ---- Shelter [SPEC B14 3.7 unless marked] ----
    // About once per second. [SPEC]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shelter")
    float shelterIntervalSeconds = 1.0f;

    // About 10 m. [SPEC]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shelter")
    float shelterUpTraceDistance = 1000.0f;

    // "A few". [UNWRITTEN] the exact count.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shelter")
    int32 shelterUpTraceCount = 3;

    // Ring radius of the upward traces, cm. [UNWRITTEN]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shelter")
    float shelterUpTraceSpread = 50.0f;

    // Eight sideways traces. [SPEC]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shelter")
    int32 shelterSideTraceCount = 8;

    // Length of a sideways trace, cm. [UNWRITTEN]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shelter")
    float shelterSideTraceDistance = 300.0f;

    // Six or more hits count as indoors. [SPEC]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shelter")
    int32 indoorsWallHitCount = 6;

    // A value at or above this counts as sheltered from rain and snow. [UNWRITTEN]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shelter")
    float shelteredThreshold = 0.5f;

    // Which collision channel the roof and wall traces use (Open question 5).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shelter")
    TEnumAsByte<ECollisionChannel> shelterTraceChannel = ECC_Visibility;

    // ---- Rain fill and history ----
    // Units of clean water per in-game hour at rain strength 100, scaled by strength. 0 = rain fills nothing. [UNWRITTEN]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rain")
    float rainFillUnitsPerHourAtFull = 0.0f;

    // How many past days of rain hours are kept. B10 3.5 credits rain from the planting day, and crops pause through
    // Winter and Snowy, so a crop planted late in Fall still counts rain in Spring. One year is 4 seasons of 30 days. [UNWRITTEN]
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rain")
    int32 rainHistoryDays = 120;
};