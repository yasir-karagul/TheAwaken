#include "AbilityActionFramework/GA_AwakenBase.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "AbilityActionFramework/AbilityLibrary.h"
#include "StatsFramework/StatsLibrary.h"
#include "AbilityActionFramework/DA_ActionDefinition.h"
#include "AbilityActionFramework/01Structs/S_AbilityActionFramework.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"

UGA_AwakenBase::UGA_AwakenBase(){
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

const UDA_ActionDefinition* UGA_AwakenBase::findDefinition(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo) const{
    if(actorInfo == nullptr || actorInfo->AbilitySystemComponent.Get() == nullptr){
        return nullptr;
    }
    const FGameplayAbilitySpec* spec = actorInfo->AbilitySystemComponent.Get()->FindAbilitySpecFromHandle(handle);
    if(spec == nullptr){
        return nullptr;
    }
    return Cast<UDA_ActionDefinition>(spec->SourceObject.Get());
}

const UDA_ActionDefinition* UGA_AwakenBase::getDefinition() const{
    return findDefinition(CurrentSpecHandle, CurrentActorInfo);
}

FGameplayEffectContextHandle UGA_AwakenBase::MakeEffectContext(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo) const{
    FGameplayEffectContextHandle context = Super::MakeEffectContext(handle, actorInfo);
    const UDA_ActionDefinition* definition = findDefinition(handle, actorInfo);
    if(definition != nullptr){
        context.AddSourceObject(definition);
    }
    return context;
}

void UGA_AwakenBase::ApplyCooldown(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo) const{
    if(CooldownGameplayEffectClass == nullptr){
        return;
    }
    const UDA_ActionDefinition* definition = findDefinition(handle, actorInfo);
    if(definition == nullptr || definition->cooldownSeconds <= 0.0f){
        return;
    }
    FGameplayEffectSpecHandle specHandle = MakeOutgoingGameplayEffectSpec(handle, actorInfo, activationInfo, CooldownGameplayEffectClass, GetAbilityLevel(handle, actorInfo));
    if(!specHandle.IsValid()){
        return;
    }
    specHandle.Data->SetSetByCallerMagnitude(FName("cooldownSeconds"), definition->cooldownSeconds);
    ApplyGameplayEffectSpecToOwner(handle, actorInfo, activationInfo, specHandle);
}

void UGA_AwakenBase::sendActionMessage(const FGameplayTag& channel, bool cancelled){
    const UDA_ActionDefinition* definition = getDefinition();
    FS_ActionMessage message;
    message.actionTag = definition != nullptr ? definition->actionTag : FGameplayTag();
    message.instigator = GetAvatarActorFromActorInfo();
    message.cancelled = cancelled;
    AActor* avatar = GetAvatarActorFromActorInfo();
    if(avatar == nullptr){
        return;
    }
    // Not UGameplayMessageSubsystem::Get(avatar): it asserts when the game instance is already gone, which happens
    // when a held ability ends while the play session stops or the world is torn down.
    const UWorld* world = avatar->GetWorld();
    UGameInstance* gameInstance = world != nullptr ? world->GetGameInstance() : nullptr;
    UGameplayMessageSubsystem* messages = gameInstance != nullptr ? gameInstance->GetSubsystem<UGameplayMessageSubsystem>() : nullptr;
    if(messages != nullptr){
        messages->BroadcastMessage(channel, message);
    }
}

void UGA_AwakenBase::triggerStartCue(){
    const UDA_ActionDefinition* definition = getDefinition();
    // Sprint and Crouch have no cue (A12 step 1): skip when empty.
    if(definition == nullptr || !definition->cueTag.IsValid()){
        return;
    }
    FGameplayEffectContextHandle context = MakeEffectContext(CurrentSpecHandle, CurrentActorInfo);
    if(holdsCue){
        // Removed automatically when the ability ends, so a late joiner sees the right state.
        K2_AddGameplayCue(definition->cueTag, context, true);
    }else{
        K2_ExecuteGameplayCue(definition->cueTag, context);
    }
}

void UGA_AwakenBase::ActivateAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, const FGameplayEventData* triggerEventData){
    if(!CommitAbility(handle, actorInfo, activationInfo)){
        EndAbility(handle, actorInfo, activationInfo, true, true);
        return;
    }
    startedSent = true;
    sendActionMessage(T_Action_Started, false);
    triggerStartCue();
    UE_LOG(LogAwakenAbility, Log, TEXT("[%s][%s] ability activated: %s"), *UStatsLibrary::getRoleName(GetAvatarActorFromActorInfo()), *UStatsLibrary::getPlayerId(GetAvatarActorFromActorInfo()), *GetName());
}

void UGA_AwakenBase::EndAbility(const FGameplayAbilitySpecHandle handle, const FGameplayAbilityActorInfo* actorInfo, const FGameplayAbilityActivationInfo activationInfo, bool replicateEndAbility, bool wasCancelled){
    if(startedSent){
        startedSent = false;
        sendActionMessage(T_Action_Ended, wasCancelled);
        UE_LOG(LogAwakenAbility, Log, TEXT("[%s][%s] ability ended: %s, cancelled %s"), *UStatsLibrary::getRoleName(GetAvatarActorFromActorInfo()), *UStatsLibrary::getPlayerId(GetAvatarActorFromActorInfo()), *GetName(), wasCancelled ? TEXT("yes") : TEXT("no"));
    }
    Super::EndAbility(handle, actorInfo, activationInfo, replicateEndAbility, wasCancelled);
}

void UGA_AwakenBase::endOnAuthority(){
    if(HasAuthority(&CurrentActivationInfo)){
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
    }
}