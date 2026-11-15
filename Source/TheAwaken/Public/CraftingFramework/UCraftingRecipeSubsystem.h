#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UCraftingRecipeSubsystem.generated.h"

class UDA_CraftingRecipe;

// The recipe catalog (R02 3.1). Looks a recipe up by its id, or by an ingredient. Exists on every machine; the data is
// static content. It loads every recipe once, when the game instance starts (a recipe holds ids and tags only, so this
// loads no art), and builds the index from an item type to the recipes that use it.
UCLASS()
class THEAWAKEN_API UCraftingRecipeSubsystem : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static UCraftingRecipeSubsystem* get(const UObject* worldContext);

    // ---- queries ----
    // Null when no recipe has that id. Never crashes.
    const UDA_CraftingRecipe* getRecipe(FName recipeId) const;
    // Every recipe that has this item among its ingredients (R02 3.1, review R02-06).
    TArray<const UDA_CraftingRecipe*> getRecipesUsing(FName itemId) const;
    const TMap<FName, TObjectPtr<UDA_CraftingRecipe>>& getAllRecipes() const;
    // Recipe ids that were found more than once when loading. The data check reports them (R02 §12).
    const TArray<FName>& getDuplicateRecipeIds() const;

    // ---- commands ----
    // Adds one recipe and indexes it. Used by the loader and by tests.
    void registerRecipe(UDA_CraftingRecipe* recipe);
    // Tests only: forget every recipe.
    void resetForTest();

private:
    void loadAllRecipes();

    UPROPERTY() TMap<FName, TObjectPtr<UDA_CraftingRecipe>> recipes;
    // The index: ingredient item id -> the ids of the recipes that use it.
    TMap<FName, TArray<FName>> recipeIdsByIngredient;
    TArray<FName> duplicateRecipeIds;
};
