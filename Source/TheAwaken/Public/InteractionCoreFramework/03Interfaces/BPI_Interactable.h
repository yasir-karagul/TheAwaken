// Public/InteractionCoreFramework/03Interfaces/BPI_Interactable.h
// Doc B18 3.2 (named IInteractable there; Doc02 20.2 requires the UBPI_ / IBPI_ pair).
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "InteractionCoreFramework/00Enums/E_InteractionCoreFramework.h"
#include "InteractionCoreFramework/01Structs/S_InteractionCoreFramework.h"
#include "BPI_Interactable.generated.h"

UINTERFACE(BlueprintType)
class UBPI_Interactable : public UInterface{
    GENERATED_BODY()
};

/**
 * Every interactable implements this one interface (B18 3.2).
 *
 * Rules for implementers (enforced by UInteractionComponent):
 *  - canInteract and getPrompt are cheap and READ-ONLY (Doc02 4). The client calls them for the prompt, the server calls canInteract again.
 *  - canInteract returns a refusal as a TAG in failReason, never a bare false. An object may add its own tags.
 *  - interact runs on the server only and applies state last.
 *  - getNoise returns false when the object is quiet.
 *  - A Hold object implements IBPI_InteractableHoldToActivate as well; its interact is not used.
 *
 * ALL NINE functions must be overridden, including the two added on 2026-10-24:
 *  - getInteractionPriority: return 0 unless the object needs to win focus ties.
 *  - isInteractableBy: return TRUE unless the object hides itself from some players (a death bag shows only to its owner, Doc B02 4.3).
 *    The guide asked for a default of true. A default body for an interface native event is not portable across engine versions,
 *    so there is none here: an implementer that forgets this one would be invisible to everybody.
 */
class THEAWAKEN_API IBPI_Interactable{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    FGameplayTag getInteractionTag() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    E_InteractionKind getInteractionKind() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    bool canInteract(AActor* instigator, FGameplayTag& failReason) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    FS_InteractionPrompt getPrompt(AActor* instigator) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    void interact(AActor* instigator);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    E_InteractionLockPolicy getLockPolicy() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    bool getNoise(FS_InteractionNoise& noise) const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    int32 getInteractionPriority() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
    bool isInteractableBy(AActor* instigator) const;
};
