// Non-shipping dev commands of B21 (Doc B22 command table, Phase 6). They register straight with the console, like WeatherCheatCommands.cpp,
// and call the dev functions of the subsystems. Compiled out of shipping builds.
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEventSubsystem.h"

#if !UE_BUILD_SHIPPING

#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentLog.h"
#include "CosmeticEventsEnvironmentPresentationFramework/EnvironmentManager.h"
#include "CosmeticEventsEnvironmentPresentationFramework/WaterRegistrySubsystem.h"
#include "CosmeticEventsEnvironmentPresentationFramework/01Structs/S_CosmeticEventsEnvironmentPresentationFramework.h"
#include "CosmeticEventsEnvironmentPresentationFramework/02Tags/T_CosmeticEventsEnvironmentPresentationFramework.h"
#if WITH_EDITOR
#include "CosmeticEventsEnvironmentPresentationFramework/DA_GroundTypeMap.h"
#endif
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace{
    APlayerController* localController(UWorld* world){
        return world ? world->GetFirstPlayerController() : nullptr;
    }

    // The place under the cursor, or three metres in front of the pawn when there is no cursor.
    FVector cursorOrFrontLocation(APlayerController* controller){
        FHitResult hit;
        if(controller->GetHitResultUnderCursor(ECC_Visibility, false, hit) && hit.bBlockingHit){
            return hit.ImpactPoint;
        }
        if(const APawn* pawn = controller->GetPawn()){
            return pawn->GetActorLocation() + pawn->GetActorForwardVector() * 300.0f;
        }
        return FVector::ZeroVector;
    }

    bool parseTag(const FString& text, FGameplayTag& outTag){
        outTag = FGameplayTag::RequestGameplayTag(FName(*text), false);
        if(!outTag.IsValid()){
            UE_LOG(LogAwakenCosmetic, Warning, TEXT("Unknown tag %s"), *text);
            return false;
        }
        return true;
    }

    // The test listener of the B22 resend check: counts how often each sequence number (carried in magnitude) arrives on this machine.
    TMap<int32, int32> arrivalsBySequence;
    FGameplayMessageListenerHandle testListener;

    FAutoConsoleCommandWithWorldAndArgs cmdFire(TEXT("Awaken.Cosmetic.Fire"), TEXT("Awaken.Cosmetic.Fire <tag> [magnitude]. Raises one cosmetic event at the cursor. On a client it passes localOnly, because a client cannot raise through the channel."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            UCosmeticEventSubsystem* cosmetic = world ? world->GetSubsystem<UCosmeticEventSubsystem>() : nullptr;
            APlayerController* controller = localController(world);
            FGameplayTag tag;
            if(cosmetic == nullptr || controller == nullptr || args.Num() < 1 || !parseTag(args[0], tag)){
                UE_LOG(LogAwakenCosmetic, Warning, TEXT("Usage: Awaken.Cosmetic.Fire <tag> [magnitude]"));
                return;
            }
            FS_CosmeticEvent event;
            event.eventTag = tag;
            event.location = cursorOrFrontLocation(controller);
            event.instigator = UCosmeticEventSubsystem::makeInstigatorId(controller->GetPawn());
            event.magnitude = args.Num() > 1 ? FCString::Atof(*args[1]) : 1.0f;
            cosmetic->raiseCosmeticEvent(event, nullptr, nullptr, world->GetNetMode() == NM_Client);
        }));

    FAutoConsoleCommandWithWorldAndArgs cmdListen(TEXT("Awaken.Cosmetic.Listen"), TEXT("Awaken.Cosmetic.Listen <on|off|report>. Counts the arrivals of every sequence number on this machine and reports any that arrived twice."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            if(world == nullptr || args.Num() < 1){
                return;
            }
            if(args[0] == TEXT("on")){
                arrivalsBySequence.Reset();
                testListener.Unregister();
                testListener = UGameplayMessageSubsystem::Get(world).RegisterListener<FS_CosmeticEvent>(T_Cosmetic_Received.GetTag(), [](FGameplayTag channel, const FS_CosmeticEvent& event){
                    arrivalsBySequence.FindOrAdd(FMath::RoundToInt(event.magnitude))++;
                });
                UE_LOG(LogAwakenCosmetic, Log, TEXT("Cosmetic test listener on"));
            }else if(args[0] == TEXT("off") || args[0] == TEXT("report")){
                int32 duplicates = 0;
                for(const TPair<int32, int32>& pair : arrivalsBySequence){
                    if(pair.Value > 1){
                        duplicates++;
                        UE_LOG(LogAwakenCosmetic, Error, TEXT("Sequence %d arrived %d times: an event was delivered twice"), pair.Key, pair.Value);
                    }
                }
                UE_LOG(LogAwakenCosmetic, Log, TEXT("Cosmetic test listener: %d sequence numbers arrived, %d of them more than once (must be 0). Fewer arrivals than sent are allowed, the channel is unreliable."), arrivalsBySequence.Num(), duplicates);
                if(args[0] == TEXT("off")){
                    testListener.Unregister();
                }
            }
        }));

    // B22 test, steps 1 and 4: an event 1 cm inside the radius and one 1 cm outside, from the local pawn.
    FAutoConsoleCommandWithWorldAndArgs cmdDistance(TEXT("Awaken.Cosmetic.DistanceTest"), TEXT("Awaken.Cosmetic.DistanceTest <tag>. Host only. Raises one event 1 cm inside and one 1 cm outside the radius of the local pawn and logs the sends."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            UCosmeticEventSubsystem* cosmetic = world ? world->GetSubsystem<UCosmeticEventSubsystem>() : nullptr;
            APlayerController* controller = localController(world);
            FGameplayTag tag;
            float radius = 0.0f;
            int32 maxPerSecond = 0;
            if(cosmetic == nullptr || controller == nullptr || controller->GetPawn() == nullptr || world->GetNetMode() == NM_Client
                || args.Num() < 1 || !parseTag(args[0], tag) || !cosmetic->devGetRule(tag, radius, maxPerSecond)){
                UE_LOG(LogAwakenCosmetic, Warning, TEXT("Usage (host only, with a pawn): Awaken.Cosmetic.DistanceTest <tag>"));
                return;
            }
            const FVector origin = controller->GetPawn()->GetActorLocation();
            const int32 sendsBefore = cosmetic->testPlayerSends;
            FS_CosmeticEvent event;
            event.eventTag = tag;
            event.magnitude = 1.0f;
            event.location = origin + FVector(radius - 1.0f, 0.0f, 0.0f);
            cosmetic->raiseCosmeticEvent(event);
            const int32 insideSends = cosmetic->testPlayerSends - sendsBefore;
            event.magnitude = 2.0f;
            event.location = origin + FVector(radius + 1.0f, 0.0f, 0.0f);
            cosmetic->raiseCosmeticEvent(event);
            const int32 outsideSends = cosmetic->testPlayerSends - sendsBefore - insideSends;
            UE_LOG(LogAwakenCosmetic, Log, TEXT("Distance test %s (radius %.0f): inside sent to %d players, outside sent to %d players (must be 0 for every player standing at the pawn)"), *tag.ToString(), radius, insideSends, outsideSends);
        }));

    // B22 test, step 2: maxPerSecondPerArea + 5 events at one spot in under a second.
    FAutoConsoleCommandWithWorldAndArgs cmdRate(TEXT("Awaken.Cosmetic.RateTest"), TEXT("Awaken.Cosmetic.RateTest <tag>. Host only. Raises maxPerSecondPerArea + 5 events at one spot and logs how many passed the limiter."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            UCosmeticEventSubsystem* cosmetic = world ? world->GetSubsystem<UCosmeticEventSubsystem>() : nullptr;
            APlayerController* controller = localController(world);
            FGameplayTag tag;
            float radius = 0.0f;
            int32 maxPerSecond = 0;
            if(cosmetic == nullptr || controller == nullptr || controller->GetPawn() == nullptr || world->GetNetMode() == NM_Client
                || args.Num() < 1 || !parseTag(args[0], tag) || !cosmetic->devGetRule(tag, radius, maxPerSecond)){
                UE_LOG(LogAwakenCosmetic, Warning, TEXT("Usage (host only, with a pawn): Awaken.Cosmetic.RateTest <tag>"));
                return;
            }
            const int32 eventsBefore = cosmetic->testEventsSent;
            const int32 sendsBefore = cosmetic->testPlayerSends;
            FS_CosmeticEvent event;
            event.eventTag = tag;
            event.location = controller->GetPawn()->GetActorLocation();
            const int32 raised = maxPerSecond + 5;
            for(int32 sequence = 1; sequence <= raised; sequence++){
                event.magnitude = static_cast<float>(sequence);
                cosmetic->raiseCosmeticEvent(event);
            }
            const int32 passed = cosmetic->testEventsSent - eventsBefore;
            UE_LOG(LogAwakenCosmetic, Log, TEXT("Rate test %s: raised %d, %d passed the limiter (expected %d if nothing of this tag was raised in the last second), %d player sends"), *tag.ToString(), raised, passed, maxPerSecond, cosmetic->testPlayerSends - sendsBefore);
        }));

    // ---- Environment ----

    FAutoConsoleCommandWithWorldAndArgs cmdPushZone(TEXT("Awaken.Environment.PushZone"), TEXT("Awaken.Environment.PushZone <number>. Pushes a dark test zone with a fixed id made from the number. Nests: push 1, then 2."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            AEnvironmentManager* manager = AEnvironmentManager::find(world);
            if(manager == nullptr){
                UE_LOG(LogAwakenEnvironment, Warning, TEXT("No AEnvironmentManager in this world"));
                return;
            }
            const int32 number = args.Num() > 0 ? FCString::Atoi(*args[0]) : 1;
            FS_EnvironmentZoneSettings zone;
            zone.hideSky = true;
            zone.fogDensity = 0.02f * number;
            zone.blendSeconds = 2.0f;
            manager->pushZone(FGuid(0xA0000000, 0, 0, static_cast<uint32>(number)), zone);
        }));

    FAutoConsoleCommandWithWorldAndArgs cmdPopZone(TEXT("Awaken.Environment.PopZone"), TEXT("Awaken.Environment.PopZone <number>. Pops the test zone with that number, from any position of the stack."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            if(AEnvironmentManager* manager = AEnvironmentManager::find(world)){
                const int32 number = args.Num() > 0 ? FCString::Atoi(*args[0]) : 1;
                manager->popZone(FGuid(0xA0000000, 0, 0, static_cast<uint32>(number)));
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs cmdLightning(TEXT("Awaken.Environment.Lightning"), TEXT("Awaken.Environment.Lightning. Fires one lightning flash now, on this machine only."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            if(AEnvironmentManager* manager = AEnvironmentManager::find(world)){
                manager->devForceLightning();
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs cmdSnap(TEXT("Awaken.Environment.Snap"), TEXT("Awaken.Environment.Snap. Hard start: puts the look on its target at once, with no blend."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            if(AEnvironmentManager* manager = AEnvironmentManager::find(world)){
                manager->snapToTarget();
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs cmdZones(TEXT("Awaken.Environment.Zones"), TEXT("Awaken.Environment.Zones. Logs the active zone ids, bottom to top."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            if(AEnvironmentManager* manager = AEnvironmentManager::find(world)){
                const TArray<FGuid> ids = manager->getActiveZoneIds();
                UE_LOG(LogAwakenEnvironment, Log, TEXT("%d active zones"), ids.Num());
                for(const FGuid& id : ids){
                    UE_LOG(LogAwakenEnvironment, Log, TEXT("  %s"), *id.ToString());
                }
            }
        }));

    // ---- Water ----

    FAutoConsoleCommandWithWorldAndArgs cmdWaterQuery(TEXT("Awaken.Water.Query"), TEXT("Awaken.Water.Query. Runs the five queries and the ground type query at the local pawn and logs the answers. Run it on the host and on a client to compare."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            UWaterRegistrySubsystem* water = world ? world->GetSubsystem<UWaterRegistrySubsystem>() : nullptr;
            APlayerController* controller = localController(world);
            if(water == nullptr || controller == nullptr || controller->GetPawn() == nullptr){
                return;
            }
            const FVector location = controller->GetPawn()->GetActorLocation();
            E_WaterKind kind = E_WaterKind::Lake;
            const bool inWater = water->isLocationInWater(location);
            const bool hasKind = water->getWaterKindAt(location, kind);
            const FVector flow = water->getFlowAt(location);
            UE_LOG(LogAwakenEnvironment, Log, TEXT("Water at %s: inWater=%d kind=%s flow=%s freezing=%d frozenWalkable=%d ground=%s"), *location.ToCompactString(), inWater,
                hasKind ? *StaticEnum<E_WaterKind>()->GetNameStringByValue(static_cast<int64>(kind)) : TEXT("none"), *flow.ToCompactString(),
                water->isFreezing(location), water->isFrozenWalkable(location), *water->getGroundTypeAt(location).ToString());
        }));

#if WITH_EDITOR
    FAutoConsoleCommandWithWorldAndArgs cmdGroundBake(TEXT("Awaken.Ground.Bake"), TEXT("Awaken.Ground.Bake. Editor only. Bakes DA_GroundTypeMap_Default from the world, then save the asset."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            if(UDA_GroundTypeMap* map = UDA_GroundTypeMap::loadDefault()){
                map->bakeFromWorld(world);
            }else{
                UE_LOG(LogAwakenEnvironment, Warning, TEXT("DA_GroundTypeMap_Default is missing"));
            }
        }));
#endif
}

#endif
