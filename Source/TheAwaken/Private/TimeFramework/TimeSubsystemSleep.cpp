#include "TimeFramework/TimeSubsystem.h"
#include "TimeFramework/DA_TimeSettings.h"
#include "TimeFramework/TimeMathLibrary.h"
#include "TimeFramework/02Tags/T_TimeFramework.h"
#include "TimeFramework/03Interfaces/BPI_SleepEligibility.h"
#include "GameFrameworkClassTable/TimeCarrierComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "TimerManager.h"

bool UTimeSubsystem::isSleepWindowOpenAt(int32 timeOfDaySeconds) const{
    if(!settings){
        return false;
    }
    const int32 open = settings->sleepWindowOpenHour * static_cast<int32>(UTimeMathLibrary::secondsPerHour);
    const int32 lastStart = settings->sleepLastStartHour * static_cast<int32>(UTimeMathLibrary::secondsPerHour);
    // Inclusive at both ends (R12-IG9, Doc02 19.9). The window wraps midnight.
    return timeOfDaySeconds >= open || timeOfDaySeconds <= lastStart;
}

bool UTimeSubsystem::isSleepWindowOpen() const{
    const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(extrapolatedGameTime(), makeCalendarConfig());
    return isSleepWindowOpenAt(date.timeOfDaySeconds);
}

E_SleepCountState UTimeSubsystem::countStateOf(APlayerState* player) const{
    if(player && player->Implements<UBPI_SleepEligibility>()){
        return IBPI_SleepEligibility::Execute_getSleepCountState(player);
    }
    return E_SleepCountState::Away;   // a player we cannot judge is never counted
}

bool UTimeSubsystem::isGroupSleepSkipRunning() const{
    return carrier && carrier->skipState.phase != E_SkipPhase::None;
}

FGameplayTag UTimeSubsystem::requestSleepTimeSkip(APlayerState* player, const FVector& sleepObjectLocation){
    if(!isAuthority() || !player){
        return T_Time_FailReason_NotAuthority;
    }
    if(isGroupSleepSkipRunning()){
        return T_Time_FailReason_SkipRunning;   // the group is complete: no new registrations (R12 3.7)
    }
    if(!isSleepWindowOpen()){
        UE_LOG(LogAwakenTime, Log, TEXT("[%s] Sleep refused: window closed"), *roleName());
        return T_Time_FailReason_WindowClosed;
    }
    if(countStateOf(player) != E_SleepCountState::Counted){
        return T_Time_FailReason_NotCounted;
    }
    if(!IBPI_SleepEligibility::Execute_canRegisterToSleep(player)){
        return T_Time_FailReason_InScene;
    }
    // All checks passed. Now change state.
    registrations.RemoveAll([player](const FS_SleepRegistration& entry){
        return entry.player.Get() == player;
    });
    FS_SleepRegistration entry;
    entry.player = player;
    entry.sleepObjectLocation = sleepObjectLocation;
    registrations.Add(entry);
    if(!GetWorld()->GetTimerManager().IsTimerActive(sleepMonitorTimer)){
        GetWorld()->GetTimerManager().SetTimer(sleepMonitorTimer, this, &UTimeSubsystem::onSleepMonitor, settings->sleepMonitorRealSeconds, true);
    }
    UE_LOG(LogAwakenTime, Log, TEXT("[%s] Sleep registered: %s"), *roleName(), *player->GetPlayerName());
    refreshSleepState();
    evaluateGroupSkip();   // the window is checked at the moment the last counted player registers
    return FGameplayTag();
}

void UTimeSubsystem::cancelSleepWait(APlayerState* player){
    if(!isAuthority()){
        return;   // host only: registrations exist on the host (the function is now callable from Blueprint)
    }
    const int32 removed = registrations.RemoveAll([player](const FS_SleepRegistration& entry){
        return entry.player.Get() == player || !entry.player.IsValid();
    });
    if(removed > 0){
        UE_LOG(LogAwakenTime, Log, TEXT("[%s] Sleep cancelled"), *roleName());
        refreshSleepState();
        evaluateGroupSkip();   // someone leaving may have completed the group
    }
}

