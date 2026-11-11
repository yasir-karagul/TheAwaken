#include "CosmeticEventsEnvironmentPresentationFramework/EnvironmentManager.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentLog.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentMath.h"
#include "CosmeticEventsEnvironmentPresentationFramework/DA_EnvironmentCurves.h"
#include "CosmeticEventsEnvironmentPresentationFramework/DA_EnvironmentSettings.h"
#include "CosmeticEventsEnvironmentPresentationFramework/DA_WeatherPresentation.h"
#include "TimeFramework/02Tags/T_TimeFramework.h"
#include "WeatherFramework/02Tags/T_WeatherFramework.h"
#include "WeatherFramework/WeatherMathLibrary.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "EngineUtils.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/DataValidation.h"

AEnvironmentManager::AEnvironmentManager(){
    PrimaryActorTick.bCanEverTick = false;   // the loop is a timer, not Tick (Doc02 5)
    bReplicates = false;
    SetCanBeDamaged(false);
#if WITH_EDITORONLY_DATA
    // D1: World Partition must never stream the manager out. The data check fails the build if a designer ticks it back on.
    bIsSpatiallyLoaded = false;
#endif

    root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(root);

    sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
    sun->SetupAttachment(root);
    sun->SetMobility(EComponentMobility::Movable);
    sun->SetAtmosphereSunLight(true);   // the bUsedAsAtmosphereSunLight member is not public in UE 5.8
    sun->AtmosphereSunLightIndex = 0;

    moon = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Moon"));
    moon->SetupAttachment(root);
    moon->SetMobility(EComponentMobility::Movable);
    moon->SetAtmosphereSunLight(true);
    moon->AtmosphereSunLightIndex = 1;

    skyAtmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("SkyAtmosphere"));
    skyAtmosphere->SetupAttachment(root);

    skyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    skyLight->SetupAttachment(root);
    skyLight->SetMobility(EComponentMobility::Movable);
    skyLight->bRealTimeCapture = true;

    volumetricCloud = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("VolumetricCloud"));
    volumetricCloud->SetupAttachment(root);

    heightFog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("HeightFog"));
    heightFog->SetupAttachment(root);

    globalPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("GlobalPostProcess"));
    globalPostProcess->SetupAttachment(root);
    globalPostProcess->bUnbound = true;
    globalPostProcess->Priority = 0.0f;

    zoneLowerPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("ZoneLowerPostProcess"));
    zoneLowerPostProcess->SetupAttachment(root);
    zoneLowerPostProcess->bUnbound = true;
    zoneLowerPostProcess->Priority = 10.0f;
    zoneLowerPostProcess->BlendWeight = 0.0f;

    zoneUpperPostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("ZoneUpperPostProcess"));
    zoneUpperPostProcess->SetupAttachment(root);
    zoneUpperPostProcess->bUnbound = true;
    zoneUpperPostProcess->Priority = 11.0f;
    zoneUpperPostProcess->BlendWeight = 0.0f;
}

