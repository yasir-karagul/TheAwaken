#include "AbilitySystemComponent.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "StatsFramework/StatsLibrary.h"
#include "StatsFramework/AS_CoreVitals.h"
#include "StatsFramework/VitalsRegenComponent.h"
#include "StatsFramework/02Tags/T_StatsFramework.h"
#include "GameFrameworkClassTable/AwakenPlayerState.h"

#if !UE_BUILD_SHIPPING

// Doc B22 3.2. The existing B22 commands (SetMeter, Heal, Kill, God) are owned by B22.
// Awaken.Player.God must add or remove the loose tag Stats.Dev.God on the target's ASC.
namespace{
    AAwakenPlayerState* resolveStatsTarget(UWorld* world, const TArray<FString>& args){
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
}

static FAutoConsoleCommandWithWorldAndArgs setStaminaCommand(TEXT("Awaken.Player.SetStamina"), TEXT("Awaken.Player.SetStamina <value> [player=<index>]. Skips the multipliers."), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
    AAwakenPlayerState* playerState = resolveStatsTarget(world, args);
    if(playerState == nullptr || world->GetNetMode() == NM_Client){
        UE_LOG(LogAwakenStats, Warning, TEXT("Awaken.Player.SetStamina runs on the host with a target"));
        return;
    }
    float value = 0.0f;
    for(const FString& arg : args){
        if(!arg.Contains(TEXT("="))){
            value = FCString::Atof(*arg);
        }
    }
    UStatsLibrary::setStamina(playerState, value);
}));

static FAutoConsoleCommandWithWorldAndArgs dumpCommand(TEXT("Awaken.Stats.Dump"), TEXT("Awaken.Stats.Dump [player=<index>]. Local."), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
    AAwakenPlayerState* playerState = resolveStatsTarget(world, args);
    if(playerState == nullptr){
        return;
    }
    const UAS_CoreVitals* vitals = UStatsLibrary::getVitals(playerState);
    UVitalsRegenComponent* regen = playerState->FindComponentByClass<UVitalsRegenComponent>();
    if(vitals == nullptr || regen == nullptr){
        return;
    }
    FName capSource;
    const float cap = regen->getRegenCapWithSource(capSource);
    const bool isServer = world->GetNetMode() != NM_Client;
    const FString regenState = isServer ? FString::Printf(TEXT("health %s, stamina %s"), regen->isHealthRegenRunning() ? TEXT("running") : TEXT("stopped"), regen->isStaminaRegenRunning() ? TEXT("running") : TEXT("stopped")) : FString(TEXT("server only"));
    UE_LOG(LogAwakenStats, Log, TEXT("[%s][%s] health %.2f/%.2f, stamina %.2f/%.2f, drain x%.2f, regen x%.2f, armor %.2f, regen cap %.1f%% (%s), low-health tag %s, regeneration: %s"), *UStatsLibrary::getRoleName(world), *UStatsLibrary::getPlayerId(playerState), vitals->getCurrentHP(), vitals->getMaxHP(), vitals->getCurrentStamina(), vitals->getMaxStamina(), vitals->getStaminaDrainMultiplier(), vitals->getStaminaRegenMultiplier(), vitals->getAggregatedArmor(), cap, *capSource.ToString(), playerState->GetAbilitySystemComponent()->HasMatchingGameplayTag(T_Stats_State_LowHealth) ? TEXT("set") : TEXT("not set"), *regenState);
}));

#endif