void UTimeSubsystem::refreshSleepState(){
    if(!carrier){
        return;
    }
    FS_SleepWaitState state;
    const AGameStateBase* gameState = GetWorld()->GetGameState();
    if(registrations.Num() > 0 && gameState){
        for(APlayerState* player : gameState->PlayerArray){
            if(countStateOf(player) != E_SleepCountState::Counted){
                continue;
            }
            state.requiredPlayerCount++;
            const bool isRegistered = registrations.ContainsByPredicate([player](const FS_SleepRegistration& entry){
                return entry.player.Get() == player;
            });
            if(isRegistered){
                state.waitingPlayers.Add(player);
            }else{
                state.stillNeededPlayers.Add(player);   // R12-IG11: the names the UI shows
            }
        }
    }
    // An empty state means the counter disappears (R12 3.2b). Assign only on change,
    // so the monitor timer causes no network traffic.
    const FS_SleepWaitState& old = carrier->sleepState;
    const bool same = old.requiredPlayerCount == state.requiredPlayerCount
        && old.waitingPlayers == state.waitingPlayers
        && old.stillNeededPlayers == state.stillNeededPlayers;
    if(!same){
        carrier->sleepState = state;
        carrier->GetOwner()->ForceNetUpdate();
        handleSleepStateReplicated();   // OnRep does not run on the host
    }
}

void UTimeSubsystem::handleSleepStateReplicated(){
    if(!carrier){
        sleepArrivedEarly = true;   // an OnRep ran before BeginPlay; registerCarrier replays it
        return;
    }
    FS_TimeSleepWaitingMessage message;
    message.waitingCount = carrier->sleepState.waitingPlayers.Num();
    message.requiredCount = carrier->sleepState.requiredPlayerCount;
    raise(T_Time_SleepWaitingForGroup, message);
}

void UTimeSubsystem::evaluateGroupSkip(){
    if(isGroupSleepSkipRunning() || registrations.Num() == 0 || !isSleepWindowOpen()){
        return;
    }
    const AGameStateBase* gameState = GetWorld()->GetGameState();
    int32 counted = 0;
    for(APlayerState* player : gameState->PlayerArray){
        if(countStateOf(player) != E_SleepCountState::Counted){
            continue;
        }
        counted++;
        const bool isRegistered = registrations.ContainsByPredicate([player](const FS_SleepRegistration& entry){
            return entry.player.Get() == player;
        });
        if(!isRegistered){
            return;   // someone counted is still awake: keep waiting, no timeout, no majority
        }
    }
    if(counted > 0){
        beginGroupSkip();   // phase one of R12 3.7 (Step 12)
    }
}

void UTimeSubsystem::onSleepMonitor(){
    if(!isSleepWindowOpen()){
        // The window closed at 04:00: all registrations clear, no text, no skip that night.
        UE_LOG(LogAwakenTime, Log, TEXT("[%s] Sleep window closed: %d registrations cleared"), *roleName(), registrations.Num());
        clearRegistrations();
        return;
    }
    for(int32 index = registrations.Num() - 1; index >= 0; index--){
        APlayerState* player = registrations[index].player.Get();
        const APawn* pawn = player ? player->GetPawn() : nullptr;
        const bool tooFar = !pawn || FVector::Dist(pawn->GetActorLocation(), registrations[index].sleepObjectLocation) > settings->sleepRegisterRadiusCm;
        const bool stillCounted = player && countStateOf(player) == E_SleepCountState::Counted;
        const bool hostileNear = player && IBPI_SleepEligibility::Execute_hasAwareHostileNearby(player);
        if(tooFar || !stillCounted || hostileNear){
            registrations.RemoveAt(index);
            UE_LOG(LogAwakenTime, Log, TEXT("[%s] Sleep registration cancelled (tooFar=%d stillCounted=%d hostileNear=%d)"), *roleName(), tooFar, stillCounted, hostileNear);
        }
    }
    refreshSleepState();
    evaluateGroupSkip();
    if(registrations.Num() == 0){
        GetWorld()->GetTimerManager().ClearTimer(sleepMonitorTimer);
    }
}

void UTimeSubsystem::clearRegistrations(){
    registrations.Reset();
    GetWorld()->GetTimerManager().ClearTimer(sleepMonitorTimer);
    refreshSleepState();
}

void UTimeSubsystem::beginGroupSkip(){
    // Phase one (R12 3.7). The caller (evaluateGroupSkip) has checked the window and the group.
    skipRegisteredPlayers.Reset();
    for(const FS_SleepRegistration& entry : registrations){
        skipRegisteredPlayers.Add(entry.player);
    }
    UE_LOG(LogAwakenTime, Log, TEXT("[%s] Group complete: %d registered, fade-out %.1f s"), *roleName(), skipRegisteredPlayers.Num(), settings->skipFadeOutSeconds);
    clearRegistrations();   // empties the counter and stops the monitor

    const FS_SkipState previous = carrier->skipState;
    FS_SkipState next;
    next.phase = E_SkipPhase::FadingOut;
    next.fadeOutSeconds = settings->skipFadeOutSeconds;
    carrier->skipState = next;
    carrier->GetOwner()->ForceNetUpdate();
    handleSkipStateReplicated(previous);   // OnRep does not run on the host: raises Time.SkipStarted

    GetWorld()->GetTimerManager().SetTimer(skipFadeTimer, this, &UTimeSubsystem::onSkipFadeDone, settings->skipFadeOutSeconds, false);
}

