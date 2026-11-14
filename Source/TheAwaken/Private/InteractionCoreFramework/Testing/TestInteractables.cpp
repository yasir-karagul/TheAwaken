// Private/InteractionCoreFramework/Testing/TestInteractables.cpp (TEMPORARY, TEST ONLY)
#include "InteractionCoreFramework/Testing/TestInteractables.h"
#include "InteractionCoreFramework/InteractionComponent.h"
#include "InteractionCoreFramework/InteractionLockComponent.h"
#include "InteractionCoreFramework/02Tags/T_InteractionCoreFramework.h"
#include "InteractionFrameworkExtension/02Tags/T_InteractionFrameworkExtension.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

// ---- Base ----

ATestInteractableBase::ATestInteractableBase(){
    bReplicates = true;   // the lock flag only reaches clients if the owning actor replicates
    box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
    SetRootComponent(box);
    box->SetBoxExtent(FVector(50.0f, 50.0f, 50.0f));
    // Overlaps every channel, so the focus sweep finds it whatever interactionChannel is (interactables Overlap, solids Block).
    box->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    // Built-in profiles name only the engine channels, so a project channel (the Interaction channel) would fall back to its
    // default response, Block. Overlap every channel explicitly.
    box->SetCollisionResponseToAllChannels(ECR_Overlap);

    cube = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Cube"));
    cube->SetupAttachment(box);
    cube->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> cubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if(cubeMesh.Succeeded()){
        cube->SetStaticMesh(cubeMesh.Object);
    }
}

FGameplayTag ATestInteractableBase::getInteractionTag_Implementation() const{
    return T_Interaction_WorldObject;
}

bool ATestInteractableBase::canInteract_Implementation(AActor* instigator, FGameplayTag& failReason) const{
    if(isBroken){
        failReason = T_Interaction_FailReason_NotAllowedInState;
        return false;
    }
    return true;
}

FS_InteractionPrompt ATestInteractableBase::getPrompt_Implementation(AActor* instigator) const{
    // PLACEHOLDER keys: not in any String Table. The framework stamps key, isHold, holdProgress and blockedReason.
    FS_InteractionPrompt prompt;
    prompt.verbText = TEXT("Interaction.Test.Verb");
    prompt.objectName = TEXT("Interaction.Test.Object");
    return prompt;
}

void ATestInteractableBase::interact_Implementation(AActor* instigator){
    UE_LOG(LogAwakenInteraction, Log, TEXT("TEST %s interact by %s"), *GetNameSafe(this), *GetNameSafe(instigator));
}

bool ATestInteractableBase::getNoise_Implementation(FS_InteractionNoise& noise) const{
    if(testLoudness <= 0.0f){
        return false;
    }
    noise.loudness = testLoudness;
    noise.noiseTag = noiseTag;
    return true;
}

int32 ATestInteractableBase::getInteractionPriority_Implementation() const{
    return testPriority;
}

bool ATestInteractableBase::isInteractableBy_Implementation(AActor* instigator) const{
    return true;
}

// ---- Press, None ----

ATestPress::ATestPress(){
}

void ATestPress::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ATestPress, isOn);
}

E_InteractionKind ATestPress::getInteractionKind_Implementation() const{
    return E_InteractionKind::Press;
}

E_InteractionLockPolicy ATestPress::getLockPolicy_Implementation() const{
    return E_InteractionLockPolicy::None;
}

void ATestPress::interact_Implementation(AActor* instigator){
    isOn = !isOn;
    UE_LOG(LogAwakenInteraction, Log, TEXT("TEST %s toggled to %s by %s"), *GetNameSafe(this), isOn ? TEXT("on") : TEXT("off"), *GetNameSafe(instigator));
}

// ---- Press, Shared ----

ATestShared::ATestShared(){
    lock = CreateDefaultSubobject<UInteractionLockComponent>(TEXT("InteractionLock"));
    // Shared containers let two players look in at once, so the lock must outlive the press until the framework releases it.
    lock->keepsLockUntilReleased = true;
}

E_InteractionKind ATestShared::getInteractionKind_Implementation() const{
    return E_InteractionKind::Press;
}

E_InteractionLockPolicy ATestShared::getLockPolicy_Implementation() const{
    return E_InteractionLockPolicy::Shared;
}

// ---- Hold, Exclusive ----

ATestHold::ATestHold(){
    lock = CreateDefaultSubobject<UInteractionLockComponent>(TEXT("InteractionLock"));
}

E_InteractionKind ATestHold::getInteractionKind_Implementation() const{
    return E_InteractionKind::Hold;
}

E_InteractionLockPolicy ATestHold::getLockPolicy_Implementation() const{
    return E_InteractionLockPolicy::Exclusive;
}

float ATestHold::getHoldDuration_Implementation() const{
    return holdSeconds;
}

bool ATestHold::canActivate_Implementation(AActor* instigator, FGameplayTag& failReason) const{
    if(blockActivation){
        failReason = T_Interaction_FailReason_NotAllowedInState;
        return false;
    }
    return true;
}

void ATestHold::onActivationComplete_Implementation(AActor* instigator){
    ++completions;
    UE_LOG(LogAwakenInteraction, Log, TEXT("TEST %s hold completed by %s (completion %d)"), *GetNameSafe(this), *GetNameSafe(instigator), completions);
}