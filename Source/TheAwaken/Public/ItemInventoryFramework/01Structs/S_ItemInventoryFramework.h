#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "ItemInventoryFramework/00Enums/E_ItemInventoryFramework.h"
#include "S_ItemInventoryFramework.generated.h"

class APlayerState;
class UDA_InventoryPageLayout;

// =====================================================================
// S1.9  Blueprint-visible delegates (moved here from 04Delegates/D_ItemInventoryFramework.h: UHT does not
// register a dynamic delegate in a header with no UCLASS or USTRUCT). Each is raised locally on the machine that holds the state.
// =====================================================================

// The bag, the hotbar or the hold slots of this player changed.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FD_PlayerInventoryChanged, APlayerState*, player);
// What is in the hand changed.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FD_HeldItemChanged);
// The container this player has open changed, opened (a valid id) or closed (an invalid id).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FD_ContainerViewChanged, FGuid, containerId);
// The server refused a request of this player. The reason is the tag the UI turns into text (Doc B17 3.14).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FD_InventoryRequestRefused, FGameplayTag, reason);

// =====================================================================
// S1.2  Items and entries (R01 3.2, §4)
// =====================================================================

// The state of one instanced item. It travels with the item wherever it goes.
USTRUCT(BlueprintType)
struct FS_ItemInstanceState{
    GENERATED_BODY()

    // -1 = unbreakable (the starter Axe only), 0 = the item has no durability (I4).
    UPROPERTY(BlueprintReadOnly) float durability = 0.0f;
    // Portable Containers only: the id of the inventory that holds the contents (R01 3.4).
    UPROPERTY(BlueprintReadOnly) FGuid containedInventoryId;
    // Crossbow only (the spec's bLoaded, Index A111).
    UPROPERTY(BlueprintReadOnly) bool isLoaded = false;
    // Firearm only (Index A116).
    UPROPERTY(BlueprintReadOnly) int32 loadedRounds = 0;

    bool operator==(const FS_ItemInstanceState& other) const{
        return durability == other.durability && containedInventoryId == other.containedInventoryId && isLoaded == other.isLoaded && loadedRounds == other.loadedRounds;
    }
};

// One line of an inventory. Stackable: itemId + quantities (+ spoil time). Instanced: itemId + instanceId + state.
USTRUCT(BlueprintType)
struct FS_InventoryEntry{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName itemId;
    // Stackable: how many. Instanced: always 1.
    UPROPERTY(BlueprintReadOnly) int32 totalQuantity = 0;
    // Staged for a craft (R01 3.6). Instanced: 1 while reserved. Never saved.
    UPROPERTY(BlueprintReadOnly) int32 reservedQuantity = 0;
    // Stackable items that can spoil: the absolute GameTime at which the stack spoils. 0 = no spoil clock (R01 3.2a).
    UPROPERTY(BlueprintReadOnly) int64 spoilAtTime = 0;
    // Instanced only. Created with the instance and never changed (Doc02 19.3, player-created kind).
    UPROPERTY(BlueprintReadOnly) FGuid instanceId;
    UPROPERTY(BlueprintReadOnly) FS_ItemInstanceState state;

    bool isInstanced() const{
        return instanceId.IsValid();
    }

    int32 getAvailableQuantity() const{
        return totalQuantity - reservedQuantity;
    }

    // The same line of an inventory: the same stack, or the same physical item.
    bool isSameLineAs(const FS_InventoryEntry& other) const{
        return itemId == other.itemId && instanceId == other.instanceId;
    }

    bool operator==(const FS_InventoryEntry& other) const{
        return isSameLineAs(other) && totalQuantity == other.totalQuantity && reservedQuantity == other.reservedQuantity && spoilAtTime == other.spoilAtTime && state == other.state;
    }
};

// A request to grant or remove (R01 §4).
USTRUCT(BlueprintType)
struct FS_ItemStack{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName itemId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1")) int32 quantity = 1;
};

// "ItemID or InstanceId" (R01 §5): names a stack by its item id, or one physical item by its instance id.
USTRUCT(BlueprintType)
struct FS_ItemRef{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FName itemId;
    UPROPERTY(BlueprintReadWrite) FGuid instanceId;

    bool isInstance() const{
        return instanceId.IsValid();
    }
};