AEnvironmentManager* AEnvironmentManager::find(const UObject* worldContext){
    const UWorld* world = GEngine ? GEngine->GetWorldFromContextObject(worldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    if(world == nullptr){
        return nullptr;
    }
    TActorIterator<AEnvironmentManager> it(const_cast<UWorld*>(world));
    return it ? *it : nullptr;
}

void AEnvironmentManager::BeginPlay(){
    Super::BeginPlay();

    // A dedicated server draws nothing.
    if(GetNetMode() == NM_DedicatedServer){
        Destroy();
        return;
    }

    // One per world. A second one would fight the first over every component value.
    for(TActorIterator<AEnvironmentManager> it(GetWorld()); it; ++it){
        if(*it != this && IsValid(*it) && it->HasActorBegunPlay()){
            UE_LOG(LogAwakenEnvironment, Error, TEXT("A second AEnvironmentManager (%s) was found. This one is destroyed."), *GetNameSafe(*it));
            Destroy();
            return;
        }
    }

    if(!loadData()){
        return;
    }

    // Without this the first update sees the whole session as one delta.
    lastUpdateTime = GetWorld()->GetRealTimeSeconds();
    maxUpdateDeltaSeconds = settings->maxUpdateDeltaSeconds;

    registerListeners();
    applyQuality();
    snapToTarget();
    startUpdateTimer(settings->idleUpdateRate);

#if !UE_BUILD_SHIPPING
    GetWorldTimerManager().SetTimer(debugTimer, FTimerDelegate::CreateLambda([this](){
        drawDebug();
    }), 1.0f, true);
#endif
}

void AEnvironmentManager::EndPlay(const EEndPlayReason::Type reason){
    GetWorldTimerManager().ClearTimer(updateTimer);
#if !UE_BUILD_SHIPPING
    GetWorldTimerManager().ClearTimer(debugTimer);
#endif
    unregisterListeners();
    zoneStack.clear();
    Super::EndPlay(reason);
}

bool AEnvironmentManager::loadData(){
    settings = UDA_EnvironmentSettings::loadDefault();
    if(settings == nullptr){
        UE_LOG(LogAwakenEnvironment, Error, TEXT("DA_EnvironmentSettings_Default is missing. The environment manager cannot run."));
        return false;
    }
    curves = UDA_EnvironmentCurves::loadDefault();
    if(curves == nullptr){
        UE_LOG(LogAwakenEnvironment, Error, TEXT("DA_EnvironmentCurves_Default is missing. The sun, moon and sky will not follow the clock."));
    }
    parameterCollection = settings->parameterCollection.LoadSynchronous();
    if(parameterCollection == nullptr){
        UE_LOG(LogAwakenEnvironment, Warning, TEXT("MPC_Environment is not set. No wetness, snow, wind or rain value reaches the materials."));
    }

    presentations.Reset();
    TArray<FPrimaryAssetId> assetIds;
    UAssetManager::Get().GetPrimaryAssetIdList(FPrimaryAssetType(UDA_WeatherPresentation::primaryAssetType), assetIds);
    for(const FPrimaryAssetId& assetId : assetIds){
        UDA_WeatherPresentation* presentation = Cast<UDA_WeatherPresentation>(UAssetManager::Get().GetPrimaryAssetPath(assetId).TryLoad());
        if(presentation != nullptr){
            presentations.Add(presentation->weatherType, presentation);
        }
    }
    if(presentations.Num() == 0){
        UE_LOG(LogAwakenEnvironment, Error, TEXT("No DA_WeatherPresentation was found. Every weather type will use the default look."));
    }
    return true;
}

void AEnvironmentManager::registerListeners(){
    UGameplayMessageSubsystem& messages = UGameplayMessageSubsystem::Get(this);
    weatherListener = messages.RegisterListener(T_Weather_Changed.GetTag(), this, &AEnvironmentManager::onWeatherChanged);
    skipStartedListener = messages.RegisterListener(T_Time_SkipStarted.GetTag(), this, &AEnvironmentManager::onSkipStarted);
    skipEndedListener = messages.RegisterListener(T_Time_SkipEnded.GetTag(), this, &AEnvironmentManager::onSkipEnded);
    phaseListener = messages.RegisterListener(T_Time_PhaseChanged.GetTag(), this, &AEnvironmentManager::onPhaseChanged);
    seasonListener = messages.RegisterListener(T_Time_SeasonChanged.GetTag(), this, &AEnvironmentManager::onSeasonChanged);
}

void AEnvironmentManager::unregisterListeners(){
    weatherListener.Unregister();
    skipStartedListener.Unregister();
    skipEndedListener.Unregister();
    phaseListener.Unregister();
    seasonListener.Unregister();
}

void AEnvironmentManager::readInputs(){
    timeInput = EnvironmentSources::readTime(this);
    weatherInput = EnvironmentSources::readWeather(this);
}

bool AEnvironmentManager::isInSettleWindow() const{
    return GetWorld() && GetWorld()->GetRealTimeSeconds() < snapSettleUntil;
}

bool AEnvironmentManager::isSkipActive() const{
    return skipPending || timeInput.isGroupSleepSkipRunning;
}

// ---------------- Messages ----------------

void AEnvironmentManager::onWeatherChanged(FGameplayTag channel, const FS_WeatherChangedMessage& message){
    handleWeatherTarget(message.newType);
}

void AEnvironmentManager::onSkipStarted(FGameplayTag channel, const FS_TimeSkipStartedMessage& message){
    skipPending = true;
    skipPendingSince = GetWorld()->GetRealTimeSeconds();
}

// Hidden by the sleep fade (B17). Snap inside the handler, in the same frame, because the fade-in has only just started.
void AEnvironmentManager::onSkipEnded(FGameplayTag channel, const FS_TimeSkipEndedMessage& message){
    skipPending = false;
    applySnap(true);
}

void AEnvironmentManager::onPhaseChanged(FGameplayTag channel, const FS_TimePhaseChangedMessage& message){
    if(hasSnappedToReal){
        readInputs();
        applyTime();
        applyLights();
    }
}

void AEnvironmentManager::onSeasonChanged(FGameplayTag channel, const FS_TimeSeasonChangedMessage& message){
    if(hasSnappedToReal){
        readInputs();
        applyTime();
        applyLights();
    }
}

// ---------------- The weather blend (B.3) ----------------

// The one place a new target type is handled. A Weather.Changed and the self-correction of step 5 both come here.
void AEnvironmentManager::handleWeatherTarget(E_WeatherType newType){
    // While a group sleep skip runs, only record the target. The host resolves the skipped hours in one frame, and the snap at
    // Time.SkipEnded puts the look on the right weather.
    if(isSkipActive()){
        targetType = newType;
        return;
    }
    beginBlendTo(newType);
}

void AEnvironmentManager::beginBlendTo(E_WeatherType newType){
    targetType = newType;
    // Time and Weather replicate on different carriers, so the weather state can reach a client just after Time.SkipEnded (gap G10).
    // Inside the settle window a change snaps instead of blending. It does not open a new window.
    if(isInSettleWindow()){
        applySnap(false);
        return;
    }
    // Start from the current mixed look, not from the old target. That is what makes a change that arrives mid-blend continuous.
    blendFrom = currentLook;
    blendElapsed = 0.0f;
    isBlending = true;
    refreshUpdateRate();
}

FS_WeatherLook AEnvironmentManager::computeTargetLook() const{
    const TObjectPtr<UDA_WeatherPresentation>* found = presentations.Find(targetType);
    if(found == nullptr || found->Get() == nullptr){
        return FS_WeatherLook();
    }
    // Rain and snow strength are 0 to 100 and change inside a period, so the target is scaled by them on every update.
    float strength = 0.0f;
    if(UWeatherMathLibrary::isRainType(targetType)){
        strength = weatherInput.rainStrength;
    }else if(UWeatherMathLibrary::isSnowType(targetType)){
        strength = weatherInput.snowStrength;
    }
    return (*found)->getTargetLook(strength);
}

void AEnvironmentManager::advanceBlend(float deltaSeconds){
    const FS_WeatherLook targetLook = computeTargetLook();
    if(!isBlending){
        currentLook = targetLook;
        return;
    }
    blendElapsed += deltaSeconds;
    const float alpha = UCosmeticEnvironmentMathLibrary::computeBlendAlpha(blendElapsed, settings->blendSeconds);
    currentLook = UCosmeticEnvironmentMathLibrary::lerpLook(blendFrom, targetLook, alpha);
    if(alpha >= 1.0f){
        isBlending = false;
    }
}

// ---------------- Snap ----------------

void AEnvironmentManager::snapToTarget(){
    applySnap(true);
}

void AEnvironmentManager::applySnap(bool startSettle){
    if(settings == nullptr){
        return;
    }
    readInputs();
    const double now = GetWorld()->GetRealTimeSeconds();

    if(weatherInput.isReady){
        targetType = weatherInput.type;
    }
    isBlending = false;
    blendElapsed = settings->blendSeconds;
    currentLook = computeTargetLook();
    blendFrom = currentLook;

    // EF6: wetness is at the current rain target if it is raining, otherwise 0.
    const bool isRaining = UWeatherMathLibrary::isRainType(targetType) && weatherInput.rainStrength > 0.0f;
    wetness = isRaining ? currentLook.wetnessTarget : 0.0f;

    // A zone pushed in the same frame (a pawn that starts inside a cave) jumps to weight 1, because the cover hides it.
    zoneStack.snapAll();

    lightningPulseValue = 0.0f;
    nextLightningTime = 0.0;

    if(timeInput.hasSkipSerial){
        lastSkipSerial = timeInput.skipSerial;
    }
    hasSnappedToReal = timeInput.isReady && weatherInput.isReady;
    if(startSettle){
        snapSettleUntil = now + settings->postSnapSettleSeconds;
    }
    lastUpdateTime = now;

    applyAll();
    refreshUpdateRate();
    UE_LOG(LogAwakenEnvironment, Log, TEXT("Environment snapped (weather %d, wetness %.2f, ready %d)"), static_cast<int32>(targetType), wetness, hasSnappedToReal);
}

// A clock jump with no fade (a B22 time command) sets no skip state and raises no Time.SkipEnded (R12 3.7). The jump counter of the
// clock sync (gap G9) is the one thing that changes. Nothing hides the pop, which a dev build accepts.
void AEnvironmentManager::detectClockJump(){
    if(!timeInput.hasSkipSerial){
        return;
    }
    if(lastSkipSerial == INDEX_NONE){
        lastSkipSerial = timeInput.skipSerial;
        return;
    }
    if(timeInput.skipSerial == lastSkipSerial){
        return;
    }
    lastSkipSerial = timeInput.skipSerial;
    if(!isSkipActive()){
        applySnap(true);
    }
}

// Step 5 of the blend: the message was missed, or arrived before the listeners were registered.
void AEnvironmentManager::detectWeatherChange(){
    if(weatherInput.isReady && weatherInput.type != targetType){
        handleWeatherTarget(weatherInput.type);
    }
}

// ---------------- The update loop ----------------

void AEnvironmentManager::updateEnvironment(){
    const double now = GetWorld()->GetRealTimeSeconds();
    const float deltaSeconds = UCosmeticEnvironmentMathLibrary::clampUpdateDelta(now, lastUpdateTime, maxUpdateDeltaSeconds);
    lastUpdateTime = now;

    readInputs();

    // Safety net: a Time.SkipStarted that never gets its Time.SkipEnded must not freeze the blend forever.
    if(skipPending && now - skipPendingSince > settings->skipPendingTimeoutSeconds){
        UE_LOG(LogAwakenEnvironment, Warning, TEXT("Time.SkipStarted was never followed by Time.SkipEnded. Dropping the pending skip."));
        skipPending = false;
        applySnap(true);
        return;
    }

    // Time and Weather may only be ready after the manager began play (a join). The first read with both ready is a hard start.
    if(!hasSnappedToReal && timeInput.isReady && weatherInput.isReady){
        applySnap(true);
        return;
    }

    detectClockJump();
    detectWeatherChange();

    advanceBlend(deltaSeconds);
    advanceZones(deltaSeconds);
    advanceLightning(now);
    applyTime();
    applyLook();
    advanceWetness(deltaSeconds);
    applySnowCover();
    applyZones();
    applyLights();
    writeParameterCollection();
    refreshUpdateRate();
}

// The same steps the update loop runs, without advancing anything. Used after a snap.
void AEnvironmentManager::applyAll(){
    applyTime();
    applyLook();
    applySnowCover();
    applyZones();
    applyLights();
    writeParameterCollection();
}

void AEnvironmentManager::startUpdateTimer(float updatesPerSecond){
    GetWorldTimerManager().SetTimer(updateTimer, this, &AEnvironmentManager::updateEnvironment, 1.0f / FMath::Max(updatesPerSecond, 0.1f), true);
}

// Doc02 5: timers run only while something is active. A few updates a second suit the sun, but would visibly step a 30 second blend,
// a zone fade or a flash that lasts a few tenths of a second.
void AEnvironmentManager::refreshUpdateRate(){
    if(settings == nullptr || !GetWorldTimerManager().TimerExists(updateTimer)){
        return;
    }
    const bool needsActive = isBlending || zoneStack.isMoving() || lightningPulseValue > 0.0f;
    if(needsActive == isActiveRate){
        return;
    }
    isActiveRate = needsActive;
    startUpdateTimer(needsActive ? settings->activeUpdateRate : settings->idleUpdateRate);
}