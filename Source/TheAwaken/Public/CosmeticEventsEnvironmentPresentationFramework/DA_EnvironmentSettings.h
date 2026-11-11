#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "CosmeticEventsEnvironmentPresentationFramework/01Structs/S_CosmeticEventsEnvironmentPresentationFramework.h"
#include "DA_EnvironmentSettings.generated.h"

class UMaterialParameterCollection;

// The tuning numbers of B21 (PROPOSED, not in B21 section 10). Every number of the framework is data (Doc02 19.8).
// Loaded through the Asset Manager as DA_EnvironmentSettings_Default, Primary Asset Type "EnvironmentSettings".
UCLASS(BlueprintType)
class THEAWAKEN_API UDA_EnvironmentSettings : public UPrimaryDataAsset{
    GENERATED_BODY()

public:
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    // Returns null when the asset is missing. Callers log their own error.
    static UDA_EnvironmentSettings* loadDefault();

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& context) const override;
#endif

    // ---- Part A: cosmetic events ----

    // DT_CosmeticEvents, row struct FS_CosmeticEventRule. A soft pointer, loaded by the subsystem in Initialize.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cosmetic Events")
    TSoftObjectPtr<UDataTable> cosmeticEventTable;

    // The rate limiter counts the sends of this many real seconds (B21 A.2: one second).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cosmetic Events", meta=(ClampMin="0.1"))
    float rateWindowSeconds = 1.0f;

    // Used by rows with readsHearingDistance until Doc R09 names its accessor (gap G9).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cosmetic Events", meta=(ClampMin="0.0"))
    float fallbackHearingDistance = 3000.0f;

    // ---- Part B: the update loop ----

    // Updates per second while nothing moves (B.1: a few a second).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Update", meta=(ClampMin="0.1"))
    float idleUpdateRate = 4.0f;

    // Updates per second while a weather blend, a zone fade or a lightning pulse runs.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Update", meta=(ClampMin="0.1"))
    float activeUpdateRate = 30.0f;

    // The delta of one update is clamped to this, so a hitch or an alt-tab cannot make wetness or a blend jump.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Update", meta=(ClampMin="0.01"))
    float maxUpdateDeltaSeconds = 0.5f;

    // After a snap, a Weather.Changed or a self-correction snaps instead of blending for this long (gap G10).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Update", meta=(ClampMin="0.0"))
    float postSnapSettleSeconds = 5.0f;

    // Safety net: a Time.SkipStarted that is never followed by Time.SkipEnded is dropped after this long.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Update", meta=(ClampMin="1.0"))
    float skipPendingTimeoutSeconds = 30.0f;

    // ---- Part B: weather ----

    // B14 3.7: 30 real seconds. Must be above 0.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Weather", meta=(ClampMin="0.1"))
    float blendSeconds = 30.0f;

    // ---- Part B: sky ----

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Sky")
    float sunYawDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Sky")
    float sunOrbitTiltDegrees = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Sky", meta=(ClampMin="0.0"))
    float skyLightBaseIntensity = 1.0f;

    // The sky light at full night, as a share of the day value. Above 0: the night is dark but never black (approved).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Sky", meta=(ClampMin="0.0", ClampMax="1.0"))
    float nightSkyLightScale = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Sky", meta=(ClampMin="0.0"))
    float baseFogHeightFalloff = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Sky")
    FLinearColor baseFogInscatteringColor = FLinearColor(0.447f, 0.638f, 1.0f, 1.0f);

    // ---- Part B: wetness (EF6) ----

    // Seconds to go from 0 to 1 in rain of strength 100. Weaker rain is slower.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Wetness", meta=(ClampMin="0.1"))
    float wetnessRiseSecondsAtFullRain = 60.0f;

    // Seconds to go from 1 to 0 once the rain has stopped.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Wetness", meta=(ClampMin="0.1"))
    float wetnessDrySeconds = 300.0f;

    // ---- Part B: lightning (EF4) ----

    // Real seconds between flashes in a storm.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Lightning", meta=(ClampMin="0.1"))
    float lightningIntervalMinSeconds = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Lightning", meta=(ClampMin="0.1"))
    float lightningIntervalMaxSeconds = 60.0f;

    // Centimetres. One distance is drawn per flash, so the bolt and the thunder agree.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Lightning", meta=(ClampMin="0.0"))
    float lightningDistanceMin = 30000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Lightning", meta=(ClampMin="0.0"))
    float lightningDistanceMax = 300000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Lightning")
    float lightningPitchMinDegrees = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Lightning")
    float lightningPitchMaxDegrees = 35.0f;

    // A few tenths of a second.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Lightning", meta=(ClampMin="0.05"))
    float lightningPulseSeconds = 0.4f;

    // Added to the sky light intensity at the peak of the pulse.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Lightning", meta=(ClampMin="0.0"))
    float lightningSkyLightBoost = 6.0f;

    // ---- Part B: zones (EF1) ----

    // Used when the zone's own blendSeconds is 0.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Zones", meta=(ClampMin="0.0"))
    float defaultZoneBlendSeconds = 3.0f;

    // ---- Part B: quality (B17 3.13) ----

    // Index 0 is Low, then Medium, High, Epic. The index is the engine scalability level until B17 sends Settings.Changed with its own preset.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Quality")
    TArray<FS_EnvironmentQualityPreset> qualityPresets;

    // ---- Part B: the parameter collection (read by R15, R09 and the materials) ----

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Parameters")
    TSoftObjectPtr<UMaterialParameterCollection> parameterCollection;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Parameters")
    FName wetnessParameter = TEXT("wetness");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Parameters")
    FName snowCoverParameter = TEXT("snowCover");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Parameters")
    FName windStrengthParameter = TEXT("windStrength");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Parameters")
    FName windDirectionParameter = TEXT("windDirection");

    // 0 to 100, as B14 gives it. R15 reads it for its particles.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Parameters")
    FName rainStrengthParameter = TEXT("rainStrength");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Parameters")
    FName snowStrengthParameter = TEXT("snowStrength");

    // PROPOSED extra parameters, for the sky materials. Remove them from the collection if the materials do not need them.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Parameters")
    FName cloudCoverageParameter = TEXT("cloudCoverage");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Parameters")
    FName cloudDensityParameter = TEXT("cloudDensity");

    // Night factor times one minus the cloud coverage (B.1). The star field is a sky material parameter.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Parameters")
    FName starOpacityParameter = TEXT("starOpacity");

    // 1 outside, 0 deep inside a hide-sky zone. The cloud material fades with it.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Parameters")
    FName skyVisibilityParameter = TEXT("skyVisibility");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment|Parameters")
    FName lightningPulseParameter = TEXT("lightningPulse");

    // ---- Part C: water ----

    // The ice look follows the replicated ice level on this timer. The same timer re-checks the ice collision against Weather's
    // iceSolid, as a safety net behind Weather.WaterFroze and Weather.WaterThawed (gap G10).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(ClampMin="0.1"))
    float iceReconcileSeconds = 1.0f;

    // A player who flickers across a body edge is not announced again inside this time (PROPOSED, B03 adds a bump per entry).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(ClampMin="0.0"))
    float waterReentryDelaySeconds = 0.5f;

    // Centimetres per second for one point of river strength at baseFlowSpeed 1 (the data scale of B14 3.3, PROPOSED).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(ClampMin="0.0"))
    float flowSpeedPerStrength = 2.0f;

    // When the ice turns on under a swimmer, the swimmer is put this far above the ice surface.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(ClampMin="0.0"))
    float iceLiftClearanceCm = 5.0f;

    // How far from the swimmer the nearest free point on the ice is searched when there is no room straight above.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water", meta=(ClampMin="0.0"))
    float iceLiftSearchRadiusCm = 400.0f;
};
