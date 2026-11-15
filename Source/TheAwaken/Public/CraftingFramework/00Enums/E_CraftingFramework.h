#pragma once

#include "CoreMinimal.h"
#include "E_CraftingFramework.generated.h"

// R02 3.4, §6 (names PROPOSED, Index A115). The three states of a craft button.
UENUM(BlueprintType)
enum class E_RecipeListState : uint8{
    Craftable,     // green: nothing missing, nothing extra. The only state that can be pressed
    ExtraItems,    // white: nothing missing, but an item type the recipe does not use is staged
    Missing        // red: an ingredient quantity or the station is missing. Red wins over white
};

// R02 3.8: why a session ended (K11). The screen uses it to choose its notice.
UENUM(BlueprintType)
enum class E_CraftingCloseReason : uint8{
    BagClosed,       // the player closed the bag: the normal end
    Downed,          // the player is Downed (Index A115, S12-P3)
    Died,
    PlayerLeft,      // disconnect or leaving the session (Doc B20 3.6)
    SceneStarted,    // a scene locked the player's input (Doc R17)
    StationLost,     // the station was destroyed or collapsed (Index A103, S4-F24)
    OutOfRange,      // something moved the player away from the station (Doc B18 3.7)
    BagLost,         // the bag is gone while the session was open
    PawnRemoved      // the safety net of K12: the Character was destroyed and nobody closed the session first
};
