#include "AbilityActionFramework/GA_Reload.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "AbilityActionFramework/DA_ActionDefinition.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"
#include "CarryEncumbranceFramework/02Tags/T_CarryEncumbranceFramework.h"
#include "DialogueFramework/02Tags/T_DialogueFramework.h"
#include "ItemInventoryFramework/HeldItemComponent.h"

UGA_Reload::UGA_Reload(){
    SetAssetTags(FGameplayTagContainer(T_Action_Reload));
    ActivationBlockedTags.AddTag(T_Action_Dodge);
    ActivationBlockedTags.AddTag(T_Carry_State_Active);
    ActivationBlockedTags.AddTag(T_Player_State_Incapacitated);
    ActivationBlockedTags.AddTag(T_Dialogue_State_Active);
    ActivationOwnedTags.AddTag(T_Action_Reload);
    holdsCue = true;
}

void UGA_Reload::ActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, const FGameplayEventData* triggerEventData){
    Super::ActivateAbility(handle, actorInfo, activationInfo, triggerEventData);
    if(!IsActive()){
        return;
    }
    const UDA_ActionDefinition* definition = getDefinition();
    const float seconds = definition != nullptr ? FMath::Max(0.01f, definition->durationSeconds) : 1.0f;
    UAbilityTask_WaitDelay* wait = UAbilityTask_WaitDelay::WaitDelay(this, seconds);
    wait->OnFinish.AddDynamic(this, &UGA_Reload::onReloadFinished);
    wait->ReadyForActivation();
}

// Loading happens on the authority only, then the end replicates to the owning client.
void UGA_Reload::onReloadFinished(){
    if(!HasAuthority(&CurrentActivationInfo)){
        return;
    }
    AActor* avatar = GetAvatarActorFromActorInfo();
    UHeldItemComponent* held = avatar != nullptr ? avatar->FindComponentByClass<UHeldItemComponent>() : nullptr;
    if(held != nullptr){
        held->applyReload(avatar, held->getHeldItem());
    }
    endOnAuthority();
}
