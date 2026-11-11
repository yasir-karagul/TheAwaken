#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BPI_PlayerStateQuery.generated.h"

class APlayerState;

UINTERFACE(BlueprintType)
class UBPI_PlayerStateQuery : public UInterface{
    GENERATED_BODY()
};

class IBPI_PlayerStateQuery{
    GENERATED_BODY()

public:
    // True while the player may not act: Downed, Restrained, Arriving, dead, or locked in a scene (Doc02 19.6).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool isIncapacitated(APlayerState* player);
};
