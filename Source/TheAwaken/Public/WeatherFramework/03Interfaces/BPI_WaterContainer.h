#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WeatherFramework/01Structs/S_WeatherFramework.h"
#include "BPI_WaterContainer.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UBPI_WaterContainer : public UInterface{
    GENERATED_BODY()
};

class THEAWAKEN_API IBPI_WaterContainer{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FS_ContainerWaterState getWaterState();

    // Adds clean water. Returns how many units went in.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    int32 addCleanWater(int32 units);
};
