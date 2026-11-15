#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "BPI_ProgressionQuery.generated.h"

UINTERFACE(BlueprintType)
class UBPI_ProgressionQuery : public UInterface{
    GENERATED_BODY()
};

// The outgoing seam to Doc R06 Progression. Doc R06 exists in this project, so UCraftingServices registers
// UCraftingProgressionBridge (it asks R06's UProgressionComponent) by default. Tests and the dev command
// Awaken.Craft.StubUnlock swap in UStubProgressionQuery.
class IBPI_ProgressionQuery{
    GENERATED_BODY()

public:
    // Doc R06 §4 IsUnlocked(Tag): is this tag in the shared unlocked set? The set is on the GameState and replicates,
    // so the real implementation answers the same on every machine. Unlocks are shared by everyone in co-op.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool isUnlocked(FGameplayTag unlockTag);
};
