#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"

void FS_InventoryEntryList::syncFrom(const TArray<FS_InventoryEntry>& source){
    // 1. Lines that left the inventory.
    for(int32 index = items.Num() - 1; index >= 0; index--){
        const FS_InventoryEntry& mirrored = items[index].entry;
        const bool stillThere = source.ContainsByPredicate([&mirrored](const FS_InventoryEntry& wanted){
            return wanted.isSameLineAs(mirrored);
        });
        if(!stillThere){
            items.RemoveAt(index);
            MarkArrayDirty();
        }
    }
    // 2. New lines and changed lines.
    for(const FS_InventoryEntry& wanted : source){
        FS_ReplicatedInventoryEntry* existing = items.FindByPredicate([&wanted](const FS_ReplicatedInventoryEntry& item){
            return item.entry.isSameLineAs(wanted);
        });
        if(existing == nullptr){
            FS_ReplicatedInventoryEntry& added = items.AddDefaulted_GetRef();
            added.entry = wanted;
            MarkItemDirty(added);
        }else if(!(existing->entry == wanted)){
            existing->entry = wanted;
            MarkItemDirty(*existing);
        }
    }
}

TArray<FS_InventoryEntry> FS_InventoryEntryList::toEntries() const{
    TArray<FS_InventoryEntry> entries;
    entries.Reserve(items.Num());
    for(const FS_ReplicatedInventoryEntry& item : items){
        entries.Add(item.entry);
    }
    return entries;
}
