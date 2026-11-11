#include "ItemInventoryFramework/UBFL_InventorySave.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

TArray<uint8> UBFL_InventorySave::toBytes(UScriptStruct* structType, const void* data){
    TArray<uint8> bytes;
    FMemoryWriter writer(bytes, true);
    FObjectAndNameAsStringProxyArchive archive(writer, false);
    structType->SerializeItem(archive, const_cast<void*>(data), nullptr);   // tagged properties: name + value
    return bytes;
}

bool UBFL_InventorySave::fromBytes(UScriptStruct* structType, void* data, const TArray<uint8>& bytes){
    if(bytes.Num() == 0){
        return false;
    }
    FMemoryReader reader(bytes, true);
    FObjectAndNameAsStringProxyArchive archive(reader, true);
    structType->SerializeItem(archive, data, nullptr);
    return !archive.IsError() && !reader.IsError();
}

TArray<FS_InventoryEntry> UBFL_InventorySave::toSavedEntries(const UInventoryData* inventory, bool usesTimeLeft, int64 now){
    TArray<FS_InventoryEntry> saved = inventory->getEntries();
    for(FS_InventoryEntry& entry : saved){
        entry.reservedQuantity = 0;   // reservations are never saved
        if(usesTimeLeft && entry.spoilAtTime > 0){
            // A stack that is already due is saved with one second left: it spoils at the first request after the load.
            // This keeps the capture a pure query (Doc02 §9).
            entry.spoilAtTime = FMath::Max<int64>(1, entry.spoilAtTime - now);
        }
    }
    return saved;
}

TArray<FS_InventoryEntry> UBFL_InventorySave::toLiveEntries(const UObject* worldContext, const FS_SavedInventory& saved, int64 now){
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(worldContext);
    TArray<FS_InventoryEntry> live;
    for(const FS_InventoryEntry& savedEntry : saved.entries){
        if(catalog == nullptr || catalog->getDefinition(savedEntry.itemId) == nullptr){
            // Doc R13 3.11 (Index A102, ST-13): skip the entry, one log line, the load continues. Never crash.
            INVENTORY_LOG(Warning, worldContext, "Skipped saved item '%s': it no longer exists", *savedEntry.itemId.ToString());
            continue;
        }
        FS_InventoryEntry entry = savedEntry;
        entry.reservedQuantity = 0;
        if(saved.usesTimeLeft && entry.spoilAtTime > 0){
            entry.spoilAtTime = now + entry.spoilAtTime;   // the spoil time is rebuilt from the current clock
        }
        live.Add(entry);
    }
    return live;
}

void UBFL_InventorySave::capture(const UInventoryRegistrySubsystem* registry, const UInventoryData* inventory, bool usesTimeLeft, int64 now, FS_SavedInventory& outInventory, TArray<FS_SavedInventory>& outContained){
    outInventory.inventoryId = inventory->getInventoryId();
    outInventory.inventoryType = inventory->getInventoryType();
    outInventory.isDeathBag = inventory->isDeathBag();
    outInventory.usesTimeLeft = usesTimeLeft;
    outInventory.entries = toSavedEntries(inventory, usesTimeLeft, now);
    // A Portable Container cannot hold another one, so one level is enough (R01 3.4a).
    for(const FS_InventoryEntry& entry : inventory->getEntries()){
        const UInventoryData* contained = registry->findInventory(entry.state.containedInventoryId);
        if(contained != nullptr && !contained->isEmpty()){
            FS_SavedInventory& savedContained = outContained.AddDefaulted_GetRef();
            savedContained.inventoryId = contained->getInventoryId();
            savedContained.inventoryType = E_InventoryType::PortableContainer;
            savedContained.usesTimeLeft = usesTimeLeft;
            savedContained.entries = toSavedEntries(contained, usesTimeLeft, now);
        }
    }
}

UInventoryData* UBFL_InventorySave::restore(UInventoryRegistrySubsystem* registry, const FS_SavedInventory& saved, const TArray<FS_SavedInventory>& containedPool, int64 now){
    if(registry == nullptr || !saved.inventoryId.IsValid()){
        return nullptr;
    }
    // A world container whose actor is loaded already has its inventory (and its rules): reuse it.
    UInventoryData* inventory = registry->findOrCreateInventory(saved.inventoryType, saved.inventoryId, FS_ContainerRules());
    if(inventory == nullptr){
        return nullptr;
    }
    inventory->setDeathBag(saved.isDeathBag);
    inventory->replaceEntries(toLiveEntries(registry, saved, now));
    // Each Portable Container gets its contents back, with the rules of its own item definition.
    for(const FS_InventoryEntry& entry : inventory->getEntries()){
        const FGuid& containedId = entry.state.containedInventoryId;
        const FS_SavedInventory* savedContained = containedPool.FindByPredicate([&containedId](const FS_SavedInventory& candidate){
            return candidate.inventoryId == containedId;
        });
        if(containedId.IsValid() && savedContained != nullptr){
            UInventoryData* contained = registry->getContainedInventory(entry, true);
            if(contained != nullptr){
                contained->replaceEntries(toLiveEntries(registry, *savedContained, now));
            }
        }
    }
    return inventory;
}
