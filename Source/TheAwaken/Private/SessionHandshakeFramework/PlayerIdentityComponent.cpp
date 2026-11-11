#include "SessionHandshakeFramework/PlayerIdentityComponent.h"
#include "SessionHandshakeFramework/02Tags/T_SessionHandshakeFramework.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

UPlayerIdentityComponent::UPlayerIdentityComponent(){
    PrimaryComponentTick.bCanEverTick = false;   // Doc02 5: nothing here is polled
    SetIsReplicatedByDefault(true);
}

void UPlayerIdentityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UPlayerIdentityComponent, primaryId, COND_None);
    DOREPLIFETIME_CONDITION(UPlayerIdentityComponent, kind, COND_None);
}

bool UPlayerIdentityComponent::setIdentityServer(const FS_PlayerIdentity& newIdentity){
    const AActor* owner = GetOwner();
    if(owner == nullptr || !owner->HasAuthority() || !newIdentity.isValid()){
        return false;
    }
    if(!primaryId.IsEmpty()){
        // Written once. The same id again is a quiet success (a reconnect); a different id is refused.
        return primaryId.Equals(newIdentity.primaryId, ESearchCase::IgnoreCase);
    }
    primaryId = newIdentity.primaryId;
    kind = newIdentity.kind;
    linkedIds = newIdentity.linkedIds;
    raiseIdentityMessage();   // the host raises it here; a client raises it from OnRep_primaryId
    return true;
}

void UPlayerIdentityComponent::OnRep_primaryId(){
    // kind arrives in the same update, so both are set by now.
    raiseIdentityMessage();
}

void UPlayerIdentityComponent::raiseIdentityMessage() const{
    AActor* owner = GetOwner();
    const UWorld* world = owner != nullptr ? owner->GetWorld() : nullptr;
    UGameInstance* gameInstance = world != nullptr ? world->GetGameInstance() : nullptr;
    UGameplayMessageSubsystem* messages = gameInstance != nullptr ? gameInstance->GetSubsystem<UGameplayMessageSubsystem>() : nullptr;
    if(messages == nullptr){
        return;
    }
    FS_PlayerIdentityReplicatedMessage message;
    message.playerState = Cast<APlayerState>(owner);
    message.primaryId = primaryId;
    message.kind = kind;
    messages->BroadcastMessage(T_Session_PlayerIdentity_Replicated, message);
}

FS_PlayerIdentity UPlayerIdentityComponent::getIdentity() const{
    FS_PlayerIdentity identity;
    identity.primaryId = primaryId;
    identity.kind = kind;
    identity.linkedIds = linkedIds;
    return identity;
}

bool UPlayerIdentityComponent::hasIdentity() const{
    return !primaryId.IsEmpty();
}
