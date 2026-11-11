#pragma once
#include "Engine/DataAsset.h"
#include "DA_TimeSettings.generated.h"

UCLASS(BlueprintType)
class THEAWAKEN_API UDA_TimeSettings : public UPrimaryDataAsset{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 realSecondsPerGameDay = 3600;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 daysPerSeason = 30;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 snowyMinDays = 5;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 hourBroadcastEveryHours = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float worldStartTimeOfDay = 7.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 wakeHour = 5;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 sleepWindowOpenHour = 21;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 sleepLastStartHour = 4;

    // Unreal units are centimetres: 300 is the 3 metres of R12 3.2.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(Units="cm"))
    float sleepRegisterRadiusCm = 300.0f;

    // R12-IG4, RESOLVED (designer-approved 2026-10-21): 1800 s (30 minutes). 0 or less means idle detection is off.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float awayIdleSeconds = 1800.0f;

    // R12 3.7, RESOLVED (R12-IG1, designer-approved 2026-10-21): start value 2 s. Agree any later change with Doc B17.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float skipFadeOutSeconds = 2.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float clockStepRealSeconds = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float sleepMonitorRealSeconds = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float dawnStartHour = 5.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float dayStartHour = 7.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float duskStartHour = 18.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    float nightStartHour = 20.0f;

    float gameSecondsPerRealSecond() const;
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& context) const override;
#endif
};