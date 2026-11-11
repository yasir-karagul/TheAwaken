#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEventSubsystem.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEventRelayComponent.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentLog.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentMath.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentValidation.h"
#include "CosmeticEventsEnvironmentPresentationFramework/DA_EnvironmentSettings.h"
#include "CosmeticEventsEnvironmentPresentationFramework/02Tags/T_CosmeticEventsEnvironmentPresentationFramework.h"
#include "GameFrameworkClassTable/AwakenPlayerState.h"
#include "SessionHandshakeFramework/PlayerIdentityComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

UCosmeticEventSubsystem* UCosmeticEventSubsystem::get(const UObject* worldContext){
    const UWorld* world = GEngine ? GEngine->GetWorldFromContextObject(worldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    return world ? world->GetSubsystem<UCosmeticEventSubsystem>() : nullptr;
}

// It exists on every machine, including a listen server, a dedicated server and offline play.
bool UCosmeticEventSubsystem::ShouldCreateSubsystem(UObject* outer) const{
    const UWorld* world = Cast<UWorld>(outer);
    return world && world->IsGameWorld() && Super::ShouldCreateSubsystem(outer);
}

void UCosmeticEventSubsystem::Initialize(FSubsystemCollectionBase& collection){
    Super::Initialize(collection);
    settings = UDA_EnvironmentSettings::loadDefault();
    if(settings == nullptr){
        UE_LOG(LogAwakenCosmetic, Error, TEXT("[%s] DA_EnvironmentSettings_Default is missing. No cosmetic event can be raised."), *roleName());
        return;
    }
    ruleTable = settings->cosmeticEventTable.LoadSynchronous();
    if(ruleTable == nullptr){
        UE_LOG(LogAwakenCosmetic, Error, TEXT("[%s] DA_EnvironmentSettings_Default has no DT_CosmeticEvents. No cosmetic event can be raised."), *roleName());
    }
}

void UCosmeticEventSubsystem::OnWorldBeginPlay(UWorld& world){
    Super::OnWorldBeginPlay(world);
#if !UE_BUILD_SHIPPING
    // The Net view needs a refresh once a second while it is switched on. The body returns at once with the switch off.
    world.GetTimerManager().SetTimer(debugTimerHandle, FTimerDelegate::CreateLambda([this](){
        drawDebug();
    }), 1.0f, true);
#endif
}

void UCosmeticEventSubsystem::Deinitialize(){
#if !UE_BUILD_SHIPPING
    if(UWorld* world = GetWorld()){
        world->GetTimerManager().ClearTimer(debugTimerHandle);
    }
#endif
    Super::Deinitialize();
}

FString UCosmeticEventSubsystem::roleName() const{
    const UWorld* world = GetWorld();
    if(!world){
        return TEXT("Unknown");
    }
    switch(world->GetNetMode()){
        case NM_Client:
            return TEXT("Client");
        case NM_Standalone:
            return TEXT("Offline");
        default:
            return TEXT("Host");
    }
}

void UCosmeticEventSubsystem::raiseCosmeticEvent(const FS_CosmeticEvent& event, AController* ownerPlayer, AController* excludePlayer, bool localOnly){
    UWorld* world = GetWorld();
    if(world == nullptr || !event.eventTag.IsValid()){
        return;
    }
    if(localOnly){
        deliverLocally(event);
        return;
    }
    // A.2: only the host raises. Offline is not NM_Client, so the same call plays locally there with no special branch.
    if(world->GetNetMode() == NM_Client){
        UE_LOG(LogAwakenCosmetic, Verbose, TEXT("Client call ignored for %s; pass localOnly for local feedback"), *event.eventTag.ToString());
        return;
    }
    const FS_CosmeticEventRule* rule = findRule(event.eventTag);
    if(rule == nullptr){
        UE_LOG(LogAwakenCosmetic, Warning, TEXT("No rule for %s"), *event.eventTag.ToString());
        return;
    }
    const float radius = resolveRadius(*rule);
    // A.2: the limiter runs before any send, so a dropped event is also dropped on the host.
    if(isRateLimited(event, radius, rule->maxPerSecondPerArea)){
        UE_LOG(LogAwakenCosmetic, VeryVerbose, TEXT("Rate limit dropped %s"), *event.eventTag.ToString());
        return;
    }
#if !UE_BUILD_SHIPPING
    testEventsSent++;
#endif
    for(FConstPlayerControllerIterator it = world->GetPlayerControllerIterator(); it; ++it){
        APlayerController* controller = it->Get();
        if(controller == nullptr){
            continue;
        }
        if(controller == excludePlayer){
            continue;
        }
        // A.5: the owner bypasses the radius only. The rate limit and the radius stay data.
        const bool isOwner = (controller == ownerPlayer);
        if(isOwner == false && isInsideRadius(controller, event.location, radius) == false){
            continue;
        }
        sendToPlayer(controller, event);
    }
}

const FS_CosmeticEventRule* UCosmeticEventSubsystem::findRule(const FGameplayTag& eventTag) const{
    if(ruleTable == nullptr){
        return nullptr;
    }
    const FString tagString = eventTag.ToString();
    const FS_CosmeticEventRule* rule = ruleTable->FindRow<FS_CosmeticEventRule>(CosmeticEnvironmentValidation::makeRowName(tagString), TEXT("findRule"), false);
    if(rule != nullptr){
        return rule;
    }
    // The table import may have rejected dots in row names.
    rule = ruleTable->FindRow<FS_CosmeticEventRule>(CosmeticEnvironmentValidation::makeUnderscoreRowName(tagString), TEXT("findRule"), false);
    if(rule != nullptr){
        return rule;
    }
    return ruleTable->FindRow<FS_CosmeticEventRule>(FName("Default"), TEXT("findRule"), false);
}

float UCosmeticEventSubsystem::resolveRadius(const FS_CosmeticEventRule& rule) const{
    if(rule.readsHearingDistance){
        // The one hearing distance is owned by Doc R09. It names no accessor yet (gap G9), so the data fallback stands in.
        return settings ? settings->fallbackHearingDistance : rule.relevanceRadius;
    }
    return rule.relevanceRadius;
}

bool UCosmeticEventSubsystem::isRateLimited(const FS_CosmeticEvent& event, float radius, int32 maxPerSecond){
    const double now = GetWorld()->GetRealTimeSeconds();   // real time: a crowd fight is a network problem and the game clock may be paused or skipped
    const double window = settings ? settings->rateWindowSeconds : 1.0;
    TArray<FS_RecentSend>& recent = recentByTag.FindOrAdd(event.eventTag);
    return UCosmeticEnvironmentMathLibrary::isRateLimited(recent, now, event.location, radius, maxPerSecond, window);
}

bool UCosmeticEventSubsystem::isInsideRadius(const APlayerController* controller, const FVector& location, float radius) const{
    // A.2: measured from the player's pawn. A player with no pawn is skipped unless they are the owner.
    const APawn* pawn = controller->GetPawn();
    if(pawn == nullptr){
        return false;
    }
    return FVector::DistSquared(pawn->GetActorLocation(), location) <= static_cast<double>(radius) * static_cast<double>(radius);
}

void UCosmeticEventSubsystem::sendToPlayer(APlayerController* controller, const FS_CosmeticEvent& event){
#if !UE_BUILD_SHIPPING
    testPlayerSends++;
#endif
    // A.3 step 4: the host plays too. Its controller is in the same loop and is a local controller, so no RPC is used.
    if(controller->IsLocalController()){
        deliverLocally(event);
        return;
    }
    UCosmeticEventRelayComponent* relay = controller->FindComponentByClass<UCosmeticEventRelayComponent>();
    if(relay == nullptr){
        UE_LOG(LogAwakenCosmetic, Warning, TEXT("No relay component on %s"), *GetNameSafe(controller));
        return;
    }
    relay->clientReceiveCosmeticEvent(event);
}

void UCosmeticEventSubsystem::deliverLocally(const FS_CosmeticEvent& event){
#if !UE_BUILD_SHIPPING
    recentReceived.Add(event);
    if(recentReceived.Num() > 32){
        recentReceived.RemoveAt(0);
    }
#endif
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Cosmetic_Received.GetTag(), event);
}


