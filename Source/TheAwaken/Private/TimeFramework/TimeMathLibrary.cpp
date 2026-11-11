#include "TimeFramework/TimeMathLibrary.h"

int64 UTimeMathLibrary::floorDiv(int64 value, int64 divisor){
    int64 quotient = value / divisor;
    if((value % divisor != 0) && ((value < 0) != (divisor < 0))){
        quotient--;
    }
    return quotient;
}

int64 UTimeMathLibrary::floorMod(int64 value, int64 divisor){
    return value - floorDiv(value, divisor) * divisor;
}

FS_CalendarDate UTimeMathLibrary::deriveCalendar(int64 gameTime, const FS_TimeCalendarConfig& config){
    FS_CalendarDate date;
    const int64 absolute = gameTime + config.startOffsetSeconds;
    const int64 wake = config.wakeSeconds;
    date.timeOfDaySeconds = static_cast<int32>(floorMod(absolute, secondsPerDay));
    // The first 05:00 after the world starts is day 1 whatever the start time (R12 3.3a).
    date.daysPassed = floorDiv(absolute - wake, secondsPerDay) - floorDiv(config.startOffsetSeconds - wake, secondsPerDay);
    const int64 cycleLength = static_cast<int64>(config.daysPerSeason) * seasonSlots;
    const int64 cycleDay = date.daysPassed + config.seasonStartOffset;
    date.cycleNumber = floorDiv(cycleDay, cycleLength);
    const int64 inCycle = floorMod(cycleDay, cycleLength);
    date.season = static_cast<E_Season>(inCycle / config.daysPerSeason);
    date.dayInSeason = static_cast<int32>(inCycle % config.daysPerSeason);
    return date;
}

int64 UTimeMathLibrary::dayStartGameTime(int64 day, const FS_TimeCalendarConfig& config){
    const int64 wake = config.wakeSeconds;
    const int64 firstDayOffset = floorDiv(config.startOffsetSeconds - wake, secondsPerDay);
    return wake + (day + firstDayOffset) * secondsPerDay - config.startOffsetSeconds;
}

int64 UTimeMathLibrary::nextWakeGameTime(int64 fromGameTime, const FS_TimeCalendarConfig& config){
    const int64 absolute = fromGameTime + config.startOffsetSeconds;
    const int64 wake = config.wakeSeconds;
    const int64 nextWake = (floorDiv(absolute - wake, secondsPerDay) + 1) * secondsPerDay + wake;
    return nextWake - config.startOffsetSeconds;
}

FS_SnowyDraw UTimeMathLibrary::drawSnowyBlock(FRandomStream& stream, int32 winterLength, int32 snowyMinDays, int64 cycleNumber, int32 firstAllowedDay){
    FS_SnowyDraw draw;
    const int32 lastStart = winterLength - snowyMinDays;
    if(firstAllowedDay > lastStart){
        return draw;   // fewer than 5 days left: no block this Winter (R12 3.4, Index A95 UD28 b)
    }
    draw.startDay = stream.RandRange(firstAllowedDay, lastStart);
    // The longest block is whatever remains of Winter, so it always ends inside Winter.
    draw.lengthDays = stream.RandRange(snowyMinDays, winterLength - draw.startDay);
    draw.winterCycle = cycleNumber;
    draw.isValid = true;
    return draw;
}

bool UTimeMathLibrary::isSnowyDay(const FS_SnowyDraw& draw, const FS_CalendarDate& date){
    return date.season == E_Season::Winter
        && draw.isValid
        && draw.winterCycle == date.cycleNumber
        && date.dayInSeason >= draw.startDay
        && date.dayInSeason < draw.startDay + draw.lengthDays;
}
