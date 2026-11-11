#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "BPI_HealthService.generated.h"

class APlayerState;

UINTERFACE(BlueprintType)
class UBPI_HealthService : public UInterface{
    GENERATED_BODY()
};

class IBPI_HealthService{
    GENERATED_BODY()

public:
    // Doc R08 ModifyHP with a positive amount (medicine, R01 3.9).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void healPlayer(APlayerState* player, float amount, FGameplayTag sourceTag);

    // The normal damage path of Doc R03 3.7. That path applies the Peaceful floor and the dialogue refusal (R01 3.2a).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void damagePlayer(APlayerState* player, float amount, FGameplayTag damageTypeTag);
};
