#pragma once

#include "CoreMinimal.h"
#include "AbilityActionFramework/GA_AwakenBase.h"
#include "GA_Crouch.generated.h"

// A9. No cost, a toggle. Its only job is to hold Action.Crouch; the movement component (Doc R07 3.5)
// reads the tag and owns the capsule and the speed (gap 21). Carrying does not block Crouch.
UCLASS()
class THEAWAKEN_API UGA_Crouch : public UGA_AwakenBase{
    GENERATED_BODY()
public:
    UGA_Crouch();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, const FGameplayEventData* triggerEventData) override;
    // A second press ends the toggle (A5 step 4).
    virtual void InputPressed(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo) override;

private:
    UFUNCTION()
    void onSecondPress(float timeWaited);
};
