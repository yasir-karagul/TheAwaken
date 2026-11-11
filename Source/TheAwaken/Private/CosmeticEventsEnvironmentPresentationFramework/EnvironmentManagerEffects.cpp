// The part of AEnvironmentManager that writes to the engine: sun, moon, sky, fog, post-process, parameter collection, zones, lightning, quality.
// EnvironmentManager.cpp holds the lifecycle, the update loop, the blend and the snap.
#include "CosmeticEventsEnvironmentPresentationFramework/EnvironmentManager.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentLog.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentMath.h"
#include "CosmeticEventsEnvironmentPresentationFramework/DA_EnvironmentCurves.h"
#include "CosmeticEventsEnvironmentPresentationFramework/DA_EnvironmentSettings.h"
#include "CosmeticEventsEnvironmentPresentationFramework/02Tags/T_CosmeticEventsEnvironmentPresentationFramework.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Misc/DataValidation.h"
#include "Scalability.h"

// ---------------- Sun, moon, sky, exposure ----------------

void AEnvironmentManager::applyTime(){
    if(curves == nullptr || settings == nullptr){
        return;
    }
    // R12 gives hours (0 to 24). Normalise before sampling the curves.
    const float dayFraction = UCosmeticEnvironmentMathLibrary::normalizeTimeOfDay(timeInput.timeOfDayHours);

    const FVector toSun = UCosmeticEnvironmentMathLibrary::computeToSunDirection(dayFraction, settings->sunYawDegrees, settings->sunOrbitTiltDegrees);
    sun->SetWorldRotation((-toSun).Rotation());   // a directional light points from the light toward the ground
    moon->SetWorldRotation(toSun.Rotation());     // the moon is on the opposite side of the sky

    timeSunIntensity = curves->sampleSunIntensity(dayFraction);
    timeSunColor = curves->sampleSunColor(dayFraction);
    timeMoonIntensity = curves->sampleMoonIntensity(dayFraction);
    timeMoonColor = curves->sampleMoonColor(dayFraction);
    timeSkyTint = curves->sampleSkyTint(dayFraction);
    timeNightFactor = curves->sampleNightFactor(dayFraction);

    // Exposure and the season grade. The Snowy block uses the winter grade (handled by getSeasonGrade).
    const FLinearColor grade = curves->getSeasonGrade(timeInput.season, timeInput.isSnowyBlock);
    FPostProcessSettings& post = globalPostProcess->Settings;
    post.bOverride_AutoExposureBias = true;
    post.AutoExposureBias = curves->sampleExposureBias(dayFraction);
    post.bOverride_ColorGain = true;
    post.ColorGain = FVector4(grade.R, grade.G, grade.B, 1.0f);
}

// The blended look drives the sky tint. Clouds and wind reach the materials through the parameter collection.
void AEnvironmentManager::applyLook(){
    skyAtmosphere->SetSkyLuminanceFactor(timeSkyTint * currentLook.skyTint);
}

void AEnvironmentManager::advanceWetness(float deltaSeconds){
    wetness = UCosmeticEnvironmentMathLibrary::stepWetness(wetness, currentLook.wetnessTarget, weatherInput.rainStrength, deltaSeconds, settings->wetnessRiseSecondsAtFullRain, settings->wetnessDrySeconds);
}

// Snow cover is the replicated snow level, mapped straight to snowCover. It rises and melts with the level.
void AEnvironmentManager::applySnowCover(){
    snowCover = FMath::Clamp(weatherInput.snowLevel, 0.0f, 1.0f);
}

