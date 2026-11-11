// Private/GameFrameworkClassTable/AwakenGameMode.cpp
#include "GameFrameworkClassTable/AwakenGameMode.h"
#include "GameFrameworkClassTable/AwakenGameState.h"
#include "GameFrameworkClassTable/AwakenPlayerState.h"
#include "GameFrameworkClassTable/AwakenPlayerController.h"
#include "GameFrameworkClassTable/AwakenCharacter.h"
#include "SessionHandshakeFramework/SessionHandshakeSubsystem.h"
#include "SessionHandshakeFramework/02Tags/T_SessionHandshakeFramework.h"
#include "TimeFramework/TimeSubsystem.h"
#include "WeatherFramework/WeatherSubsystem.h"
#include "ProgressionFramework/ProgressionComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/OnlineReplStructs.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

#if !UE_BUILD_SHIPPING
#include "HAL/IConsoleManager.h"
#include "SessionHandshakeFramework/NullPlatformAuthProvider.h"

// TEMPORARY dev stand-in for Doc B20 (Session Lobby), which would normally run the handshake before the engine connection opens.
// It applies only while the handshake uses the Null provider (editor and development builds), so a Shipping build never compiles it.
// Set it to 0 to test the strict door of the guide (test cases 17 and 18).
static TAutoConsoleVariable<int32> cvarHandshakeDevAdmitAll(
    TEXT("Awaken.Handshake.DevAdmitAll"), 1,
    TEXT("Non-shipping, Null provider only: admit every remote peer with a dev identity until Doc B20 runs the real handshake."));

static bool devAdmitsEveryone(const USessionHandshakeSubsystem* handshake){
    return cvarHandshakeDevAdmitAll.GetValueOnGameThread() != 0
        && handshake != nullptr
        && Cast<UNullPlatformAuthProvider>(handshake->getProvider()) != nullptr;
}
#endif

AAwakenGameMode::AAwakenGameMode(){
    GameStateClass = AAwakenGameState::StaticClass();
    PlayerStateClass = AAwakenPlayerState::StaticClass();
    PlayerControllerClass = AAwakenPlayerController::StaticClass();
    DefaultPawnClass = AAwakenCharacter::StaticClass();
}

USessionHandshakeSubsystem* AAwakenGameMode::getHandshake() const{
    const UWorld* world = GetWorld();
    UGameInstance* gameInstance = world != nullptr ? world->GetGameInstance() : nullptr;
    return gameInstance != nullptr ? gameInstance->GetSubsystem<USessionHandshakeSubsystem>() : nullptr;
}

void AAwakenGameMode::PreLogin(const FString& options, const FString& address, const FUniqueNetIdRepl& uniqueId, FString& errorMessage){
    Super::PreLogin(options, address, uniqueId, errorMessage);
    if(!errorMessage.IsEmpty()){
        return;
    }
    // PreLogin only ever sees a REMOTE connection, so it needs a live admission in every mode. A missing subsystem fails closed (Doc B05 3.2).
    // Doc B20 will also check capacity and bans before it asks the handshake.
    USessionHandshakeSubsystem* handshake = getHandshake();
#if !UE_BUILD_SHIPPING
    if(devAdmitsEveryone(handshake)){
        UE_LOG(LogTemp, Warning, TEXT("Handshake dev door: a remote peer is admitted without a check (Awaken.Handshake.DevAdmitAll)."));
        return;
    }
#endif
    const FString peerId = USessionHandshakeSubsystem::peerIdFromUniqueNetId(uniqueId);
    if(handshake == nullptr || !handshake->hasLiveAdmission(peerId)){
        errorMessage = T_Session_JoinFailReason_OwnershipUnverified.GetTag().ToString();
    }
}

void AAwakenGameMode::PostLogin(APlayerController* newPlayer){
    if(newPlayer == nullptr || newPlayer->PlayerState == nullptr){
        Super::PostLogin(newPlayer);
        return;
    }

    // The identity is written BEFORE Super::PostLogin. The engine's PostLogin starts the player (HandleStartingNewPlayer), and Doc B06's
    // restore must find the identity already on the PlayerState.
    USessionHandshakeSubsystem* handshake = getHandshake();
    if(newPlayer->IsLocalController()){
        if(handshake != nullptr){
            handshake->applyLocalPlayer(newPlayer->PlayerState);   // the host's own player, or the offline player
        }
    }else{
        // A remote controller with no valid admission is never let in, in any mode.
        const FString peerId = USessionHandshakeSubsystem::peerIdFromUniqueNetId(newPlayer->PlayerState->GetUniqueId());
        FS_JoinValidationResult admitted;
        bool admittedOk = handshake != nullptr
            && handshake->tryConsumeAdmission(peerId, admitted)
            && handshake->applyJoinResultToPlayerState(newPlayer->PlayerState, admitted);
#if !UE_BUILD_SHIPPING
        if(!admittedOk && devAdmitsEveryone(handshake)){
            // A PIE client has no platform id, so the dev identity is made from the engine's player id.
            FS_JoinValidationResult devResult;
            devResult.allowed = true;
            devResult.identity.primaryId = FString::Printf(TEXT("dev-%d"), newPlayer->PlayerState->GetPlayerId());
            devResult.identity.kind = E_PlayerIdKind::LocalFallback;
            devResult.platformDisplayName = newPlayer->PlayerState->GetPlayerName();
            admittedOk = handshake->applyJoinResultToPlayerState(newPlayer->PlayerState, devResult);
        }
#endif
        if(!admittedOk){
            // The admission ran out between PreLogin and here, or the write was refused (a different id on a reused PlayerState).
            if(handshake != nullptr){
                handshake->releasePeer(peerId);
            }
            GameSession->KickPlayer(newPlayer, FText::FromString(T_Session_JoinFailReason_OwnershipUnverified.GetTag().ToString()));
            return;
        }
    }

    Super::PostLogin(newPlayer);
    // From here Doc B06 runs P1 to P6. P1 is done: the identity is on the PlayerState.
}

