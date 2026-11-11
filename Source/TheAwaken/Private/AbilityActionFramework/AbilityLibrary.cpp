#include "AbilityActionFramework/AbilityLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "StatsFramework/StatsLibrary.h"
#include "AbilityActionFramework/DA_AbilityLoadout.h"
#include "AbilityActionFramework/DA_ActionDefinition.h"
#include "AbilityActionFramework/01Structs/S_AbilityActionFramework.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"
#include "AINPCFramework/DA_NPCProfile.h"
#include "CarryEncumbranceFramework/02Tags/T_CarryEncumbranceFramework.h"
#include "DialogueFramework/02Tags/T_DialogueFramework.h"

namespace{
    // A13 step 3: enemies have no PlayerState, so the carry event is bound on the enemy's own ASC.
    // Carrying cancels a running Dodge or Block. Server only.
    void bindCarryCancel(UAbilitySystemComponent* asc){
        if(asc == nullptr || asc->GetOwner() == nullptr || !asc->GetOwner()->HasAuthority()){
            return;
        }
        TWeakObjectPtr<UAbilitySystemComponent> weakAsc(asc);
        asc->RegisterGameplayTagEvent(T_Carry_State_Active, EGameplayTagEventType::NewOrRemoved).AddWeakLambda(asc, [weakAsc](const FGameplayTag tag, int32 count){
            if(count > 0 && weakAsc.IsValid()){
                FGameplayTagContainer cancelTags;
                cancelTags.AddTag(T_Action_Dodge);
                cancelTags.AddTag(T_Action_Block);
                weakAsc->CancelAbilities(&cancelTags);
            }
        });
    }
}

DEFINE_LOG_CATEGORY(LogAwakenAbility);

FGameplayAbilitySpec* UAbilityLibrary::findSpecByActionTag(UAbilitySystemComponent* asc, const FGameplayTag& actionTag){
    if(asc == nullptr || !actionTag.IsValid()){
        return nullptr;
    }
    for(FGameplayAbilitySpec& spec : asc->GetActivatableAbilities()){
        if(spec.GetDynamicSpecSourceTags().HasTagExact(actionTag)){
            return &spec;
        }
    }
    return nullptr;
}

bool UAbilityLibrary::isActionAllowed(AActor* target, const FGameplayTag& actionTag){
    UAbilitySystemComponent* asc = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(target);
    FGameplayAbilitySpec* spec = findSpecByActionTag(asc, actionTag);
    if(spec == nullptr || spec->Ability == nullptr || asc->AbilityActorInfo.Get() == nullptr){
        return false;
    }
    return spec->Ability->CanActivateAbility(spec->Handle, asc->AbilityActorInfo.Get());
}

bool UAbilityLibrary::isBlocking(AActor* target){
    UAbilitySystemComponent* asc = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(target);
    return asc != nullptr && asc->HasMatchingGameplayTag(T_Action_Block);
}

void UAbilityLibrary::cancelAction(AActor* target, const FGameplayTag& actionTag){
    UAbilitySystemComponent* asc = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(target);
    if(asc == nullptr || !asc->GetOwner()->HasAuthority() || !actionTag.IsValid()){
        return;
    }
    const FGameplayTagContainer tags(actionTag);
    asc->CancelAbilities(&tags);
    UE_LOG(LogAwakenAbility, Log, TEXT("[%s][%s] cancelAction %s"), *UStatsLibrary::getRoleName(asc->GetOwner()), *UStatsLibrary::getPlayerId(asc->GetOwner()), *actionTag.ToString());
}

bool UAbilityLibrary::grantEntry(UAbilitySystemComponent* asc, const FS_AbilityLoadoutEntry& entry){
    if(asc == nullptr || !asc->GetOwner()->HasAuthority() || entry.definition == nullptr || !entry.actionTag.IsValid()){
        return false;
    }
    if(findSpecByActionTag(asc, entry.actionTag) != nullptr){
        return false;
    }
    UClass* abilityClass = entry.definition->abilityClass.LoadSynchronous();
    if(abilityClass == nullptr){
        UE_LOG(LogAwakenAbility, Warning, TEXT("[%s] entry %s has no loadable abilityClass"), *UStatsLibrary::getRoleName(asc->GetOwner()), *entry.actionTag.ToString());
        return false;
    }
    // The definition is the spec's source object (A4 step 2); the action tag is a dynamic source tag (A5 step 1).
    FGameplayAbilitySpec spec(abilityClass, 1, INDEX_NONE, entry.definition);
    spec.GetDynamicSpecSourceTags().AddTag(entry.actionTag);
    asc->GiveAbility(spec);
    UE_LOG(LogAwakenAbility, Log, TEXT("[%s][%s] ability granted: %s"), *UStatsLibrary::getRoleName(asc->GetOwner()), *UStatsLibrary::getPlayerId(asc->GetOwner()), *entry.actionTag.ToString());
    return true;
}

int32 UAbilityLibrary::grantLoadout(UAbilitySystemComponent* asc, const UDA_AbilityLoadout* loadout, const FGameplayTagContainer& deniedActions){
    int32 granted = 0;
    if(loadout == nullptr){
        return granted;
    }
    for(const FS_AbilityLoadoutEntry& entry : loadout->entries){
        if(entry.unlockTag.IsValid() || deniedActions.HasTagExact(entry.actionTag)){
            continue;
        }
        if(grantEntry(asc, entry)){
            granted++;
        }
    }
    return granted;
}

int32 UAbilityLibrary::grantUnlockEntries(UAbilitySystemComponent* asc, const UDA_AbilityLoadout* loadout, const FGameplayTag& unlockTag, const FGameplayTagContainer& deniedActions){
    int32 granted = 0;
    if(loadout == nullptr || !unlockTag.IsValid()){
        return granted;
    }
    for(const FS_AbilityLoadoutEntry& entry : loadout->entries){
        if(entry.unlockTag != unlockTag || deniedActions.HasTagExact(entry.actionTag)){
            continue;
        }
        if(grantEntry(asc, entry)){
            granted++;
        }
    }
    return granted;
}

void UAbilityLibrary::grantNpcLoadout(AActor* npc, const UDA_NPCProfile* profile){
    UAbilitySystemComponent* asc = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(npc);
    if(asc == nullptr || profile == nullptr){
        return;
    }
    FGameplayTagContainer denied;
    if(!profile->canDodge){
        denied.AddTag(T_Action_Dodge);
    }
    if(!profile->canBlock){
        denied.AddTag(T_Action_Block);
    }
    grantLoadout(asc, profile->abilityLoadout, denied);
    bindCarryCancel(asc);
}
