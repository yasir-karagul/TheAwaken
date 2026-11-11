#include "CosmeticEventsEnvironmentPresentationFramework/EnvironmentZoneVolume.h"
#include "CosmeticEventsEnvironmentPresentationFramework/EnvironmentManager.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentLog.h"
#include "Components/BrushComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/DataValidation.h"

AEnvironmentZoneVolume::AEnvironmentZoneVolume(){
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    SetCanBeDamaged(false);
    // A plain AVolume does not generate overlap events. The brush must overlap pawns, like a trigger volume does.
    // "OverlapAllDynamic" is a default engine collision profile. UCollisionProfile has no constant for it in UE 5.8.
    GetBrushComponent()->SetCollisionProfileName(FName(TEXT("OverlapAllDynamic")));
    GetBrushComponent()->SetGenerateOverlapEvents(true);
    SetActorEnableCollision(true);
}

void AEnvironmentZoneVolume::PostActorCreated(){
    Super::PostActorCreated();
    if(!zoneId.IsValid()){
        zoneId = FGuid::NewGuid();
    }
}

void AEnvironmentZoneVolume::BeginPlay(){
    Super::BeginPlay();

    if(GetNetMode() == NM_DedicatedServer){
        Destroy();
        return;
    }

    OnActorBeginOverlap.AddDynamic(this, &AEnvironmentZoneVolume::onBeginOverlap);
    OnActorEndOverlap.AddDynamic(this, &AEnvironmentZoneVolume::onEndOverlap);

    // Overlap events do not fire for a pawn that is already inside, so look once the local pawn exists. The timer stops after that (Doc02 5).
    GetWorldTimerManager().SetTimer(initialCheckTimer, this, &AEnvironmentZoneVolume::tryInitialCheck, 0.5f, true);
}

// Streaming can unload the volume while the player is inside, so EndPlay pops the zone. It cannot stay on the stack.
void AEnvironmentZoneVolume::EndPlay(const EEndPlayReason::Type reason){
    GetWorldTimerManager().ClearTimer(initialCheckTimer);
    leaveZone();
    Super::EndPlay(reason);
}

void AEnvironmentZoneVolume::tryInitialCheck(){
    APlayerController* controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if(controller == nullptr || !controller->IsLocalController() || controller->GetPawn() == nullptr){
        return;
    }

    GetWorldTimerManager().ClearTimer(initialCheckTimer);
    controller->OnPossessedPawnChanged.AddUniqueDynamic(this, &AEnvironmentZoneVolume::onPossessedPawnChanged);

    if(IsOverlappingActor(controller->GetPawn())){
        enterZone();
    }
}

// A destroyed pawn ends its overlaps, so the zone pops here. The new pawn runs the same check as a pawn that starts inside.
void AEnvironmentZoneVolume::onPossessedPawnChanged(APawn* oldPawn, APawn* newPawn){
    if(oldPawn != nullptr){
        leaveZone();
    }
    if(newPawn != nullptr && newPawn->IsLocallyControlled() && IsOverlappingActor(newPawn)){
        enterZone();
    }
}

void AEnvironmentZoneVolume::onBeginOverlap(AActor* overlappedActor, AActor* otherActor){
    if(isLocalPlayerPawn(otherActor)){
        enterZone();
    }
}

void AEnvironmentZoneVolume::onEndOverlap(AActor* overlappedActor, AActor* otherActor){
    if(isLocalPlayerPawn(otherActor)){
        leaveZone();
    }
}

// True for the host's pawn and an offline player's pawn, false for other players, NPCs and thrown items. Actor-level overlap events fire once
// per actor however many of its components touch the brush, so this volume needs no counting (unlike the swim volumes of AAwakenWaterBody).
bool AEnvironmentZoneVolume::isLocalPlayerPawn(const AActor* actor) const{
    const APawn* pawn = Cast<APawn>(actor);
    return pawn != nullptr && pawn->IsLocallyControlled();
}

AEnvironmentManager* AEnvironmentZoneVolume::findManager() const{
    return AEnvironmentManager::find(this);
}

// The inside flag makes a repeated enter or leave harmless.
void AEnvironmentZoneVolume::enterZone(){
    if(inside){
        return;
    }
    AEnvironmentManager* manager = findManager();
    if(manager == nullptr){
        UE_LOG(LogAwakenEnvironment, Warning, TEXT("Zone %s has no AEnvironmentManager to push to"), *zoneId.ToString());
        return;
    }
    inside = true;
    manager->pushZone(zoneId, settings);
}

void AEnvironmentZoneVolume::leaveZone(){
    if(!inside){
        return;
    }
    inside = false;
    if(AEnvironmentManager* manager = findManager()){
        manager->popZone(zoneId);
    }
}

#if WITH_EDITOR
EDataValidationResult AEnvironmentZoneVolume::IsDataValid(FDataValidationContext& context) const{
    EDataValidationResult result = Super::IsDataValid(context);
    bool isInvalid = false;

    // Doc02 19.3 kind 1: an empty or duplicate id fails the build.
    if(!zoneId.IsValid()){
        context.AddError(FText::FromString(TEXT("AEnvironmentZoneVolume has an empty zoneId")));
        isInvalid = true;
    }else if(const UWorld* world = GetWorld()){
        for(TActorIterator<AEnvironmentZoneVolume> it(const_cast<UWorld*>(world)); it; ++it){
            if(*it != this && it->zoneId == zoneId){
                context.AddError(FText::FromString(FString::Printf(TEXT("AEnvironmentZoneVolume %s has the same zoneId as %s"), *GetName(), *it->GetName())));
                isInvalid = true;
                break;
            }
        }
    }
    if(isInvalid){
        return EDataValidationResult::Invalid;
    }
    return result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : result;
}
#endif