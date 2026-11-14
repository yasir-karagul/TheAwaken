// Private/InteractionCoreFramework/InteractionLockComponent.cpp
#include "InteractionCoreFramework/InteractionLockComponent.h"
#include "InteractionCoreFramework/InteractionComponent.h"
#include "InteractionCoreFramework/InteractionRules.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/Actor.h"

UInteractionLockComponent::UInteractionLockComponent(){
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UInteractionLockComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    // Every player's prompt needs it, so no condition (B18 3.7).
    DOREPLIFETIME_CONDITION(UInteractionLockComponent, isInUse, COND_None);
}

void UInteractionLockComponent::pruneHolders(){
    holders.RemoveAll([](const TWeakObjectPtr<AActor>& holder){
        return !holder.IsValid();
    });
}

bool UInteractionLockComponent::canAcquire(AActor* player, E_InteractionLockPolicy policy) const{
    bool hasOtherHolder = false;
    for(const TWeakObjectPtr<AActor>& holder : holders){
        if(holder.IsValid() && holder.Get() != player){
            hasOtherHolder = true;
            break;
        }
    }
    return FInteractionRules::lockAllows(policy, hasOtherHolder);
}

void UInteractionLockComponent::acquire(AActor* player, E_InteractionLockPolicy policy){
    if(player == nullptr){
        return;
    }
    pruneHolders();
    holders.AddUnique(player);
    if(policy == E_InteractionLockPolicy::Exclusive){
        holdsExclusive = true;
        isInUse = true;
    }
    UE_LOG(LogAwakenInteraction, Log, TEXT("lock acquired on %s by %s (holders now %d)"), *GetNameSafe(GetOwner()), *GetNameSafe(player), holders.Num());
}

void UInteractionLockComponent::release(AActor* player){
    pruneHolders();
    holders.Remove(player);
    if(holders.IsEmpty()){
        holdsExclusive = false;
    }
    isInUse = holdsExclusive;
    UE_LOG(LogAwakenInteraction, Log, TEXT("lock released on %s by %s (holders now %d)"), *GetNameSafe(GetOwner()), *GetNameSafe(player), holders.Num());
}

bool UInteractionLockComponent::isHeldBy(const AActor* player) const{
    for(const TWeakObjectPtr<AActor>& holder : holders){
        if(holder.Get() == player){
            return true;
        }
    }
    return false;
}

void UInteractionLockComponent::OnRep_isInUse(){
    // Nothing to raise: the focus tick re-reads isInUse when it rebuilds the prompt (B18 3.7 "raise nothing else for locks").
}

void UInteractionLockComponent::EndPlay(const EEndPlayReason::Type endPlayReason){
    // The piece is destroyed or collapses (B18 3.7, Doc B01 3.6): end every hold on it now and forget the holders.
    AActor* owner = GetOwner();
    if(owner != nullptr && owner->HasAuthority()){
        TArray<TWeakObjectPtr<AActor>> previousHolders = holders;
        holders.Reset();
        holdsExclusive = false;
        isInUse = false;
        for(const TWeakObjectPtr<AActor>& holder : previousHolders){
            if(holder.IsValid()){
                if(UInteractionComponent* interaction = holder->FindComponentByClass<UInteractionComponent>()){
                    interaction->notifyTargetDestroyed(owner);
                }
            }
        }
    }
    Super::EndPlay(endPlayReason);
}