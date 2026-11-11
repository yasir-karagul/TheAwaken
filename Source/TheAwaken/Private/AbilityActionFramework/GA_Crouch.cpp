#include "AbilityActionFramework/GA_Crouch.h"
#include "Abilities/Tasks/AbilityTask_WaitInputPress.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"

UGA_Crouch::UGA_Crouch(){
    SetAssetTags(FGameplayTagContainer(T_Action_Crouch));
    ActivationBlockedTags.AddTag(T_Player_State_Incapacitated);
    ActivationOwnedTags.AddTag(T_Action_Crouch);
    holdsCue = false;
}

void UGA_Crouch::ActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, const FGameplayEventData* triggerEventData){
    Super::ActivateAbility(handle, actorInfo, activationInfo, triggerEventData);
    if(!IsActive()){
        return;
    }
    // Belt and braces for the toggle: whichever of InputPressed or this task fires first ends the ability.
    // The initial state is not tested, because the press that started the ability is still down.
    UAbilityTask_WaitInputPress* press = UAbilityTask_WaitInputPress::WaitInputPress(this, false);
    press->OnPress.AddDynamic(this, &UGA_Crouch::onSecondPress);
    press->ReadyForActivation();
}

void UGA_Crouch::InputPressed(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo){
    Super::InputPressed(handle, actorInfo, activationInfo);
    if(IsActive()){
        EndAbility(handle, actorInfo, activationInfo, true, false);
    }
}

void UGA_Crouch::onSecondPress(float timeWaited){
    if(IsActive()){
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}
