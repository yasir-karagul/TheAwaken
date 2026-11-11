#pragma once

#include "CoreMinimal.h"
#include "AbilityActionFramework/GA_AwakenBase.h"
#include "GA_Block.generated.h"

// A9. Held ability, no timing window, no Parry. Combat reads Action.Block (isBlocking) at step 3 of its
// damage order and refuses attacks while it is held. Cost fields stay 0 until the designer decides (gap 7).
UCLASS()
class THEAWAKEN_API UGA_Block : public UGA_AwakenBase{
    GENERATED_BODY()
public:
    UGA_Block();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, const FGameplayEventData* triggerEventData) override;

private:
    UFUNCTION()
    void onInputReleased(float timeHeld);
};
