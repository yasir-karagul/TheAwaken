#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ItemInventoryFramework/03Interfaces/BPI_LooseItemSpawner.h"
#include "ULooseItemSpawner.generated.h"

// The real implementation of the loose-item seam: spawns an ALooseItemActor.
UCLASS()
class THEAWAKEN_API ULooseItemSpawner : public UObject, public IBPI_LooseItemSpawner{
    GENERATED_BODY()

public:
    virtual AActor* spawnLooseItem_Implementation(APlayerState* instigatorPlayer, const FS_InventoryEntry& entry, FVector location) override;
};
