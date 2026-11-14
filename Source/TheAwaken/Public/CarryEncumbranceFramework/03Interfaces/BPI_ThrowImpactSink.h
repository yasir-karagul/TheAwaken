#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "BPI_ThrowImpactSink.generated.h"

class AActor;

// OUTGOING. Owner: Doc R03 Combat (C8).
UINTERFACE(BlueprintType)
class UBPI_ThrowImpactSink : public UInterface{
    GENERATED_BODY()
};

class IBPI_ThrowImpactSink{
    GENERATED_BODY()

public:
    // Server. A thrown item hit target. Doc R03 runs its normal damage order (a thrown item is ATTACK damage,
    // R03 3.9) with thrower as the instigator. It may change the entry (a thrown weapon loses one use, R03 3.5).
    // Returns false when the item must NOT land as a loose item (a weapon that reached zero durability breaks).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool applyThrowImpact(AActor* target, AActor* thrower, UPARAM(ref) FS_InventoryEntry& entry, float damage, FVector location);
};