// One entry of an item's ConsumeEffects (R01 3.1, 3.2a). Every value is data.
USTRUCT(BlueprintType)
struct FS_ConsumeEffect{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) E_ConsumeEffectKind kind = E_ConsumeEffectKind::ModifyMeter;
    // ModifyMeter: the meter (Survival.Meter.*). IllnessChance: the illness source (Survival.IllnessSource.*).
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag targetTag;
    // ModifyMeter: the change. HealthLoss: the HP lost (placeholder 10).
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float amount = 0.0f;
    // IllnessChance only: 0 to 1 (placeholder 0.3).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1")) float chance = 1.0f;
    // IllnessChance only: how long the source lasts, in in-game hours (Doc B03 3.4, SourceDuration).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0")) float sourceDurationHours = 0.0f;
};

// What a Container or a Portable Container accepts (R01 3.1 CapacityRules and AllowedTags, 3.4; I21).
USTRUCT(BlueprintType)
struct FS_ContainerRules{
    GENERATED_BODY()

    // 0 = use each item's own perTypeLimit, the same model as the bag (PROPOSED in the spec).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0")) int32 defaultPerTypeLimit = 0;
    // A limit for one item id; wins over the default.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, int32> perItemLimits;
    // Empty = accepts everything. Otherwise the item's category tag or one of its item tags must match.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTagContainer allowedTags;
    // Of the items that carry this tag, the container holds only one item id at a time (clean OR unclean water, R01 3.4).
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag singleItemTag;
};

// =====================================================================
// S1.3  Results (Doc02 §6: a refusal is always explained)
// =====================================================================

// The result of every inventory command. Success means the failReason tag is empty.
USTRUCT(BlueprintType)
struct FS_InventoryResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FGameplayTag failReason;
    // How many units the command really moved, added or removed.
    UPROPERTY(BlueprintReadOnly) int32 quantity = 0;
    // Set when the command chose or created one instance.
    UPROPERTY(BlueprintReadOnly) FGuid instanceId;

    bool succeeded() const{
        return !failReason.IsValid();
    }
};

// R01 3.7 step 3. quantityCarried is the I16 addition (a heavy item that went into a carry slot).
USTRUCT(BlueprintType)
struct FS_GrantResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 quantityStored = 0;
    UPROPERTY(BlueprintReadOnly) int32 quantityDropped = 0;
    UPROPERTY(BlueprintReadOnly) int32 quantityCarried = 0;
};

// =====================================================================
// S1.4  Hotbar, held item, staging (R01 3.5, 3.6, 3.8)
// =====================================================================

// A hotbar slot points at an item TYPE, never at one physical stack (R01 3.5).
USTRUCT(BlueprintType)
struct FS_HotbarBinding{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 slotIndex = 0;
    UPROPERTY(BlueprintReadOnly) FName boundItemId;
};

// resolveHotbarSlot(): what the slot would use right now.
USTRUCT(BlueprintType)
struct FS_ResolvedHotbarSlot{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 slotIndex = 0;
    UPROPERTY(BlueprintReadOnly) FName itemId;
    // Instanced items: the copy that will be used (the last used one, decision D4).
    UPROPERTY(BlueprintReadOnly) FGuid instanceId;
    UPROPERTY(BlueprintReadOnly) int32 availableQuantity = 0;
    // False for an empty slot, a slot whose pool is gone, and a locked slot of a Bagless player.
    UPROPERTY(BlueprintReadOnly) bool isUsable = false;
};

// What is in the hand. Replicated to everyone so other players see it. Never saved (R01 3.5).
USTRUCT(BlueprintType)
struct FS_HeldItem{
    GENERATED_BODY()

    // Empty = nothing in the hand (Combat then reads the Unarmed entry).
    UPROPERTY(BlueprintReadOnly) FName itemId;
    UPROPERTY(BlueprintReadOnly) FGuid instanceId;
    // True only for a weapon held by a player who has the bag (I10). A Bagless player's weapon is shown, not wielded.
    UPROPERTY(BlueprintReadOnly) bool isWielded = false;
    // The hotbar slot it was taken from, or -1 for a direct pick from the bag.
    UPROPERTY(BlueprintReadOnly) int32 sourceSlot = -1;

    bool isEmpty() const{
        return itemId.IsNone();
    }

    bool operator==(const FS_HeldItem& other) const{
        return itemId == other.itemId && instanceId == other.instanceId && isWielded == other.isWielded && sourceSlot == other.sourceSlot;
    }
};

// One of the three hold slots of a Bagless player: exactly one item, no stacking (R01 3.8).
USTRUCT(BlueprintType)
struct FS_BaglessHoldSlot{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool isOccupied = false;
    UPROPERTY(BlueprintReadOnly) FS_InventoryEntry entry;
    // Food does not spoil in the hand: the time left is kept here and the spoil time is rebuilt when the item leaves.
    UPROPERTY(BlueprintReadOnly) bool hasSpoilClock = false;
    UPROPERTY(BlueprintReadOnly) int64 timeToSpoil = 0;
};

