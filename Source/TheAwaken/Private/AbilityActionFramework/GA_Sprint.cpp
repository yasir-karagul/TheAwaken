#include "AbilityActionFramework/GA_Sprint.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitAttributeChange.h"
#include "AbilityActionFramework/AbilityLibrary.h"
#include "StatsFramework/StatsLibrary.h"
#include "AbilityActionFramework/GE_Cost_Sprint.h"
#include "AbilityActionFramework/GE_State_SprintExhausted.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"
#include "StatsFramework/AS_CoreVitals.h"
#include "CarryEncumbranceFramework/02Tags/T_CarryEncumbranceFramework.h"
#include "DialogueFramework/02Tags/T_DialogueFramework.h"

UGA_Sprint::UGA_Sprint(){
    SetAssetTags(FGameplayTagContainer(T_Action_Sprint));
    ActivationBlockedTags.AddTag(T_Carry_State_Active);
    ActivationBlockedTags.AddTag(T_Player_State_Incapacitated);
    ActivationBlockedTags.AddTag(T_Dialogue_State_Active);
    ActivationBlockedTags.AddTag(T_Action_State_SprintExhausted);
    ActivationOwnedTags.AddTag(T_Action_Sprint);
    // No cost effect and no cooldown on purpose: the drain is applied by hand (A8 step 1).
    holdsCue = false;
}

bool UGA_Sprint::CanActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayTagContainer* sourceTags, const FGameplayTagContainer* targetTags, FGameplayTagContainer* optionalRelevantTags) const{
    if(!Super::CanActivateAbility(handle, actorInfo, sourceTags, targetTags, optionalRelevantTags)){
        return false;
    }
    const UAS_CoreVitals* vitals = actorInfo->AbilitySystemComponent->GetSet<UAS_CoreVitals>();
    return vitals != nullptr && vitals->getCurrentStamina() > 0.0f;
}

void UGA_Sprint::ActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, const FGameplayEventData* triggerEventData){
    Super::ActivateAbility(handle, actorInfo, activationInfo, triggerEventData);
    if(!IsActive()){
        return;
    }
    // The drain ticks on the server. GAS does not predict periodic effects (verify in 5.8), so the owning
    // client predicts only the start; the stamina value reaches it by replication (S8).
    if(HasAuthority(&CurrentActivationInfo)){
        drainHandle = BP_ApplyGameplayEffectToOwner(UGE_Cost_Sprint::StaticClass(), 1, 1);
    }
    UAbilityTask_WaitInputRelease* release = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
    release->OnRelease.AddDynamic(this, &UGA_Sprint::onInputReleased);
    release->ReadyForActivation();
    UAbilityTask_WaitAttributeChange* wait = UAbilityTask_WaitAttributeChange::WaitForAttributeChange(this, UAS_CoreVitals::currentStaminaAttribute(), FGameplayTag(), FGameplayTag(), false, nullptr);
    wait->OnChange.AddDynamic(this, &UGA_Sprint::onStaminaChanged);
    wait->ReadyForActivation();
}

void UGA_Sprint::onInputReleased(float timeHeld){
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

// At 0 stamina: apply the exhausted latch and end. Server only; the end replicates to the client.
void UGA_Sprint::onStaminaChanged(){
    if(!HasAuthority(&CurrentActivationInfo)){
        return;
    }
    const UAS_CoreVitals* vitals = GetAbilitySystemComponentFromActorInfo()->GetSet<UAS_CoreVitals>();
    if(vitals == nullptr || vitals->getCurrentStamina() > 0.0f){
        return;
    }
    BP_ApplyGameplayEffectToOwner(UGE_State_SprintExhausted::StaticClass(), 1, 1);
    UE_LOG(LogAwakenAbility, Log, TEXT("[%s][%s] sprint exhaustion latch set"), *UStatsLibrary::getRoleName(GetAvatarActorFromActorInfo()), *UStatsLibrary::getPlayerId(GetAvatarActorFromActorInfo()));
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UGA_Sprint::EndAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, bool replicateEndAbility, bool wasCancelled){
    if(drainHandle.IsValid()){
        UAbilitySystemComponent* asc = GetAbilitySystemComponentFromActorInfo();
        if(asc != nullptr){
            asc->RemoveActiveGameplayEffect(drainHandle);
        }
        drainHandle.Invalidate();
    }
    Super::EndAbility(handle, actorInfo, activationInfo, replicateEndAbility, wasCancelled);
}
