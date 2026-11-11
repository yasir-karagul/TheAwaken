#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TimeFramework/01Structs/S_TimeFramework.h"
#include "TimeMathLibrary.generated.h"

UCLASS()
class THEAWAKEN_API UTimeMathLibrary : public UBlueprintFunctionLibrary{
    GENERATED_BODY()
public:
    static constexpr int64 secondsPerHour = 3600;
    static constexpr int64 secondsPerDay = 86400;
    static constexpr int64 seasonSlots = 4;

    static int64 floorDiv(int64 value, int64 divisor);
    static int64 floorMod(int64 value, int64 divisor);
    static FS_CalendarDate deriveCalendar(int64 gameTime, const FS_TimeCalendarConfig& config);
    static int64 dayStartGameTime(int64 day, const FS_TimeCalendarConfig& config);
    static int64 nextWakeGameTime(int64 fromGameTime, const FS_TimeCalendarConfig& config);

    // The Snowy draw rules, kept here so they can be tested with no world (bodies in Step 9).
    static FS_SnowyDraw drawSnowyBlock(FRandomStream& stream, int32 winterLength, int32 snowyMinDays, int64 cycleNumber, int32 firstAllowedDay);
    static bool isSnowyDay(const FS_SnowyDraw& draw, const FS_CalendarDate& date);
};