void AAwakenGameMode::ChangeName(AController* controller, const FString& newName, bool nameChange){
    if(nameChange){
        return;   // a player asked for a new name (SetName, ServerChangeName): refused (Doc B05 3.4). The handshake writes the name with SetPlayerName, which does not pass here
    }
    Super::ChangeName(controller, newName, nameChange);   // the engine's automatic first name still goes through; the handshake overwrites it
}

void AAwakenGameMode::Logout(AController* exiting){
    // Doc R12 11.4: a player who leaves is dropped from the sleep wait. This stands in for the Session.PlayerLeft listener
    // of the Time glue until Doc B20 raises that message. cancelSleepWait is safe for a player who is not registered.
    if(exiting != nullptr && exiting->PlayerState != nullptr){
        if(UTimeSubsystem* time = UTimeSubsystem::get(this)){
            time->cancelSleepWait(exiting->PlayerState);
        }
    }
    Super::Logout(exiting);
}

// TEMPORARY until Doc B06 3.6 calls onWorldRestoreFinished itself: release the held players at once.
void AAwakenGameMode::StartPlay(){
    Super::StartPlay();
    // TEMPORARY: the host clock does not run until initializeNewWorld or restoreFromSnapshot has been called (Time guide 14.2).
    // B06 calls initializeNewWorld for a new game and the Time save participant for a loaded one. Remove this with the line below.
    if(UTimeSubsystem* time = UTimeSubsystem::get(this)){
        if(!time->isClockReady()){
            time->initializeNewWorld();
        }
    }
    // TEMPORARY: Weather starts after Time and before any player is released (Weather guide, Step 12 and Step 19, B06 row).
    // B06 calls initializeNewWorld for a new game and the Weather save participant restores a loaded one. Remove this with the Time block above.
    if(UWeatherSubsystem* weather = UWeatherSubsystem::get(this)){
        if(!weather->isWeatherReady()){
            weather->initializeNewWorld();
        }
    }
    // TEMPORARY (Progression guide, Step 8.4): the set must be seeded before any player is released. New worlds only:
    // if a save is loaded afterwards, the restore is refused because the set is already seeded. Remove this with the block above when B06 makes the calls.
    UProgressionComponent* progression = UProgressionComponent::findProgression(this);
    if(progression != nullptr && !progression->isReady()){
        progression->seedNewWorld();
    }
    onWorldRestoreFinished();
}

void AAwakenGameMode::HandleStartingNewPlayer_Implementation(APlayerController* newPlayer){
    if(!worldRestored){
        UE_LOG(LogTemp, Warning, TEXT("Restore gate: player held")); // test 8, remove afterwards
        pendingPlayers.AddUnique(newPlayer);
        return;
    }
    beginPlayerRestore(newPlayer);
}

void AAwakenGameMode::onWorldRestoreFinished(){
    UE_LOG(LogTemp, Warning, TEXT("Restore gate: onWorldRestoreFinished")); // test 8, remove afterwards
    worldRestored = true;
    for(int32 index = 0; index < pendingPlayers.Num(); index++){
        if(pendingPlayers[index].IsValid()){
            beginPlayerRestore(pendingPlayers[index].Get());
        }
    }
    pendingPlayers.Reset();
}

void AAwakenGameMode::beginPlayerRestore(APlayerController* player){
    UE_LOG(LogTemp, Warning, TEXT("Restore gate: beginPlayerRestore")); // test 8, remove afterwards
    // Doc B06 3.6 P1 to P6 run from here: identity, source (held state, Character file or new),
    // spawn at the saved position, bag reconciliation, per-player restore, loading cover.
    // P3 spawns with RestartPlayerAtTransform, never with the default start.
    // TEMPORARY: until B06 exists, a plain engine start at a PlayerStart in the map.
    RestartPlayer(player);
}