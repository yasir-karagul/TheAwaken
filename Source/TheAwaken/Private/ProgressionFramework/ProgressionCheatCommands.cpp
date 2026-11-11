// Non-shipping dev commands of the Progression guide (section "Dev Tools and Testing").
// NOT in the guide's code: the guide describes these four commands in a table only. Written from that table, in the pattern of
// TimeCheatCommands.cpp: they register straight with the console and use only the component's public functions, so nothing else changes.
// When Doc B22's cheat manager exists, move the registration there. Delete this file to remove the commands.
// B22 3.2: a command runs on the host, goes through the owner's public functions, logs one line with its result, and never saves.
#include "ProgressionFramework/ProgressionComponent.h"

#if !UE_BUILD_SHIPPING

#include "ProgressionFramework/02Tags/T_ProgressionFramework.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/IConsoleManager.h"

namespace{
    UProgressionComponent* progressionOf(UWorld* world){
        const AGameStateBase* gameState = world != nullptr ? world->GetGameState() : nullptr;
        return gameState != nullptr ? gameState->FindComponentByClass<UProgressionComponent>() : nullptr;
    }

    const TCHAR* roleOf(const UWorld* world){
        const ENetMode netMode = world->GetNetMode();
        if(netMode == NM_Standalone){
            return TEXT("Offline");
        }
        if(netMode == NM_Client){
            return TEXT("Client");
        }
        return TEXT("Host");
    }

    // For the commands that change the set. A client does nothing until B22's cheat manager forwards the command to the host.
    UProgressionComponent* hostProgressionOf(UWorld* world, const TCHAR* command){
        UProgressionComponent* progression = progressionOf(world);
        if(progression == nullptr){
            UE_LOG(LogAwakenProgression, Warning, TEXT("%s: there is no Progression component (no GameState yet)."), command);
            return nullptr;
        }
        if(world->GetNetMode() == NM_Client){
            UE_LOG(LogAwakenProgression, Warning, TEXT("[Client] %s ignored: run it on the host."), command);
            return nullptr;
        }
        return progression;
    }

    // "granted", or the Progression.FailReason.* tag of the refusal.
    FString outcomeOf(const FS_ProgressionResult& result){
        return result.success ? FString(TEXT("granted")) : result.failReason.ToString();
    }

    bool isAlreadyUnlocked(const FS_ProgressionResult& result){
        return result.failReason.MatchesTagExact(T_Progression_FailReason_AlreadyUnlocked.GetTag());
    }

    FAutoConsoleCommandWithWorldAndArgs cmdUnlockGrant(TEXT("Awaken.Unlock.Grant"), TEXT("Awaken.Unlock.Grant <tag>. Host. Calls grantUnlock for one tag and logs the result, with the reason tag when it was refused."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            if(args.Num() < 1){
                UE_LOG(LogAwakenProgression, Warning, TEXT("Usage: Awaken.Unlock.Grant <tag>"));
                return;
            }
            UProgressionComponent* progression = hostProgressionOf(world, TEXT("Awaken.Unlock.Grant"));
            if(progression == nullptr){
                return;
            }
            // A name that is not a tag gives an empty tag on purpose: grantUnlock refuses it with InvalidTag, one of the cases this command exists to try.
            const FGameplayTag tag = FGameplayTag::RequestGameplayTag(FName(*args[0]), false);
            const FS_ProgressionResult result = progression->grantUnlock(tag);
            UE_LOG(LogAwakenProgression, Log, TEXT("[%s] Awaken.Unlock.Grant '%s': %s"), roleOf(world), *args[0], *outcomeOf(result));
        }));

    FAutoConsoleCommandWithWorldAndArgs cmdUnlockAll(TEXT("Awaken.Unlock.All"), TEXT("Awaken.Unlock.All. Host. Calls grantUnlock for every Blueprint, Recipe and Ability leaf tag. These grants announce. Safe to repeat."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            UProgressionComponent* progression = hostProgressionOf(world, TEXT("Awaken.Unlock.All"));
            if(progression == nullptr){
                return;
            }
            const FGameplayTagContainer everything = progression->getEveryUnlockTag();
            int32 granted = 0;
            for(const FGameplayTag& tag : everything){
                const FS_ProgressionResult result = progression->grantUnlock(tag);
                if(result.success){
                    granted++;
                }else if(!isAlreadyUnlocked(result)){
                    UE_LOG(LogAwakenProgression, Warning, TEXT("[%s] Awaken.Unlock.All: '%s' refused (%s)."), roleOf(world), *tag.ToString(), *outcomeOf(result));
                }
            }
            UE_LOG(LogAwakenProgression, Log, TEXT("[%s] Awaken.Unlock.All: %d granted, %d tags in the list."), roleOf(world), granted, everything.Num());
        }));

    FAutoConsoleCommandWithWorldAndArgs cmdStorySetPart(TEXT("Awaken.Story.SetPart"), TEXT("Awaken.Story.SetPart N. Host. Grants Progression.Story.Part1 to PartN in order and stops at the first refusal. Does not skip a day gate: use Awaken.Time.SetDay first."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            if(args.Num() < 1){
                UE_LOG(LogAwakenProgression, Warning, TEXT("Usage: Awaken.Story.SetPart N"));
                return;
            }
            UProgressionComponent* progression = hostProgressionOf(world, TEXT("Awaken.Story.SetPart"));
            if(progression == nullptr){
                return;
            }
            const int32 lastPart = FCString::Atoi(*args[0]);
            for(int32 part = 1; part <= lastPart; part++){
                const FString partName = FString::Printf(TEXT("Progression.Story.Part%d"), part);
                const FGameplayTag tag = FGameplayTag::RequestGameplayTag(FName(*partName), false);
                const FS_ProgressionResult result = progression->grantUnlock(tag);
                if(result.success || isAlreadyUnlocked(result)){
                    continue;   // a part that is already unlocked is not a refusal
                }
                UE_LOG(LogAwakenProgression, Log, TEXT("[%s] Awaken.Story.SetPart %d stopped at '%s': %s"), roleOf(world), lastPart, *partName, *outcomeOf(result));
                return;
            }
            UE_LOG(LogAwakenProgression, Log, TEXT("[%s] Awaken.Story.SetPart %d: done."), roleOf(world), lastPart);
        }));

    // Local read command: works on every machine, so the two-player check can compare the host's set with a client's.
    FAutoConsoleCommandWithWorldAndArgs cmdUnlockList(TEXT("Awaken.Unlock.List"), TEXT("Awaken.Unlock.List. Logs every tag in this machine's unlocked set, and the count."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            UProgressionComponent* progression = progressionOf(world);
            if(progression == nullptr){
                UE_LOG(LogAwakenProgression, Warning, TEXT("Awaken.Unlock.List: there is no Progression component (no GameState yet)."));
                return;
            }
            TArray<FGameplayTag> tags;
            progression->getAllUnlocked().GetGameplayTagArray(tags);
            tags.Sort([](const FGameplayTag& left, const FGameplayTag& right){
                return left.ToString() < right.ToString();
            });
            UE_LOG(LogAwakenProgression, Log, TEXT("[%s] Awaken.Unlock.List: %d unlocked, ready: %s."), roleOf(world), tags.Num(), progression->isReady() ? TEXT("yes") : TEXT("no"));
            for(const FGameplayTag& tag : tags){
                UE_LOG(LogAwakenProgression, Log, TEXT("[%s]   %s"), roleOf(world), *tag.ToString());
            }
        }));
}

#endif
