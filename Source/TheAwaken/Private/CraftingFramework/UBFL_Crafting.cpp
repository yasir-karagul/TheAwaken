#include "CraftingFramework/UBFL_Crafting.h"
#include "CraftingFramework/CraftingServices.h"
#include "CraftingFramework/UCraftingComponent.h"
#include "CraftingFramework/UCraftingRecipeSubsystem.h"
#include "CraftingFramework/UCraftingSettings.h"
#include "CraftingFramework/UDA_CraftingRecipe.h"
#include "GameFramework/PlayerState.h"

FS_AvailableRecipeOption UBFL_Crafting::evaluateRecipe(const UDA_CraftingRecipe& recipe, const TMap<FName, int32>& stagedQuantities, const FGameplayTagContainer& stationTags){
    FS_AvailableRecipeOption option;
    option.recipeId = recipe.recipeId;

    // What is missing, and how many times the staged items would make the recipe.
    int32 craftableCount = MAX_int32;
    for(const FS_CraftingRecipeRequirement& requirement : recipe.requirementList){
        const int32 staged = stagedQuantities.FindRef(requirement.itemId);
        if(staged < requirement.quantity){
            FS_CraftingRecipeRequirement missing;
            missing.itemId = requirement.itemId;
            missing.quantity = requirement.quantity - staged;
            option.missingIngredients.Add(missing);
        }
        craftableCount = FMath::Min(craftableCount, staged / FMath::Max(1, requirement.quantity));
    }
    // A station recipe is red away from the station (R02 3.9).
    option.isStationMissing = recipe.requiredStationTag.IsValid() && !stationTags.HasTag(recipe.requiredStationTag);

    // What is staged that the recipe does not use. Extra QUANTITY of a used item is not an extra item.
    for(const TPair<FName, int32>& staged : stagedQuantities){
        if(staged.Value > 0 && !recipe.usesItem(staged.Key)){
            option.extraItems.Add(staged.Key);
        }
    }
    option.extraItems.Sort([](const FName& left, const FName& right){
        return left.LexicalLess(right);
    });

    // Red wins over white (R02 3.4).
    if(option.missingIngredients.Num() > 0 || option.isStationMissing || recipe.requirementList.Num() == 0){
        option.state = E_RecipeListState::Missing;
        option.maxCraftableCount = 0;
    }else{
        option.state = option.extraItems.Num() > 0 ? E_RecipeListState::ExtraItems : E_RecipeListState::Craftable;
        option.maxCraftableCount = craftableCount;
    }
    return option;
}

TArray<FS_AvailableRecipeOption> UBFL_Crafting::buildRecipeList(const UObject* worldContext, const TMap<FName, int32>& stagedQuantities, const FGameplayTagContainer& stationTags){
    TArray<FS_AvailableRecipeOption> green;
    TArray<FS_AvailableRecipeOption> white;
    TArray<FS_AvailableRecipeOption> red;
    const UCraftingRecipeSubsystem* recipes = UCraftingRecipeSubsystem::get(worldContext);
    const UCraftingServices* services = UCraftingServices::get(worldContext);
    if(recipes == nullptr || services == nullptr){
        return green;
    }

    // From the staged item types to the recipes that use them: nothing else is looked at (R02 3.1, review R02-06).
    TSet<FName> seenRecipeIds;
    for(const TPair<FName, int32>& staged : stagedQuantities){
        if(staged.Value <= 0){
            continue;
        }
        for(const UDA_CraftingRecipe* recipe : recipes->getRecipesUsing(staged.Key)){
            if(seenRecipeIds.Contains(recipe->recipeId)){
                continue;
            }
            seenRecipeIds.Add(recipe->recipeId);
            if(!services->isUnlocked(recipe->requiredBlueprintTag)){
                continue;   // a locked recipe is never listed until it is unlocked (R02 3.9)
            }
            const FS_AvailableRecipeOption option = evaluateRecipe(*recipe, stagedQuantities, stationTags);
            if(option.state == E_RecipeListState::Craftable){
                green.Add(option);
            }else if(option.state == E_RecipeListState::ExtraItems){
                white.Add(option);
            }else{
                red.Add(option);
            }
        }
    }

    // A stable order, so the list does not shuffle while the player stages (K5).
    const auto byRecipeId = [](const FS_AvailableRecipeOption& left, const FS_AvailableRecipeOption& right){
        return left.recipeId.LexicalLess(right.recipeId);
    };
    green.Sort(byRecipeId);
    white.Sort(byRecipeId);
    red.Sort([](const FS_AvailableRecipeOption& left, const FS_AvailableRecipeOption& right){
        const int32 leftMissing = left.getMissingCount();
        const int32 rightMissing = right.getMissingCount();
        return leftMissing != rightMissing ? leftMissing < rightMissing : left.recipeId.LexicalLess(right.recipeId);
    });

    // Every green and every white is always listed; reds only fill what is left of the cap (R02 3.4).
    TArray<FS_AvailableRecipeOption> list = MoveTemp(green);
    list.Append(white);
    const int32 placesLeft = UCraftingSettings::get()->maxVisibleRecipes - list.Num();
    for(int32 index = 0; index < red.Num() && index < placesLeft; index++){
        list.Add(red[index]);
    }
    return list;
}

// ---------- for other frameworks (CR7.3) ----------

bool UBFL_Crafting::isSessionOpen(const AActor* crafter){
    const UCraftingComponent* crafting = UCraftingComponent::findOn(crafter);
    return crafting != nullptr && crafting->isSessionOpen();
}

void UBFL_Crafting::closeSession(AActor* crafter, E_CraftingCloseReason reason){
    UCraftingComponent* crafting = UCraftingComponent::findOn(crafter);
    if(crafting != nullptr){
        crafting->closeCraftingSession(reason);
    }
}

void UBFL_Crafting::handlePlayerLeft(APlayerState* player){
    UCraftingComponent* crafting = UCraftingComponent::findFor(player);
    if(crafting != nullptr){
        crafting->closeCraftingSession(E_CraftingCloseReason::PlayerLeft);
    }
}
