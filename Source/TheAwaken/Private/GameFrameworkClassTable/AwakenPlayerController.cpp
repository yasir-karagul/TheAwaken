// Private/GameFrameworkClassTable/AwakenPlayerController.cpp
#include "GameFrameworkClassTable/AwakenPlayerController.h"
#include "GameFrameworkClassTable/AwakenGameMode.h"
#include "DialogueFramework/DialogueComponent.h"
#include "AbilityActionFramework/ActionInputComponent.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEventRelayComponent.h"
#include "CameraFramework/ScreenEffectComponent.h"
#include "AudioFramework/AudioListenerComponent.h"
#include "DevToolsTestingFramework/AwakenCheatManager.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

AAwakenPlayerController::AAwakenPlayerController(){
    dialogue = CreateDefaultSubobject<UDialogueComponent>(TEXT("Dialogue"));
    actionInput = CreateDefaultSubobject<UActionInputComponent>(TEXT("ActionInput"));
    cosmeticRelay = CreateDefaultSubobject<UCosmeticEventRelayComponent>(TEXT("CosmeticRelay"));
#if UE_WITH_CHEAT_MANAGER
    CheatClass = UAwakenCheatManager::StaticClass();
#endif
}

void AAwakenPlayerController::BeginPlay(){
    Super::BeginPlay();
    if(HasAuthority()){
        stampInputOnHost();   // a new player is not Away from the first second
    }
    if(!IsLocalController()){
        return;
    }
    screenEffects = NewObject<UScreenEffectComponent>(this);
    screenEffects->RegisterComponent();
    audioListener = NewObject<UAudioListenerComponent>(this);
    audioListener->RegisterComponent();
    addInputMappings();
}

void AAwakenPlayerController::SetupInputComponent(){
    Super::SetupInputComponent();
    actionInput->bindToInputComponent(InputComponent);
}

void AAwakenPlayerController::stampInputOnHost(){
    if(const UWorld* world = GetWorld()){
        lastInputHostTime = world->GetTimeSeconds();
    }
}

void AAwakenPlayerController::noteInputActivity(){
    const UWorld* world = GetWorld();
    if(world == nullptr || !IsLocalController()){
        return;
    }
    if(HasAuthority()){
        stampInputOnHost();   // the host's own player needs no call
        return;
    }
    const double now = world->GetTimeSeconds();
    if(now - lastHeartbeatSentTime < heartbeatIntervalSeconds){
        return;
    }
    lastHeartbeatSentTime = now;
    serverInputHeartbeat();
}

void AAwakenPlayerController::serverInputHeartbeat_Implementation(){
    stampInputOnHost();
}

double AAwakenPlayerController::getSecondsSinceLastInput() const{
    const UWorld* world = GetWorld();
    return world != nullptr ? FMath::Max(0.0, world->GetTimeSeconds() - lastInputHostTime) : 0.0;
}

void AAwakenPlayerController::addInputMappings(){
    UEnhancedInputLocalPlayerSubsystem* inputSubsystem =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
    if(inputSubsystem == nullptr){
        return;
    }
    for(int32 index = 0; index < defaultMappings.Num(); index++){
        inputSubsystem->AddMappingContext(defaultMappings[index], 0);
    }
}

// TEMPORARY (test 6), remove afterwards.
void AAwakenPlayerController::debugRespawn(){
    serverDebugRespawn();
}

// TEMPORARY (test 6), remove afterwards.
void AAwakenPlayerController::serverDebugRespawn_Implementation(){
    AAwakenGameMode* gameMode = GetWorld()->GetAuthGameMode<AAwakenGameMode>();
    if(gameMode == nullptr){
        return;
    }
    APawn* oldPawn = GetPawn();
    if(oldPawn != nullptr){
        UnPossess();
        oldPawn->Destroy();
    }
    gameMode->RestartPlayer(this);
}
