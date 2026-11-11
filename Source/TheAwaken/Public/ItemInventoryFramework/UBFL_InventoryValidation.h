#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UBFL_InventoryValidation.generated.h"

UCLASS()
class THEAWAKEN_API UBFL_InventoryValidation : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // Query. Checks every loaded item against the rules of R01 3.1a, 3.2a and 3.10 and Doc02 19.8.
    // Fills the list with one line per error and returns how many there are.
    static int32 validateCatalog(const UObject* worldContext, TArray<FString>& outErrors);
};