// One staging reservation (R01 §4). Server only: never saved, never replicated.
USTRUCT()
struct FS_StagedReservation{
    GENERATED_BODY()

    UPROPERTY() FName itemId;
    UPROPERTY() FGuid instanceId;
    UPROPERTY() int32 reservedQuantity = 0;
    // The station of the crafting session. An invalid id is a hand-craft session (no station, Doc R02 3.8).
    UPROPERTY() FGuid owningStationId;
};

// =====================================================================
// S1.5  The bag and worn gear (R01 3.8; Doc B02 3.2, 3.6; Doc R05 3.3)
// =====================================================================

// Where one worn armor piece sat, so recovery puts it back in the same place (Doc B02).
USTRUCT(BlueprintType)
struct FS_GearSlotRecord{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FGuid instanceId;
    UPROPERTY(BlueprintReadOnly) int32 stackIndex = 0;
};

// What Equipment hands over when the bag is detached (Doc R05 ReleaseEquippedGear).
USTRUCT(BlueprintType)
struct FS_GearReleaseResult{
    GENERATED_BODY()

    // "Nothing was worn" is a success.
    UPROPERTY(BlueprintReadOnly) bool succeeded = true;
    // Every worn piece as a full bag entry: same instance id, current durability.
    UPROPERTY(BlueprintReadOnly) TArray<FS_InventoryEntry> gearEntries;
    UPROPERTY(BlueprintReadOnly) TArray<FS_GearSlotRecord> gearSlots;
};

// What a bag lying in the world holds besides its items (Doc R13 3.6: contents, hotbar snapshot, equipped gear).
USTRUCT(BlueprintType)
struct FS_BagContents{
    GENERATED_BODY()

    // The inventory in the registry that IS the bag. Not a copy.
    UPROPERTY(BlueprintReadOnly) FGuid inventoryId;
    UPROPERTY(BlueprintReadOnly) TArray<FS_HotbarBinding> hotbar;
    // The armor that was worn. The pieces themselves are entries of the bag's inventory, listed once.
    UPROPERTY(BlueprintReadOnly) TArray<FS_GearSlotRecord> gear;
    // The item that was in the hand: a pointer to an entry of the bag, never a copy (I18).
    UPROPERTY(BlueprintReadOnly) FS_ItemRef wielded;
};

// A bag record as the bag service answers it (the record itself is S_BagRecord of Doc R13).
USTRUCT(BlueprintType)
struct FS_BagInfo{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool isValid = false;
    UPROPERTY(BlueprintReadOnly) FGuid bagId;
    // A bag belongs to a character, not to a player (Index A93).
    UPROPERTY(BlueprintReadOnly) FGuid ownerCharacterId;
    UPROPERTY(BlueprintReadOnly) FVector location = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FS_BagContents contents;
};

USTRUCT(BlueprintType)
struct FS_BagDetachResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FGameplayTag failReason;
    UPROPERTY(BlueprintReadOnly) FGuid bagId;

    bool succeeded() const{
        return !failReason.IsValid();
    }
};

// =====================================================================
// S1.6  Message payloads (R01 §6, Doc02 §11)
// =====================================================================

// Inventory.Changed
USTRUCT(BlueprintType)
struct FS_InventoryChangedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FGuid inventoryId;
};

// Inventory.GrantDropped
USTRUCT(BlueprintType)
struct FS_GrantDroppedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<APlayerState> player = nullptr;
    UPROPERTY(BlueprintReadOnly) FName itemId;
    UPROPERTY(BlueprintReadOnly) int32 quantity = 0;
};

// Hotbar.Changed
USTRUCT(BlueprintType)
struct FS_HotbarChangedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<APlayerState> player = nullptr;
    UPROPERTY(BlueprintReadOnly) int32 slotIndex = 0;
};

// =====================================================================
// S1.7  The replication mirror (R01 3.3 "replicates as a fast array"; I2)
// =====================================================================

USTRUCT()
struct FS_ReplicatedInventoryEntry : public FFastArraySerializerItem{
    GENERATED_BODY()

    UPROPERTY() FS_InventoryEntry entry;
};

// A copy of one inventory's entries that replicates only what changed.
// It is a member of a component, which decides who receives it (owner only).
USTRUCT()
struct FS_InventoryEntryList : public FFastArraySerializer{
    GENERATED_BODY()

