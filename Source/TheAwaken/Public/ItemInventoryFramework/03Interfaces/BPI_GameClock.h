#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BPI_GameClock.generated.h"

UINTERFACE(BlueprintType)
class UBPI_GameClock : public UInterface{
    GENERATED_BODY()
};

class IBPI_GameClock{
    GENERATED_BODY()

public:
    // Doc R12 GetGameTime(): game seconds since the world was created, a 64-bit whole number (Doc02 19.4).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    int64 getGameTime();
};
