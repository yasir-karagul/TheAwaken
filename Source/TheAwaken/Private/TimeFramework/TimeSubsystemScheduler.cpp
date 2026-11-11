#include "TimeFramework/TimeSubsystem.h"
#include "TimeFramework/TimeMathLibrary.h"

FS_TimerHandle UTimeSubsystem::scheduleAt(int64 finishGameTime, const FD_TimeDue& callback){
    return addTimer(finishGameTime, E_TimerKind::Absolute, callback);
}

FS_TimerHandle UTimeSubsystem::scheduleAtDayStart(int64 finishDay, const FD_TimeDue& callback){
    return addTimer(UTimeMathLibrary::dayStartGameTime(finishDay, makeCalendarConfig()), E_TimerKind::DayStart, callback);
}

FS_TimerHandle UTimeSubsystem::addTimer(int64 finishGameTime, E_TimerKind kind, const FD_TimeDue& callback){
    FS_ScheduledTimer timer;
    timer.handle.id = FGuid::NewGuid();
    timer.finishGameTime = finishGameTime;
    timer.kind = kind;
    timer.callback = callback;
    timers.Add(timer);
    timers.StableSort([](const FS_ScheduledTimer& left, const FS_ScheduledTimer& right){
        return left.finishGameTime < right.finishGameTime;
    });
    return timer.handle;
}

void UTimeSubsystem::cancelTimer(const FS_TimerHandle& handle){
    timers.RemoveAll([&handle](const FS_ScheduledTimer& timer){
        return timer.handle.id == handle.id;
    });
}

void UTimeSubsystem::fireTimers(int64 upTo, E_TimerKind kind){
    // Collect first: a callback may schedule new timers, and those wait for the next boundary.
    TArray<FS_ScheduledTimer> due;
    for(int32 index = timers.Num() - 1; index >= 0; index--){
        if(timers[index].kind == kind && timers[index].finishGameTime <= upTo){
            due.Insert(timers[index], 0);
            timers.RemoveAt(index);
        }
    }
    for(const FS_ScheduledTimer& timer : due){
        timer.callback.ExecuteIfBound(timer.handle, timer.finishGameTime);
    }
}
