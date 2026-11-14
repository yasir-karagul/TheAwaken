// Public/InteractionFrameworkExtension/03Interfaces/BPI_InteractableHoldToActivate.h
// Doc B11 3.4. Lives in the Extension folder because it is a Doc B11 file, but it is built with Core: holds (guide Step 7) need it.
// Signatures take instigator and failReason (Doc02 6, no bare false). ASavePoint uses exactly these three.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "BPI_InteractableHoldToActivate.generated.h"

UINTERFACE(BlueprintType)
class UBPI_InteractableHoldToActivate : public UInterface{
    GENERATED_BODY()
};

/**
 * Hold-to-activate objects: Save, Sleep, Escape, Harvest, revive. The server starts the hold and the server completes it.
 * The object owns its own cancel conditions through canActivate, which runs when the hold starts and AGAIN at completion.
 */
class THEAWAKEN_API IBPI_InteractableHoldToActivate{
    GENERATED_BODY()
public:
    // Seconds. Data on the object or its owner's asset, never a constant in the Interaction framework.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    float getHoldDuration() const;

    // Server-authoritative validation. Return a refusal as a tag in failReason.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    bool canActivate(AActor* instigator, FGameplayTag& failReason) const;

    // Runs on the server when the duration has passed and canActivate still holds.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    void onActivationComplete(AActor* instigator);
};
