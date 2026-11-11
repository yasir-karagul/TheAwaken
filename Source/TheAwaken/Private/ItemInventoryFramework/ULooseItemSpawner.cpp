#include "ItemInventoryFramework/ULooseItemSpawner.h"
#include "ItemInventoryFramework/ALooseItemActor.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

AActor* ULooseItemSpawner::spawnLooseItem_Implementation(APlayerState* instigatorPlayer, const FS_InventoryEntry& entry, FVector location){
    UWorld* world = instigatorPlayer != nullptr ? instigatorPlayer->GetWorld() : GetWorld();
    if(world == nullptr || entry.itemId.IsNone() || entry.totalQuantity < 1){
        return nullptr;
    }
    FActorSpawnParameters parameters;
    parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    ALooseItemActor* looseItem = world->SpawnActor<ALooseItemActor>(location, FRotator::ZeroRotator, parameters);
    if(looseItem != nullptr){
        looseItem->setEntry(entry);
        INVENTORY_LOG(Log, world, "Loose item %s x%d at %s", *entry.itemId.ToString(), entry.totalQuantity, *location.ToCompactString());
    }
    return looseItem;
}
