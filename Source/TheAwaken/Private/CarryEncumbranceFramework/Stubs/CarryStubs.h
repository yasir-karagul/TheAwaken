// TEMPORARY STUBS — the owner of each is named on its class. Delete a class when the real framework lands.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/Object.h"
#include "CarryEncumbranceFramework/03Interfaces/BPI_CarryableActor.h"
#include "CarryEncumbranceFramework/03Interfaces/BPI_ThrowImpactSink.h"
#include "CarryStubs.generated.h"

// TEMPORARY STUB — owner: Doc R03 Combat. Remembers the hits; hurts nobody; the item always lands.
UCLASS()
class UStubThrowImpactSink : public UObject, public IBPI_ThrowImpactSink{
    GENERATED_BODY()

public:
    UPROPERTY() TArray<TObjectPtr<AActor>> hitTargets;
    UPROPERTY() float totalDamage = 0.0f;
    // Tests only: false makes every hit say "do not land".
    UPROPERTY() bool letItemLand = true;

    virtual bool applyThrowImpact_Implementation(AActor* target, AActor* thrower, FS_InventoryEntry& entry, float damage, FVector location) override;
};

// TEMPORARY STUB — owners: Doc B08 (the corpse) and Doc B12 (the wounded ally). A carryable dummy for tests and the dev command.
UCLASS()
class AStubCarryableActor : public AActor, public IBPI_CarryableActor{
    GENERATED_BODY()

public:
    AStubCarryableActor();

    UPROPERTY(EditAnywhere) int32 slotCost = 1;
    UPROPERTY() int32 beganCount = 0;
    UPROPERTY() int32 endedCount = 0;
    UPROPERTY() E_EndCarryReason lastReason = E_EndCarryReason::Dropped;

    virtual int32 getCarrySlotCost_Implementation() override;
    virtual FName getCarryLabel_Implementation() override;
    virtual void onCarryBegan_Implementation(AActor* carrier) override;
    virtual void onCarryEnded_Implementation(AActor* carrier, E_EndCarryReason reason) override;
};
