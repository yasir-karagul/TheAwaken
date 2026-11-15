#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CraftingFramework/01Structs/S_CraftingFramework.h"
#include "UDA_CraftingRecipe.generated.h"

// R02 3.1: one recipe. A new recipe is a new asset of this class, never new code.
UCLASS(BlueprintType)
class THEAWAKEN_API UDA_CraftingRecipe : public UPrimaryDataAsset{
    GENERATED_BODY()

public:
    static FPrimaryAssetType getPrimaryAssetType(){
        return FPrimaryAssetType(TEXT("Recipe"));
    }

    virtual FPrimaryAssetId GetPrimaryAssetId() const override{
        return FPrimaryAssetId(getPrimaryAssetType(), recipeId);
    }

    // The stable id (K1). Requests and messages carry this name. Never rename it after release.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") FName recipeId;

    // One entry per ingredient. An item may appear only once. An ingredient may be a weapon (R02 3.10).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe") TArray<FS_CraftingRecipeRequirement> requirementList;
    // What one craft makes. An instanced output is made new, at full condition (R02 3.10).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe") FName outputItemId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = "1")) int32 outputQuantity = 1;

    // Optional. Empty = can be made anywhere (R02 3.9).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gating", meta = (Categories = "Station")) FGameplayTag requiredStationTag;
    // Optional. A tag that must be in the shared unlocked set (Doc R06). Until then the recipe is never listed.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gating", meta = (Categories = "Progression")) FGameplayTag requiredBlueprintTag;

    // The short animation played while the item is made (Doc R07). Presentation only.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation") FGameplayTag animationTag;
    // How long the server waits before the item is made (K2). It must equal the length of the animation above.
    // 0 = the item is made at once.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0")) float craftSeconds = 0.0f;
    // Kept as data. No tab row of the screen uses it for now (Index A115, S12-P8).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation") FGameplayTag recipeCategory;

    // ---- queries ----
    // How many units of that item one craft needs. 0 when the recipe does not use it.
    int32 getRequiredQuantity(FName itemId) const;
    bool usesItem(FName itemId) const{
        return getRequiredQuantity(itemId) > 0;
    }

#if WITH_EDITOR
    // The checks that need only this asset. The checks against the item catalog are in UBFL_CraftingValidation.
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