void AEnvironmentManager::applyLights(){
    if(settings == nullptr){
        return;
    }
    const float lightScale = currentLook.lightScale;
    const float visibility = zoneSkyVisibility;

    sun->SetIntensity(timeSunIntensity * lightScale * visibility);
    sun->SetLightColor(timeSunColor, false);
    moon->SetIntensity(timeMoonIntensity * lightScale * visibility);
    moon->SetLightColor(timeMoonColor, false);

    // The sky light never goes to zero at night: the night is dark but never black. The lightning pulse is added on top.
    const float nightScale = FMath::Lerp(1.0f, settings->nightSkyLightScale, timeNightFactor);
    skyLight->SetIntensity((settings->skyLightBaseIntensity * nightScale * lightScale + settings->lightningSkyLightBoost * lightningPulseValue) * visibility);

    // Inside a hide-sky zone the atmosphere and clouds are switched off once they have faded out.
    const bool isSkyVisible = visibility > 0.01f;
    if(isSkyVisible != wasSkyVisible){
        wasSkyVisible = isSkyVisible;
        skyAtmosphere->SetVisibility(isSkyVisible);
        volumetricCloud->SetVisibility(isSkyVisible);
    }
}

void AEnvironmentManager::writeParameterCollection(){
    if(parameterCollection == nullptr || settings == nullptr || GetWorld() == nullptr){
        return;
    }
    UMaterialParameterCollectionInstance* instance = GetWorld()->GetParameterCollectionInstance(parameterCollection);
    if(instance == nullptr){
        return;
    }
    instance->SetScalarParameterValue(settings->wetnessParameter, wetness);
    instance->SetScalarParameterValue(settings->snowCoverParameter, snowCover);
    instance->SetScalarParameterValue(settings->windStrengthParameter, currentLook.windStrength);
    const FVector windDirection = FRotator(0.0f, currentLook.windDirectionDegrees, 0.0f).Vector();
    instance->SetVectorParameterValue(settings->windDirectionParameter, FLinearColor(windDirection.X, windDirection.Y, 0.0f, 0.0f));
    // 0 to 100, as B14 gives them. R15 reads them for its particles and spawns the particles; this document spawns none.
    instance->SetScalarParameterValue(settings->rainStrengthParameter, weatherInput.rainStrength);
    instance->SetScalarParameterValue(settings->snowStrengthParameter, weatherInput.snowStrength);
    instance->SetScalarParameterValue(settings->cloudCoverageParameter, currentLook.cloudCoverage);
    instance->SetScalarParameterValue(settings->cloudDensityParameter, currentLook.cloudDensity);
    // B.1: the star opacity is the night factor times one minus the cloud coverage.
    instance->SetScalarParameterValue(settings->starOpacityParameter, timeNightFactor * (1.0f - currentLook.cloudCoverage));
    instance->SetScalarParameterValue(settings->skyVisibilityParameter, zoneSkyVisibility);
    instance->SetScalarParameterValue(settings->lightningPulseParameter, lightningPulseValue);
}

// ---------------- Quality (B17 3.13) ----------------

void AEnvironmentManager::applyQuality(){
    if(settings == nullptr || settings->qualityPresets.Num() == 0){
        return;
    }
    // B17 has not defined Settings.Changed yet, so the engine scalability level stands in for the graphics preset.
    const Scalability::FQualityLevels levels = Scalability::GetQualityLevels();
    int32 level = levels.GetSingleQualityLevel();
    if(level < 0){
        level = levels.EffectsQuality;
    }
    const int32 index = FMath::Clamp(level, 0, settings->qualityPresets.Num() - 1);
    for(const TPair<FName, float>& pair : settings->qualityPresets[index].consoleVariables){
        IConsoleVariable* variable = IConsoleManager::Get().FindConsoleVariable(*pair.Key.ToString());
        if(variable == nullptr){
            UE_LOG(LogAwakenEnvironment, Warning, TEXT("Quality preset %d names the unknown console variable %s"), index, *pair.Key.ToString());
            continue;
        }
        variable->Set(pair.Value, ECVF_SetByGameSetting);
    }
    UE_LOG(LogAwakenEnvironment, Log, TEXT("Quality preset %d applied"), index);
}

void AEnvironmentManager::notifySettingsChanged(){
    applyQuality();
}

// ---------------- Zones (EF1, B.7) ----------------

