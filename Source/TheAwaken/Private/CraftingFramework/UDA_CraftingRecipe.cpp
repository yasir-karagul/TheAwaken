#include "CraftingFramework/UDA_CraftingRecipe.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

int32 UDA_CraftingRecipe::getRequiredQuantity(FName itemId) const{
    for(const FS_CraftingRecipeRequirement& requirement : requirementList){
        if(requirement.itemId == itemId){
            return requirement.quantity;
        }
    }
    return 0;
}

#if WITH_EDITOR
EDataValidationResult UDA_CraftingRecipe::IsDataValid(FDataValidationContext& Context) const{
    EDataValidationResult result = Super::IsDataValid(Context);
    if(result == EDataValidationResult::NotValidated){
        result = EDataValidationResult::Valid;
    }
    const auto fail = [&Context, &result](const TCHAR* text){
        Context.AddError(FText::FromString(text));
        result = EDataValidationResult::Invalid;
    };

    if(recipeId.IsNone()){
        fail(TEXT("recipeId is empty. It is the stable id of the recipe (Doc02 19.8)."));
    }
    if(requirementList.Num() == 0){
        fail(TEXT("A recipe needs at least one ingredient: a button is listed only for a staged item (R02 3.4)."));
    }
    if(outputItemId.IsNone()){
        fail(TEXT("outputItemId is empty."));
    }
    TSet<FName> seenItems;
    for(const FS_CraftingRecipeRequirement& requirement : requirementList){
        if(requirement.itemId.IsNone() || requirement.quantity < 1){
            fail(TEXT("Every ingredient needs an item id and a quantity of at least 1."));
        }
        if(seenItems.Contains(requirement.itemId)){
            fail(TEXT("An item appears twice in requirementList. Use one entry with the total quantity."));
        }
        seenItems.Add(requirement.itemId);
    }
    if(animationTag.IsValid() && craftSeconds <= 0.0f){
        Context.AddWarning(FText::FromString(TEXT("The recipe has an animation but craftSeconds is 0, so the item is made before the animation ends (K2).")));
    }
    return result;
}
#endif
