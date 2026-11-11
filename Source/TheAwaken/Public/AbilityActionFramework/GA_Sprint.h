#pragma once

#include "CoreMinimal.h"
#include "AbilityActionFramework/GA_AwakenBase.h"
#include "GameplayEffectTypes.h"
#include "GA_Sprint.generated.h"

// A8. Continuous stamina drain while held. Ends at 0 stamina and cannot restart until stamina is above
// restartThreshold (the latch is cleared by the PlayerState). The movement component reads Action.Sprint
// for the speed; the ability never changes speed itself (A8 step 3).
UCLASS()
class THEAWAKEN_API UGA_Sprint : public UGA_AwakenBase{
    GENERATED_BODY()
public:
    UGA_Sprint();

    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayTagContainer* sourceTags, const FGameplayTagContainer* targetTags, FGameplayTagContainer* optionalRelevantTags) const override;
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, const FGameplayEventData* triggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, bool replicateEndAbility, bool wasCancelled) override;

private:
    UFUNCTION()
    void onInputReleased(float timeHeld);
    UFUNCTION()
    void onStaminaChanged();

    FActiveGameplayEffectHandle drainHandle;
};
