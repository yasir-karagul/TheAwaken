#include "AbilityActionFramework/GA_Dodge.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilityActionFramework/DA_ActionDefinition.h"
#include "AbilityActionFramework/GE_Cooldown_Dodge.h"
#include "AbilityActionFramework/GE_Cost_Dodge.h"
#include "AbilityActionFramework/GE_Dodge_Invulnerable.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"
#include "CarryEncumbranceFramework/02Tags/T_CarryEncumbranceFramework.h"
#include "DialogueFramework/02Tags/T_DialogueFramework.h"

UGA_Dodge::UGA_Dodge(){
    // 5.5 and later: the ability tag is set through SetAssetTags. Check the 5.8 header for the others.
    SetAssetTags(FGameplayTagContainer(T_Action_Dodge));
    CancelAbilitiesWithTag.AddTag(T_Action_WeaponCharge);
    CancelAbilitiesWithTag.AddTag(T_Action_Reload);
    BlockAbilitiesWithTag.AddTag(T_Action_WeaponCharge);
    BlockAbilitiesWithTag.AddTag(T_Action_Reload);
    ActivationBlockedTags.AddTag(T_Carry_State_Active);
    ActivationBlockedTags.AddTag(T_Player_State_Incapacitated);
    ActivationBlockedTags.AddTag(T_Dialogue_State_Active);
    ActivationOwnedTags.AddTag(T_Action_Dodge);
    CostGameplayEffectClass = UGE_Cost_Dodge::StaticClass();
    CooldownGameplayEffectClass = UGE_Cooldown_Dodge::StaticClass();
    holdsCue = false;
}

void UGA_Dodge::ActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, const FGameplayEventData* triggerEventData){
    Super::ActivateAbility(handle, actorInfo, activationInfo, triggerEventData);
    if(!IsActive()){
        return;
    }
    applyInvulnerability();
    const UDA_ActionDefinition* definition = getDefinition();
    const float seconds = definition != nullptr ? FMath::Max(0.01f, definition->durationSeconds) : 0.3f;
    UAbilityTask_WaitDelay* wait = UAbilityTask_WaitDelay::WaitDelay(this, seconds);
    wait->OnFinish.AddDynamic(this, &UGA_Dodge::onDodgeFinished);
    wait->ReadyForActivation();
}

void UGA_Dodge::onDodgeFinished(){
    endOnAuthority();
}

// Combat resolves damage on the server only, so the invulnerability effect lives on the authority.
void UGA_Dodge::applyInvulnerability(){
    if(!HasAuthority(&CurrentActivationInfo)){
        return;
    }
    const UDA_ActionDefinition* definition = getDefinition();
    if(definition == nullptr || definition->invulnerabilitySeconds <= 0.0f){
        return;
    }
    FGameplayEffectSpecHandle specHandle = MakeOutgoingGameplayEffectSpec(UGE_Dodge_Invulnerable::StaticClass(), 1.0f);
    if(!specHandle.IsValid()){
        return;
    }
    specHandle.Data->SetSetByCallerMagnitude(FName("invulnerabilitySeconds"), definition->invulnerabilitySeconds);
    invulnerabilityHandle = ApplyGameplayEffectSpecToOwner(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, specHandle);
}

void UGA_Dodge::removeInvulnerability(){
    if(invulnerabilityHandle.IsValid()){
        UAbilitySystemComponent* asc = GetAbilitySystemComponentFromActorInfo();
        if(asc != nullptr){
            asc->RemoveActiveGameplayEffect(invulnerabilityHandle);
        }
        invulnerabilityHandle.Invalidate();
    }
}

// On any early end, remove the invulnerability effect (A7 step 4).
void UGA_Dodge::EndAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, bool replicateEndAbility, bool wasCancelled){
    removeInvulnerability();
    Super::EndAbility(handle, actorInfo, activationInfo, replicateEndAbility, wasCancelled);
}
