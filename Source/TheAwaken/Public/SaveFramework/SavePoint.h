#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "SavePoint.generated.h"

class APlayerState;
class USaveSubsystem;

/**
 * A Save Point is a player-built piece (Doc R13 3.2, A50): the Building framework owns its record and creates this actor as the
 * loaded view of it. The actor adds one thing, the hold-to-save interaction. It stores no save state, and it is NOT a respawn checkpoint.
 *
 * WHEN THE INTERACTION FRAMEWORK EXISTS (BPI_Interactable.h and BPI_InteractableHoldToActivate.h are empty files today):
 *   1. #include both interface headers here.
 *   2. class ASavePoint : public AActor, public IBPI_Interactable, public IBPI_InteractableHoldToActivate
 *   3. Rename the five functions below to <name>_Implementation, mark them "virtual ... override", and match the signatures.
 *   4. getInteractionTag must return T_Interaction_Save (already declared in T_SaveFramework.h).
 * Nothing else changes: the three bodies hold the logic.
 */
UCLASS(Blueprintable)
class THEAWAKEN_API ASavePoint : public AActor{
    GENERATED_BODY()
public:
    ASavePoint();

    /** Read from the Building record. placementOrder breaks distance ties for the nearest-Save-Point rule of B06
     *  (the earliest-placed wins, Doc R13 3.11 S1-F25). Neither is written by Save. */
    UPROPERTY(BlueprintReadOnly, Category = "Save") FGuid savePointId;
    UPROPERTY(BlueprintReadOnly, Category = "Save") int64 placementOrder = 0;

    // ---- IBPI_Interactable shape (Doc B18 3.2). Hold, Exclusive lock (Doc B18 3.7). ----
    FGameplayTag getInteractionTag() const;
    bool canInteract(AActor* instigator, FGameplayTag& failReason) const;
    // plus GetInteractionKind -> Hold, GetLockPolicy -> Exclusive, GetPrompt -> the R10 keys for "Save"

    // ---- IBPI_InteractableHoldToActivate shape (Doc B11 3.4) ----
    float getHoldDuration() const;
    bool canActivate(AActor* instigator, FGameplayTag& failReason) const;
    void onActivationComplete(AActor* instigator);

private:
    USaveSubsystem* getSave() const;
    static APlayerState* resolvePlayerState(const AActor* instigator);
    bool evaluateGate(AActor* instigator, FGameplayTag& failReason) const;
};