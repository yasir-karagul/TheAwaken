#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ItemInventoryFramework/04Delegates/D_ItemInventoryFramework.h"
#include "UInventoryData.generated.h"

class UDA_ItemDefinition;

// R01 3.3: the bag, a chest and the inside of a pouch are all one of these.
UCLASS()
class THEAWAKEN_API UInventoryData : public UObject{
    GENERATED_BODY()

public:
    virtual UWorld* GetWorld() const override;

    // Command. Called once, right after the object is created.
    void setup(const FGuid& newInventoryId, E_InventoryType newType, const FS_ContainerRules& newRules);

    // ---- S3.2 queries ----
    const FGuid& getInventoryId() const;
    E_InventoryType getInventoryType() const;
    // True while this inventory is a bag lying in the world: nobody may take from it or store into it (R01 3.4).
    bool isDeathBag() const;
    bool isEmpty() const;
    const TArray<FS_InventoryEntry>& getEntries() const;
    const FS_InventoryEntry* findStack(FName itemId) const;
    const FS_InventoryEntry* findInstance(const FGuid& instanceId) const;
    // Stackable: the quantity of the stack. Instanced: the number of copies.
    int32 getTotalQuantity(FName itemId) const;
    int32 getInstanceCount(FName itemId) const;
    // R01 §5 GetAvailableQuantity: total minus reserved. The number every consumption check reads.
    int32 getAvailableQuantity(FName itemId) const;
    // The most of this item type the inventory may hold.
    int32 getLimit(const UDA_ItemDefinition& definition) const;
    // An empty tag when this inventory may hold that item type at all (rules, not room).
    FGameplayTag checkAccepts(const UDA_ItemDefinition& definition) const;
    // R01 §5 CanFit: how many units of the stack fit right now (0 to stack.quantity).
    int32 canFit(const FS_ItemStack& stack) const;
    // The reason to give when nothing fits: BagFull for a bag, ContainerFull for a container.
    FGameplayTag getFullReason() const;

    // ---- S3.3, S3.4 commands (server) ----
    // Adds as much as the limit allows and reports how much. Refused only when nothing fits.
    FS_InventoryResult addStack(FName itemId, int32 quantity, int64 spoilAtTime = 0);
    FS_InventoryResult addInstance(const FS_InventoryEntry& entry);
    // Ignore the limit and the rules. Only for moves that must never lose an item:
    // worn armor going into the detached bag, and a spoiled stack replacing a fresh one.
    void addStackForced(FName itemId, int32 quantity, int64 spoilAtTime);
    void addInstanceForced(const FS_InventoryEntry& entry);
    // R01 §5 RemoveStack: removes from the AVAILABLE quantity only.
    FS_InventoryResult removeStack(FName itemId, int32 quantity);
    // R01 §5 RemoveInstance. Refused while the instance is reserved.
    FS_InventoryResult removeInstance(const FGuid& instanceId);
    // Durability, the loaded flag, loaded rounds. False when the instance is not here.
    bool setInstanceState(const FGuid& instanceId, const FS_ItemInstanceState& newState);
    void setDeathBag(bool newValue);
    // Replaces everything: a restore from a save, or a client copy being refreshed.
    void replaceEntries(const TArray<FS_InventoryEntry>& newEntries);
    void clearAll();

    // R01 3.2a: turns every stack that has reached its spoil time into its spoiled item. Section 8 fills this in.
    void evaluateSpoilage(int64 now);

    // C++ only. Raised after every change, on the machine that holds this object.
    FD_InventoryDataChanged onChanged;

protected:
    const UDA_ItemDefinition* findDefinition(FName itemId) const;
    FS_InventoryEntry* findStackMutable(FName itemId);
    FS_InventoryEntry* findInstanceMutable(const FGuid& instanceId);
    void notifyChanged();
    static FS_InventoryResult refusal(const FGameplayTag& reason);

    UPROPERTY() FGuid inventoryId;
    UPROPERTY() E_InventoryType inventoryType = E_InventoryType::Personal;
    UPROPERTY() FS_ContainerRules rules;
    UPROPERTY() bool deathBag = false;
    // Screen order and position come from data (R01 3.1a), never from the order of this array.
    UPROPERTY() TArray<FS_InventoryEntry> entries;

public:
    // ---- Section 7: staging reservations (R01 3.6). Server only ----
    FS_InventoryResult reserveStack(FName itemId, int32 quantity, const FGuid& stationId);
    FS_InventoryResult reserveInstance(const FGuid& instanceId, const FGuid& stationId);
    // Return staged units to available. Each answers how many units it released.
    int32 releaseItem(const FS_ItemRef& ref, int32 quantity);
    int32 releaseStation(const FGuid& stationId);
    int32 releaseAll();
    // The craft happened: reserved units of that station leave the inventory for good.
    FS_InventoryResult consumeReserved(const FGuid& stationId, const FS_ItemRef& ref, int32 quantity);
    const TArray<FS_StagedReservation>& getReservations() const;
    // The copy Crafting takes when it is given an item id: the most worn one that is not staged and not the excluded one.
    FGuid findMostWornInstance(FName itemId, const FGuid& excludedInstanceId) const;

    // C++ only. A staged stack spoiled, so its reservation is gone (R01 3.2a). Doc R02 refuses the craft.
    FD_StagedItemLost onStagedItemLost;

protected:
    // Gives back the units of one record and removes the record when it is empty.
    void releaseRecord(int32 recordIndex, int32 quantity);

    // Server only. Never saved, never replicated.
    UPROPERTY() TArray<FS_StagedReservation> reservations;
};
