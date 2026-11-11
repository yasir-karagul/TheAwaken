#pragma once
#include "UObject/Interface.h"
#include "TimeFramework/00Enums/E_TimeFramework.h"
#include "BPI_SleepEligibility.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UBPI_SleepEligibility : public UInterface{
    GENERATED_BODY()
};

class THEAWAKEN_API IBPI_SleepEligibility{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    E_SleepCountState getSleepCountState();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool canRegisterToSleep();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool hasAwareHostileNearby();
};
