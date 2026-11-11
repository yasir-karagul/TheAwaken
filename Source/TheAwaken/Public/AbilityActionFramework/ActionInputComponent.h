#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ActionInputComponent.generated.h"

class UDA_ActionInputMap;
class UInputComponent;
class UAbilitySystemComponent;

// A5 steps 2 and 3. Lives on the PlayerController (it owns the input component). Binds each input action
// of DA_ActionInputMap to an ability through its Action tag, so a rebind or a gamepad is a data change.
// Call bindToInputComponent(InputComponent) from the controller's SetupInputComponent.
// A firearm shot is not bound here; it goes to requestFireShot in Combat (A5 step 5).
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API UActionInputComponent : public UActorComponent{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<UDA_ActionInputMap> inputMap;

    void bindToInputComponent(UInputComponent* playerInputComponent);

private:
    UAbilitySystemComponent* getAbilitySystem() const;
    void onActionPressed(FGameplayTag actionTag);
    void onActionReleased(FGameplayTag actionTag);
};