FGuid UCosmeticEventSubsystem::makeInstigatorId(const AActor* actor){
    const APawn* pawn = Cast<APawn>(actor);
    if(pawn == nullptr){
        return FGuid();
    }
    if(const AAwakenPlayerState* playerState = pawn->GetPlayerState<AAwakenPlayerState>()){
        const UPlayerIdentityComponent* identity = playerState->getIdentityComponent();
        if(identity != nullptr && identity->hasIdentity()){
            return UCosmeticEnvironmentMathLibrary::makePlayerInstigatorId(identity->getIdentity().primaryId);
        }
        return FGuid();
    }
    // An NPC: UCosmeticEnvironmentMathLibrary::makeNpcInstigatorId(npcId) once R16 names the NPCId accessor (gap G9).
    return FGuid();
}

AActor* UCosmeticEventSubsystem::findInstigatorActor(const FS_CosmeticEvent& event) const{
    if(!event.instigator.IsValid()){
        return nullptr;
    }
    const UWorld* world = GetWorld();
    const AGameStateBase* gameState = world ? world->GetGameState() : nullptr;
    if(gameState != nullptr){
        // The player registry of B05: match on the makeInstigatorId value of each player's PlayerIdentity.
        for(const TObjectPtr<APlayerState>& playerState : gameState->PlayerArray){
            const AAwakenPlayerState* awakenState = Cast<AAwakenPlayerState>(playerState.Get());
            const UPlayerIdentityComponent* identity = awakenState ? awakenState->getIdentityComponent() : nullptr;
            if(identity == nullptr || !identity->hasIdentity()){
                continue;
            }
            if(UCosmeticEnvironmentMathLibrary::makePlayerInstigatorId(identity->getIdentity().primaryId) == event.instigator){
                return awakenState->GetPawn();
            }
        }
    }
    // The NPC registry of R16 (match on NPCId) is not named in the specs. Stub: not found (gap G9).
    return nullptr;
}

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> cvarDebugCosmetic(TEXT("Awaken.Debug.Cosmetic"), 0, TEXT("Draw the cosmetic event channel (the Net view of B22)."), ECVF_Default);

