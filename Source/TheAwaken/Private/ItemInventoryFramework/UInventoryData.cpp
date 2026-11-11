#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/InventoryTime.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "GameFramework/GameplayMessageSubsystem.h"

// ---------- setup ----------

UWorld* UInventoryData::GetWorld() const{
    if(HasAnyFlags(RF_ClassDefaultObject) || GetOuter() == nullptr){
        return nullptr;
    }
    return GetOuter()->GetWorld();
}

void UInventoryData::setup(const FGuid& newInventoryId, E_InventoryType newType, const FS_ContainerRules& newRules){
    inventoryId = newInventoryId;
    inventoryType = newType;
    rules = newRules;
}

// ---------- queries ----------

const FGuid& UInventoryData::getInventoryId() const{
    return inventoryId;
}

E_InventoryType UInventoryData::getInventoryType() const{
    return inventoryType;
}

bool UInventoryData::isDeathBag() const{
    return deathBag;
}

bool UInventoryData::isEmpty() const{
    return entries.Num() == 0;
}

const TArray<FS_InventoryEntry>& UInventoryData::getEntries() const{
    return entries;
}

const FS_InventoryEntry* UInventoryData::findStack(FName itemId) const{
    return entries.FindByPredicate([itemId](const FS_InventoryEntry& entry){
        return !entry.isInstanced() && entry.itemId == itemId;
    });
}

const FS_InventoryEntry* UInventoryData::findInstance(const FGuid& instanceId) const{
    if(!instanceId.IsValid()){
        return nullptr;
    }
    return entries.FindByPredicate([&instanceId](const FS_InventoryEntry& entry){
        return entry.instanceId == instanceId;
    });
}

int32 UInventoryData::getTotalQuantity(FName itemId) const{
    const FS_InventoryEntry* stack = findStack(itemId);
    return stack != nullptr ? stack->totalQuantity : getInstanceCount(itemId);
}

int32 UInventoryData::getInstanceCount(FName itemId) const{
    int32 count = 0;
    for(const FS_InventoryEntry& entry : entries){
        if(entry.isInstanced() && entry.itemId == itemId){
            count++;
        }
    }
    return count;
}

int32 UInventoryData::getAvailableQuantity(FName itemId) const{
    const FS_InventoryEntry* stack = findStack(itemId);
    if(stack != nullptr){
        return stack->getAvailableQuantity();
    }
    int32 count = 0;
    for(const FS_InventoryEntry& entry : entries){
        if(entry.isInstanced() && entry.itemId == itemId && entry.reservedQuantity == 0){
            count++;
        }
    }
    return count;
}

int32 UInventoryData::getLimit(const UDA_ItemDefinition& definition) const{
    if(inventoryType == E_InventoryType::Personal){
        return definition.perTypeLimit;   // the bag limits each item type (designer-confirmed)
    }
    const int32* itemLimit = rules.perItemLimits.Find(definition.itemId);
    if(itemLimit != nullptr){
        return *itemLimit;
    }
    return rules.defaultPerTypeLimit > 0 ? rules.defaultPerTypeLimit : definition.perTypeLimit;
}

FGameplayTag UInventoryData::checkAccepts(const UDA_ItemDefinition& definition) const{
    if(inventoryType == E_InventoryType::Personal){
        // The bag accepts every item type, except that a heavy item never enters it (R01 3.7 step 0).
        return definition.isHeavy() ? T_Inventory_FailReason_ItemNotAllowed.GetTag() : FGameplayTag();
    }
    if(!rules.allowedTags.IsEmpty() && !definition.categoryTag.MatchesAny(rules.allowedTags) && !definition.itemTags.HasAny(rules.allowedTags)){
        return T_Inventory_FailReason_ItemNotAllowed;
    }
    if(definition.carriesTag(rules.singleItemTag)){
        // Only one item id of that group at a time: a container holds clean OR unclean water (R01 3.4).
        for(const FS_InventoryEntry& entry : entries){
            const UDA_ItemDefinition* other = entry.itemId != definition.itemId ? findDefinition(entry.itemId) : nullptr;
            if(other != nullptr && other->carriesTag(rules.singleItemTag)){
                return T_Inventory_FailReason_ItemNotAllowed;
            }
        }
    }
    return FGameplayTag();
}

