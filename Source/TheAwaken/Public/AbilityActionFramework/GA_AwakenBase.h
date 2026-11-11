#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GA_AwakenBase.generated.h"

class UDA_ActionDefinition;

// A4. Parent of the six actions. LocalPredicted, one instance per actor. Every number comes from the
// UDA_ActionDefinition that was the source object of the granted spec (A5).
UCLASS(Abstract)
class THEAWAKEN_API UGA_AwakenBase : public UGameplayAbility{
    GENERATED_BODY()
public:
    UGA_AwakenBase();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, const FGameplayEventData* triggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, bool replicateEndAbility, bool wasCancelled) override;
    // Adds the definition as the context's source object so cost and cooldown calculations can read it,
    // even when the check runs on the class default object (A4 step 2).
    virtual FGameplayEffectContextHandle MakeEffectContext(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo) const override;
    // A7 step 2: builds the cooldown spec and sets cooldownSeconds from the definition.
    virtual void ApplyCooldown(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo) const override;

    // Instance only.
    const UDA_ActionDefinition* getDefinition() const;
    // Works on the class default object too.
    const UDA_ActionDefinition* findDefinition(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo) const;

protected:
    // Held actions (Block, Weapon Charge, Reload) add their cue and remove it on end.
    // One-shot actions (Dodge) execute it (A12 step 2).
    bool holdsCue = false;

    void sendActionMessage(const FGameplayTag& channel, bool cancelled);
    void triggerStartCue();
    // Timed abilities end on the authority only; the end replicates to the owning client. This stops a
    // predicted client from ending the server's ability before the server finished its work.
    void endOnAuthority();

private:
    bool startedSent = false;
};
