#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BPI_IndoorZone.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UBPI_IndoorZone : public UInterface{
    GENERATED_BODY()
};

class THEAWAKEN_API IBPI_IndoorZone{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool containsPoint(FVector location);
};
