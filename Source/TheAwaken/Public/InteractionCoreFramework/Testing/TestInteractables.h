// Public/InteractionCoreFramework/Testing/TestInteractables.h (TEMPORARY, TEST ONLY)
// The three throwaway actors of guide Step 12. Place them in a test map and run the 25 cases on a listen server with two clients.
// DELETE this file and its .cpp once Doc B11 objects exist. Verb and object keys are placeholders: they are not in any String Table.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "InteractionCoreFramework/03Interfaces/BPI_Interactable.h"
#include "InteractionFrameworkExtension/03Interfaces/BPI_InteractableHoldToActivate.h"
#include "TestInteractables.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UInteractionLockComponent;

// Shared plumbing: a visible cube, an overlap box (the focus sweep finds it on any channel) and the nine interface functions.
UCLASS(Abstract)
class THEAWAKEN_API ATestInteractableBase : public AActor, public IBPI_Interactable{
    GENERATED_BODY()
public:
    ATestInteractableBase();

    // Tick to make canInteract refuse (cases 5, 21, 23).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    bool isBroken = false;

    // Above zero, getNoise reports this loudness (case 16).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    float testLoudness = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    FGameplayTag noiseTag;

    // Higher wins an angle tie (case 2).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    int32 testPriority = 0;

    virtual FGameplayTag getInteractionTag_Implementation() const override;
    virtual bool canInteract_Implementation(AActor* instigator, FGameplayTag& failReason) const override;
    virtual FS_InteractionPrompt getPrompt_Implementation(AActor* instigator) const override;
    virtual void interact_Implementation(AActor* instigator) override;
    virtual bool getNoise_Implementation(FS_InteractionNoise& noise) const override;
    virtual int32 getInteractionPriority_Implementation() const override;
    virtual bool isInteractableBy_Implementation(AActor* instigator) const override;

protected:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UBoxComponent> box;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> cube;
};

// Press, lock None. Prompt, focus choice, request, noise, canInteract refusal.
UCLASS(Blueprintable)
class THEAWAKEN_API ATestPress : public ATestInteractableBase{
    GENERATED_BODY()
public:
    ATestPress();
    // The parameter must be called OutLifetimeProps: the DOREPLIFETIME macros use that exact name (Doc02 20.5).
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Test")
    bool isOn = false;

    virtual E_InteractionKind getInteractionKind_Implementation() const override;
    virtual E_InteractionLockPolicy getLockPolicy_Implementation() const override;
    virtual void interact_Implementation(AActor* instigator) override;
};

// Press, lock Shared. Two players on one object (case 13).
UCLASS(Blueprintable)
class THEAWAKEN_API ATestShared : public ATestInteractableBase{
    GENERATED_BODY()
public:
    ATestShared();

    virtual E_InteractionKind getInteractionKind_Implementation() const override;
    virtual E_InteractionLockPolicy getLockPolicy_Implementation() const override;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UInteractionLockComponent> lock;
};

// Hold, lock Exclusive. Hold start, progress, completion, every interrupt, lock release.
UCLASS(Blueprintable)
class THEAWAKEN_API ATestHold : public ATestInteractableBase, public IBPI_InteractableHoldToActivate{
    GENERATED_BODY()
public:
    ATestHold();

    // TEST value in seconds (B18 section 10: durations are data, not chosen here).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    float holdSeconds = 2.0f;

    // Tick during a hold to prove canActivate is checked again at completion (case: enemy turns aware mid-hold).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test")
    bool blockActivation = false;

    UPROPERTY(BlueprintReadOnly, Category = "Test")
    int32 completions = 0;

    virtual E_InteractionKind getInteractionKind_Implementation() const override;
    virtual E_InteractionLockPolicy getLockPolicy_Implementation() const override;

    virtual float getHoldDuration_Implementation() const override;
    virtual bool canActivate_Implementation(AActor* instigator, FGameplayTag& failReason) const override;
    virtual void onActivationComplete_Implementation(AActor* instigator) override;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UInteractionLockComponent> lock;
};