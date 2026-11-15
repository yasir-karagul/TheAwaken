#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "CraftingFramework/03Interfaces/BPI_ProgressionQuery.h"
#include "CraftingProgressionBridge.generated.h"

// The real IBPI_ProgressionQuery: Doc R06 already exists in this project (UProgressionComponent on the GameState), so
// Crafting asks it directly instead of using the guide's stub (CR1.6). The answer is the replicated shared set, the same
// on every machine. A world whose GameState has no Progression component (TestMap_Inventory) answers "locked".
UCLASS()
class THEAWAKEN_API UCraftingProgressionBridge : public UObject, public IBPI_ProgressionQuery{
    GENERATED_BODY()

public:
    virtual bool isUnlocked_Implementation(FGameplayTag unlockTag) override;
};
