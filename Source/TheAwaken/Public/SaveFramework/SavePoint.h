#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "InteractionCoreFramework/03Interfaces/BPI_Interactable.h"
#include "InteractionFrameworkExtension/03Interfaces/BPI_InteractableHoldToActivate.h"
#include "SavePoint.generated.h"

class APlayerState;
class USaveSubsystem;
class UInteractionLockComponent;

/**
 * A Save Point is a player-built piece (Doc R13 3.2, A50): the Building framework owns its record and creates this actor as the
 * loaded view of it. The actor adds one thing, the hold-to-save interaction. It stores no save state, and it is NOT a respawn checkpoint.
 *
 * WIRED to the Interaction framework (Doc B18): it implements IBPI_Interactable and IBPI_InteractableHoldToActivate.
 * The three original bodies (tag, canInteract, canActivate, getHoldDuration, onActivationComplete) are unchanged; only their
 * names and signatures follow the interface. A Hold object with an Exclusive lock carries a UInteractionLockComponent.
 */
UCLASS(Blueprintable)
class THEAWAKEN_API ASavePoint : public AActor, public IBPI_Interactable, public IBPI_InteractableHoldToActivate{
    GENERATED_BODY()
public:
    ASavePoint();

    /** Read from the Building record. placementOrder breaks distance ties for the nearest-Save-Point rule of B06
     *  (the earliest-placed wins, Doc R13 3.11 S1-F25). Neither is written by Save. */
    UPROPERTY(BlueprintReadOnly, Category = "Save") FGuid savePointId;
    UPROPERTY(BlueprintReadOnly, Category = "Save") int64 placementOrder = 0;

    // ---- IBPI_Interactable (Doc B18 3.2). Hold, Exclusive lock (Doc B18 3.7). ----
    virtual FGameplayTag getInteractionTag_Implementation() const override;
    virtual E_InteractionKind getInteractionKind_Implementation() const override;
    virtual bool canInteract_Implementation(AActor* instigator, FGameplayTag& failReason) const override;
    virtual FS_InteractionPrompt getPrompt_Implementation(AActor* instigator) const override;
    virtual void interact_Implementation(AActor* instigator) override;
    virtual E_InteractionLockPolicy getLockPolicy_Implementation() const override;
    virtual bool getNoise_Implementation(FS_InteractionNoise& noise) const override;
    virtual int32 getInteractionPriority_Implementation() const override;
    virtual bool isInteractableBy_Implementation(AActor* instigator) const override;

    // ---- IBPI_InteractableHoldToActivate (Doc B11 3.4) ----
    virtual float getHoldDuration_Implementation() const override;
    virtual bool canActivate_Implementation(AActor* instigator, FGameplayTag& failReason) const override;
    virtual void onActivationComplete_Implementation(AActor* instigator) override;

private:
    USaveSubsystem* getSave() const;
    static APlayerState* resolvePlayerState(const AActor* instigator);
    bool evaluateGate(AActor* instigator, FGameplayTag& failReason) const;

    // Exclusive lock (B18 3.7): two players cannot save at the same Save Point at once.
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UInteractionLockComponent> lock;
};
