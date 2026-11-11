#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BPI_ArmorStatsSink.generated.h"

class APlayerState;

UINTERFACE(BlueprintType)
class UBPI_ArmorStatsSink : public UInterface{
    GENERATED_BODY()
};

class IBPI_ArmorStatsSink{
    GENERATED_BODY()

public:
    // Doc R08 §5 ApplyArmorAggregate(TotalArmorValue): writes the AggregatedArmor attribute. Server only.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void applyArmorAggregate(APlayerState* player, float totalArmorValue);
};
