#include "CarryEncumbranceFramework/UBFL_Carry.h"
#include "CarryEncumbranceFramework/AThrownItemActor.h"
#include "CarryEncumbranceFramework/CarryLog.h"
#include "CarryEncumbranceFramework/UCarryComponent.h"
#include "CarryEncumbranceFramework/02Tags/T_CarryEncumbranceFramework.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

bool UBFL_Carry::isCarryActive(const AActor* carrier){
    const UCarryComponent* carry = UCarryComponent::findOn(carrier);
    return carry != nullptr && carry->isCarryActive();
}

FGameplayTag UBFL_Carry::getHandsBlockReason(const AActor* carrier){
    return isCarryActive(carrier) ? T_Interaction_FailReason_HandsFull.GetTag() : FGameplayTag();
}

int32 UBFL_Carry::getCarriedItemsForCost(const AActor* carrier, FName itemId){
    const UCarryComponent* carry = UCarryComponent::findOn(carrier);
    return carry != nullptr ? carry->getCarriedItemsForCost(itemId) : 0;
}

float UBFL_Carry::getSpeedMultiplier(const AActor* carrier){
    const UCarryComponent* carry = UCarryComponent::findOn(carrier);
    return carry != nullptr ? carry->getSpeedMultiplier() : 1.0f;
}

int32 UBFL_Carry::consumeCarriedForCost(AActor* carrier, FName itemId, int32 quantity){
    UCarryComponent* carry = UCarryComponent::findOn(carrier);
    return carry != nullptr ? carry->consumeCarriedForCost(itemId, quantity) : 0;
}

int32 UBFL_Carry::interruptCarry(AActor* carrier, FGameplayTag causeTag){
    UCarryComponent* carry = UCarryComponent::findOn(carrier);
    return carry != nullptr ? carry->interruptCarry(causeTag) : 0;
}

void UBFL_Carry::handlePlayerLeft(APlayerState* player){
    UCarryComponent* carry = UCarryComponent::findFor(player);
    if(carry != nullptr){
        carry->interruptCarry(T_Carry_InterruptCause_PlayerLeft);   // a carried corpse's despawn timer resumes in its onCarryEnded (Index A106)
    }
}

AThrownItemActor* UBFL_Carry::throwEntry(AActor* thrower, const FS_InventoryEntry& entry, const FVector& origin, const FVector& direction, float speed, float impactDamage){
    UWorld* world = thrower != nullptr ? thrower->GetWorld() : nullptr;
    const FVector throwDirection = direction.GetSafeNormal();
    if(world == nullptr || !thrower->HasAuthority() || entry.itemId.IsNone() || entry.totalQuantity < 1 || throwDirection.IsNearlyZero()){
        return nullptr;
    }
    const FTransform startTransform(throwDirection.Rotation(), origin);
    // Deferred, so the velocity and the entry are set before the actor begins play and before it first replicates.
    AThrownItemActor* thrown = world->SpawnActorDeferred<AThrownItemActor>(AThrownItemActor::StaticClass(), startTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if(thrown == nullptr){
        return nullptr;
    }
    thrown->launch(thrower, entry, throwDirection * speed, impactDamage);
    thrown->FinishSpawning(startTransform);
    CARRY_LOG(Log, thrower, "%s threw %s x%d (speed %.0f, impact damage %.1f)", *GetNameSafe(thrower), *entry.itemId.ToString(), entry.totalQuantity, speed, impactDamage);
    return thrown;
}
