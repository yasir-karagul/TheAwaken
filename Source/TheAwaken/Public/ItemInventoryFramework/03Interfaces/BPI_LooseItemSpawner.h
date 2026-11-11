#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "BPI_LooseItemSpawner.generated.h"

class APlayerState;

UINTERFACE(BlueprintType)
class UBPI_LooseItemSpawner : public UInterface{
    GENERATED_BODY()
};

class IBPI_LooseItemSpawner{
    GENERATED_BODY()

public:
    // Puts one entry on the ground as a loose item (Doc02 19.7). instigatorPlayer may be null.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    AActor* spawnLooseItem(APlayerState* instigatorPlayer, const FS_InventoryEntry& entry, FVector location);
};
