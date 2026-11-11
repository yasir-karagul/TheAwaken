// Public/CombatFramework/CombatComponent.h (TEMPORARY)
#pragma once
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "CombatComponent.generated.h"

// TEMPORARY until Doc R03 is built. The functions below are the ones the Stats and Ability & Action frameworks call.
// Deviation from the guide: onHealthDepleted also gets the effect's asset tags, because the DamageType.* tag
// lives on the effect spec and not on the effect context. Keep the one that R03 ends up with.
UCLASS(ClassGroup=(Awaken), meta=(BlueprintSpawnableComponent))
class THEAWAKEN_API UCombatComponent : public UActorComponent{
    GENERATED_BODY()
public:
    UCombatComponent();

    virtual void onHealthDepleted(const FGameplayEffectContextHandle& context, const FGameplayTagContainer& damageTags);
    virtual bool isAttackAllowed() const;
    virtual void beginWeaponCharge();
    virtual void releaseWeaponCharge();
    // PROPOSED request to R03 (Ability guide A10 step 5): clears the charge state of a cancelled draw.
    virtual void cancelWeaponCharge();
};
