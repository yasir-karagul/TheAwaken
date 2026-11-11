#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "BPI_ActionService.generated.h"

class APlayerState;

UINTERFACE(BlueprintType)
class UBPI_ActionService : public UInterface{
    GENERATED_BODY()
};

class IBPI_ActionService{
    GENERATED_BODY()

public:
    // Doc R04 cancelAction(ActionTag). R01 calls it with Action.Reload when the held item changes (R04-R08-IG17).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void cancelAction(APlayerState* player, FGameplayTag actionTag);
};
