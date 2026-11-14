// Public/GameFrameworkClassTable/AwakenCharacter.h
#pragma once
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AwakenCharacter.generated.h"

class UInputComponent;
class UInputAction;
struct FInputActionValue;
class UHeldItemComponent;
class UCombatComponent;
class UCarryComponent;
class UCraftingComponent;
class UBuildToolComponent;
class UInteractionComponent;
class UDownedComponent;
class UShelterComponent;
class UFirstPersonCameraComponent;

UCLASS()
class THEAWAKEN_API AAwakenCharacter : public ACharacter, public IAbilitySystemInterface{
    GENERATED_BODY()
public:
    AAwakenCharacter(const FObjectInitializer& objectInitializer);

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void PossessedBy(AController* newController) override;
    virtual void OnRep_PlayerState() override;
    virtual void BeginPlay() override;
    // Doc B04 steps 4 and 7: runs on the server and on the owning client whenever the controller changes (possess and release).
    virtual void NotifyControllerChanged() override;
    // Doc B18 guide Step 4 and 9: input binding on the local pawn, and the cleanup when control is lost.
    virtual void SetupPlayerInputComponent(UInputComponent* playerInputComponent) override;
    virtual void UnPossessed() override;

    UShelterComponent* getShelter() const{
        return shelter;
    }

private:
    void bindAbilitySystem();

    // ---- TEMPORARY move and look (2026-11-14) so the game can be play-tested. No framework guide covers them yet; the owner
    // (Camera / Game Framework Class Table) replaces these with the real bindings and deletes this block, the two functions and
    // IA_Move / IA_Look in /Game/Awaken/Input. Set in BP_AwakenCharacter. ----
    UPROPERTY(EditDefaultsOnly, Category = "Awaken|Input|TEMPORARY")
    TObjectPtr<UInputAction> tempMoveAction;

    UPROPERTY(EditDefaultsOnly, Category = "Awaken|Input|TEMPORARY")
    TObjectPtr<UInputAction> tempLookAction;

    void tempMove(const FInputActionValue& value);
    void tempLook(const FInputActionValue& value);

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UHeldItemComponent> heldItem;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCombatComponent> combat;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCarryComponent> carry;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCraftingComponent> crafting;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UBuildToolComponent> buildTool;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UInteractionComponent> interaction;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UDownedComponent> downed;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UShelterComponent> shelter;
};
