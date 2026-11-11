#pragma once

#include "CoreMinimal.h"
#include "AbilityActionFramework/GA_AwakenBase.h"
#include "GA_WeaponCharge.generated.h"

// A10. Wraps Combat's beginWeaponCharge() and releaseWeaponCharge(). Holds no charge state of its own.
// Bow and crossbow draw only; a firearm never uses this ability. A cancelled draw spends nothing.
UCLASS()
class THEAWAKEN_API UGA_WeaponCharge : public UGA_AwakenBase{
    GENERATED_BODY()
public:
    UGA_WeaponCharge();

    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayTagContainer* sourceTags, const FGameplayTagContainer* targetTags, FGameplayTagContainer* optionalRelevantTags) const override;
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, const FGameplayEventData* triggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, bool replicateEndAbility, bool wasCancelled) override;

private:
    UFUNCTION()
    void onInputReleased(float timeHeld);
};