void AEnvironmentManager::pushZone(const FGuid& zoneId, const FS_EnvironmentZoneSettings& zoneSettings){
    if(settings == nullptr || !zoneId.IsValid()){
        return;
    }
    // A zone pushed inside the settle window (a pawn that starts in a cave) jumps to weight 1: the loading cover hides it.
    zoneStack.push(zoneId, zoneSettings, settings->defaultZoneBlendSeconds, isInSettleWindow());
    // The settings of an id already on the stack may have changed, so the post-process copy is redone.
    appliedUpperId.Invalidate();
    appliedLowerId.Invalidate();
    UE_LOG(LogAwakenEnvironment, Log, TEXT("Zone %s pushed (%d on the stack)"), *zoneId.ToString(), zoneStack.num());
    refreshUpdateRate();
}

void AEnvironmentManager::popZone(const FGuid& zoneId){
    zoneStack.pop(zoneId);
    UE_LOG(LogAwakenEnvironment, Log, TEXT("Zone %s popped (%d left, some may be fading)"), *zoneId.ToString(), zoneStack.num());
    refreshUpdateRate();
}

TArray<FGuid> AEnvironmentManager::getActiveZoneIds() const{
    return zoneStack.getActiveIds();
}

void AEnvironmentManager::advanceZones(float deltaSeconds){
    zoneStack.advance(deltaSeconds);
}

void AEnvironmentManager::applyZones(){
    if(settings == nullptr){
        return;
    }
    zoneSkyVisibility = zoneStack.layerSkyVisibility();

    // Fog: the look's fog, then each zone that overrides it, lerped in by its weight.
    FS_ZoneFog fog;
    fog.density = currentLook.fogDensity;
    fog.heightFalloff = settings->baseFogHeightFalloff;
    fog.inscatteringColor = settings->baseFogInscatteringColor;
    fog.startDistance = 0.0f;
    fog.maxOpacity = 1.0f;
    zoneStack.layerFog(fog);
    heightFog->SetFogDensity(fog.density);
    heightFog->SetFogHeightFalloff(fog.heightFalloff);
    heightFog->SetFogInscatteringColor(fog.inscatteringColor);
    heightFog->SetStartDistance(fog.startDistance);
    heightFog->SetFogMaxOpacity(fog.maxOpacity);

    // Post-process: the two topmost visible zones, so a fading top zone blends into the zone below it and not into the global look.
    const FS_ZoneEntry* lower = nullptr;
    const FS_ZoneEntry* upper = nullptr;
    zoneStack.getPostProcessLayers(lower, upper);
    auto applyLayer = [](UPostProcessComponent* component, const FS_ZoneEntry* entry, FGuid& appliedId){
        if(entry == nullptr){
            component->BlendWeight = 0.0f;
            component->bEnabled = false;
            appliedId.Invalidate();
            return;
        }
        if(appliedId != entry->zoneId){
            component->Settings = entry->settings.postProcess;   // a large struct: copied only when the zone on this layer changes
            appliedId = entry->zoneId;
        }
        component->bEnabled = true;
        component->BlendWeight = entry->weight;
    };
    applyLayer(zoneUpperPostProcess, upper, appliedUpperId);
    applyLayer(zoneLowerPostProcess, lower, appliedLowerId);
}

// ---------------- Lightning (EF4) ----------------

void AEnvironmentManager::scheduleNextLightning(double now){
    nextLightningTime = now + FMath::FRandRange(settings->lightningIntervalMinSeconds, settings->lightningIntervalMaxSeconds);
}

void AEnvironmentManager::fireLightning(double now){
    // One direction and one distance per flash, so the bolt and the thunder agree. Flashes are not synchronised between machines
    // and never reach the ground: nothing gameplay-related happens.
    const float yaw = FMath::FRandRange(0.0f, 360.0f);
    const float pitch = FMath::FRandRange(settings->lightningPitchMinDegrees, settings->lightningPitchMaxDegrees);
    FS_LightningFlashMessage message;
    message.direction = FRotator(pitch, yaw, 0.0f).Vector();
    message.distance = FMath::FRandRange(settings->lightningDistanceMin, settings->lightningDistanceMax);

    pulseStartTime = now;
    lightningPulseValue = 1.0f;

    // R09 delays the thunder by the distance (R09 3.10). R15 plays the bolt as a VFX.Source.Weather effect at that direction and distance.
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Environment_LightningFlash.GetTag(), message);
    UE_LOG(LogAwakenEnvironment, Verbose, TEXT("Lightning: direction %s distance %.0f"), *message.direction.ToCompactString(), message.distance);
}

