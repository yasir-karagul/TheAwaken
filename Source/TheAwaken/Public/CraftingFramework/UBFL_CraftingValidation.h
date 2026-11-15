#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UBFL_CraftingValidation.generated.h"

UCLASS()
class THEAWAKEN_API UBFL_CraftingValidation : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // R02 §12. Checks every loaded recipe against the item catalog. Returns the number of errors and fills outErrors
    // with one line each. Warnings go to the log only.
    static int32 validateRecipes(const UObject* worldContext, TArray<FString>& outErrors);
};
