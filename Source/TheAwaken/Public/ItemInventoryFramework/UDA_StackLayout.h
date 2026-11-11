#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UDA_StackLayout.generated.h"

// How a pile is built (R01 3.1a): unit n always sits at the same place with the same rotation,
// so adding a unit moves no other unit, and nothing is random or saved. A few layouts are shared
// (a heap, a stack, a row, a bundle) and each item picks one.
UCLASS(BlueprintType)
class THEAWAKEN_API UDA_StackLayout : public UDataAsset{
    GENERATED_BODY()

public:
    // Entry 0 is unit 1. The visible capacity of the pile is the length of this list: there is no separate cap.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout") TArray<FS_StackUnit> units;
    // The size a full pile takes in the slot, in the same units as the slot size (I14).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout") FVector2D pileFootprint = FVector2D(100.0, 100.0);

    // Query. How many units the UI draws for a quantity: beyond the end of the list the pile stays as full as it can look.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory")
    int32 getVisibleUnitCount(int32 quantity) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
