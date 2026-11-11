#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AbilityActionFramework/01Structs/S_AbilityActionFramework.h"
#include "DA_ActionInputMap.generated.h"

// A5 step 2. The only place an input action is tied to an Action tag. Key rebinding stays in the
// Enhanced Input mapping contexts (Doc B17 3.13).
UCLASS(BlueprintType)
class THEAWAKEN_API UDA_ActionInputMap : public UPrimaryDataAsset{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly)
    TArray<FS_ActionInputBinding> bindings;
};
