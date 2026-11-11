#include "TimeFramework/TimeSubsystem.h"
#include "TimeFramework/DA_TimeSettings.h"
#include "TimeFramework/TimeMathLibrary.h"
#include "TimeFramework/02Tags/T_TimeFramework.h"
#include "TimeFramework/03Interfaces/BPI_SleepEligibility.h"
#include "GameFrameworkClassTable/TimeCarrierComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogAwakenTime);

UTimeSubsystem* UTimeSubsystem::get(const UObject* worldContext){
    const UWorld* world = GEngine ? GEngine->GetWorldFromContextObject(worldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    return world ? world->GetSubsystem<UTimeSubsystem>() : nullptr;
}

bool UTimeSubsystem::ShouldCreateSubsystem(UObject* outer) const{
    const UWorld* world = Cast<UWorld>(outer);
    return world && world->IsGameWorld() && Super::ShouldCreateSubsystem(outer);
}

void UTimeSubsystem::Initialize(FSubsystemCollectionBase& collection){
    Super::Initialize(collection);
    // Seeded here, not in initializeNewWorld: a world that is loaded from a save never
    // calls initializeNewWorld, and an unseeded FRandomStream always starts at seed 0,
    // which would give every loaded world the same Snowy draws.
    randomStream.GenerateNewSeed();
    const FPrimaryAssetId assetId(FPrimaryAssetType("TimeSettings"), FName("DA_TimeSettings_Default"));
    settings = Cast<UDA_TimeSettings>(UAssetManager::Get().GetPrimaryAssetPath(assetId).TryLoad());
    if(!settings){
        UE_LOG(LogAwakenTime, Error, TEXT("[%s] DA_TimeSettings_Default is missing. The clock cannot run."), *roleName());
    }
}

void UTimeSubsystem::Deinitialize(){
    if(UWorld* world = GetWorld()){
        world->GetTimerManager().ClearTimer(clockTimer);
        world->GetTimerManager().ClearTimer(sleepMonitorTimer);
        world->GetTimerManager().ClearTimer(skipFadeTimer);
    }
    Super::Deinitialize();
}

void UTimeSubsystem::OnWorldBeginPlay(UWorld& world){
    Super::OnWorldBeginPlay(world);
    if(!settings){
        return;
    }
    // The timer runs on every machine. On the host it does nothing until clockReady (onClockStep).
    world.GetTimerManager().SetTimer(clockTimer, this, &UTimeSubsystem::onClockStep, settings->clockStepRealSeconds, true);
}

void UTimeSubsystem::registerCarrier(UTimeCarrierComponent* newCarrier){
    carrier = newCarrier;
    firstWatch = true;
    if(!carrier){
        return;
    }
    // Replay what arrived before the carrier was known (7: an OnRep can run before BeginPlay).
    if(calendarArrivedEarly){
        calendarArrivedEarly = false;
        handleCalendarReplicated(FS_CalendarState());
    }
    if(sleepArrivedEarly){
        sleepArrivedEarly = false;
        handleSleepStateReplicated();
    }
    if(skipArrivedEarly){
        skipArrivedEarly = false;
        handleSkipStateReplicated(FS_SkipState());
    }
}

double UTimeSubsystem::serverWorldSeconds() const{
    const UWorld* world = GetWorld();
    const AGameStateBase* gameState = world ? world->GetGameState() : nullptr;
    if(gameState){
        return gameState->GetServerWorldTimeSeconds();
    }
    return world ? world->GetTimeSeconds() : 0.0;
}

int64 UTimeSubsystem::extrapolatedGameTime() const{
    if(!carrier){
        return processedGameTime;
    }
    const FS_TimeSyncState& sync = carrier->syncState;
    const double elapsed = FMath::Max(0.0, serverWorldSeconds() - sync.serverWorldSecondsAtSync);
    return sync.gameTimeAtSync + static_cast<int64>(FMath::FloorToDouble(elapsed * sync.gameSecondsPerRealSecond));
}

FS_TimeCalendarConfig UTimeSubsystem::makeCalendarConfig() const{
    FS_TimeCalendarConfig config;
    if(!settings){
        return config;   // data asset missing: the defaults of the struct
    }
    config.wakeSeconds = settings->wakeHour * static_cast<int32>(UTimeMathLibrary::secondsPerHour);
    config.daysPerSeason = settings->daysPerSeason;
    if(carrier){
        config.startOffsetSeconds = carrier->syncState.startOffsetSeconds;
        config.seasonStartOffset = carrier->syncState.seasonStartOffset;
    }
    return config;
}

void UTimeSubsystem::publishSync(int64 newGameTime, bool isJump){
    if(!isAuthority() || !carrier){
        return;
    }
    const FS_TimeSyncState previous = carrier->syncState;
    FS_TimeSyncState sync = previous;
    sync.gameTimeAtSync = newGameTime;
    sync.serverWorldSecondsAtSync = serverWorldSeconds();
    sync.gameSecondsPerRealSecond = settings->gameSecondsPerRealSecond();
    if(isJump){
        sync.skipSerial++;
    }
    carrier->syncState = sync;
    carrier->GetOwner()->ForceNetUpdate();
    handleSyncReplicated(previous);
}

float UTimeSubsystem::getRealSecondsPerGameDay() const{
    return settings ? static_cast<float>(settings->realSecondsPerGameDay) : 3600.0f;
}

float UTimeSubsystem::getAwayIdleSeconds() const{
    return settings ? settings->awayIdleSeconds : 0.0f;
}

void UTimeSubsystem::onClockStep(){
    if(isAuthority() && !clockReady){
        return;   // no world yet: wait for initializeNewWorld or restoreFromSnapshot (R12-IG14)
    }
    if(isAuthority() && !skipRunning){
        processTo(extrapolatedGameTime());
    }
    localWatch();
#if !UE_BUILD_SHIPPING
    drawDebug();
#endif
}

void UTimeSubsystem::processTo(int64 target){
    if(target <= processedGameTime){
        return;
    }
    const FS_TimeCalendarConfig config = makeCalendarConfig();
    const int64 offset = config.startOffsetSeconds;
    const int64 hour = UTimeMathLibrary::secondsPerHour;
    const int64 wakeHourOfDay = config.wakeSeconds / hour;
    int64 hourIndex = UTimeMathLibrary::floorDiv(processedGameTime + offset, hour) + 1;
    const int64 lastHourIndex = UTimeMathLibrary::floorDiv(target + offset, hour);
    for(; hourIndex <= lastHourIndex; hourIndex++){
        const int64 boundary = hourIndex * hour - offset;
        fireTimers(boundary, E_TimerKind::Absolute);
        processedGameTime = boundary;
        if(UTimeMathLibrary::floorMod(hourIndex, 24) == wakeHourOfDay){
            runRollover();
        }
    }
    fireTimers(target, E_TimerKind::Absolute);
    processedGameTime = target;
}

E_DayNightPhase UTimeSubsystem::phaseForTime(int32 timeOfDaySeconds) const{
    if(!settings){
        return E_DayNightPhase::Day;
    }
    const float hours = static_cast<float>(timeOfDaySeconds) / 3600.0f;
    if(hours >= settings->nightStartHour || hours < settings->dawnStartHour){
        return E_DayNightPhase::Night;
    }
    if(hours >= settings->duskStartHour){
        return E_DayNightPhase::Dusk;
    }
    if(hours >= settings->dayStartHour){
        return E_DayNightPhase::Day;
    }
    return E_DayNightPhase::Dawn;
}

// Runs on every machine, from replicated state (Doc02 19.2: Local on every machine).
void UTimeSubsystem::localWatch(){
    if(!carrier || !settings){
        return;
    }
    if(isAuthority() && (!clockReady || skipRunning)){
        // The host announces nothing before the world exists, and nothing in the middle of a skip:
        // the single call at the end of executeSkip raises PhaseChanged after the calendar has moved.
        return;
    }
    const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(extrapolatedGameTime(), makeCalendarConfig());
    const int32 hour = date.timeOfDaySeconds / static_cast<int32>(UTimeMathLibrary::secondsPerHour);
    const E_DayNightPhase phase = phaseForTime(date.timeOfDaySeconds);
    const bool jumped = carrier->syncState.skipSerial != watchedSkipSerial;
    if(jumped || firstWatch){
        // Re-baseline silently: HourChanged is never raised for skipped hours (R12 3.6 step 6).
        watchedSkipSerial = carrier->syncState.skipSerial;
    }else if(hour != watchedHour && (hour % FMath::Max(1, settings->hourBroadcastEveryHours)) == 0){
        FS_TimeHourChangedMessage message;
        message.newHour = hour;
        raise(T_Time_HourChanged, message);
    }
    if(firstWatch || phase != watchedPhase){
        FS_TimePhaseChangedMessage message;
        message.newPhase = phase;
        raise(T_Time_PhaseChanged, message);
        UE_LOG(LogAwakenTime, Log, TEXT("[%s] Phase is now %d"), *roleName(), static_cast<int32>(phase));
    }
    watchedHour = hour;
    watchedPhase = phase;
    firstWatch = false;
}

void UTimeSubsystem::handleSyncReplicated(const FS_TimeSyncState& previous){
    // A jump (skipSerial changed) is picked up by localWatch. On the host that call stays silent while
    // skipRunning is true; the call at the end of executeSkip raises PhaseChanged once, after the calendar.
    localWatch();
}

// Plain queries
int64 UTimeSubsystem::getGameTime() const{
    return extrapolatedGameTime();
}

float UTimeSubsystem::getCurrentTimeOfDay() const{
    const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(extrapolatedGameTime(), makeCalendarConfig());
    return static_cast<float>(date.timeOfDaySeconds) / 3600.0f;
}

E_DayNightPhase UTimeSubsystem::getCurrentPhase() const{
    const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(extrapolatedGameTime(), makeCalendarConfig());
    return phaseForTime(date.timeOfDaySeconds);
}

void UTimeSubsystem::runRollover(){
    const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(processedGameTime, makeCalendarConfig());

    // 1 and 2: the day count and the season. A new Winter makes its Snowy draw now.
    if(date.season == E_Season::Winter && date.dayInSeason == 0){
        snowyDraw = UTimeMathLibrary::drawSnowyBlock(randomStream, settings->daysPerSeason, settings->snowyMinDays, date.cycleNumber, 0);
        UE_LOG(LogAwakenTime, Log, TEXT("[%s] Winter began. Snowy draw valid=%d start=%d length=%d"), *roleName(), snowyDraw.isValid, snowyDraw.startDay, snowyDraw.lengthDays);
    }
    FS_CalendarState next;
    next.daysPassed = date.daysPassed;
    next.season = date.season;
    next.dayInSeason = date.dayInSeason;
    next.snowyBlockActive = UTimeMathLibrary::isSnowyDay(snowyDraw, date);
    applyCalendar(next);

    // 3: the day-start hooks (timers set for a day, such as re-garrison, R12 3.6).
    fireTimers(processedGameTime, E_TimerKind::DayStart);
}

void UTimeSubsystem::applyCalendar(const FS_CalendarState& next){
    const FS_CalendarState previous = carrier->calendar;
    carrier->calendar = next;
    carrier->GetOwner()->ForceNetUpdate();
    if(skipRunning){
        // Step 6 of the protocol: raised once, after the jump.
        if(!hasPendingCalendar){
            pendingCalendarPrevious = previous;
            hasPendingCalendar = true;
        }
        return;
    }
    handleCalendarReplicated(previous);   // OnRep does not run on the host
}

void UTimeSubsystem::handleCalendarReplicated(const FS_CalendarState& previous){
    if(!carrier){
        calendarArrivedEarly = true;   // an OnRep ran before BeginPlay; registerCarrier replays it
        return;
    }
    const FS_CalendarState& current = carrier->calendar;
    if(firstWatch || previous.season != current.season || previous.snowyBlockActive != current.snowyBlockActive){
        FS_TimeSeasonChangedMessage message;
        message.newSeason = current.season;
        message.snowyBlockActive = current.snowyBlockActive;
        raise(T_Time_SeasonChanged, message);
        UE_LOG(LogAwakenTime, Log, TEXT("[%s] Season is %d, snowy=%d"), *roleName(), static_cast<int32>(current.season), current.snowyBlockActive);
    }
    if(firstWatch || previous.daysPassed != current.daysPassed){
        FS_TimeDayPassedMessage message;
        message.newDayCount = current.daysPassed;
        raise(T_Time_DayPassed, message);
        UE_LOG(LogAwakenTime, Log, TEXT("[%s] Day passed: %lld"), *roleName(), current.daysPassed);
    }
}

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> cvarTimeSeed(TEXT("Awaken.Time.Seed"), 0, TEXT("Fixed seed for new worlds and Snowy draws. 0 draws a random seed."));
static TAutoConsoleVariable<int32> cvarTimeStartSeasonOffset(TEXT("Awaken.Time.StartSeasonOffset"), -1, TEXT("Fixed seasonStartOffset for new worlds. -1 draws it."));
#endif

void UTimeSubsystem::initializeNewWorld(){
    if(!isAuthority() || !carrier || !settings){
        return;
    }
#if !UE_BUILD_SHIPPING
    if(cvarTimeSeed.GetValueOnGameThread() != 0){
        randomStream.Initialize(cvarTimeSeed.GetValueOnGameThread());
    }
#endif
    const int32 cycleLength = settings->daysPerSeason * static_cast<int32>(UTimeMathLibrary::seasonSlots);
    int64 seasonOffset = randomStream.RandRange(0, cycleLength - 1);
#if !UE_BUILD_SHIPPING
    if(cvarTimeStartSeasonOffset.GetValueOnGameThread() >= 0){
        seasonOffset = cvarTimeStartSeasonOffset.GetValueOnGameThread() % cycleLength;
    }
#endif
    const FS_CalendarState previousCalendar = carrier->calendar;
    FS_TimeSyncState sync;
    sync.startOffsetSeconds = FMath::RoundToInt(settings->worldStartTimeOfDay * 3600.0f);   // 07:00
    sync.seasonStartOffset = seasonOffset;
    sync.gameSecondsPerRealSecond = settings->gameSecondsPerRealSecond();
    sync.gameTimeAtSync = 0;
    sync.serverWorldSecondsAtSync = serverWorldSeconds();
    carrier->syncState = sync;
    processedGameTime = 0;

    const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(0, makeCalendarConfig());
    snowyDraw = FS_SnowyDraw();
    if(date.season == E_Season::Winter){
        // Later days only, and only if 5 days still fit (R12 3.4, A95 UD28 b).
        snowyDraw = UTimeMathLibrary::drawSnowyBlock(randomStream, settings->daysPerSeason, settings->snowyMinDays, date.cycleNumber, date.dayInSeason + 1);
    }
    FS_CalendarState calendar;
    calendar.daysPassed = date.daysPassed;   // 0
    calendar.season = date.season;
    calendar.dayInSeason = date.dayInSeason;
    calendar.snowyBlockActive = UTimeMathLibrary::isSnowyDay(snowyDraw, date);   // false: the current day cannot start a block
    carrier->calendar = calendar;
    carrier->skipState = FS_SkipState();
    clockReady = true;   // the host clock may run from here
    firstWatch = true;
    carrier->GetOwner()->ForceNetUpdate();
    UE_LOG(LogAwakenTime, Log, TEXT("[%s] New world: seasonStartOffset=%lld season=%d day=%d snowyValid=%d"), *roleName(), sync.seasonStartOffset, static_cast<int32>(date.season), date.dayInSeason, snowyDraw.isValid);

    // OnRep does not run on the host. Raise what listeners need once: SeasonChanged and DayPassed
    // from the calendar (firstWatch is still true), then PhaseChanged from localWatch.
    handleCalendarReplicated(previousCalendar);
    localWatch();
}

FS_TimeSaveRecord UTimeSubsystem::getSnapshotForSave() const{
    FS_TimeSaveRecord record;
    record.gameTime = processedGameTime;   // matches the calendar and the Snowy draw
    if(carrier){
        record.startOffsetSeconds = carrier->syncState.startOffsetSeconds;
        record.seasonStartOffset = carrier->syncState.seasonStartOffset;
    }
    record.snowyDraw = snowyDraw;
    return record;
}

void UTimeSubsystem::restoreFromSnapshot(const FS_TimeSaveRecord& record){
    if(!isAuthority() || !carrier || !settings){
        return;
    }
    if(record.version > 1){
        UE_LOG(LogAwakenTime, Error, TEXT("[%s] Time save version %d is newer than this build"), *roleName(), record.version);
        return;
    }
    GetWorld()->GetTimerManager().ClearTimer(skipFadeTimer);   // a restore ends any skip in progress
    skipRegisteredPlayers.Reset();
    clearRegistrations();   // registrations are never saved
    timers.Reset();          // owners re-register from their own saved finish times, after this step
    snowyDraw = record.snowyDraw;

    const FS_CalendarState previousCalendar = carrier->calendar;
    FS_TimeSyncState sync = carrier->syncState;
    sync.startOffsetSeconds = record.startOffsetSeconds;
    sync.seasonStartOffset = record.seasonStartOffset;
    sync.gameTimeAtSync = record.gameTime;
    sync.serverWorldSecondsAtSync = serverWorldSeconds();
    sync.gameSecondsPerRealSecond = settings->gameSecondsPerRealSecond();
    sync.skipSerial++;
    carrier->syncState = sync;
    processedGameTime = record.gameTime;

    const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(record.gameTime, makeCalendarConfig());
    FS_CalendarState calendar;
    calendar.daysPassed = date.daysPassed;
    calendar.season = date.season;
    calendar.dayInSeason = date.dayInSeason;
    calendar.snowyBlockActive = UTimeMathLibrary::isSnowyDay(snowyDraw, date);
    carrier->calendar = calendar;
    const FS_SkipState previousSkip = carrier->skipState;
    carrier->skipState = FS_SkipState();
    clockReady = true;
    firstWatch = true;
    carrier->GetOwner()->ForceNetUpdate();
    UE_LOG(LogAwakenTime, Log, TEXT("[%s] Time restored: gameTime=%lld day=%lld"), *roleName(), record.gameTime, date.daysPassed);

    handleCalendarReplicated(previousCalendar);   // SeasonChanged and DayPassed once (firstWatch is true)
    handleSkipStateReplicated(previousSkip);       // SkipEnded if a fade was running
    localWatch();                                  // PhaseChanged once
}

bool UTimeSubsystem::isAuthority() const{
    const UWorld* world = GetWorld();
    return world && world->GetNetMode() != NM_Client;
}

FString UTimeSubsystem::roleName() const{
    const UWorld* world = GetWorld();
    if(!world){
        return TEXT("Offline");
    }
    if(world->GetNetMode() == NM_Client){
        return TEXT("Client");
    }
    if(world->GetNetMode() == NM_Standalone){
        return TEXT("Offline");
    }
    return TEXT("Host");
}

int64 UTimeSubsystem::getDaysPassed() const{
    return carrier ? carrier->calendar.daysPassed : 0;
}

E_Season UTimeSubsystem::getCurrentSeason() const{
    return carrier ? carrier->calendar.season : E_Season::Spring;
}

int32 UTimeSubsystem::getDayInSeason() const{
    return carrier ? carrier->calendar.dayInSeason : 0;
}

bool UTimeSubsystem::isSnowyBlockActive() const{
    return carrier ? carrier->calendar.snowyBlockActive : false;
}

#if !UE_BUILD_SHIPPING
bool UTimeSubsystem::devSkipTo(int64 toGameTime){
    if(!isAuthority() || !clockReady){
        return false;
    }
    if(isGroupSleepSkipRunning()){
        UE_LOG(LogAwakenTime, Warning, TEXT("[%s] Dev time move refused: %s"), *roleName(), *T_Time_FailReason_SkipRunning.GetTag().ToString());
        return false;
    }
    const int64 now = extrapolatedGameTime();
    if(toGameTime <= now){
        UE_LOG(LogAwakenTime, Warning, TEXT("[%s] Dev time move refused: %lld is not after %lld"), *roleName(), toGameTime, now);
        return false;
    }
    processTo(now);
    executeSkip(toGameTime, E_SkipKind::DevCommand);
    return true;
}
#endif

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> cvarDebugTime(TEXT("Awaken.Debug.Time"), 0, TEXT("Draw the Time framework state."), ECVF_Default);

void UTimeSubsystem::drawDebug() const{
    if(cvarDebugTime.GetValueOnGameThread() == 0 || !GEngine || !settings || !carrier){
        return;
    }
    const float lifetime = settings->clockStepRealSeconds + 0.25f;
    // On-screen messages are shared by every Play In Editor window, so host and client use different keys and both lines stay visible.
    const int32 keyBase = isAuthority() ? 1000 : 2000;
    const FS_CalendarDate date = UTimeMathLibrary::deriveCalendar(extrapolatedGameTime(), makeCalendarConfig());
    GEngine->AddOnScreenDebugMessage(keyBase + 1, lifetime, FColor::Cyan, FString::Printf(TEXT("[%s] gameTime=%lld day=%lld season=%d.%d snowy=%d skip=%d"), *roleName(), extrapolatedGameTime(), date.daysPassed, static_cast<int32>(date.season), date.dayInSeason, isSnowyBlockActive(), static_cast<int32>(carrier->skipState.phase)));
    const FS_SleepWaitState& sleep = carrier->sleepState;
    FString needed;
    for(const TObjectPtr<APlayerState>& player : sleep.stillNeededPlayers){
        needed += (player ? player->GetPlayerName() : FString(TEXT("?"))) + TEXT(" ");
    }
    GEngine->AddOnScreenDebugMessage(keyBase + 2, lifetime, FColor::Green, FString::Printf(TEXT("sleep %d of %d, still needed: %s"), sleep.waitingPlayers.Num(), sleep.requiredPlayerCount, *needed));
    if(!isAuthority()){
        GEngine->AddOnScreenDebugMessage(keyBase + 3, lifetime, FColor::Yellow, TEXT("Timers and the Snowy draw exist on the host only"));
        return;
    }
    GEngine->AddOnScreenDebugMessage(keyBase + 4, lifetime, FColor::White, FString::Printf(TEXT("snowy draw valid=%d start=%d length=%d winter=%lld"), snowyDraw.isValid, snowyDraw.startDay, snowyDraw.lengthDays, snowyDraw.winterCycle));
    for(int32 index = 0; index < FMath::Min(5, timers.Num()); index++){
        GEngine->AddOnScreenDebugMessage(keyBase + 10 + index, lifetime, FColor::White, FString::Printf(TEXT("timer %d finish=%lld kind=%d"), index, timers[index].finishGameTime, static_cast<int32>(timers[index].kind)));
    }
}
#endif