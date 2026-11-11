#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "AbilityActionFramework/AbilityLibrary.h"
#include "StatsFramework/StatsLibrary.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"
#include "GameFrameworkClassTable/AwakenPlayerState.h"

#if !UE_BUILD_SHIPPING

// Doc B22 3.2 command names. State changes run on the host, take an optional player=<index> argument,
// and write one LogAwakenAbility line with the result.
namespace{
    AAwakenPlayerState* resolveTarget(UWorld* world, const TArray<FString>& args){
        if(world == nullptr){
            return nullptr;
        }
        int32 playerIndex = 0;
        for(const FString& arg : args){
            FString value;
            if(arg.Split(TEXT("player="), nullptr, &value)){
                playerIndex = FCString::Atoi(*value);
            }
        }
        APlayerController* controller = UGameplayStatics::GetPlayerController(world, playerIndex);
        return controller != nullptr ? controller->GetPlayerState<AAwakenPlayerState>() : nullptr;
    }

    bool resolveTag(const TArray<FString>& args, FGameplayTag& tag){
        for(const FString& arg : args){
            if(!arg.Contains(TEXT("="))){
                tag = FGameplayTag::RequestGameplayTag(FName(*arg), false);
                return tag.IsValid();
            }
        }
        return false;
    }

    bool hostOnly(UWorld* world){
        if(world == nullptr || world->GetNetMode() == NM_Client){
            UE_LOG(LogAwakenAbility, Warning, TEXT("[Client] this command runs on the host only"));
            return false;
        }
        return true;
    }
}

static FAutoConsoleCommandWithWorldAndArgs grantCommand(TEXT("Awaken.Action.Grant"), TEXT("Awaken.Action.Grant <actionTag> [player=<index>]"), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
    FGameplayTag tag;
    AAwakenPlayerState* playerState = resolveTarget(world, args);
    if(!hostOnly(world) || playerState == nullptr || !resolveTag(args, tag)){
        return;
    }
    const bool granted = playerState->grantAction(tag);
    UE_LOG(LogAwakenAbility, Log, TEXT("[%s] Awaken.Action.Grant %s: %s"), *UStatsLibrary::getRoleName(world), *tag.ToString(), granted ? TEXT("granted") : TEXT("not granted"));
}));

static FAutoConsoleCommandWithWorldAndArgs activateCommand(TEXT("Awaken.Action.Activate"), TEXT("Awaken.Action.Activate <actionTag> [player=<index>]. Held abilities stay active until Awaken.Action.Cancel."), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
    FGameplayTag tag;
    AAwakenPlayerState* playerState = resolveTarget(world, args);
    if(!hostOnly(world) || playerState == nullptr || !resolveTag(args, tag)){
        return;
    }
    UAbilitySystemComponent* asc = playerState->GetAbilitySystemComponent();
    FGameplayAbilitySpec* spec = UAbilityLibrary::findSpecByActionTag(asc, tag);
    if(spec == nullptr){
        UE_LOG(LogAwakenAbility, Log, TEXT("[%s] Awaken.Action.Activate %s: not granted"), *UStatsLibrary::getRoleName(world), *tag.ToString());
        return;
    }
    // A command cannot send the input release, so mark the input as pressed like a real press does.
    asc->AbilitySpecInputPressed(*spec);
    const bool activated = asc->TryActivateAbility(spec->Handle);
    // A refusal is also logged with its reason tag by the failure callback of the PlayerState.
    UE_LOG(LogAwakenAbility, Log, TEXT("[%s] Awaken.Action.Activate %s: %s"), *UStatsLibrary::getRoleName(world), *tag.ToString(), activated ? TEXT("activated") : TEXT("refused"));
}));

static FAutoConsoleCommandWithWorldAndArgs cancelCommand(TEXT("Awaken.Action.Cancel"), TEXT("Awaken.Action.Cancel <actionTag> [player=<index>]"), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
    FGameplayTag tag;
    AAwakenPlayerState* playerState = resolveTarget(world, args);
    if(!hostOnly(world) || playerState == nullptr || !resolveTag(args, tag)){
        return;
    }
    UAbilityLibrary::cancelAction(playerState, tag);
}));

static FAutoConsoleCommandWithWorldAndArgs listCommand(TEXT("Awaken.Action.List"), TEXT("Awaken.Action.List [player=<index>]. Local: granted abilities, active Action tags, cooldown time left."), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
    AAwakenPlayerState* playerState = resolveTarget(world, args);
    if(playerState == nullptr){
        return;
    }
    UAbilitySystemComponent* asc = playerState->GetAbilitySystemComponent();
    for(const FGameplayAbilitySpec& spec : asc->GetActivatableAbilities()){
        UE_LOG(LogAwakenAbility, Log, TEXT("[%s] granted: %s, active %s"), *UStatsLibrary::getRoleName(world), *spec.GetDynamicSpecSourceTags().ToStringSimple(), spec.IsActive() ? TEXT("yes") : TEXT("no"));
    }
    FGameplayTagContainer owned;
    asc->GetOwnedGameplayTags(owned);
    const FGameplayTag actionRoot = FGameplayTag::RequestGameplayTag(FName("Action"), false);
    FString actionTags;
    for(const FGameplayTag& tag : owned){
        if(tag.MatchesTag(actionRoot)){
            actionTags += tag.ToString() + TEXT(" ");
        }
    }
    const FGameplayTagContainer cooldownTags(T_Action_Cooldown.GetTag());
    const TArray<float> cooldowns = asc->GetActiveEffectsTimeRemaining(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(cooldownTags));
    float cooldownLeft = 0.0f;
    for(const float value : cooldowns){
        cooldownLeft = FMath::Max(cooldownLeft, value);
    }
    UE_LOG(LogAwakenAbility, Log, TEXT("[%s] Action tags: %s| cooldown left %.2f s"), *UStatsLibrary::getRoleName(world), *actionTags, cooldownLeft);
}));

#endif