    UPROPERTY() TArray<FS_ReplicatedInventoryEntry> items;

    // Client: runs once after each batch of changes has arrived. Not a UPROPERTY, so it never replicates.
    FSimpleDelegate onReceived;

    // Server. Makes the mirror equal to the real entries and marks only the lines that changed.
    void syncFrom(const TArray<FS_InventoryEntry>& source);
    // The mirror as plain entries.
    TArray<FS_InventoryEntry> toEntries() const;

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& deltaParameters){
        return FFastArraySerializer::FastArrayDeltaSerialize<FS_ReplicatedInventoryEntry, FS_InventoryEntryList>(items, deltaParameters, *this);
    }

    void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& parameters){
        onReceived.ExecuteIfBound();
    }
};

template<>
struct TStructOpsTypeTraits<FS_InventoryEntryList> : public TStructOpsTypeTraitsBase2<FS_InventoryEntryList>{
    enum{
        WithNetDeltaSerializer = true
    };
};

// =====================================================================
// S2.1  Bag-screen data (R01 3.1a, 3.1b). Presentation only: never saved, never replicated, never read by gameplay.
// =====================================================================

// One fixed slot of a page. slotId is unique inside its layout and is never renumbered or reused (R01 3.1b).
USTRUCT(BlueprintType)
struct FS_InventorySlotDef{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName slotId;
    // Position and size on the page, chosen when the layout is authored. A long item gets a wide slot.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D position = FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D size = FVector2D(100.0, 100.0);
};

// How an item points at its slot: the page layout and the slot id, so moving or resizing the slot breaks nothing.
USTRUCT(BlueprintType)
struct FS_InventorySlotRef{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UDA_InventoryPageLayout> pageLayout;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName slotId;

    bool isSet() const{
        return !pageLayout.IsNull() && !slotId.IsNone();
    }
};

// Where unit n of a pile sits inside the slot's pocket and how it is rotated (R01 3.1a).
USTRUCT(BlueprintType)
struct FS_StackUnit{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector position = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FRotator rotation = FRotator::ZeroRotator;
};

// One row of the page mapping (R01 3.1a): which categories land on which page, and that page's layout.
USTRUCT(BlueprintType)
struct FS_InventoryPageMapping{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Inventory.Page")) FGameplayTag pageTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Item.Category")) FGameplayTagContainer categories;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UDA_InventoryPageLayout> layout;
};

// The answer of the slot lookup (Section 10): where an item is drawn.
USTRUCT(BlueprintType)
struct FS_ItemSlotInfo{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FGameplayTag pageTag;
    UPROPERTY(BlueprintReadOnly) FS_InventorySlotRef slot;
    // A declared variant pair (fresh and spoiled, clean and unclean) shares the slot; each id is its own pile.
    UPROPERTY(BlueprintReadOnly) TArray<FName> itemsInSlot;
};

// =====================================================================
// S11.2  Save records (R01 §8; Doc02 §9). Plain data only: ids, never object pointers (Doc R13 3.11).
// =====================================================================

// One inventory as it is written to a file.
USTRUCT()
struct FS_SavedInventory{
    GENERATED_BODY()

    UPROPERTY() FGuid inventoryId;
    UPROPERTY() E_InventoryType inventoryType = E_InventoryType::Personal;
    UPROPERTY() bool isDeathBag = false;
    // True in a Character file: each entry's spoilAtTime field then holds the time LEFT, not the time on the clock.
    UPROPERTY() bool usesTimeLeft = false;
    // reservedQuantity is always 0 here: reservations are never saved.
    UPROPERTY() TArray<FS_InventoryEntry> entries;
};

// Key Inventory.Character.
USTRUCT()
struct FS_PlayerInventorySave{
    GENERATED_BODY()

    UPROPERTY() int32 version = 1;
    UPROPERTY() bool isKitGranted = false;
    UPROPERTY() TArray<FS_HotbarBinding> hotbar;
    UPROPERTY() FS_SavedInventory bag;
    // The contents of the Portable Containers that are in the bag: saved once, with what holds them (R01 3.4).
    UPROPERTY() TArray<FS_SavedInventory> containedInventories;
};

// Key Inventory.World.
USTRUCT()
struct FS_WorldInventorySave{
    GENERATED_BODY()

    UPROPERTY() int32 version = 1;
    // World containers and bags lying in the world.
    UPROPERTY() TArray<FS_SavedInventory> inventories;
    UPROPERTY() TArray<FS_SavedInventory> containedInventories;
};