int32 UInventoryData::canFit(const FS_ItemStack& stack) const{
    const UDA_ItemDefinition* definition = findDefinition(stack.itemId);
    if(definition == nullptr || stack.quantity < 1 || checkAccepts(*definition).IsValid()){
        return 0;
    }
    const int32 room = getLimit(*definition) - getTotalQuantity(stack.itemId);
    return FMath::Clamp(room, 0, stack.quantity);
}

FGameplayTag UInventoryData::getFullReason() const{
    return inventoryType == E_InventoryType::Personal ? T_Interaction_FailReason_BagFull.GetTag() : T_Inventory_FailReason_ContainerFull.GetTag();
}

// ---------- commands ----------

FS_InventoryResult UInventoryData::addStack(FName itemId, int32 quantity, int64 spoilAtTime){
    const UDA_ItemDefinition* definition = findDefinition(itemId);
    if(definition == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(quantity < 1 || !definition->isStackable()){
        return refusal(T_Inventory_FailReason_InvalidRequest);
    }
    const FGameplayTag blocked = checkAccepts(*definition);
    if(blocked.IsValid()){
        return refusal(blocked);
    }
    FS_ItemStack wanted;
    wanted.itemId = itemId;
    wanted.quantity = quantity;
    const int32 fits = canFit(wanted);
    if(fits < 1){
        return refusal(getFullReason());
    }
    addStackForced(itemId, fits, definition->canSpoil ? spoilAtTime : 0);
    FS_InventoryResult result;
    result.quantity = fits;
    return result;
}

FS_InventoryResult UInventoryData::addInstance(const FS_InventoryEntry& entry){
    const UDA_ItemDefinition* definition = findDefinition(entry.itemId);
    if(definition == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(!entry.isInstanced() || !definition->isInstanced() || findInstance(entry.instanceId) != nullptr){
        return refusal(T_Inventory_FailReason_InvalidRequest);
    }
    const FGameplayTag blocked = checkAccepts(*definition);
    if(blocked.IsValid()){
        return refusal(blocked);
    }
    FS_ItemStack single;
    single.itemId = entry.itemId;
    single.quantity = 1;
    if(canFit(single) < 1){
        return refusal(getFullReason());
    }
    addInstanceForced(entry);
    FS_InventoryResult result;
    result.quantity = 1;
    result.instanceId = entry.instanceId;
    return result;
}

void UInventoryData::addStackForced(FName itemId, int32 quantity, int64 spoilAtTime){
    if(quantity < 1){
        return;
    }
    FS_InventoryEntry* stack = findStackMutable(itemId);
    if(stack == nullptr){
        FS_InventoryEntry& added = entries.AddDefaulted_GetRef();
        added.itemId = itemId;
        added.totalQuantity = quantity;
        added.spoilAtTime = spoilAtTime;
    }else{
        stack->totalQuantity += quantity;
        // Merging keeps the earliest time, so a mixed stack is as old as its oldest unit (R01 3.2a).
        if(spoilAtTime > 0 && (stack->spoilAtTime == 0 || spoilAtTime < stack->spoilAtTime)){
            stack->spoilAtTime = spoilAtTime;
        }
    }
    notifyChanged();
}

void UInventoryData::addInstanceForced(const FS_InventoryEntry& entry){
    FS_InventoryEntry& added = entries.Add_GetRef(entry);
    added.totalQuantity = 1;
    added.reservedQuantity = 0;
    added.spoilAtTime = 0;
    notifyChanged();
}

FS_InventoryResult UInventoryData::removeStack(FName itemId, int32 quantity){
    FS_InventoryEntry* stack = findStackMutable(itemId);
    if(stack == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(quantity < 1){
        return refusal(T_Inventory_FailReason_InvalidRequest);
    }
    if(stack->getAvailableQuantity() < quantity){
        // Enough is held but part of it is staged, or there simply is not enough.
        return refusal(stack->totalQuantity >= quantity ? T_Inventory_FailReason_ReservedQuantity : T_Inventory_FailReason_InsufficientQuantity);
    }
    stack->totalQuantity -= quantity;
    if(stack->totalQuantity == 0){
        entries.RemoveAll([itemId](const FS_InventoryEntry& entry){
            return !entry.isInstanced() && entry.itemId == itemId;
        });
    }
    notifyChanged();
    FS_InventoryResult result;
    result.quantity = quantity;
    return result;
}

FS_InventoryResult UInventoryData::removeInstance(const FGuid& instanceId){
    const FS_InventoryEntry* entry = findInstance(instanceId);
    if(entry == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(entry->reservedQuantity > 0){
        return refusal(T_Inventory_FailReason_ReservedQuantity);
    }
    entries.RemoveAll([&instanceId](const FS_InventoryEntry& candidate){
        return candidate.instanceId == instanceId;
    });
    notifyChanged();
    FS_InventoryResult result;
    result.quantity = 1;
    result.instanceId = instanceId;
    return result;
}

bool UInventoryData::setInstanceState(const FGuid& instanceId, const FS_ItemInstanceState& newState){
    FS_InventoryEntry* entry = findInstanceMutable(instanceId);
    if(entry == nullptr){
        return false;
    }
    if(!(entry->state == newState)){
        entry->state = newState;
        notifyChanged();
    }
    return true;
}

void UInventoryData::setDeathBag(bool newValue){
    deathBag = newValue;
}

void UInventoryData::replaceEntries(const TArray<FS_InventoryEntry>& newEntries){
    reservations.Reset();
    entries = newEntries;
    notifyChanged();
}

void UInventoryData::clearAll(){
    reservations.Reset();
    entries.Reset();
    notifyChanged();
}

void UInventoryData::evaluateSpoilage(int64 now){
    if(!UBFL_Inventory::isServerWorld(this)){
        return;   // a client never converts a stack: it shows the state it is given (R01 3.2a)
    }
    // Collect first: converting changes the array.
    TArray<FS_InventoryEntry> due;
    for(const FS_InventoryEntry& entry : entries){
        if(!entry.isInstanced() && entry.spoilAtTime > 0 && now >= entry.spoilAtTime){
            due.Add(entry);
        }
    }
    for(const FS_InventoryEntry& fresh : due){
        const UDA_ItemDefinition* definition = findDefinition(fresh.itemId);
        const UDA_ItemDefinition* spoiledDefinition = definition != nullptr ? findDefinition(definition->spoiledItemId) : nullptr;
        if(spoiledDefinition == nullptr){
            // Bad data must never destroy food or stop the game: the stack simply stops ageing.
            findStackMutable(fresh.itemId)->spoilAtTime = 0;
            INVENTORY_LOG(Error, this, "%s has reached its spoil time but its spoiledItemId does not exist; the stack was left as it is", *fresh.itemId.ToString());
            continue;
        }
        // Reservations on the converting stack are released; Doc R02 hears it and refuses the craft (Index A106, S3-F04).
        for(int32 index = reservations.Num() - 1; index >= 0; index--){
            if(!reservations[index].instanceId.IsValid() && reservations[index].itemId == fresh.itemId){
                onStagedItemLost.Broadcast(reservations[index].owningStationId, fresh.itemId);
                reservations.RemoveAt(index);
            }
        }
        const FName freshId = fresh.itemId;
        entries.RemoveAll([freshId](const FS_InventoryEntry& entry){
            return !entry.isInstanced() && entry.itemId == freshId;
        });
        // The same quantity of the spoiled item, ignoring its per-type limit. A spoiled item that can itself spoil
        // starts its own clock at the moment the fresh one ran out.
        const int64 nextSpoilAt = spoiledDefinition->canSpoil ? fresh.spoilAtTime + InventoryTime::hoursToGameSeconds(spoiledDefinition->spoilTimeHours) : 0;
        addStackForced(spoiledDefinition->itemId, fresh.totalQuantity, nextSpoilAt);   // raises the change
        INVENTORY_LOG(Log, this, "%d %s spoiled into %s in inventory %s", fresh.totalQuantity, *fresh.itemId.ToString(), *spoiledDefinition->itemId.ToString(), *inventoryId.ToString());
    }
}

// ---------- helpers ----------

const UDA_ItemDefinition* UInventoryData::findDefinition(FName itemId) const{
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    return catalog != nullptr ? catalog->getDefinition(itemId) : nullptr;
}

FS_InventoryEntry* UInventoryData::findStackMutable(FName itemId){
    return entries.FindByPredicate([itemId](const FS_InventoryEntry& entry){
        return !entry.isInstanced() && entry.itemId == itemId;
    });
}

FS_InventoryEntry* UInventoryData::findInstanceMutable(const FGuid& instanceId){
    if(!instanceId.IsValid()){
        return nullptr;
    }
    return entries.FindByPredicate([&instanceId](const FS_InventoryEntry& entry){
        return entry.instanceId == instanceId;
    });
}

void UInventoryData::notifyChanged(){
    onChanged.Broadcast(this);
    // Inventory.Changed is local on every machine (R01 §6). The host raises it here; a client raises it
    // from the same function when its copy is refreshed from the replicated mirror.
    if(UGameplayMessageSubsystem::HasInstance(this)){
        FS_InventoryChangedMessage message;
        message.inventoryId = inventoryId;
        UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Inventory_Changed, message);
    }
}

FS_InventoryResult UInventoryData::refusal(const FGameplayTag& reason){
    FS_InventoryResult result;
    result.failReason = reason;
    return result;
}

// ---------- Section 7: staging reservations ----------

FS_InventoryResult UInventoryData::reserveStack(FName itemId, int32 quantity, const FGuid& stationId){
    FS_InventoryEntry* stack = findStackMutable(itemId);
    if(stack == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(quantity < 1){
        return refusal(T_Inventory_FailReason_InvalidRequest);
    }
    if(stack->getAvailableQuantity() < quantity){
        return refusal(T_Inventory_FailReason_InsufficientQuantity);   // a reserved amount cannot be reserved twice
    }
    stack->reservedQuantity += quantity;
    FS_StagedReservation* record = reservations.FindByPredicate([itemId, &stationId](const FS_StagedReservation& candidate){
        return !candidate.instanceId.IsValid() && candidate.itemId == itemId && candidate.owningStationId == stationId;
    });
    if(record == nullptr){
        record = &reservations.AddDefaulted_GetRef();
        record->itemId = itemId;
        record->owningStationId = stationId;
    }
    record->reservedQuantity += quantity;
    notifyChanged();
    FS_InventoryResult result;
    result.quantity = quantity;
    return result;
}

FS_InventoryResult UInventoryData::reserveInstance(const FGuid& instanceId, const FGuid& stationId){
    FS_InventoryEntry* entry = findInstanceMutable(instanceId);
    if(entry == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(entry->reservedQuantity > 0){
        return refusal(T_Inventory_FailReason_ReservedQuantity);
    }
    entry->reservedQuantity = 1;
    FS_StagedReservation& record = reservations.AddDefaulted_GetRef();
    record.itemId = entry->itemId;
    record.instanceId = instanceId;
    record.reservedQuantity = 1;
    record.owningStationId = stationId;
    notifyChanged();
    FS_InventoryResult result;
    result.quantity = 1;
    result.instanceId = instanceId;
    return result;
}

void UInventoryData::releaseRecord(int32 recordIndex, int32 quantity){
    FS_StagedReservation& record = reservations[recordIndex];
    FS_InventoryEntry* entry = record.instanceId.IsValid() ? findInstanceMutable(record.instanceId) : findStackMutable(record.itemId);
    if(entry != nullptr){
        entry->reservedQuantity = FMath::Max(0, entry->reservedQuantity - quantity);
    }
    record.reservedQuantity -= quantity;
    if(record.reservedQuantity <= 0){
        reservations.RemoveAt(recordIndex);
    }
}

int32 UInventoryData::releaseItem(const FS_ItemRef& ref, int32 quantity){
    int32 released = 0;
    for(int32 index = reservations.Num() - 1; index >= 0 && released < quantity; index--){
        const FS_StagedReservation& record = reservations[index];
        const bool matches = ref.isInstance() ? record.instanceId == ref.instanceId : (!record.instanceId.IsValid() && record.itemId == ref.itemId);
        if(matches){
            const int32 amount = FMath::Min(quantity - released, record.reservedQuantity);
            releaseRecord(index, amount);
            released += amount;
        }
    }
    if(released > 0){
        notifyChanged();
    }
    return released;
}

int32 UInventoryData::releaseStation(const FGuid& stationId){
    int32 released = 0;
    for(int32 index = reservations.Num() - 1; index >= 0; index--){
        if(reservations[index].owningStationId == stationId){
            const int32 amount = reservations[index].reservedQuantity;
            releaseRecord(index, amount);
            released += amount;
        }
    }
    if(released > 0){
        notifyChanged();
    }
    return released;
}

int32 UInventoryData::releaseAll(){
    int32 released = 0;
    for(int32 index = reservations.Num() - 1; index >= 0; index--){
        const int32 amount = reservations[index].reservedQuantity;
        releaseRecord(index, amount);
        released += amount;
    }
    if(released > 0){
        notifyChanged();
    }
    return released;
}

FS_InventoryResult UInventoryData::consumeReserved(const FGuid& stationId, const FS_ItemRef& ref, int32 quantity){
    const int32 recordIndex = reservations.IndexOfByPredicate([&stationId, &ref](const FS_StagedReservation& record){
        const bool sameItem = ref.isInstance() ? record.instanceId == ref.instanceId : (!record.instanceId.IsValid() && record.itemId == ref.itemId);
        return sameItem && record.owningStationId == stationId;
    });
    if(recordIndex == INDEX_NONE || quantity < 1 || reservations[recordIndex].reservedQuantity < quantity){
        return refusal(T_Inventory_FailReason_InsufficientQuantity);   // only what this session staged can be consumed
    }
    // Everything checked. The record first, then the units themselves.
    FS_StagedReservation& record = reservations[recordIndex];
    record.reservedQuantity -= quantity;
    if(record.reservedQuantity <= 0){
        reservations.RemoveAt(recordIndex);
    }
    if(ref.isInstance()){
        entries.RemoveAll([&ref](const FS_InventoryEntry& entry){
            return entry.instanceId == ref.instanceId;
        });
    }else{
        FS_InventoryEntry* stack = findStackMutable(ref.itemId);
        stack->reservedQuantity -= quantity;
        stack->totalQuantity -= quantity;
        if(stack->totalQuantity <= 0){
            const FName itemId = ref.itemId;
            entries.RemoveAll([itemId](const FS_InventoryEntry& entry){
                return !entry.isInstanced() && entry.itemId == itemId;
            });
        }
    }
    notifyChanged();
    FS_InventoryResult result;
    result.quantity = quantity;
    result.instanceId = ref.instanceId;
    return result;
}

const TArray<FS_StagedReservation>& UInventoryData::getReservations() const{
    return reservations;
}

FGuid UInventoryData::findMostWornInstance(FName itemId, const FGuid& excludedInstanceId) const{
    const FS_InventoryEntry* mostWorn = nullptr;
    for(const FS_InventoryEntry& entry : entries){
        if(entry.isInstanced() && entry.itemId == itemId && entry.reservedQuantity == 0 && entry.instanceId != excludedInstanceId){
            if(mostWorn == nullptr || entry.state.durability < mostWorn->state.durability){
                mostWorn = &entry;
            }
        }
    }
    return mostWorn != nullptr ? mostWorn->instanceId : FGuid();
}
