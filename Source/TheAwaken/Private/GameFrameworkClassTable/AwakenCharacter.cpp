// Private/GameFrameworkClassTable/AwakenCharacter.cpp
#include "GameFrameworkClassTable/AwakenCharacter.h"
#include "GameFrameworkClassTable/AwakenMovementComponent.h"
#include "GameFrameworkClassTable/AwakenPlayerState.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "CombatFramework/CombatComponent.h"
#include "CarryEncumbranceFramework/UCarryComponent.h"
#include "CraftingFramework/UCraftingComponent.h"
#include "BuildingFramework/BuildToolComponent.h"
#include "InteractionCoreFramework/InteractionComponent.h"
#include "CheckpointRespawnFramework/DownedComponent.h"
#include "WeatherFramework/ShelterComponent.h"
#include "CameraFramework/FirstPersonCameraComponent.h"
#include "AbilitySystemComponent.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"

AAwakenCharacter::AAwakenCharacter(const FObjectInitializer& objectInitializer)
    : Super(objectInitializer.SetDefaultSubobjectClass<UAwakenMovementComponent>(ACharacter::CharacterMovementComponentName)){
    heldItem = CreateDefaultSubobject<UHeldItemComponent>(TEXT("HeldItem"));
    combat = CreateDefaultSubobject<UCombatComponent>(TEXT("Combat"));
    carry = CreateDefaultSubobject<UCarryComponent>(TEXT("Carry"));
    crafting = CreateDefaultSubobject<UCraftingComponent>(TEXT("Crafting"));
    buildTool = CreateDefaultSubobject<UBuildToolComponent>(TEXT("BuildTool"));
    interaction = CreateDefaultSubobject<UInteractionComponent>(TEXT("Interaction"));
    downed = CreateDefaultSubobject<UDownedComponent>(TEXT("Downed"));
    shelter = CreateDefaultSubobject<UShelterComponent>(TEXT("Shelter"));
}

UAbilitySystemComponent* AAwakenCharacter::GetAbilitySystemComponent() const{
    const AAwakenPlayerState* awakenState = GetPlayerState<AAwakenPlayerState>();
    if(awakenState == nullptr){
        return nullptr;
    }
    return awakenState->GetAbilitySystemComponent();
}

void AAwakenCharacter::BeginPlay(){
    Super::BeginPlay();
    // The camera is the Blueprint child BPC_FirstPersonCameraComponent, added in the editor. Attach it to the head socket without scale.
    UFirstPersonCameraComponent* camera = FindComponentByClass<UFirstPersonCameraComponent>();
    if(camera != nullptr){
        camera->attachToHeadSocket(GetMesh());
    }
}

void AAwakenCharacter::NotifyControllerChanged(){
    Super::NotifyControllerChanged();
    UFirstPersonCameraComponent* camera = FindComponentByClass<UFirstPersonCameraComponent>();
    if(camera == nullptr){
        return;
    }
    if(IsLocallyControlled()){
        // Same moment as setLocalListening(true) (step 4). Only the owner's machine hides the head.
        camera->setLocalListening(true);
        camera->applyControllerSettings();
        camera->applyOwnerHeadHidingFromConfig(GetMesh());
    }else{
        // Released, or a remote Character: nothing registered, head always shown.
        camera->setLocalListening(false);
        camera->clearOwnerHeadHiding();
    }
}

void AAwakenCharacter::PossessedBy(AController* newController){
    Super::PossessedBy(newController);
    bindAbilitySystem();
}

void AAwakenCharacter::OnRep_PlayerState(){
    Super::OnRep_PlayerState();
    bindAbilitySystem();
}

void AAwakenCharacter::bindAbilitySystem(){
    AAwakenPlayerState* awakenState = GetPlayerState<AAwakenPlayerState>();
    if(awakenState == nullptr){
        return;
    }
    UAbilitySystemComponent* abilitySystem = awakenState->GetAbilitySystemComponent();
    if(HasAuthority()){
        // A3 step 5: end everything before the new pawn is bound. Only the Action.State.* and Action.Cooldown.* effects go;
        // effects and loose tags that other frameworks own (Player.State.Incapacitated, Doc B02) stay.
        abilitySystem->CancelAllAbilities();
        FGameplayTagContainer actionStateTags;
        actionStateTags.AddTag(T_Action_State);
        actionStateTags.AddTag(T_Action_Cooldown);
        abilitySystem->RemoveActiveEffectsWithGrantedTags(actionStateTags);
    }
    abilitySystem->InitAbilityActorInfo(awakenState, this);
    if(HasAuthority()){
        awakenState->grantLoadoutOnce();   // A3 step 4: once per PlayerState, not once per pawn
        // Doc B18 guide Step 9: server only. Runs again after every respawn, and bindAbilitySystem unbinds before it binds.
        interaction->bindAbilitySystem(abilitySystem);
    }
}

void AAwakenCharacter::SetupPlayerInputComponent(UInputComponent* playerInputComponent){
    Super::SetupPlayerInputComponent(playerInputComponent);
    // Doc B18 guide Step 4: Unreal calls this on the locally controlled pawn only. The Interaction component binds IA_Interact
    // and starts its focus timer. Camera look and movement binding belong to their own frameworks.
    interaction->bindInput(playerInputComponent);

    // TEMPORARY move and look (see the header). Triggered fires every frame while the key or mouse moves, which is what an axis needs.
    UEnhancedInputComponent* input = Cast<UEnhancedInputComponent>(playerInputComponent);
    if(input == nullptr){
        return;
    }
    if(tempMoveAction != nullptr){
        input->BindAction(tempMoveAction, ETriggerEvent::Triggered, this, &AAwakenCharacter::tempMove);
    }
    if(tempLookAction != nullptr){
        input->BindAction(tempLookAction, ETriggerEvent::Triggered, this, &AAwakenCharacter::tempLook);
    }
}

void AAwakenCharacter::tempMove(const FInputActionValue& value){
    // x = right, y = forward, relative to where the player looks (yaw only).
    const FVector2D axis = value.Get<FVector2D>();
    const FRotationMatrix yaw(FRotator(0.0f, GetControlRotation().Yaw, 0.0f));
    AddMovementInput(yaw.GetUnitAxis(EAxis::X), axis.Y);
    AddMovementInput(yaw.GetUnitAxis(EAxis::Y), axis.X);
}

void AAwakenCharacter::tempLook(const FInputActionValue& value){
    // Mouse up is positive Y; Unreal's positive pitch input looks down, so it is negated.
    const FVector2D axis = value.Get<FVector2D>();
    AddControllerYawInput(axis.X);
    AddControllerPitchInput(-axis.Y);
}

void AAwakenCharacter::UnPossessed(){
    // Doc B18 guide Step 4 and 11: stop focus, send a lost key release, end any hold and free the locks.
    interaction->onUnpossessed();
    Super::UnPossessed();
}
