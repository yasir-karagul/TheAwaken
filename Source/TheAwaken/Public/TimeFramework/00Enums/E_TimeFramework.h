#pragma once
#include "CoreMinimal.h"
#include "E_TimeFramework.generated.h"

UENUM(BlueprintType)
enum class E_DayNightPhase : uint8{
    Dawn,
    Day,
    Dusk,
    Night
};

UENUM(BlueprintType)
enum class E_Season : uint8{
    Spring,
    Summer,
    Fall,
    Winter
};

UENUM(BlueprintType)
enum class E_SleepCountState : uint8{
    Counted,
    Downed,
    Restrained,
    Arriving,
    Bagless,
    Away
};

UENUM(BlueprintType)
enum class E_SkipKind : uint8{
    None,
    GroupSleep,
    DevCommand
};

UENUM(BlueprintType)
enum class E_SkipPhase : uint8{
    None,
    FadingOut
};

UENUM(BlueprintType)
enum class E_TimerKind : uint8{
    Absolute,
    DayStart
};
