#include "WeatherFramework/WeatherSubsystem.h"
#include "WeatherFramework/WeatherLog.h"
#include "WeatherFramework/WeatherMathLibrary.h"
#include "WeatherFramework/DA_WeatherTuning.h"
#include "WeatherFramework/ShelterComponent.h"
#include "WeatherFramework/RainCollectorComponent.h"
#include "WeatherFramework/02Tags/T_WeatherFramework.h"
#include "WeatherFramework/03Interfaces/BPI_IndoorZone.h"
#include "GameFrameworkClassTable/WeatherCarrierComponent.h"
#include "TimeFramework/TimeSubsystem.h"
#include "TimeFramework/02Tags/T_TimeFramework.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"

UWeatherSubsystem* UWeatherSubsystem::get(const UObject* worldContext){
    const UWorld* world = GEngine ? GEngine->GetWorldFromContextObject(worldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    return world ? world->GetSubsystem<UWeatherSubsystem>() : nullptr;
}

bool UWeatherSubsystem::ShouldCreateSubsystem(UObject* outer) const{
    const UWorld* world = Cast<UWorld>(outer);
    return world && world->IsGameWorld() && Super::ShouldCreateSubsystem(outer);
}

void UWeatherSubsystem::Initialize(FSubsystemCollectionBase& collection){
    Super::Initialize(collection);
    collection.InitializeDependency<UTimeSubsystem>();
    // Seeded here, not in initializeNewWorld: a world loaded from a save never calls it (same reason as Time).
    simulation.random.GenerateNewSeed();
    loadData();
}

void UWeatherSubsystem::OnWorldBeginPlay(UWorld& world){
    Super::OnWorldBeginPlay(world);
#if !UE_BUILD_SHIPPING
    // The debug view needs a refresh once a second while it is switched on. Non-shipping only; the body returns at once with the switch off.
    // It starts on every machine, so a client draws its own [Client] line too.
    world.GetTimerManager().SetTimer(debugTimerHandle, FTimerDelegate::CreateLambda([this](){
        drawDebug();
    }), 1.0f, true);
#endif
    if(!tuning || !isAuthority()){
        return;
    }
    // No standing timer starts here. The step timer runs only while something moves (Step 11);
    // the shelter timer runs only while a player is registered (Step 14).
    registerTimeListeners();
}

void UWeatherSubsystem::Deinitialize(){
    if(UWorld* world = GetWorld()){
        world->GetTimerManager().ClearTimer(stepTimerHandle);
        world->GetTimerManager().ClearTimer(shelterTimerHandle);
#if !UE_BUILD_SHIPPING
        world->GetTimerManager().ClearTimer(debugTimerHandle);
#endif
    }
    seasonListener.Unregister();
    dayListener.Unregister();
    skipListener.Unregister();
    if(UTimeSubsystem* time = getTime()){
        if(periodTimerHandle.id.IsValid()){
            time->cancelTimer(periodTimerHandle);
        }
    }
    Super::Deinitialize();
}

bool UWeatherSubsystem::isAuthority() const{
    const UWorld* world = GetWorld();
    return world && world->GetNetMode() != NM_Client;
}

FString UWeatherSubsystem::roleName() const{
    const UWorld* world = GetWorld();
    if(!world){
        return TEXT("Unknown");
    }
    switch(world->GetNetMode()){
        case NM_Client:
            return TEXT("Client");
        case NM_Standalone:
            return TEXT("Offline");
        default:
            return TEXT("Host");
    }
}

UTimeSubsystem* UWeatherSubsystem::getTime() const{
    return UTimeSubsystem::get(this);
}

void UWeatherSubsystem::registerCarrier(UWeatherCarrierComponent* newCarrier){
    carrier = newCarrier;
    if(!carrier){
        return;
    }
    // Replay what arrived before the carrier was known (an OnRep can run before BeginPlay).
    if(weatherArrivedEarly){
        weatherArrivedEarly = false;
        handleWeatherStateReplicated(FS_WeatherState());
    }
    if(bodiesArrivedEarly){
        bodiesArrivedEarly = false;
        handleWaterBodiesReplicated(TArray<FS_WaterBodyState>());
    }
}

const FS_WeatherState& UWeatherSubsystem::readState() const{
    return carrier ? carrier->weatherState : emptyState;
}

const TArray<FS_WaterBodyState>& UWeatherSubsystem::readBodies() const{
    return carrier ? carrier->waterBodies : emptyBodies;
}

const FS_WaterBodyState* UWeatherSubsystem::findBody(const FGuid& bodyId) const{
    for(const FS_WaterBodyState& body : readBodies()){
        if(body.bodyId == bodyId){
            return &body;
        }
    }
    return nullptr;
}

const UDA_WeatherTuning* UWeatherSubsystem::getTuning() const{
    return tuning;
}

void UWeatherSubsystem::setRandomSeedForTesting(int32 seed){
    simulation.random.Initialize(seed);
}

E_WeatherType UWeatherSubsystem::getCurrentWeather() const{
    return readState().type;
}

float UWeatherSubsystem::getRainStrength() const{
    const FS_WeatherState& current = readState();
    return UWeatherMathLibrary::isRainType(current.type) ? current.strength : 0.0f;
}

float UWeatherSubsystem::getSnowStrength() const{
    const FS_WeatherState& current = readState();
    return UWeatherMathLibrary::isSnowType(current.type) ? current.strength : 0.0f;
}

float UWeatherSubsystem::getSnowLevel() const{
    return readState().snowLevel;
}

float UWeatherSubsystem::getIceLevel(const FGuid& bodyId) const{
    const FS_WaterBodyState* body = findBody(bodyId);
    return body ? body->iceLevel : 0.0f;
}

float UWeatherSubsystem::getRiverStrength(const FGuid& bodyId) const{
    const FS_WaterBodyState* body = findBody(bodyId);
    return body ? body->riverStrength : 0.0f;
}

// Season based: all river and lake water is freezing in Winter and the Snowy block (B14 3.7).
bool UWeatherSubsystem::isWaterFreezing(const FGuid& bodyId) const{
    const FS_WaterBodyState* body = findBody(bodyId);
    if(!body || !body->canFreeze){
        return false;
    }
    const E_WeatherSeasonKey key = getCurrentSeasonKey();
    return key == E_WeatherSeasonKey::Winter || key == E_WeatherSeasonKey::Snowy;
}

// Ice-solid is the one test for walking on ice, immersion illness and the Anchor refusal (B14 section 8, Index A106 S3-F16).
bool UWeatherSubsystem::isWaterFrozen(const FGuid& bodyId) const{
    const FS_WaterBodyState* body = findBody(bodyId);
    return body && body->iceSolid;
}

float UWeatherSubsystem::getStrongCurrentThreshold() const{
    return tuning ? tuning->strongCurrentThreshold : 85.0f;
}

E_WeatherSeasonKey UWeatherSubsystem::getCurrentSeasonKey() const{
    const UTimeSubsystem* time = getTime();
    if(!time){
        return E_WeatherSeasonKey::Spring;
    }
    return UWeatherMathLibrary::makeSeasonKey(time->getCurrentSeason(), time->isSnowyBlockActive());
}

float UWeatherSubsystem::getRainHoursOnDay(int64 dayNumber) const{
    const float* hours = simulation.rainHoursByDay.Find(dayNumber);
    return hours ? *hours : 0.0f;
}

float UWeatherSubsystem::getRainHoursInRange(int64 firstDay, int64 lastDay) const{
    float total = 0.0f;
    for(int64 day = firstDay; day <= lastDay; day++){
        total += getRainHoursOnDay(day);
    }
    return total;
}

void UWeatherSubsystem::loadData(){
    const FPrimaryAssetId assetId(FPrimaryAssetType("WeatherTuning"), FName("DA_WeatherTuning_Default"));
    tuning = Cast<UDA_WeatherTuning>(UAssetManager::Get().GetPrimaryAssetPath(assetId).TryLoad());
    simulation.tuning = tuning;
    if(!tuning){
        UE_LOG(LogAwakenWeather, Error, TEXT("[%s] DA_WeatherTuning_Default is missing. Weather cannot run."), *roleName());
        return;
    }
    const UDataTable* table = tuning->seasonWeatherTable.LoadSynchronous();
    if(!table){
        UE_LOG(LogAwakenWeather, Error, TEXT("[%s] DA_WeatherTuning_Default has no season table. Weather cannot run."), *roleName());
        return;
    }
    TArray<FS_SeasonWeatherRow> allRows;
    table->ForeachRow<FS_SeasonWeatherRow>(TEXT("UWeatherSubsystem::loadData"), [&allRows](const FName& rowName, const FS_SeasonWeatherRow& row){
        allRows.Add(row);
    });
    simulation.setRows(allRows);
}

void UWeatherSubsystem::reschedulePeriodTimer(){
    UTimeSubsystem* time = getTime();
    if(!time){
        return;
    }
    if(periodTimerHandle.id.IsValid()){
        time->cancelTimer(periodTimerHandle);
    }
    scheduledPeriodEnd = simulation.state.periodEndGameTime;
    periodTimerHandle = time->scheduleAt(simulation.state.periodEndGameTime, FD_TimeDue::CreateUObject(this, &UWeatherSubsystem::onPeriodTimer));
}

void UWeatherSubsystem::onPeriodTimer(const FS_TimerHandle& handle, int64 finishGameTime){
    if(finishGameTime != simulation.state.periodEndGameTime){
        return;
    }
    advanceTo(finishGameTime);
    flushRainFill();
    publishState();
}

void UWeatherSubsystem::advanceTo(int64 targetGameTime){
    const UTimeSubsystem* time = getTime();
    if(!isAuthority() || !isInitialized || tuning == nullptr || time == nullptr){
        return;
    }
    simulation.currentDayNumber = time->getDaysPassed();
    simulation.advanceTo(targetGameTime);
    if(simulation.state.periodEndGameTime != scheduledPeriodEnd){
        reschedulePeriodTimer();
    }
}

void UWeatherSubsystem::flushRainFill(){
    if(simulation.pendingFillUnits <= 0.0f){
        return;
    }
    const float units = simulation.pendingFillUnits;
    simulation.pendingFillUnits = 0.0f;
    for(int32 index = rainCollectors.Num() - 1; index >= 0; index--){
        URainCollectorComponent* collector = rainCollectors[index].Get();
        if(collector == nullptr){
            rainCollectors.RemoveAtSwap(index);
            continue;
        }
        collector->collect(units);
    }
}

void UWeatherSubsystem::pruneRainHistory(int64 currentDay){
    if(tuning == nullptr){
        return;
    }
    const int64 oldestKept = currentDay - tuning->rainHistoryDays;
    for(auto iterator = simulation.rainHoursByDay.CreateIterator(); iterator; ++iterator){
        if(iterator.Key() < oldestKept){
            iterator.RemoveCurrent();
        }
    }
}

void UWeatherSubsystem::updateStepTimer(){
    UWorld* world = GetWorld();
    if(world == nullptr || tuning == nullptr || !isAuthority()){
        return;
    }
    FTimerManager& timers = world->GetTimerManager();
    const bool isMovingNow = isInitialized && simulation.isMoving();
    const bool isTimerActive = timers.IsTimerActive(stepTimerHandle);
    if(isMovingNow && !isTimerActive){
        timers.SetTimer(stepTimerHandle, this, &UWeatherSubsystem::onStepTimer, tuning->weatherStepSeconds, true);
        UE_LOG(LogAwakenWeather, Verbose, TEXT("[%s] Step timer started"), *roleName());
    }else if(!isMovingNow && isTimerActive){
        timers.ClearTimer(stepTimerHandle);
        UE_LOG(LogAwakenWeather, Verbose, TEXT("[%s] Step timer stopped, nothing moves"), *roleName());
    }
}

void UWeatherSubsystem::onStepTimer(){
    const UTimeSubsystem* time = getTime();
    if(time == nullptr || !isInitialized){
        return;
    }
    advanceTo(time->getGameTime());
    flushRainFill();
    publishState();
}

void UWeatherSubsystem::registerTimeListeners(){
    UGameplayMessageSubsystem& messages = UGameplayMessageSubsystem::Get(this);
    seasonListener = messages.RegisterListener(T_Time_SeasonChanged.GetTag(), this, &UWeatherSubsystem::onSeasonChangedMessage);
    dayListener = messages.RegisterListener(T_Time_DayPassed.GetTag(), this, &UWeatherSubsystem::onDayPassedMessage);
    skipListener = messages.RegisterListener(T_Time_SkipAnnounced.GetTag(), this, &UWeatherSubsystem::onSkipAnnouncedMessage);
}

void UWeatherSubsystem::onSeasonChangedMessage(FGameplayTag channel, const FS_TimeSeasonChangedMessage& message){
    UTimeSubsystem* time = getTime();
    if(!isAuthority() || !isInitialized || time == nullptr){
        return;
    }
    const int64 now = time->getGameTime();
    advanceTo(now);
    flushRainFill();

    simulation.activeSeasonKey = UWeatherMathLibrary::makeSeasonKey(message.newSeason, message.snowyBlockActive);
    if(simulation.needsCalendarRederive){
        rederiveFromCalendar();
    }
    const bool endsRunningPeriod = !simulation.isTypeAllowed(simulation.activeSeasonKey, simulation.state.type);
    if(endsRunningPeriod){
        simulation.startNextPeriod(now);
        reschedulePeriodTimer();
    }
    UE_LOG(LogAwakenWeather, Log, TEXT("[%s] Season key now %d, running period %s"), *roleName(), static_cast<int32>(simulation.activeSeasonKey), endsRunningPeriod ? TEXT("ended, next drawn") : TEXT("continues"));
    publishState();
}

void UWeatherSubsystem::onDayPassedMessage(FGameplayTag channel, const FS_TimeDayPassedMessage& message){
    if(!isAuthority() || !isInitialized){
        return;
    }
    if(simulation.needsCalendarRederive){
        rederiveFromCalendar();
        publishState();
    }
    simulation.rainHoursByDay.FindOrAdd(message.newDayCount);
    pruneRainHistory(message.newDayCount);
}

void UWeatherSubsystem::onSkipAnnouncedMessage(FGameplayTag channel, const FS_TimeSkipAnnouncedMessage& message){
    if(!isAuthority() || !isInitialized){
        return;
    }
    const int32 periodsBefore = simulation.state.periodIndex;
    advanceTo(message.toGameTime);
    flushRainFill();
    publishState();
    UE_LOG(LogAwakenWeather, Log, TEXT("[%s] Skip to game time %lld: %d new weather periods"), *roleName(), message.toGameTime, simulation.state.periodIndex - periodsBefore);
}

float UWeatherSubsystem::deriveLevelFromCalendar(float winterDays, float snowyDays, float thawDays) const{
    const UTimeSubsystem* time = getTime();
    if(time == nullptr){
        return 0.0f;
    }
    return UWeatherMathLibrary::deriveCalendarLevel(simulation.activeSeasonKey, time->getDayInSeason(), time->getSnowyBlockStartDay(), time->getSnowyBlockLengthDays(), winterDays, snowyDays, thawDays);
}

void UWeatherSubsystem::resetBodyFromCalendar(FS_WaterBodyState& body) const{
    body.riverStrength = tuning->riverNormalLevel;
    if(body.canFreeze){
        // Same day counts as the ice times (3, 1 and 2 days), so creation and running agree. [W-IG15]
        body.iceLevel = deriveLevelFromCalendar(tuning->iceHoursToFreezeWinter / 24.0f, tuning->iceHoursToFreezeSnowy / 24.0f, tuning->iceHoursToThaw / 24.0f);
    }else{
        body.iceLevel = 0.0f;
    }
    body.iceSolid = body.iceLevel >= 1.0f;
}

// The season key is refreshed from Time first. If Time.DayPassed reaches Weather before Time.SeasonChanged,
// the key would otherwise still be the old one while the flag is already cleared.
void UWeatherSubsystem::rederiveFromCalendar(){
    simulation.activeSeasonKey = getCurrentSeasonKey();
    // A skip longer than a day is not integrated [W-IG16], so the running period may have ended long ago and its timer
    // already fired with nothing to do. Draw a new period at the arrival time, from the new season's rows.
    if(UTimeSubsystem* time = getTime()){
        const int64 now = time->getGameTime();
        if(simulation.state.periodEndGameTime <= now){
            simulation.startNextPeriod(now);
            reschedulePeriodTimer();
        }
    }
    for(FS_WaterBodyState& body : simulation.bodies){
        resetBodyFromCalendar(body);
    }
    simulation.state.snowLevel = deriveLevelFromCalendar(tuning->iceHoursToFreezeWinter / 24.0f, tuning->iceHoursToFreezeSnowy / 24.0f, tuning->iceHoursToThaw / 24.0f);
    simulation.needsCalendarRederive = false;
    UE_LOG(LogAwakenWeather, Log, TEXT("[%s] State re-derived from the calendar after a long time jump"), *roleName());
}

void UWeatherSubsystem::initializeNewWorld(){
    UTimeSubsystem* time = getTime();
    if(!isAuthority() || tuning == nullptr || time == nullptr){
        return;
    }
    if(!time->isClockReady()){
        UE_LOG(LogAwakenWeather, Warning, TEXT("[%s] initializeNewWorld called before Time is ready. Call Time's initializeNewWorld first (Doc B06)."), *roleName());
        return;
    }
    const int64 now = time->getGameTime();
    simulation.activeSeasonKey = getCurrentSeasonKey();

    FS_WeatherState fresh;
    fresh.type = E_WeatherType::Clear;
    fresh.strength = 0.0f;
    const float hours = simulation.random.FRandRange(tuning->defaultPeriodMinHours, FMath::Max(tuning->defaultPeriodMinHours, tuning->defaultPeriodMaxHours));
    fresh.periodEndGameTime = now + static_cast<int64>(hours * static_cast<float>(gameSecondsPerHour));
    // Always above the published value, so clients raise Weather.Changed even if the old value was 1. [W-IG5]
    fresh.periodIndex = publishedState.periodIndex + 1;
    simulation.state = fresh;

    for(FS_WaterBodyState& body : simulation.bodies){
        resetBodyFromCalendar(body);
    }
    pendingDeriveIds.Reset();
    simulation.state.snowLevel = deriveLevelFromCalendar(tuning->iceHoursToFreezeWinter / 24.0f, tuning->iceHoursToFreezeSnowy / 24.0f, tuning->iceHoursToThaw / 24.0f);

    simulation.rainHoursByDay.Reset();
    simulation.pendingFillUnits = 0.0f;
    simulation.lastAdvanceGameTime = now;
    simulation.needsCalendarRederive = false;
    isInitialized = true;

    reschedulePeriodTimer();
    publishState();
    UE_LOG(LogAwakenWeather, Log, TEXT("[%s] New world: Clear, snow level %.2f, period ends at game time %lld"), *roleName(), simulation.state.snowLevel, simulation.state.periodEndGameTime);
}

void UWeatherSubsystem::registerWaterBody(const FS_WaterBodyRegistration& registration){
    if(!isAuthority() || tuning == nullptr){
        return;
    }
    if(!registration.bodyId.IsValid()){
        UE_LOG(LogAwakenWeather, Error, TEXT("[%s] A water body tried to register with an invalid id and was refused (Doc02 19.3)"), *roleName());
        return;
    }
    FS_WaterBodyState* existing = simulation.bodies.FindByPredicate([&registration](const FS_WaterBodyState& body){
        return body.bodyId == registration.bodyId;
    });
    if(existing != nullptr){
        existing->hasCurrent = registration.hasCurrent;
        existing->canFreeze = registration.canFreeze;
        if(!existing->canFreeze){
            existing->iceLevel = 0.0f;
            existing->iceSolid = false;
        }
    }else{
        FS_WaterBodyState created;
        created.bodyId = registration.bodyId;
        created.hasCurrent = registration.hasCurrent;
        created.canFreeze = registration.canFreeze;
        created.riverStrength = tuning->riverNormalLevel;
        if(isInitialized){
            resetBodyFromCalendar(created);
        }else{
            pendingDeriveIds.Add(registration.bodyId);
        }
        simulation.bodies.Add(created);
    }
    if(isInitialized){
        publishState();
    }
    UE_LOG(LogAwakenWeather, Log, TEXT("[%s] Water body %s registered (current %d, can freeze %d)"), *roleName(), *registration.bodyId.ToString(), registration.hasCurrent, registration.canFreeze);
}

void UWeatherSubsystem::unregisterWaterBody(const FGuid& bodyId){
    // The record stays: state is data, the actor is only the loaded view (Doc02 18.1).
    UE_LOG(LogAwakenWeather, Verbose, TEXT("[%s] Water body %s unloaded"), *roleName(), *bodyId.ToString());
}

FGameplayTag UWeatherSubsystem::forceWeather(E_WeatherType type, float strength){
    UTimeSubsystem* time = getTime();
    if(!isAuthority()){
        return T_Weather_FailReason_NotAuthority.GetTag();
    }
    if(!isInitialized || tuning == nullptr || time == nullptr){
        return T_Weather_FailReason_NotReady.GetTag();
    }
    if(strength < 0.0f || strength > 100.0f){
        return T_Weather_FailReason_InvalidArgument.GetTag();
    }
    const int64 now = time->getGameTime();
    advanceTo(now);
    flushRainFill();

    simulation.startForcedPeriod(type, strength, now);
    reschedulePeriodTimer();
    publishState();
    UE_LOG(LogAwakenWeather, Log, TEXT("[%s] Forced weather %d at strength %.1f"), *roleName(), static_cast<int32>(type), strength);
    return FGameplayTag();
}

void UWeatherSubsystem::publishState(){
    if(!isAuthority() || !carrier){
        return;
    }
    const FS_WeatherState previousState = publishedState;
    const TArray<FS_WaterBodyState> previousBodies = publishedBodies;
    publishedState = simulation.state;
    publishedBodies = simulation.bodies;

    if(!(carrier->weatherState == simulation.state)){
        carrier->weatherState = simulation.state;
    }
    if(!(carrier->waterBodies == simulation.bodies)){
        carrier->waterBodies = simulation.bodies;
    }
    carrier->GetOwner()->ForceNetUpdate();

    // OnRep does not run on the host, so the host calls the same handlers the clients run (R12 7).
    raiseWeatherMessages(previousState, simulation.state);
    raiseWaterMessages(previousBodies, simulation.bodies);
    updateStepTimer();
}

void UWeatherSubsystem::handleWeatherStateReplicated(const FS_WeatherState& previous){
    if(!carrier){
        weatherArrivedEarly = true;   // an OnRep ran before BeginPlay; registerCarrier replays it
        return;
    }
    raiseWeatherMessages(previous, carrier->weatherState);
}

void UWeatherSubsystem::handleWaterBodiesReplicated(const TArray<FS_WaterBodyState>& previous){
    if(!carrier){
        bodiesArrivedEarly = true;
        return;
    }
    raiseWaterMessages(previous, carrier->waterBodies);
}

void UWeatherSubsystem::raiseWeatherMessages(const FS_WeatherState& previousState, const FS_WeatherState& newState){
    UGameplayMessageSubsystem& messages = UGameplayMessageSubsystem::Get(this);

    // A new period, including a new period of the same type and the first state a late joiner receives.
    if(newState.periodIndex != previousState.periodIndex){
        FS_WeatherChangedMessage payload;
        payload.oldType = previousState.type;
        payload.newType = newState.type;
        messages.BroadcastMessage(T_Weather_Changed.GetTag(), payload);
        UE_LOG(LogAwakenWeather, Log, TEXT("[%s] Weather period %d: type %d to %d, strength %.0f, ends at game time %lld"), *roleName(), newState.periodIndex, static_cast<int32>(previousState.type), static_cast<int32>(newState.type), newState.strength, newState.periodEndGameTime);
    }

    // Coarse buckets, not every small change (Doc02 5).
    const int32 bucketCount = (tuning != nullptr) ? FMath::Max(1, tuning->snowBucketCount) : 10;
    const int32 oldBucket = FMath::Min(bucketCount, FMath::FloorToInt(previousState.snowLevel * static_cast<float>(bucketCount)));
    const int32 newBucket = FMath::Min(bucketCount, FMath::FloorToInt(newState.snowLevel * static_cast<float>(bucketCount)));
    if(oldBucket != newBucket){
        FS_WeatherSnowMessage payload;
        payload.snowLevel = newState.snowLevel;
        payload.bucket = newBucket;
        messages.BroadcastMessage(T_Weather_SnowLevelChanged.GetTag(), payload);
        UE_LOG(LogAwakenWeather, Log, TEXT("[%s] Snow level crossed to bucket %d (level %.2f)"), *roleName(), newBucket, newState.snowLevel);
    }
}

void UWeatherSubsystem::raiseWaterMessages(const TArray<FS_WaterBodyState>& previousBodies, const TArray<FS_WaterBodyState>& newBodies){
    UGameplayMessageSubsystem& messages = UGameplayMessageSubsystem::Get(this);
    for(const FS_WaterBodyState& body : newBodies){
        const FS_WaterBodyState* before = previousBodies.FindByPredicate([&body](const FS_WaterBodyState& candidate){
            return candidate.bodyId == body.bodyId;
        });
        // A body that was not in the earlier snapshot is initial state, not a change (late joiner).
        if(before == nullptr || before->iceSolid == body.iceSolid){
            continue;
        }
        FS_WeatherWaterMessage payload;
        payload.bodyId = body.bodyId;
        messages.BroadcastMessage(body.iceSolid ? T_Weather_WaterFroze.GetTag() : T_Weather_WaterThawed.GetTag(), payload);
        UE_LOG(LogAwakenWeather, Log, TEXT("[%s] Water body %s %s (ice level %.2f)"), *roleName(), *body.bodyId.ToString(), body.iceSolid ? TEXT("froze") : TEXT("thawed"), body.iceLevel);
    }
}

void UWeatherSubsystem::registerShelterComponent(UShelterComponent* component){
    if(isAuthority() && component != nullptr){
        shelterComponents.AddUnique(component);
        updateShelterTimer();
    }
}

void UWeatherSubsystem::unregisterShelterComponent(UShelterComponent* component){
    shelterComponents.Remove(component);
    updateShelterTimer();
}

void UWeatherSubsystem::updateShelterTimer(){
    UWorld* world = GetWorld();
    if(world == nullptr || tuning == nullptr || !isAuthority()){
        return;
    }
    FTimerManager& timers = world->GetTimerManager();
    const bool isNeeded = shelterComponents.Num() > 0;
    const bool isTimerActive = timers.IsTimerActive(shelterTimerHandle);
    if(isNeeded && !isTimerActive){
        timers.SetTimer(shelterTimerHandle, this, &UWeatherSubsystem::onShelterTimer, tuning->shelterIntervalSeconds, true);
    }else if(!isNeeded && isTimerActive){
        timers.ClearTimer(shelterTimerHandle);
    }
}

void UWeatherSubsystem::registerIndoorZone(UObject* zone){
    if(zone != nullptr && zone->Implements<UBPI_IndoorZone>()){
        indoorZones.AddUnique(zone);
    }
}

void UWeatherSubsystem::unregisterIndoorZone(UObject* zone){
    indoorZones.Remove(zone);
}

bool UWeatherSubsystem::isInsideIndoorZone(const FVector& location) const{
    for(const TWeakObjectPtr<UObject>& zone : indoorZones){
        UObject* object = zone.Get();
        if(object != nullptr && IBPI_IndoorZone::Execute_containsPoint(object, location)){
            return true;
        }
    }
    return false;
}

void UWeatherSubsystem::onShelterTimer(){
    for(int32 index = shelterComponents.Num() - 1; index >= 0; index--){
        UShelterComponent* component = shelterComponents[index].Get();
        if(component == nullptr){
            shelterComponents.RemoveAtSwap(index);
            continue;
        }
        component->evaluate();
    }
}

float UWeatherSubsystem::getColdExposureRate(const FS_ShelterState& shelter) const{
    if(tuning == nullptr){
        return 0.0f;
    }
    return UWeatherMathLibrary::getColdRate(tuning->getColdBaseRate(getCurrentSeasonKey()), getRainStrength() / 100.0f, readState().snowLevel, shelter, *tuning);
}

void UWeatherSubsystem::registerRainCollector(URainCollectorComponent* component){
    if(isAuthority() && component != nullptr){
        rainCollectors.AddUnique(component);
    }
}

void UWeatherSubsystem::unregisterRainCollector(URainCollectorComponent* component){
    rainCollectors.Remove(component);
}

#if !UE_BUILD_SHIPPING
bool UWeatherSubsystem::devForceWeather(E_WeatherType type, float strength){
    const FGameplayTag reason = forceWeather(type, strength);
    if(reason.IsValid()){
        UE_LOG(LogAwakenWeather, Warning, TEXT("[%s] Awaken.Weather.Set refused: %s"), *roleName(), *reason.ToString());
        return false;
    }
    return true;
}

void UWeatherSubsystem::devDump() const{
    const UEnum* weatherEnum = StaticEnum<E_WeatherType>();
    const FS_WeatherState& current = readState();
    const UTimeSubsystem* time = getTime();
    const float hoursLeft = time ? static_cast<float>(current.periodEndGameTime - time->getGameTime()) / static_cast<float>(gameSecondsPerHour) : 0.0f;
    UE_LOG(LogAwakenWeather, Log, TEXT("[%s] Weather %s strength %.0f, %.1f h left, snow level %.2f, season key %d, period %d"), *roleName(), *weatherEnum->GetNameStringByValue(static_cast<int64>(current.type)), current.strength, hoursLeft, current.snowLevel, static_cast<int32>(getCurrentSeasonKey()), current.periodIndex);
    for(const FS_WaterBodyState& body : readBodies()){
        UE_LOG(LogAwakenWeather, Log, TEXT("[%s]   body %s ice %.2f%s river %.0f current %d freeze %d"), *roleName(), *body.bodyId.ToString(), body.iceLevel, body.iceSolid ? TEXT(" solid") : TEXT(""), body.riverStrength, body.hasCurrent, body.canFreeze);
    }
}
#endif

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> cvarDebugWeather(TEXT("Awaken.Debug.Weather"), 0, TEXT("Draw the Weather framework state."), ECVF_Default);

void UWeatherSubsystem::drawDebug() const{
    if(cvarDebugWeather.GetValueOnGameThread() == 0 || !GEngine || !carrier){
        return;
    }
    const float lifetime = 1.25f;
    const int32 keyBase = isAuthority() ? 3000 : 4000;
    const UEnum* weatherEnum = StaticEnum<E_WeatherType>();
    const UTimeSubsystem* time = getTime();
    const FS_WeatherState& current = readState();
    const float hoursLeft = time ? static_cast<float>(current.periodEndGameTime - time->getGameTime()) / static_cast<float>(gameSecondsPerHour) : 0.0f;
    GEngine->AddOnScreenDebugMessage(keyBase + 1, lifetime, FColor::Cyan, FString::Printf(TEXT("[%s] weather=%s strength=%.0f left=%.1fh snow=%.2f period=%d"), *roleName(), *weatherEnum->GetNameStringByValue(static_cast<int64>(current.type)), current.strength, hoursLeft, current.snowLevel, current.periodIndex));
    int32 line = 0;
    for(const FS_WaterBodyState& body : readBodies()){
        GEngine->AddOnScreenDebugMessage(keyBase + 10 + line, lifetime, body.iceSolid ? FColor::Cyan : FColor::White, FString::Printf(TEXT("  %s ice=%.2f%s river=%.0f current=%d freeze=%d"), *body.bodyId.ToString().Left(8), body.iceLevel, body.iceSolid ? TEXT(" solid") : TEXT(""), body.riverStrength, body.hasCurrent, body.canFreeze));
        line++;
        if(line >= 20){
            break;
        }
    }
}
#endif
