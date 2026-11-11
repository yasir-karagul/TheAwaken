#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UDA_InventoryPageLayout.generated.h"

// The fixed slots of one page of the bag screen (R01 3.1a). Adding a slot is an edit of this asset, never new code.
UCLASS(BlueprintType)
class THEAWAKEN_API UDA_InventoryPageLayout : public UDataAsset{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (Categories = "Inventory.Page")) FGameplayTag pageTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout") TArray<FS_InventorySlotDef> slots;

    // Query. Null when the layout has no slot with that id.
    const FS_InventorySlotDef* findSlot(FName slotId) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