bool UCosmeticEventSubsystem::devGetRule(const FGameplayTag& eventTag, float& radius, int32& maxPerSecond) const{
    const FS_CosmeticEventRule* rule = findRule(eventTag);
    if(rule == nullptr){
        return false;
    }
    radius = resolveRadius(*rule);
    maxPerSecond = rule->maxPerSecondPerArea;
    return true;
}

void UCosmeticEventSubsystem::drawDebug() const{
    if(cvarDebugCosmetic.GetValueOnGameThread() == 0 || !GEngine){
        return;
    }
    const float lifetime = 1.25f;
    const int32 keyBase = GetWorld() && GetWorld()->GetNetMode() == NM_Client ? 6000 : 5000;
    GEngine->AddOnScreenDebugMessage(keyBase, lifetime, FColor::Cyan, FString::Printf(TEXT("[%s] cosmetic: sent=%d playerSends=%d received(last 32)=%d"), *roleName(), testEventsSent, testPlayerSends, recentReceived.Num()));
    const int32 shown = FMath::Min(recentReceived.Num(), 6);
    for(int32 line = 0; line < shown; line++){
        const FS_CosmeticEvent& event = recentReceived[recentReceived.Num() - 1 - line];
        GEngine->AddOnScreenDebugMessage(keyBase + 1 + line, lifetime, FColor::White, FString::Printf(TEXT("  %s at %s mag=%.2f"), *event.eventTag.ToString(), *event.location.ToCompactString(), event.magnitude));
    }
}
#endif
