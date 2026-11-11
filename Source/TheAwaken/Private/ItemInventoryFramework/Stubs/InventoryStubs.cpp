// TEMPORARY STUBS — see InventoryStubs.h for the owner of each.
#include "ItemInventoryFramework/Stubs/InventoryStubs.h"

int64 UStubGameClock::getGameTime_Implementation(){
    return gameTime;
}

bool UStubPlayerStateQuery::isIncapacitated_Implementation(APlayerState* player){
    return incapacitatedPlayers.Contains(player);
}

bool UStubCarryService::isCarryActive_Implementation(APlayerState* player){
    return carryingPlayers.Contains(player);
}

bool UStubCarryService::beginCarry_Implementation(APlayerState* player, const FS_InventoryEntry& entry){
    if(!acceptsNewCarry){
        return false;
    }
    carriedEntries.Add(entry);
    return true;
}

void UStubSurvivalService::modifyMeter_Implementation(APlayerState* player, FGameplayTag meterTag, float delta, FGameplayTag sourceTag){
    meterTotals.FindOrAdd(meterTag) += delta;
}

void UStubSurvivalService::registerIllnessSource_Implementation(APlayerState* player, FGameplayTag sourceTag, float durationHours){
    registeredSources.Add(sourceTag);
}

bool UStubSurvivalService::isIllnessActive_Implementation(APlayerState* player){
    return illnessActive;
}

void UStubHealthService::healPlayer_Implementation(APlayerState* player, float amount, FGameplayTag sourceTag){
    totalHealed += amount;
}

void UStubHealthService::damagePlayer_Implementation(APlayerState* player, float amount, FGameplayTag damageTypeTag){
    totalDamaged += amount;
    lastDamageType = damageTypeTag;
}

void UStubActionService::cancelAction_Implementation(APlayerState* player, FGameplayTag actionTag){
    cancelledActions.Add(actionTag);
}

AActor* UStubLooseItemSpawner::spawnLooseItem_Implementation(APlayerState* instigatorPlayer, const FS_InventoryEntry& entry, FVector location){
    spawnedEntries.Add(entry);
    spawnLocations.Add(location);
    return nullptr;
}

bool UStubBagActorService::createBag_Implementation(FGuid bagId, FGuid ownerCharacterId, FVector location){
    FS_BagInfo info;
    info.isValid = true;
    info.bagId = bagId;
    info.ownerCharacterId = ownerCharacterId;
    info.location = location;
    bags.Add(bagId, info);
    return true;
}

void UStubBagActorService::setBagContents_Implementation(FGuid bagId, const FS_BagContents& contents){
    FS_BagInfo* info = bags.Find(bagId);
    if(info != nullptr){
        info->contents = contents;
    }
}

FS_BagInfo UStubBagActorService::findBag_Implementation(FGuid bagId){
    const FS_BagInfo* info = bags.Find(bagId);
    return info != nullptr ? *info : FS_BagInfo();
}

FS_BagInfo UStubBagActorService::findBagOwnedBy_Implementation(FGuid ownerCharacterId){
    for(const TPair<FGuid, FS_BagInfo>& pair : bags){
        if(pair.Value.ownerCharacterId == ownerCharacterId){
            return pair.Value;
        }
    }
    return FS_BagInfo();
}

void UStubBagActorService::deleteBag_Implementation(FGuid bagId){
    bags.Remove(bagId);
}

TArray<FGuid> UStubBagActorService::removeBagsOwnedBy_Implementation(FGuid ownerCharacterId){
    TArray<FGuid> inventoryIds;
    for(auto iterator = bags.CreateIterator(); iterator; ++iterator){
        if(iterator.Value().ownerCharacterId == ownerCharacterId){
            inventoryIds.Add(iterator.Value().contents.inventoryId);
            iterator.RemoveCurrent();
        }
    }
    return inventoryIds;
}
