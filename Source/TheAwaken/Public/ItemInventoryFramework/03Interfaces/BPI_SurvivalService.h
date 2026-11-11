#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "BPI_SurvivalService.generated.h"

class APlayerState;

UINTERFACE(BlueprintType)
class UBPI_SurvivalService : public UInterface{
    GENERATED_BODY()
};

class IBPI_SurvivalService{
    GENERATED_BODY()

public:
    // Doc B03 ModifyMeter(MeterType, Delta, SourceTag).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void modifyMeter(APlayerState* player, FGameplayTag meterTag, float delta, FGameplayTag sourceTag);

    // Doc B03 RegisterIllnessSource, for a source that comes from eating or drinking (it ends by itself).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void registerIllnessSource(APlayerState* player, FGameplayTag sourceTag, float durationHours);

    // False when the Illness meter is switched off (Doc B13): then neither the illness chance nor the HP loss applies.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool isIllnessActive(APlayerState* player);
};
