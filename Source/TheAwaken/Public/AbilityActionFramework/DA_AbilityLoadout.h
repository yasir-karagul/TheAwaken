#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AbilityActionFramework/01Structs/S_AbilityActionFramework.h"
#include "DA_AbilityLoadout.generated.h"

// A2 step 2. One loadout for the player; NPCs get their own (A13).
UCLASS(BlueprintType)
class THEAWAKEN_API UDA_AbilityLoadout : public UPrimaryDataAsset{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly)
    TArray<FS_AbilityLoadoutEntry> entries;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& context) const override;
#endif
};
