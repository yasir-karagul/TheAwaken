// TEMPORARY STUBS — the owner of each is named on its class. Delete a class when the real framework lands.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "UObject/Object.h"
#include "CraftingFramework/03Interfaces/BPI_ProgressionQuery.h"
#include "CraftingStubs.generated.h"

class UCraftingStationComponent;

// TEMPORARY STUB — owner: Doc R06 Progression. A local set of tags. It does NOT replicate: on a client it knows only
// what was set on that client, so two-player tests of a locked recipe set the same tag on both machines.
// The real R06 set is reached through UCraftingProgressionBridge, which is the default; this stub is registered only by
// tests and by the dev command Awaken.Craft.StubUnlock (for maps whose GameState has no Progression component).
UCLASS()
class UStubProgressionQuery : public UObject, public IBPI_ProgressionQuery{
    GENERATED_BODY()

public:
    UPROPERTY() FGameplayTagContainer unlockedTags;
    // The Peaceful bulk grant of Doc B13 / R06 in one switch.
    UPROPERTY() bool unlockEverything = false;

    virtual bool isUnlocked_Implementation(FGameplayTag unlockTag) override;
};

// TEMPORARY STUB — owner: Doc B01 Building. An actor that is only a crafting station. Shows a small cube without
// collision so a spawned station can be seen.
UCLASS()
class AStubCraftingStation : public AActor{
    GENERATED_BODY()

public:
    AStubCraftingStation();

    UPROPERTY(VisibleAnywhere) TObjectPtr<UCraftingStationComponent> stationComponent;
};
