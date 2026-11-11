#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CosmeticEventsEnvironmentPresentationFramework/01Structs/S_CosmeticEventsEnvironmentPresentationFramework.h"
#include "WeatherFramework/00Enums/E_WeatherFramework.h"
#include "DA_WeatherPresentation.generated.h"

// One instance per weather type of B14: Clear, Cloudy, Rain, Storm, Fog, Snow, Blizzard (seven in all).
// Storm is heavy rain with wind, Blizzard is the strongest snow, Fog mainly changes fog and visibility: all of it is data, not code.
// Primary Asset Type "WeatherPresentation".
UCLASS(BlueprintType)
class THEAWAKEN_API UDA_WeatherPresentation : public UPrimaryDataAsset{
    GENERATED_BODY()

public:
    static constexpr const TCHAR* primaryAssetType = TEXT("WeatherPresentation");

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& context) const override;
#endif

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
    E_WeatherType weatherType = E_WeatherType::Clear;

    // Rain, Storm, Snow and Blizzard: the target is scaled by the strength each update, because the strength changes inside a period.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
    bool scalesWithStrength = false;

    // The look at strength 100, or the whole look when scalesWithStrength is off.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation")
    FS_WeatherLook look;

    // The look at strength 0. Only read when scalesWithStrength is on.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Presentation", meta=(EditCondition="scalesWithStrength"))
    FS_WeatherLook lookAtZero;

    // The target look for a strength of 0 to 100.
    FS_WeatherLook getTargetLook(float strength) const;
};
