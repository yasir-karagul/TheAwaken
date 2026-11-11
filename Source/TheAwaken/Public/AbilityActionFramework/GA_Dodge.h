#pragma once

#include "CoreMinimal.h"
#include "AbilityActionFramework/GA_AwakenBase.h"
#include "GameplayEffectTypes.h"
#include "GA_Dodge.generated.h"

// A7. Stamina cost, short cooldown, a brief invulnerability window against attack damage only (R03 3.9).
// The dash itself is movement: the movement component reads Action.Dodge and moves dashDistance (A7 step 5).
UCLASS()
class THEAWAKEN_API UGA_Dodge : public UGA_AwakenBase{
    GENERATED_BODY()
public:
    UGA_Dodge();

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, const FGameplayEventData* triggerEventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, bool replicateEndAbility, bool wasCancelled) override;

private:
    UFUNCTION()
    void onDodgeFinished();

    void applyInvulnerability();
    void removeInvulnerability();

    FActiveGameplayEffectHandle invulnerabilityHandle;
};
