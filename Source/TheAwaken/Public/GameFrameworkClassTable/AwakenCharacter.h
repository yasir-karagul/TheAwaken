// Public/GameFrameworkClassTable/AwakenCharacter.h
#pragma once
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AwakenCharacter.generated.h"

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

    UShelterComponent* getShelter() const{
        return shelter;
    }

private:
    void bindAbilitySystem();

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
