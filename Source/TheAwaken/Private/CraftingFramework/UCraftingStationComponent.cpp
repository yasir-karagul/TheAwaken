#include "CraftingFramework/UCraftingStationComponent.h"
#include "CraftingFramework/CraftingLog.h"
#include "CraftingFramework/UCraftingComponent.h"
#include "CraftingFramework/02Tags/T_CraftingFramework.h"
#include "InteractionCoreFramework/InteractionLockComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UCraftingStationComponent::UCraftingStationComponent(){
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UCraftingStationComponent::BeginPlay(){
    Super::BeginPlay();
    if(GetOwner()->HasAuthority() && !stationId.IsValid()){
        stationId = FGuid::NewGuid();   // nobody gave it one (a test station, a designer-placed prop): a player-created kind id
    }
}

void UCraftingStationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCraftingStationComponent, stationId);
    DOREPLIFETIME(UCraftingStationComponent, inUse);   // the single in-use flag of Doc B18 3.7
}

UCraftingStationComponent* UCraftingStationComponent::findOn(const AActor* actor){
    return actor != nullptr ? actor->FindComponentByClass<UCraftingStationComponent>() : nullptr;
}

FGuid UCraftingStationComponent::getStationId() const{
    return stationId;
}

bool UCraftingStationComponent::isInUse() const{
    return inUse;
}

UCraftingComponent* UCraftingStationComponent::getUser() const{
    return currentUser.Get();
}

UInteractionLockComponent* UCraftingStationComponent::findInteractionLock() const{
    return GetOwner() != nullptr ? GetOwner()->FindComponentByClass<UInteractionLockComponent>() : nullptr;
}

void UCraftingStationComponent::setStationId(const FGuid& newStationId){
    if(GetOwner()->HasAuthority() && newStationId.IsValid()){
        stationId = newStationId;
    }
}

FGameplayTag UCraftingStationComponent::tryLock(UCraftingComponent* user){
    if(user == nullptr){
        return T_Interaction_FailReason_InUse;
    }
    if(currentUser.IsValid() && currentUser.Get() != user){
        return T_Interaction_FailReason_InUse;   // one player at a time; the first request wins (Doc B18 3.7)
    }
    // Doc B18's lock on the same piece: someone holding it through another interaction also makes the station busy.
    UInteractionLockComponent* interactionLock = findInteractionLock();
    AActor* userActor = user->GetOwner();
    if(interactionLock != nullptr && !interactionLock->canAcquire(userActor, E_InteractionLockPolicy::Exclusive)){
        return T_Interaction_FailReason_InUse;
    }
    currentUser = user;
    inUse = true;
    if(interactionLock != nullptr && !interactionLock->isHeldBy(userActor)){
        interactionLock->acquire(userActor, E_InteractionLockPolicy::Exclusive);
    }
    return FGameplayTag();
}

void UCraftingStationComponent::release(UCraftingComponent* user){
    if(user == nullptr || currentUser.Get() != user){
        return;
    }
    currentUser = nullptr;
    inUse = false;
    UInteractionLockComponent* interactionLock = findInteractionLock();
    if(interactionLock != nullptr && interactionLock->isHeldBy(user->GetOwner())){
        interactionLock->release(user->GetOwner());
    }
}

void UCraftingStationComponent::forceRelease(E_CraftingCloseReason reason){
    UCraftingComponent* user = currentUser.Get();
    if(user != nullptr){
        user->closeCraftingSession(reason);   // returns the staged items and releases this station
    }
}

void UCraftingStationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason){
    if(GetOwner() != nullptr && GetOwner()->HasAuthority()){
        // Destroyed or collapsed (Doc B01 3.6): the screen closes with a short notice and the staged items return
        // (R02 3.8, Index A103 S4-F24).
        forceRelease(E_CraftingCloseReason::StationLost);
    }
    Super::EndPlay(EndPlayReason);
}
