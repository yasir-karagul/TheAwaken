#pragma once
#include "CoreMinimal.h"
#include "E_CheckpointRespawnFramework.generated.h"

// TEMPORARY until Doc B02 is built: the life state the Stats framework reads (S5 step 4). Doc B02 owns this enum.
UENUM(BlueprintType)
enum class E_PlayerLifeState : uint8{
    Healthy,
    Downed,
    Restrained,
    Arriving,
    Dead
};
