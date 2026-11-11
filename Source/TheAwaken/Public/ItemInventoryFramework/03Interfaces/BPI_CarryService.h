#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "BPI_CarryService.generated.h"

class APlayerState;

UINTERFACE(BlueprintType)
class UBPI_CarryService : public UInterface{
    GENERATED_BODY()
};

class IBPI_CarryService{
    GENERATED_BODY()

public:
    // Doc B07 IsCarryActive(): a carry slot is occupied.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool isCarryActive(APlayerState* player);

    // Doc B07 BeginCarry(): puts a heavy item into a free carry slot. False when the hands cannot take it.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool beginCarry(APlayerState* player, const FS_InventoryEntry& entry);
};
