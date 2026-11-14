#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CarryEncumbranceFramework/00Enums/E_CarryEncumbranceFramework.h"
#include "BPI_CarryableActor.generated.h"

class AActor;

// INCOMING. A world actor that can be carried whole implements it: the corpse of Doc B08 and the wounded ally of Doc B12 (C10).
UINTERFACE(BlueprintType)
class UBPI_CarryableActor : public UInterface{
    GENERATED_BODY()
};

class IBPI_CarryableActor{
    GENERATED_BODY()

public:
    // How many carry slots the actor fills. 0 or less = not carryable right now (for example mid-harvest).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    int32 getCarrySlotCost();

    // The name shown in logs, messages and the debug view in place of an item id (for example Corpse).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FName getCarryLabel();

    // Server. The actor is now attached to the carrier. Turn physics and collision off, pause the despawn timer,
    // refuse a harvest (Doc B08 3.2, 3.4).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void onCarryBegan(AActor* carrier);

    // Server. The actor has been detached and moved to where it was put down. Turn physics and collision back on,
    // let the despawn timer run again: it is an ordinary corpse where it lies (Index A93, UD11).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void onCarryEnded(AActor* carrier, E_EndCarryReason reason);
};
