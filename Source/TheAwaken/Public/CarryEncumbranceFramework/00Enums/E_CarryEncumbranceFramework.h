#pragma once

#include "CoreMinimal.h"
#include "E_CarryEncumbranceFramework.generated.h"

// B07 §5. Why a carry ended. Drives distinct animation and effect branches.
UENUM(BlueprintType)
enum class E_EndCarryReason : uint8{
    Placed,        // used up by a build: deposited into a blueprint site or placed (Doc B01)
    Thrown,        // went into the throw pipeline (B07 3.4)
    Dropped,       // put down by the player's own choice
    Sent,          // sent along a rope or slider (Doc B09 3.11, Index A92)
    Interrupted    // a forced drop: Downed, death, swimming, sleep, leaving (B07 3.6, Index A103)
};
