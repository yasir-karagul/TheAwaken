#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TimeFramework/00Enums/E_TimeFramework.h"
#include "DA_EnvironmentCurves.generated.h"

class UCurveFloat;
class UCurveLinearColor;

// Time of day curves for the sun, the moon, the sky tint, the exposure and the season grade (B21 B.1).
// The X axis of every curve is the fraction of the day: 0 is 00:00 and 1 is 24:00 (UTimeSubsystem gives hours, divide by 24).
// Loaded as DA_EnvironmentCurves_Default, Primary Asset Type "EnvironmentCurves".
UCLASS(BlueprintType)
class THEAWAKEN_API UDA_EnvironmentCurves : public UPrimaryDataAsset{
    GENERATED_BODY()

public:
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
    static UDA_EnvironmentCurves* loadDefault();

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& context) const override;
#endif

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sun")
    TObjectPtr<UCurveFloat> sunIntensity;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sun")
    TObjectPtr<UCurveLinearColor> sunColor;

    // Must stay above 0 all day: the night is dark but never black (approved).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moon")
    TObjectPtr<UCurveFloat> moonIntensity;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moon")
    TObjectPtr<UCurveLinearColor> moonColor;

    // Must stay above 0 in luminance all day.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sky")
    TObjectPtr<UCurveLinearColor> skyTint;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sky")
    TObjectPtr<UCurveFloat> exposureBias;

    // 0 in full day, 1 in full night. Drives the star opacity and the night share of the sky light.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sky")
    TObjectPtr<UCurveFloat> nightFactor;

    // The colour grade of each season. The Snowy block uses the Winter grade (B21 B.1).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Season")
    TMap<E_Season, FLinearColor> seasonGrade;

    float sampleSunIntensity(float dayFraction) const;
    FLinearColor sampleSunColor(float dayFraction) const;
    float sampleMoonIntensity(float dayFraction) const;
    FLinearColor sampleMoonColor(float dayFraction) const;
    FLinearColor sampleSkyTint(float dayFraction) const;
    float sampleExposureBias(float dayFraction) const;
    float sampleNightFactor(float dayFraction) const;
    FLinearColor getSeasonGrade(E_Season season, bool isSnowyBlock) const;

    // The lowest value of the moon intensity and of the sky tint luminance over the whole day. The data check fails the build when it is 0.
    float getNightMinimum() const;
};
