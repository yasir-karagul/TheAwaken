#include "AbilityActionFramework/GA_Block.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"
#include "CarryEncumbranceFramework/02Tags/T_CarryEncumbranceFramework.h"
#include "DialogueFramework/02Tags/T_DialogueFramework.h"

UGA_Block::UGA_Block(){
    SetAssetTags(FGameplayTagContainer(T_Action_Block));
    ActivationBlockedTags.AddTag(T_Carry_State_Active);
    ActivationBlockedTags.AddTag(T_Player_State_Incapacitated);
    ActivationBlockedTags.AddTag(T_Dialogue_State_Active);
    ActivationOwnedTags.AddTag(T_Action_Block);
    holdsCue = true;
}

void UGA_Block::ActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, const FGameplayEventData* triggerEventData){
    Super::ActivateAbility(handle, actorInfo, activationInfo, triggerEventData);
    if(!IsActive()){
        return;
    }
    UAbilityTask_WaitInputRelease* release = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
    release->OnRelease.AddDynamic(this, &UGA_Block::onInputReleased);
    release->ReadyForActivation();
}

void UGA_Block::onInputReleased(float timeHeld){
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
