#include "CraftingFramework/UCraftingRecipeSubsystem.h"
#include "CraftingFramework/UDA_CraftingRecipe.h"
#include "CraftingFramework/CraftingLog.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

void UCraftingRecipeSubsystem::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);
    loadAllRecipes();
    CRAFTING_LOG(Log, this, "Recipe catalog loaded: %d recipes", recipes.Num());
}

UCraftingRecipeSubsystem* UCraftingRecipeSubsystem::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    if(gameInstance == nullptr){
        return nullptr;
    }
    return gameInstance->GetSubsystem<UCraftingRecipeSubsystem>();
}

void UCraftingRecipeSubsystem::loadAllRecipes(){
    UAssetManager* manager = UAssetManager::GetIfInitialized();
    if(manager == nullptr){
        return;
    }
    TArray<FPrimaryAssetId> ids;
    manager->GetPrimaryAssetIdList(UDA_CraftingRecipe::getPrimaryAssetType(), ids);
    for(const FPrimaryAssetId& id : ids){
        UDA_CraftingRecipe* recipe = Cast<UDA_CraftingRecipe>(manager->GetPrimaryAssetPath(id).TryLoad());
        if(recipe != nullptr){
            registerRecipe(recipe);
        }
    }
}

void UCraftingRecipeSubsystem::registerRecipe(UDA_CraftingRecipe* recipe){
    if(recipe == nullptr || recipe->recipeId.IsNone()){
        CRAFTING_LOG(Error, this, "Recipe %s has no recipeId and was not registered", *GetNameSafe(recipe));
        return;
    }
    if(recipes.Contains(recipe->recipeId)){
        duplicateRecipeIds.AddUnique(recipe->recipeId);
        CRAFTING_LOG(Error, this, "Duplicate recipeId %s: %s was not registered", *recipe->recipeId.ToString(), *recipe->GetName());
        return;
    }
    recipes.Add(recipe->recipeId, recipe);
    for(const FS_CraftingRecipeRequirement& requirement : recipe->requirementList){
        recipeIdsByIngredient.FindOrAdd(requirement.itemId).AddUnique(recipe->recipeId);
    }
}

void UCraftingRecipeSubsystem::resetForTest(){
    recipes.Reset();
    recipeIdsByIngredient.Reset();
    duplicateRecipeIds.Reset();
}

const UDA_CraftingRecipe* UCraftingRecipeSubsystem::getRecipe(FName recipeId) const{
    const TObjectPtr<UDA_CraftingRecipe>* found = recipes.Find(recipeId);
    return found != nullptr ? found->Get() : nullptr;
}

TArray<const UDA_CraftingRecipe*> UCraftingRecipeSubsystem::getRecipesUsing(FName itemId) const{
    TArray<const UDA_CraftingRecipe*> result;
    const TArray<FName>* recipeIds = recipeIdsByIngredient.Find(itemId);
    if(recipeIds != nullptr){
        for(const FName& recipeId : *recipeIds){
            const UDA_CraftingRecipe* recipe = getRecipe(recipeId);
            if(recipe != nullptr){
                result.Add(recipe);
            }
        }
    }
    return result;
}

const TMap<FName, TObjectPtr<UDA_CraftingRecipe>>& UCraftingRecipeSubsystem::getAllRecipes() const{
    return recipes;
}

const TArray<FName>& UCraftingRecipeSubsystem::getDuplicateRecipeIds() const{
    return duplicateRecipeIds;
}
