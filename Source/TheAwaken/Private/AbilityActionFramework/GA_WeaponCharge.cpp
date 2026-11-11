#include "AbilityActionFramework/GA_WeaponCharge.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"
#include "CombatFramework/CombatComponent.h"

namespace{
    UCombatComponent* findCombat(AActor* avatar){
        return avatar != nullptr ? avatar->FindComponentByClass<UCombatComponent>() : nullptr;
    }
}

UGA_WeaponCharge::UGA_WeaponCharge(){
    SetAssetTags(FGameplayTagContainer(T_Action_WeaponCharge));
    ActivationOwnedTags.AddTag(T_Action_WeaponCharge);
    // No tag-blocked list: Combat's isAttackAllowed() already reads carry, incapacitated, dialogue,
    // mid-dodge and Block (R03 3.6). The Dodge block is set on GA_Dodge (A6).
    holdsCue = true;
}

bool UGA_WeaponCharge::CanActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayTagContainer* sourceTags, const FGameplayTagContainer* targetTags, FGameplayTagContainer* optionalRelevantTags) const{
    if(!Super::CanActivateAbility(handle, actorInfo, sourceTags, targetTags, optionalRelevantTags)){
        return false;
    }
    AActor* avatar = actorInfo != nullptr ? actorInfo->AvatarActor.Get() : nullptr;
    const UCombatComponent* combat = findCombat(avatar);
    return combat != nullptr && combat->isAttackAllowed();
}

void UGA_WeaponCharge::ActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, const FGameplayEventData* triggerEventData){
    Super::ActivateAbility(handle, actorInfo, activationInfo, triggerEventData);
    if(!IsActive()){
        return;
    }
    UCombatComponent* combat = findCombat(GetAvatarActorFromActorInfo());
    if(combat != nullptr){
        combat->beginWeaponCharge();
    }
    UAbilityTask_WaitInputRelease* release = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
    release->OnRelease.AddDynamic(this, &UGA_WeaponCharge::onInputReleased);
    release->ReadyForActivation();
}

// On release the shot resolves on the server only (R03 3.4). Combat decides what runs where.
void UGA_WeaponCharge::onInputReleased(float timeHeld){
    UCombatComponent* combat = findCombat(GetAvatarActorFromActorInfo());
    if(combat != nullptr){
        combat->releaseWeaponCharge();
    }
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// A cancelled draw (a Dodge, a tag arrives) does NOT call releaseWeaponCharge() and spends nothing.
void UGA_WeaponCharge::EndAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, bool replicateEndAbility, bool wasCancelled){
    if(wasCancelled){
        UCombatComponent* combat = findCombat(GetAvatarActorFromActorInfo());
        if(combat != nullptr){
            combat->cancelWeaponCharge();
        }
    }
    Super::EndAbility(handle, actorInfo, activationInfo, replicateEndAbility, wasCancelled);
}
