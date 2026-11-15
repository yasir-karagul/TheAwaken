#include "CraftingFramework/UBFL_CraftingValidation.h"
#include "CraftingFramework/CraftingLog.h"
#include "CraftingFramework/UCraftingRecipeSubsystem.h"
#include "CraftingFramework/UDA_CraftingRecipe.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"

int32 UBFL_CraftingValidation::validateRecipes(const UObject* worldContext, TArray<FString>& outErrors){
    const UCraftingRecipeSubsystem* recipes = UCraftingRecipeSubsystem::get(worldContext);
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(worldContext);
    if(recipes == nullptr || catalog == nullptr){
        outErrors.Add(TEXT("The recipe catalog or the item catalog is not available."));
        return outErrors.Num();
    }

    // The ammunition of every firearm: none of it is crafted (Index A116, PROPOSED for bullets).
    TSet<FName> firearmAmmoIds;
    for(const TPair<FName, TObjectPtr<UDA_ItemDefinition>>& pair : catalog->getAllDefinitions()){
        if(pair.Value != nullptr && pair.Value->weaponType == E_WeaponType::Firearm && !pair.Value->ammoItemId.IsNone()){
            firearmAmmoIds.Add(pair.Value->ammoItemId);
        }
    }

    for(const FName& duplicateId : recipes->getDuplicateRecipeIds()){
        outErrors.Add(FString::Printf(TEXT("Recipe id %s is used by more than one asset (R02 §12)."), *duplicateId.ToString()));
    }

    for(const TPair<FName, TObjectPtr<UDA_CraftingRecipe>>& pair : recipes->getAllRecipes()){
        const UDA_CraftingRecipe* recipe = pair.Value;
        if(recipe == nullptr){
            continue;
        }
        const FString name = recipe->recipeId.ToString();

        for(const FS_CraftingRecipeRequirement& requirement : recipe->requirementList){
            const UDA_ItemDefinition* ingredient = catalog->getDefinition(requirement.itemId);
            if(ingredient == nullptr){
                outErrors.Add(FString::Printf(TEXT("%s: ingredient %s does not exist (R02 §12)."), *name, *requirement.itemId.ToString()));
                continue;
            }
            if(ingredient->isHeavy()){
                outErrors.Add(FString::Printf(TEXT("%s: ingredient %s has a carry cost. A heavy item never enters the bag, so it can never be staged (Index A103, S4-F01)."), *name, *requirement.itemId.ToString()));
            }
            if(ingredient->isPermanent()){
                outErrors.Add(FString::Printf(TEXT("%s: ingredient %s is a permanent item. The Axe and the map are never ingredients (Index A92)."), *name, *requirement.itemId.ToString()));
            }
        }

        const UDA_ItemDefinition* output = catalog->getDefinition(recipe->outputItemId);
        if(output == nullptr){
            outErrors.Add(FString::Printf(TEXT("%s: output %s does not exist (R02 §12)."), *name, *recipe->outputItemId.ToString()));
            continue;
        }
        if(output->weaponType == E_WeaponType::Firearm){
            outErrors.Add(FString::Printf(TEXT("%s: the output %s is a firearm. Firearms are found in the world and never crafted (Index A116)."), *name, *recipe->outputItemId.ToString()));
        }
        if(firearmAmmoIds.Contains(recipe->outputItemId)){
            outErrors.Add(FString::Printf(TEXT("%s: the output %s is firearm ammunition, which is not crafted (Index A116)."), *name, *recipe->outputItemId.ToString()));
        }
        if(output->isPermanent()){
            outErrors.Add(FString::Printf(TEXT("%s: the output %s is a permanent item."), *name, *recipe->outputItemId.ToString()));
        }
        if(output->isHeavy()){
            CRAFTING_LOG(Warning, worldContext, "Recipe %s makes the heavy item %s: it goes to the hands or lands on the ground, never into the bag (Doc B07 3.1)", *name, *recipe->outputItemId.ToString());
        }
        if(output->isInstanced() && recipe->outputQuantity > 1){
            CRAFTING_LOG(Warning, worldContext, "Recipe %s makes %d copies of the instanced item %s; copies that do not fit the bag limit drop on the ground", *name, recipe->outputQuantity, *recipe->outputItemId.ToString());
        }
    }
    return outErrors.Num();
}
