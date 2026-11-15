#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CraftingFramework/01Structs/S_CraftingFramework.h"
#include "UBFL_Crafting.generated.h"

class AActor;
class APlayerState;
class UDA_CraftingRecipe;

UCLASS()
class THEAWAKEN_API UBFL_Crafting : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // ---- queries: the rule of R02 3.4, with no state of their own (K3) ----
    // The state of ONE recipe for these staged items and these station tags. It does not look at the blueprint.
    static FS_AvailableRecipeOption evaluateRecipe(const UDA_CraftingRecipe& recipe, const TMap<FName, int32>& stagedQuantities, const FGameplayTagContainer& stationTags);
    // The capped, ordered craft button list: green, then white, then red. Locked recipes are left out.
    static TArray<FS_AvailableRecipeOption> buildRecipeList(const UObject* worldContext, const TMap<FName, int32>& stagedQuantities, const FGameplayTagContainer& stationTags);

    // ---- for other frameworks (CR7.3) ----
    // Query. Works on the server and on the crafter's own machine.
    UFUNCTION(BlueprintPure, Category = "Awaken|Crafting") static bool isSessionOpen(const AActor* crafter);
    // R02 3.8. Server. Ends the crafter's session for that reason. Safe to call when none is open.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Crafting") static void closeSession(AActor* crafter, E_CraftingCloseReason reason);
    // Cleanup for Session.PlayerLeft (Doc B20 3.6): the leaving player's session ends and its materials are returned.
    static void handlePlayerLeft(APlayerState* player);
};
