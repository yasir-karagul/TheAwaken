#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "BPI_EquipmentGear.generated.h"

class APlayerState;

UINTERFACE(BlueprintType)
class UBPI_EquipmentGear : public UInterface{
    GENERATED_BODY()
};

class IBPI_EquipmentGear{
    GENERATED_BODY()

public:
    // Doc R05 ReleaseEquippedGear. Empties the worn armor and RETURNS the pieces; R01 stores them in the bag.
    // Must change nothing when it reports failure.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FS_GearReleaseResult releaseEquippedGear(APlayerState* player);

    // Takes each listed instance out of the recovered bag and puts it back on, in the place it occupied (Doc B02).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool reEquipGear(APlayerState* player, const TArray<FS_GearSlotRecord>& gear);
};
