// TEMPORARY STUB — owners: Doc B19 (classes) and Docs B05 / B20 (who creates a character and grants the starting kit).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AInventoryTestGameMode.generated.h"

UCLASS()
class AInventoryTestGameMode : public AGameModeBase{
    GENERATED_BODY()

public:
    AInventoryTestGameMode();

    virtual void PostLogin(APlayerController* NewPlayer) override;
};