void AEnvironmentManager::advanceLightning(double now){
    // Real time, so a paused game clock or time dilation does not move it. Scheduling stops as soon as the type is no longer Storm.
    const bool isStormy = targetType == E_WeatherType::Storm && !isSkipActive();
    if(!isStormy){
        nextLightningTime = 0.0;
    }else if(nextLightningTime <= 0.0){
        scheduleNextLightning(now);
    }else if(now >= nextLightningTime){
        fireLightning(now);
        scheduleNextLightning(now);
    }
    // A short curve sampled on the faster active updates. This document owns the light pulse only.
    lightningPulseValue = UCosmeticEnvironmentMathLibrary::computeLightningPulse(static_cast<float>(now - pulseStartTime), settings->lightningPulseSeconds);
}

// ---------------- Dev tools and debug ----------------

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> cvarDebugEnvironment(TEXT("Awaken.Debug.Environment"), 0, TEXT("Draw the environment manager state."), ECVF_Default);

void AEnvironmentManager::drawDebug() const{
    if(cvarDebugEnvironment.GetValueOnGameThread() == 0 || !GEngine || settings == nullptr){
        return;
    }
    const float lifetime = 1.25f;
    const UEnum* weatherEnum = StaticEnum<E_WeatherType>();
    GEngine->AddOnScreenDebugMessage(7000, lifetime, FColor::Cyan, FString::Printf(TEXT("[env] time=%.2fh target=%s blend=%s (%.1f/%.0fs) wetness=%.2f snow=%.2f rate=%s"),
        timeInput.timeOfDayHours, *weatherEnum->GetNameStringByValue(static_cast<int64>(targetType)), isBlending ? TEXT("on") : TEXT("off"), blendElapsed, settings->blendSeconds,
        wetness, snowCover, isActiveRate ? TEXT("active") : TEXT("idle")));
    GEngine->AddOnScreenDebugMessage(7001, lifetime, FColor::White, FString::Printf(TEXT("[env] zones=%d (active %d) skyVisibility=%.2f pulse=%.2f settleLeft=%.1fs skipPending=%d"),
        zoneStack.num(), zoneStack.getActiveIds().Num(), zoneSkyVisibility, lightningPulseValue, FMath::Max(0.0, snapSettleUntil - GetWorld()->GetRealTimeSeconds()), skipPending));
}

void AEnvironmentManager::devForceLightning(){
    if(settings != nullptr){
        fireLightning(GetWorld()->GetRealTimeSeconds());
        refreshUpdateRate();
    }
}
#endif

// ---------------- Editor data checks (Doc02 19.8) ----------------

#if WITH_EDITOR
EDataValidationResult AEnvironmentManager::IsDataValid(FDataValidationContext& context) const{
    EDataValidationResult result = Super::IsDataValid(context);
    bool isInvalid = false;

    // A placed manager that is spatially loaded fails the build (decision D1).
    if(GetIsSpatiallyLoaded()){
        context.AddError(FText::FromString(TEXT("AEnvironmentManager must not be spatially loaded: untick Is Spatially Loaded (decision D1)")));
        isInvalid = true;
    }
    // More than one in a world fails the build.
    if(const UWorld* world = GetWorld()){
        int32 count = 0;
        for(TActorIterator<AEnvironmentManager> it(const_cast<UWorld*>(world)); it; ++it){
            count++;
        }
        if(count > 1){
            context.AddError(FText::FromString(FString::Printf(TEXT("There are %d AEnvironmentManager actors in this world. There must be exactly one"), count)));
            isInvalid = true;
        }
    }
    if(isInvalid){
        return EDataValidationResult::Invalid;
    }
    return result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : result;
}
#endif