void UTimeSubsystem::onSkipFadeDone(){
    // Phase two (R12 3.7). The clock kept running during the fade.
    processTo(extrapolatedGameTime());   // flush any boundary the last clock step has not processed
    const int64 toGameTime = UTimeMathLibrary::nextWakeGameTime(processedGameTime, makeCalendarConfig());
    executeSkip(toGameTime, E_SkipKind::GroupSleep);
    skipRegisteredPlayers.Reset();

    const FS_SkipState previous = carrier->skipState;
    carrier->skipState = FS_SkipState();   // None: isGroupSleepSkipRunning is false from here
    carrier->GetOwner()->ForceNetUpdate();
    handleSkipStateReplicated(previous);   // raises Time.SkipEnded, after PhaseChanged and DayPassed
}

void UTimeSubsystem::executeSkip(int64 toGameTime, E_SkipKind kind){
    skipKind = kind;
    skipRunning = true;
    const int64 fromGameTime = processedGameTime;
    UE_LOG(LogAwakenTime, Log, TEXT("[%s] Skip begins: %lld to %lld"), *roleName(), fromGameTime, toGameTime);

    // Step 2 and 3: the announcement. Gameplay Messages are delivered synchronously, so every
    // listener's AdvanceTo has finished when raise returns. For a group sleep,
    // isGroupSleepSkipRunning() is already true (the skip state is FadingOut).
    FS_TimeSkipAnnouncedMessage announced;
    announced.fromGameTime = fromGameTime;
    announced.toGameTime = toGameTime;
    raise(T_Time_SkipAnnounced, announced);
    fireTimers(toGameTime, E_TimerKind::Absolute);   // due at or before the target counts as due (A106, S3-F25)

    // Step 4: the jump. skipSerial changes. On the host localWatch returns at once while
    // skipRunning is true, so PhaseChanged is NOT raised here, before the calendar has moved.
    publishSync(toGameTime, true);

    // Step 5: the rollover, once for every 05:00 inside the span (one for a sleep).
    const FS_TimeCalendarConfig config = makeCalendarConfig();
    for(int64 wake = UTimeMathLibrary::nextWakeGameTime(fromGameTime, config); wake <= toGameTime; wake = UTimeMathLibrary::nextWakeGameTime(wake, config)){
        processedGameTime = wake;
        runRollover();
    }
    processedGameTime = toGameTime;

    // Step 6: messages, once, in the right order: the calendar first, then the phase.
    skipRunning = false;
    skipKind = E_SkipKind::None;
    if(hasPendingCalendar){
        hasPendingCalendar = false;
        handleCalendarReplicated(pendingCalendarPrevious);
    }
    localWatch();   // re-baselines the jump silently, raises PhaseChanged once; no HourChanged for skipped hours
    UE_LOG(LogAwakenTime, Log, TEXT("[%s] Skip ends at %lld"), *roleName(), toGameTime);
}

TArray<APlayerState*> UTimeSubsystem::getSkipRegisteredPlayers() const{
    TArray<APlayerState*> result;
    for(const TWeakObjectPtr<APlayerState>& player : skipRegisteredPlayers){
        if(player.IsValid()){
            result.Add(player.Get());
        }
    }
    return result;
}

void UTimeSubsystem::handleSkipStateReplicated(const FS_SkipState& previous){
    if(!carrier){
        skipArrivedEarly = true;   // an OnRep ran before BeginPlay; registerCarrier replays it
        return;
    }
    const FS_SkipState& current = carrier->skipState;
    if(previous.phase == E_SkipPhase::None && current.phase == E_SkipPhase::FadingOut){
        FS_TimeSkipStartedMessage message;
        message.fadeOutSeconds = current.fadeOutSeconds;
        raise(T_Time_SkipStarted, message);
        UE_LOG(LogAwakenTime, Log, TEXT("[%s] Skip started: fade-out %.1f s"), *roleName(), current.fadeOutSeconds);
    }else if(previous.phase == E_SkipPhase::FadingOut && current.phase == E_SkipPhase::None){
        FS_TimeSkipEndedMessage message;
        message.newGameTime = extrapolatedGameTime();
        raise(T_Time_SkipEnded, message);
        UE_LOG(LogAwakenTime, Log, TEXT("[%s] Skip ended at %lld"), *roleName(), message.newGameTime);
    }
}
