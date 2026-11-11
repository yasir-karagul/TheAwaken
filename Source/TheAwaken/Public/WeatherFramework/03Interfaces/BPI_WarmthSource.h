#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BPI_WarmthSource.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UBPI_WarmthSource : public UInterface{
    GENERATED_BODY()
};

class THEAWAKEN_API IBPI_WarmthSource{
    GENERATED_BODY()

public:
    // True while the source is lit (Interaction.FireSource toggle, Doc B11).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool isProvidingWarmth();

    // Distance in cm within which the player counts as warmed. The value is data (Open question 8).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    float getWarmthRadius();
};
