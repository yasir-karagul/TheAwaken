#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "TimeFramework/00Enums/E_TimeFramework.h"
#include "TimeFramework/04Delegates/D_TimeFramework.h"
#include "S_TimeFramework.generated.h"

// The replicated clock: sync time, time at the sync and rate (Doc02 19.4).
USTRUCT(BlueprintType)
struct FS_TimeSyncState{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int64 gameTimeAtSync = 0;

    UPROPERTY(BlueprintReadOnly)
    double serverWorldSecondsAtSync = 0.0;

    UPROPERTY(BlueprintReadOnly)
    float gameSecondsPerRealSecond = 24.0f;

    UPROPERTY(BlueprintReadOnly)
    int32 startOffsetSeconds = 25200;

    UPROPERTY(BlueprintReadOnly)
    int64 seasonStartOffset = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 skipSerial = 0;   // changes on every jump, so clients re-baseline silently
};

USTRUCT(BlueprintType)
struct FS_CalendarState{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int64 daysPassed = 0;

    UPROPERTY(BlueprintReadOnly)
    E_Season season = E_Season::Spring;

    UPROPERTY(BlueprintReadOnly)
    int32 dayInSeason = 0;

    UPROPERTY(BlueprintReadOnly)
    bool snowyBlockActive = false;
};

// R12 section 5: WaitingPlayers and RequiredPlayerCount. stillNeededPlayers is R12-IG11.
USTRUCT(BlueprintType)
struct FS_SleepWaitState{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    TArray<TObjectPtr<APlayerState>> waitingPlayers;

    UPROPERTY(BlueprintReadOnly)
    TArray<TObjectPtr<APlayerState>> stillNeededPlayers;

    UPROPERTY(BlueprintReadOnly)
    int32 requiredPlayerCount = 0;
};

// R12 3.7: None, or FadingOut from the moment the group is complete until the jump has run.
USTRUCT(BlueprintType)
struct FS_SkipState{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    E_SkipPhase phase = E_SkipPhase::None;

    UPROPERTY(BlueprintReadOnly)
    float fadeOutSeconds = 0.0f;
};

// The Snowy draw. winterCycle says which Winter it belongs to (R13 3.5).
USTRUCT(BlueprintType)
struct FS_SnowyDraw{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, SaveGame)
    bool isValid = false;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    int64 winterCycle = 0;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    int32 startDay = 0;

    UPROPERTY(BlueprintReadOnly, SaveGame)
    int32 lengthDays = 0;
};

// Save record (Doc02 9). Derived values are not stored (R12 3.5). Every field is SaveGame, and so are
// the fields of the nested FS_SnowyDraw: the Save framework writes through SaveGameToMemory, which
// skips every property without the SaveGame flag, so an unflagged record would be saved empty.
USTRUCT(BlueprintType)
struct FS_TimeSaveRecord{
    GENERATED_BODY()

    UPROPERTY(SaveGame)
    int32 version = 1;

    UPROPERTY(SaveGame)
    int64 gameTime = 0;

    UPROPERTY(SaveGame)
    int32 startOffsetSeconds = 25200;

    UPROPERTY(SaveGame)
    int64 seasonStartOffset = 0;

    UPROPERTY(SaveGame)
    FS_SnowyDraw snowyDraw;
};

// Scheduler handle (Doc02 19.3, kind 2: created at run time, so a GUID). R12-IG2.
USTRUCT(BlueprintType)
struct FS_TimerHandle{
    GENERATED_BODY()

    UPROPERTY()
    FGuid id;
};

// Plain structs: never replicated, saved or shown to Blueprint. They carry values
// between pure functions, or live inside the subsystem on the host.
struct FS_TimeCalendarConfig{
    int32 startOffsetSeconds = 25200;
    int32 wakeSeconds = 18000;
    int32 daysPerSeason = 30;
    int64 seasonStartOffset = 0;
};

struct FS_CalendarDate{
    int64 daysPassed = 0;
    int64 cycleNumber = 0;
    E_Season season = E_Season::Spring;
    int32 dayInSeason = 0;
    int32 timeOfDaySeconds = 0;
};

struct FS_ScheduledTimer{
    FS_TimerHandle handle;
    int64 finishGameTime = 0;
    E_TimerKind kind = E_TimerKind::Absolute;
    FD_TimeDue callback;
};

struct FS_SleepRegistration{
    TWeakObjectPtr<APlayerState> player;
    FVector sleepObjectLocation = FVector::ZeroVector;
};

// Message payloads (R12 section 6).
USTRUCT(BlueprintType)
struct FS_TimeHourChangedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 newHour = 0;
};

USTRUCT(BlueprintType)
struct FS_TimePhaseChangedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    E_DayNightPhase newPhase = E_DayNightPhase::Day;
};

USTRUCT(BlueprintType)
struct FS_TimeDayPassedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int64 newDayCount = 0;
};

USTRUCT(BlueprintType)
struct FS_TimeSeasonChangedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    E_Season newSeason = E_Season::Spring;

    UPROPERTY(BlueprintReadOnly)
    bool snowyBlockActive = false;
};

USTRUCT(BlueprintType)
struct FS_TimeSleepWaitingMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int32 waitingCount = 0;

    UPROPERTY(BlueprintReadOnly)
    int32 requiredCount = 0;
};

USTRUCT(BlueprintType)
struct FS_TimeSkipAnnouncedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int64 fromGameTime = 0;

    UPROPERTY(BlueprintReadOnly)
    int64 toGameTime = 0;
};

USTRUCT(BlueprintType)
struct FS_TimeSkipStartedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    float fadeOutSeconds = 0.0f;
};

USTRUCT(BlueprintType)
struct FS_TimeSkipEndedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    int64 newGameTime = 0;
};
