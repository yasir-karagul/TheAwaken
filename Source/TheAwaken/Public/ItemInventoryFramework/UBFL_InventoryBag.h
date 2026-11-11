#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UBFL_InventoryBag.generated.h"

class APlayerState;

UCLASS()
class THEAWAKEN_API UBFL_InventoryBag : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // ---- queries ----
    // R01 §5 HasBag: true while the Personal Inventory exists.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") static bool hasBag(const APlayerState* player);
    // R01 §5 GetUsableHotbarSlotCount: the full hotbar with the bag, 3 while Bagless.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") static int32 getUsableHotbarSlotCount(const APlayerState* player);

    // ---- commands, server only ----
    // R01 §5 DetachBag. Atomic: everything moves or nothing does. Called by Doc B02 with the spot the bag will lie at.
    static FS_BagDetachResult detachBag(APlayerState* player, const FVector& location);
    // R01 §5 ReattachBag. Atomic. Called by Doc B02 when the owner's pick-up hold completes.
    static FS_InventoryResult reattachBag(APlayerState* player, const FGuid& bagId);
    // R01 §5 DiscardBaglessItems: a player died Bagless, so what the hold slots held is destroyed.
    static int32 discardBaglessItems(APlayerState* player);
    // A Bagless player eats, drinks or uses medicine from a hold slot (Index A110, S2-F33).
    static FS_InventoryResult useConsumableFromHoldSlot(APlayerState* player, int32 holdIndex);
    // A Bagless player puts a held item down as a loose item (I19).
    static FS_InventoryResult dropHoldSlot(APlayerState* player, int32 holdIndex);
    // The bag, the Axe and the map, once per character (R01 3.8). A second call is refused with KitAlreadyGranted.
    static FS_InventoryResult grantStartingKit(APlayerState* player);
    // A fresh start, Start Again, or a character given by the system: the old character's bags are removed for good.
    static void startNewCharacter(APlayerState* player, const FGuid& newCharacterId);

private:
    // Tests only: lets a test make step 1 or step 2 of detachBag fail.
    static bool shouldFailStep(int32 step);
};
