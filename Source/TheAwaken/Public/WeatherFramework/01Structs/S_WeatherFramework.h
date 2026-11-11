#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Misc/Guid.h"
#include "WeatherFramework/00Enums/E_WeatherFramework.h"
#include "S_WeatherFramework.generated.h"

class AActor;

// ---------------- Replicated state (carrier) ----------------

USTRUCT(BlueprintType)
struct FS_WeatherState{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    E_WeatherType type = E_WeatherType::Clear;

    // 0 to 100. Used by Rain, Storm, Snow and Blizzard.
    UPROPERTY(BlueprintReadOnly)
    float strength = 0.0f;

    // Absolute game time, in game seconds, at which this period ends (Doc02 19.4).
    UPROPERTY(BlueprintReadOnly)
    int64 periodEndGameTime = 0;

    // 0 to 1, one value for the world.
    UPROPERTY(BlueprintReadOnly)
    float snowLevel = 0.0f;

    // Counts up with every new period. Not saved: rebuilt as the published value plus 1 on a load. [W-IG5]
    UPROPERTY(BlueprintReadOnly)
    int32 periodIndex = 0;

    bool operator==(const FS_WeatherState& other) const{
        return type == other.type && strength == other.strength && periodEndGameTime == other.periodEndGameTime && snowLevel == other.snowLevel && periodIndex == other.periodIndex;
    }
};

USTRUCT(BlueprintType)
struct FS_WaterBodyState{
    GENERATED_BODY()

    // Stable id of the placed AWaterBody (Doc02 19.3, designer-placed kind).
    UPROPERTY(BlueprintReadOnly)
    FGuid bodyId;

    // 0 to 1.
    UPROPERTY(BlueprintReadOnly)
    float iceLevel = 0.0f;

    // 0 to 100, normal 30.
    UPROPERTY(BlueprintReadOnly)
    float riverStrength = 0.0f;

    // True while the ice collision is on (level reached 1, not yet below 1 minus the margin). Saved, because the level
    // alone cannot tell freezing from thawing near 1. [W-IG5]
    UPROPERTY(BlueprintReadOnly)
    bool iceSolid = false;

    UPROPERTY(BlueprintReadOnly)
    bool hasCurrent = false;

    UPROPERTY(BlueprintReadOnly)
    bool canFreeze = false;

    bool operator==(const FS_WaterBodyState& other) const{
        return bodyId == other.bodyId && iceLevel == other.iceLevel && riverStrength == other.riverStrength && iceSolid == other.iceSolid && hasCurrent == other.hasCurrent && canFreeze == other.canFreeze;
    }
};

// What AWaterBody (Doc B21) tells Weather when it loads. [W-IG8]
USTRUCT(BlueprintType)
struct FS_WaterBodyRegistration{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FGuid bodyId;

    // River and Stream: true. Lake and Sea: false.
    UPROPERTY(BlueprintReadWrite)
    bool hasCurrent = false;

    // River, Stream and Lake: true. Sea: see Open question 6.
    UPROPERTY(BlueprintReadWrite)
    bool canFreeze = false;
};

// Per player, owner only. [W-IG11]
USTRUCT(BlueprintType)
struct FS_ShelterState{
    GENERATED_BODY()

    // 0 to 1. The share of the upward traces that found a roof, piece or overhang.
    UPROPERTY(BlueprintReadOnly)
    float shelterValue = 0.0f;

    // Six or more of eight sideways traces hit walls, or the player is inside an indoor zone.
    UPROPERTY(BlueprintReadOnly)
    bool isIndoors = false;

    // A lit warmth source is close enough.
    UPROPERTY(BlueprintReadOnly)
    bool nearFire = false;

    bool operator==(const FS_ShelterState& other) const{
        return shelterValue == other.shelterValue && isIndoors == other.isIndoors && nearFire == other.nearFire;
    }
};

// What a water container reports to the rain collector. [W-IG12]
USTRUCT(BlueprintType)
struct FS_ContainerWaterState{
    GENERATED_BODY()

    // A container holds one kind at a time (B14 3.4).
    UPROPERTY(BlueprintReadWrite)
    bool holdsUncleanWater = false;

    UPROPERTY(BlueprintReadWrite)
    bool isBoiling = false;

    // Units of clean water the container can still take.
    UPROPERTY(BlueprintReadWrite)
    int32 freeCapacity = 0;
};

// ---------------- Data row (DT_SeasonWeather) ----------------

USTRUCT(BlueprintType)
struct FS_SeasonWeatherRow : public FTableRowBase{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    E_WeatherSeasonKey seasonKey = E_WeatherSeasonKey::Spring;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    E_WeatherType weatherType = E_WeatherType::Clear;

    // Percent of weather periods of this season. The weights of one season sum to 100.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0"))
    float weight = 0.0f;

    // Strength is drawn evenly between the two. Clear, Cloudy and Fog use 0 and 0.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="100.0"))
    float strengthMin = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0", ClampMax="100.0"))
    float strengthMax = 0.0f;

    // In-game hours. 0 means the default of the tuning table (3 to 12).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0"))
    float durationMinHours = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.0"))
    float durationMaxHours = 0.0f;
};

// ---------------- Save records (World file) ----------------

// Every field is SaveGame, and so are the fields of the nested structs: the Save framework writes through
// SaveGameToMemory, which skips every property without the SaveGame flag (same rule as FS_TimeSaveRecord).
USTRUCT(BlueprintType)
struct FS_WaterBodySaveRecord{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    FGuid bodyId;

    UPROPERTY(SaveGame)
    float iceLevel = 0.0f;

    UPROPERTY(SaveGame)
    float riverStrength = 0.0f;

    UPROPERTY(SaveGame)
    bool iceSolid = false;

    UPROPERTY(SaveGame)
    bool hasCurrent = false;

    UPROPERTY(SaveGame)
    bool canFreeze = false;
};

USTRUCT(BlueprintType)
struct FS_RainDayRecord{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    int64 dayNumber = 0;

    UPROPERTY(SaveGame)
    float rainHours = 0.0f;
};

USTRUCT(BlueprintType)
struct FS_WeatherSaveRecord{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    int32 version = 1;

    UPROPERTY(SaveGame)
    E_WeatherType type = E_WeatherType::Clear;

    UPROPERTY(SaveGame)
    float strength = 0.0f;

    UPROPERTY(SaveGame)
    int64 periodEndGameTime = 0;

    UPROPERTY(SaveGame)
    float snowLevel = 0.0f;

    UPROPERTY(SaveGame)
    TArray<FS_WaterBodySaveRecord> waterBodies;

    UPROPERTY(SaveGame)
    TArray<FS_RainDayRecord> rainDays;
};

// ---------------- Message payloads (Doc02 11) ----------------

USTRUCT(BlueprintType)
struct FS_WeatherChangedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    E_WeatherType oldType = E_WeatherType::Clear;

    UPROPERTY(BlueprintReadOnly)
    E_WeatherType newType = E_WeatherType::Clear;
};

USTRUCT(BlueprintType)
struct FS_WeatherWaterMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FGuid bodyId;
};

USTRUCT(BlueprintType)
struct FS_WeatherSnowMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float snowLevel = 0.0f;

    UPROPERTY(BlueprintReadOnly)
    int32 bucket = 0;
};

USTRUCT(BlueprintType)
struct FS_WeatherRainFilledMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TWeakObjectPtr<AActor> container;

    UPROPERTY(BlueprintReadOnly)
    int32 amount = 0;
};
