#include "AbilityActionFramework/TimedEffectLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "AbilityActionFramework/AbilityLibrary.h"
#include "StatsFramework/StatsLibrary.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"

TArray<FS_TimedEffectSaveRecord> UTimedEffectLibrary::getSnapshotForSave(AActor* target){
    TArray<FS_TimedEffectSaveRecord> records;
    UAbilitySystemComponent* asc = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(target);
    if(asc == nullptr || asc->GetWorld() == nullptr){
        return records;
    }
    const FGameplayEffectQuery query = FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(FGameplayTagContainer(T_Effect_Saved));
    for(const FActiveGameplayEffectHandle& handle : asc->GetActiveEffects(query)){
        const FActiveGameplayEffect* effect = asc->GetActiveGameplayEffect(handle);
        if(effect == nullptr || effect->Spec.Def == nullptr){
            continue;
        }
        FS_TimedEffectSaveRecord record;
        record.effectId = effect->Spec.Def->GetClass()->GetPathName();
        record.timeLeft = effect->GetTimeRemaining(asc->GetWorld()->GetTimeSeconds());
        records.Add(record);
        UE_LOG(LogAwakenAbility, Log, TEXT("[%s][%s] timed effect saved: %s, %.2f s left"), *UStatsLibrary::getRoleName(asc->GetOwner()), *UStatsLibrary::getPlayerId(asc->GetOwner()), *record.effectId, record.timeLeft);
    }
    return records;
}

void UTimedEffectLibrary::restoreFromSnapshot(AActor* target, const TArray<FS_TimedEffectSaveRecord>& records){
    UAbilitySystemComponent* asc = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(target);
    if(asc == nullptr || !asc->GetOwner()->HasAuthority()){
        return;
    }
    for(const FS_TimedEffectSaveRecord& record : records){
        UClass* effectClass = StaticLoadClass(UGameplayEffect::StaticClass(), nullptr, *record.effectId);
        if(effectClass == nullptr){
            UE_LOG(LogAwakenAbility, Warning, TEXT("[%s] timed effect %s could not be loaded"), *UStatsLibrary::getRoleName(asc->GetOwner()), *record.effectId);
            continue;
        }
        if(record.timeLeft <= 0.0f){
            continue;
        }
        // ActiveGameplayEffects is protected in 5.8, so the start time cannot be shifted after the fact.
        // Instead the spec is built first and its duration is locked to timeLeft before it is applied.
        const FGameplayEffectSpecHandle specHandle = asc->MakeOutgoingSpec(TSubclassOf<UGameplayEffect>(effectClass), 1.0f, asc->MakeEffectContext());
        if(!specHandle.IsValid()){
            continue;
        }
        specHandle.Data->SetDuration(record.timeLeft, true);
        const FActiveGameplayEffectHandle handle = asc->ApplyGameplayEffectSpecToSelf(*specHandle.Data.Get());
        if(!handle.IsValid()){
            continue;
        }
        UE_LOG(LogAwakenAbility, Log, TEXT("[%s][%s] timed effect restored: %s, %.2f s left"), *UStatsLibrary::getRoleName(asc->GetOwner()), *UStatsLibrary::getPlayerId(asc->GetOwner()), *record.effectId, record.timeLeft);
    }
}

void UTimedEffectLibrary::advanceTo(AActor* target, float newGameTime){
    // Intentionally leaves the remaining time of every timed effect unchanged (A11 step 5).
    UE_LOG(LogAwakenAbility, Verbose, TEXT("[%s] advanceTo %.2f: timed effects frozen"), *UStatsLibrary::getRoleName(target), newGameTime);
}

void UTimedEffectLibrary::clearTimedEffects(AActor* target){
    UAbilitySystemComponent* asc = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(target);
    if(asc == nullptr || !asc->GetOwner()->HasAuthority()){
        return;
    }
    const FGameplayTag tierRoot = FGameplayTag::RequestGameplayTag(FName("Survival.Tier.Debuff"), false);
    TArray<FActiveGameplayEffectHandle> toRemove;
    for(const FActiveGameplayEffectHandle& handle : asc->GetActiveEffects(FGameplayEffectQuery())){
        const FActiveGameplayEffect* effect = asc->GetActiveGameplayEffect(handle);
        if(effect == nullptr || effect->Spec.Def == nullptr){
            continue;
        }
        if(effect->Spec.Def->DurationPolicy != EGameplayEffectDurationType::HasDuration){
            continue;
        }
        const FGameplayTagContainer& granted = effect->Spec.Def->GetGrantedTags();
        const bool isTierDebuff = tierRoot.IsValid() && granted.HasTag(tierRoot);
        const bool isActionState = granted.HasTag(T_Action_State) || granted.HasTag(T_Action_Cooldown);
        if(!isTierDebuff && !isActionState){
            toRemove.Add(handle);
        }
    }
    for(const FActiveGameplayEffectHandle& handle : toRemove){
        asc->RemoveActiveGameplayEffect(handle);
    }
    UE_LOG(LogAwakenAbility, Log, TEXT("[%s][%s] timed effects cleared: %d removed"), *UStatsLibrary::getRoleName(asc->GetOwner()), *UStatsLibrary::getPlayerId(asc->GetOwner()), toRemove.Num());
}