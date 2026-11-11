// TEMPORARY STUB — owner: see each class below. Delete each class when the real framework lands.
// (The guide's UStubCosmeticEventSink is not created: Doc B21's own raise function exists, E1.4 / E10.8.)
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EquipmentFramework/03Interfaces/BPI_ArmorStatsSink.h"
#include "EquipmentStubs.generated.h"

// TEMPORARY STUB — owner: Doc R08 Stats. Remembers the last value so a test can read it.
UCLASS()
class UStubArmorStatsSink : public UObject, public IBPI_ArmorStatsSink{
    GENERATED_BODY()

public:
    UPROPERTY() float lastArmorValue = 0.0f;
    UPROPERTY() int32 callCount = 0;

    virtual void applyArmorAggregate_Implementation(APlayerState* player, float totalArmorValue) override;
};
