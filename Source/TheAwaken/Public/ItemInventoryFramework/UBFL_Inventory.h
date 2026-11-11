#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UBFL_Inventory.generated.h"

class ALooseItemActor;
class APlayerState;
class UDA_ItemDefinition;
class UInventoryData;

UCLASS()
class THEAWAKEN_API UBFL_Inventory : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // ---- queries ----
    // True on the host and offline.
    static bool isServerWorld(const UObject* worldContext);
    // Where an item dropped by this player appears: a short way in front of the pawn.
    static FVector getDropLocation(const APlayerState* player);
    // A new copy of an instanced item: a new instance id, full durability, and for a Portable Container the id of its (still empty) contents.
    static FS_InventoryEntry makeFreshInstance(const UObject* worldContext, const UDA_ItemDefinition& definition);
    // R01 §5 CanFit: whether the whole stack fits.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") static bool canFit(const UInventoryData* inventory, const FS_ItemStack& stack);

    // ---- commands, server only ----
    // R01 3.7 GrantOrDrop. Stores what fits, drops the rest, never destroys. spoilStartTime below zero = now;
    // Doc B10 passes its job's finish time (R01 amendment). Every other caller passes nothing.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Inventory") static FS_GrantResult grantOrDrop(APlayerState* player, const FS_ItemStack& stack, int64 spoilStartTime = -1);
    // The same rule for an entry that already exists: one physical item with its state, or a stack with its own spoil time.
    static FS_GrantResult grantEntryOrDrop(APlayerState* player, const FS_InventoryEntry& entry);
    // R01 3.11: the general pickup path.
    static FS_InventoryResult pickUpLooseItem(APlayerState* player, ALooseItemActor* looseItem);
    // Removes available units from the bag and hands them to the caller: the shared first half of a drop and of a throw (Doc B07).
    static FS_InventoryResult takeOutOfBag(APlayerState* player, const FS_ItemRef& ref, int32 quantity, FS_InventoryEntry& outEntry);
    // The "discard" button of the bag screen (Doc B17 3.6): the units land as a loose item in front of the player.
    static FS_InventoryResult dropFromBag(APlayerState* player, const FS_ItemRef& ref, int32 quantity);
    // R01 §5 TransferEntry: moves an entry between two inventories. There is no quick-stack or sort (Index A112).
    static FS_InventoryResult transferEntry(UInventoryData* source, UInventoryData* destination, const FS_ItemRef& ref, int32 quantity);


    // ---- Section 6 ----
    // R01 §5 GetHeldItem: what is wielded now, or the Unarmed entry.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") static const UDA_ItemDefinition* getHeldItem(const APlayerState* player);
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") static FS_HeldItem getHeldState(const APlayerState* player);
    // Server. Wears one instance in the player's bag down. At zero the item breaks and disappears (Doc R03 3.5; no repair, Index A87).
    // An unbreakable item and an item without durability are untouched.
    static FS_InventoryResult applyDurabilityLoss(APlayerState* player, const FGuid& instanceId, float amount);
    // R01 §5 ApplyReload. Server. Called by GA_Reload on normal completion (Doc R04 §6). A cancelled reload never calls it.
    static FS_InventoryResult applyReload(APlayerState* player, const FGuid& weaponInstanceId);
    // Server. Called by Doc R03 when a crossbow or a firearm fires: unloads the crossbow, or spends one round (I11).
    static FS_InventoryResult spendLoadedShot(APlayerState* player, const FGuid& weaponInstanceId);

    // ---- Section 7: craft staging (R01 3.6). Server only. Called by Doc R02 ----
    // R01 §5 ReserveForStaging. For an instanced item given by item id, the most worn copy is chosen and returned in instanceId.
    // An invalid stationId is a hand-craft session.
    static FS_InventoryResult reserveForStaging(APlayerState* player, const FS_ItemRef& ref, int32 quantity, const FGuid& stationId);
    // R01 §5 ReleaseStagedItem: one click returns one unit, Shift plus click the whole staged stack (Doc R02 UnstageIngredient).
    static FS_InventoryResult releaseStagedItem(APlayerState* player, const FS_ItemRef& ref, int32 quantity);
    // R01 §5 ReleaseStagedReservation: everything a station's session staged. Answers how many units were released.
    static int32 releaseStagedReservation(APlayerState* player, const FGuid& stationId);
    // Everything this player has staged anywhere: death, Downed, a scene, leaving the session.
    static int32 releaseAllStaged(APlayerState* player);
    // The craft completed: one recipe's worth of a staged ingredient is consumed (name PROPOSED, I12).
    static FS_InventoryResult consumeStaged(APlayerState* player, const FGuid& stationId, const FS_ItemRef& ref, int32 quantity);
    // Cleanup for Session.PlayerLeft (Doc B20 3.6): releases whatever this framework holds for the leaving player.
    static void handlePlayerLeft(APlayerState* player);

    // ---- Section 8 ----
    // The effects a consumable really has: its own entries plus, for a variant, the scaled meter values of its base item.
    static void buildConsumeEffects(const UObject* worldContext, const UDA_ItemDefinition& definition, TArray<FS_ConsumeEffect>& outEffects);
    // R01 §5 UseConsumable. Server. Eats, drinks or uses one unit from the bag.
    static FS_InventoryResult useConsumable(APlayerState* player, FName itemId);
    // Server. Applies an item's effects to a player without touching any inventory (shared with the Bagless hold slots).
    static void applyConsumeEffects(APlayerState* player, const UDA_ItemDefinition& definition);
    // Shared by the library files of this framework.
    static FS_InventoryResult refusal(const FGameplayTag& reason);

private:
    static FS_InventoryEntry makeEntry(const UObject* worldContext, const UDA_ItemDefinition& definition, int32 quantity, int64 spoilAtTime);
    static void reportDropped(APlayerState* player, FName itemId, int32 quantity, const FVector& location);
};
