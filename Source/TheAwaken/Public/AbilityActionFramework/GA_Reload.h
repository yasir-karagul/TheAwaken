#pragma once

#include "CoreMinimal.h"
#include "AbilityActionFramework/GA_AwakenBase.h"
#include "GA_Reload.generated.h"

// A10. Crossbow and firearm. Runs for durationSeconds, then calls Doc R01's applyReload on the server.
// A cancel (Dodge, weapon swap, a gating tag) loads nothing. Sprinting does not cancel it.
UCLASS()
class THEAWAKEN_API UGA_Reload : public UGA_AwakenBase{
    GENERATED_BODY()
public:
    UGA_Reload();
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, const FGameplayEventData* triggerEventData) override;

private:
    UFUNCTION()
    void onReloadFinished();
};
