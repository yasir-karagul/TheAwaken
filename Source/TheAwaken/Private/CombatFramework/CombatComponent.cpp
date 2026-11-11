// Private/CombatFramework/CombatComponent.cpp (TEMPORARY)
#include "CombatFramework/CombatComponent.h"
#include "AbilityActionFramework/AbilityLibrary.h"
#include "StatsFramework/StatsLibrary.h"

UCombatComponent::UCombatComponent(){
    PrimaryComponentTick.bCanEverTick = false;
}

void UCombatComponent::onHealthDepleted(const FGameplayEffectContextHandle& context, const FGameplayTagContainer& damageTags){
    UE_LOG(LogAwakenStats, Log, TEXT("[%s] TEMPORARY Combat.onHealthDepleted, damage tags: %s"), *UStatsLibrary::getRoleName(this), *damageTags.ToStringSimple());
}

bool UCombatComponent::isAttackAllowed() const{
    return true;
}

void UCombatComponent::beginWeaponCharge(){
    UE_LOG(LogAwakenAbility, Log, TEXT("[%s] TEMPORARY Combat.beginWeaponCharge"), *UStatsLibrary::getRoleName(this));
}

void UCombatComponent::releaseWeaponCharge(){
    UE_LOG(LogAwakenAbility, Log, TEXT("[%s] TEMPORARY Combat.releaseWeaponCharge"), *UStatsLibrary::getRoleName(this));
}

void UCombatComponent::cancelWeaponCharge(){
    UE_LOG(LogAwakenAbility, Log, TEXT("[%s] TEMPORARY Combat.cancelWeaponCharge"), *UStatsLibrary::getRoleName(this));
}
