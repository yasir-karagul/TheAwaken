#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "TimerManager.h"
#include "CosmeticEventsEnvironmentPresentationFramework/01Structs/S_CosmeticEventsEnvironmentPresentationFramework.h"
#include "CosmeticEventsEnvironmentPresentationFramework/EnvironmentZoneStack.h"
#include "CosmeticEventsEnvironmentPresentationFramework/EnvironmentSources.h"
#include "TimeFramework/01Structs/S_TimeFramework.h"
#include "WeatherFramework/00Enums/E_WeatherFramework.h"
#include "WeatherFramework/01Structs/S_WeatherFramework.h"
#include "EnvironmentManager.generated.h"

class UDA_EnvironmentSettings;
class UDA_EnvironmentCurves;
class UDA_WeatherPresentation;
class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;
class UExponentialHeightFogComponent;
class UPostProcessComponent;
class UMaterialParameterCollection;

// Part B of B21: draws the sun, moon, sky, clouds, fog, post-process, wind, wetness, snow cover and the lightning light pulse, and applies
// cave and dungeon zone looks. One per world, in the persistent level, on every machine except a dedicated server.
//
// It owns no world state and never writes to Time or Weather. It is not replicated and not saved. Nothing here changes a gameplay result.
// World Partition must never stream it out: it is not spatially loaded (decision D1).
UCLASS()
class THEAWAKEN_API AEnvironmentManager : public AActor{
    GENERATED_BODY()

public:
    AEnvironmentManager();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type reason) override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& context) const override;
#endif

    static AEnvironmentManager* find(const UObject* worldContext);

    // Called by AEnvironmentZoneVolume, which B21 owns (gap G3). PROPOSED signatures until EF1 is amended.
    // pushZone of an id already on the stack updates its settings and does not add a second entry. popZone of an unknown id is ignored.
    void pushZone(const FGuid& zoneId, const FS_EnvironmentZoneSettings& settings);
    void popZone(const FGuid& zoneId);

    // Hard start: no blend (B.3). Also opens the settle window.
    void snapToTarget();

    // B17 calls this after it raises Settings.Changed (that message and its payload are not defined in the project yet). Re-applies the preset.
    void notifySettingsChanged();

    // The ids of the zones that are on the stack and not popped, bottom to top.
    TArray<FGuid> getActiveZoneIds() const;

#if !UE_BUILD_SHIPPING
    void drawDebug() const;
    void devForceLightning();
#endif

private:
    // ---- data and listeners ----
    bool loadData();
    void registerListeners();
    void unregisterListeners();
    void readInputs();

    void onWeatherChanged(FGameplayTag channel, const FS_WeatherChangedMessage& message);
    void onSkipStarted(FGameplayTag channel, const FS_TimeSkipStartedMessage& message);
    void onSkipEnded(FGameplayTag channel, const FS_TimeSkipEndedMessage& message);
    void onPhaseChanged(FGameplayTag channel, const FS_TimePhaseChangedMessage& message);
    void onSeasonChanged(FGameplayTag channel, const FS_TimeSeasonChangedMessage& message);

    // ---- the update loop (B.1) ----
    void updateEnvironment();
    void applyAll();
    void applySnap(bool startSettle);
    bool isInSettleWindow() const;
    bool isSkipActive() const;
    void detectClockJump();
    void detectWeatherChange();
    void handleWeatherTarget(E_WeatherType newType);
    void beginBlendTo(E_WeatherType newType);
    FS_WeatherLook computeTargetLook() const;

    void advanceBlend(float deltaSeconds);
    void advanceZones(float deltaSeconds);
    void advanceLightning(double now);
    void applyTime();
    void applyLook();
    void advanceWetness(float deltaSeconds);
    void applySnowCover();
    void applyZones();
    void applyLights();
    void writeParameterCollection();
    void applyQuality();
    void refreshUpdateRate();
    void startUpdateTimer(float updatesPerSecond);

    // ---- lightning (EF4) ----
    void scheduleNextLightning(double now);
    void fireLightning(double now);

    // ---- components ----
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> root;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UDirectionalLightComponent> sun;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UDirectionalLightComponent> moon;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkyAtmosphereComponent> skyAtmosphere;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USkyLightComponent> skyLight;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UVolumetricCloudComponent> volumetricCloud;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UExponentialHeightFogComponent> heightFog;

    // The global exposure and grade. The two zone components follow the zone weights (B.7).
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UPostProcessComponent> globalPostProcess;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UPostProcessComponent> zoneLowerPostProcess;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UPostProcessComponent> zoneUpperPostProcess;

    // ---- loaded data ----
    UPROPERTY()
    TObjectPtr<UDA_EnvironmentSettings> settings;

    UPROPERTY()
    TObjectPtr<UDA_EnvironmentCurves> curves;

    UPROPERTY()
    TMap<E_WeatherType, TObjectPtr<UDA_WeatherPresentation>> presentations;

    UPROPERTY()
    TObjectPtr<UMaterialParameterCollection> parameterCollection;

    // ---- state (never saved, never sent) ----
    FTimerHandle updateTimer;
    FTimerHandle debugTimer;
    FGameplayMessageListenerHandle weatherListener;
    FGameplayMessageListenerHandle skipStartedListener;
    FGameplayMessageListenerHandle skipEndedListener;
    FGameplayMessageListenerHandle phaseListener;
    FGameplayMessageListenerHandle seasonListener;
    bool isActiveRate = false;

    FS_EnvironmentTimeInput timeInput;
    FS_EnvironmentWeatherInput weatherInput;

    // The weather blend. blendFrom is the mixed look at the moment a blend started, so a change that arrives mid-blend is continuous.
    E_WeatherType targetType = E_WeatherType::Clear;
    FS_WeatherLook blendFrom;
    FS_WeatherLook currentLook;
    float blendElapsed = 0.0f;
    bool isBlending = false;

    float wetness = 0.0f;
    float snowCover = 0.0f;
    double lastUpdateTime = 0.0;
    double snapSettleUntil = 0.0;
    float maxUpdateDeltaSeconds = 0.5f;   // loaded from UDA_EnvironmentSettings
    bool hasSnappedToReal = false;        // true once a snap ran with Time and Weather both ready
    int32 lastSkipSerial = INDEX_NONE;

    // A group sleep skip: from Time.SkipStarted until the snap at Time.SkipEnded, a Weather.Changed only records the target.
    bool skipPending = false;
    double skipPendingSince = 0.0;

    // Zones
    FEnvironmentZoneStack zoneStack;
    float zoneSkyVisibility = 1.0f;
    bool wasSkyVisible = true;
    FGuid appliedUpperId;
    FGuid appliedLowerId;

    // Lightning (real time, so it is not affected by time dilation or the game clock)
    double nextLightningTime = 0.0;
    double pulseStartTime = -1000.0;
    float lightningPulseValue = 0.0f;

    // Values computed from the curves, kept for applyLights
    float timeSunIntensity = 0.0f;
    float timeMoonIntensity = 0.0f;
    FLinearColor timeSunColor = FLinearColor::White;
    FLinearColor timeMoonColor = FLinearColor::White;
    FLinearColor timeSkyTint = FLinearColor::White;
    float timeNightFactor = 0.0f;
};
