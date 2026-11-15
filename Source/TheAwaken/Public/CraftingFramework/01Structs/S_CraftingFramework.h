#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CraftingFramework/00Enums/E_CraftingFramework.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "S_CraftingFramework.generated.h"

class AActor;

// Declared here, not in a delegate-only header: UHT does not register a dynamic delegate that sits alone in a header
// (the same fallback as R01 S1.9, R05 and B07). D_CraftingFramework.h includes this file.
// Raised on the server and on the owning client after any change to the session state: a station entered or left,
// the session opened or closed, a craft started or ended.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FD_CraftingChanged, AActor*, crafter);

// ---------- Recipe data (R02 3.1, §6) ----------

// One ingredient of a recipe. Also used for "still missing" in a list entry.
USTRUCT(BlueprintType)
struct FS_CraftingRecipeRequirement{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName itemId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1")) int32 quantity = 1;
};

// ---------- The craft button list (R02 3.4, §6; names PROPOSED, Index A115) ----------

// One craft button.
USTRUCT(BlueprintType)
struct FS_AvailableRecipeOption{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName recipeId;
    UPROPERTY(BlueprintReadOnly) E_RecipeListState state = E_RecipeListState::Missing;
    // The item id and the quantity still missing, one entry per ingredient that is short.
    UPROPERTY(BlueprintReadOnly) TArray<FS_CraftingRecipeRequirement> missingIngredients;
    // True when the recipe needs a station and the player is not at one that matches. The hover card then
    // shows the text key UI.Crafting.NeedsStation.
    UPROPERTY(BlueprintReadOnly) bool isStationMissing = false;
    // The staged item types the recipe does not use.
    UPROPERTY(BlueprintReadOnly) TArray<FName> extraItems;
    // How many times the staged items would make it. Data: the screen does not have to show it.
    UPROPERTY(BlueprintReadOnly) int32 maxCraftableCount = 0;

    // The number that orders the red entries: every missing unit counts 1, a missing station counts 1 (K5).
    int32 getMissingCount() const{
        int32 count = isStationMissing ? 1 : 0;
        for(const FS_CraftingRecipeRequirement& missing : missingIngredients){
            count += missing.quantity;
        }
        return count;
    }
};

// ---------- Session state: replicated to the owning player only (K8, K19) ----------

USTRUCT(BlueprintType)
struct FS_CraftingSessionState{
    GENERATED_BODY()

    // True from the first staged ingredient until the session ends (R02 3.2).
    UPROPERTY(BlueprintReadOnly) bool isOpen = false;
    // The station context (K8): set when the player enters a station, before anything is staged.
    UPROPERTY(BlueprintReadOnly) bool hasStation = false;
    // Invalid for a hand-craft session. It is also the key of the reservations (R01 guide S7).
    UPROPERTY(BlueprintReadOnly) FGuid stationId;
    UPROPERTY(BlueprintReadOnly) FGameplayTagContainer stationTags;
    // The recipe being made right now. None = no craft is running.
    UPROPERTY(BlueprintReadOnly) FName activeRecipeId;
    // The length of the running craft, for the progress the screen may show.
    UPROPERTY(BlueprintReadOnly) float activeCraftSeconds = 0.0f;
};

// ---------- Message payloads (R02 §7, Doc02 §11) ----------

// Crafting.SessionOpened
USTRUCT(BlueprintType)
struct FS_CraftingSessionOpenedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> crafter = nullptr;
    // Invalid for a hand-craft session.
    UPROPERTY(BlueprintReadOnly) FGuid stationId;
};

// Crafting.ItemCrafted
USTRUCT(BlueprintType)
struct FS_CraftingItemCraftedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> crafter = nullptr;
    UPROPERTY(BlueprintReadOnly) FName recipeId;
    UPROPERTY(BlueprintReadOnly) FName outputItemId;
    UPROPERTY(BlueprintReadOnly) int32 outputQuantity = 0;
};

// Crafting.SessionClosed
USTRUCT(BlueprintType)
struct FS_CraftingSessionClosedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> crafter = nullptr;
    // What was still staged and is available again. The items never left the bag.
    UPROPERTY(BlueprintReadOnly) TArray<FS_ItemStack> returnedItems;
    UPROPERTY(BlueprintReadOnly) E_CraftingCloseReason reason = E_CraftingCloseReason::BagClosed;
};
