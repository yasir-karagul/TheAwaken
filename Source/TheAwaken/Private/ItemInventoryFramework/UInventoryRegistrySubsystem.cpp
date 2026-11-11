#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UContainerComponent.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UBFL_InventorySave.h"
#include "SaveFramework/SaveSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "StructUtils/InstancedStruct.h"
#include "Engine/World.h"

bool UInventoryRegistrySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UInventoryRegistrySubsystem* UInventoryRegistrySubsystem::get(const UObject* worldContext){
    const UWorld* world = (GEngine != nullptr && worldContext != nullptr) ? GEngine->GetWorldFromContextObject(worldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    return world != nullptr ? world->GetSubsystem<UInventoryRegistrySubsystem>() : nullptr;
}

UInventoryData* UInventoryRegistrySubsystem::findInventory(const FGuid& inventoryId) const{
    const TObjectPtr<UInventoryData>* found = inventories.Find(inventoryId);
    return found != nullptr ? found->Get() : nullptr;
}

UContainerComponent* UInventoryRegistrySubsystem::findHost(const FGuid& inventoryId) const{
    const TWeakObjectPtr<UContainerComponent>* found = hosts.Find(inventoryId);
    return found != nullptr ? found->Get() : nullptr;
}

const TMap<FGuid, TObjectPtr<UInventoryData>>& UInventoryRegistrySubsystem::getAllInventories() const{
    return inventories;
}

UInventoryData* UInventoryRegistrySubsystem::createInventory(E_InventoryType type, const FGuid& inventoryId, const FS_ContainerRules& rules){
    if(!inventoryId.IsValid() || inventories.Contains(inventoryId)){
        INVENTORY_LOG(Error, this, "createInventory refused: the id %s is empty or already used", *inventoryId.ToString());
        return nullptr;
    }
    UInventoryData* inventory = NewObject<UInventoryData>(this);
    inventory->setup(inventoryId, type, rules);
    inventories.Add(inventoryId, inventory);
    return inventory;
}

UInventoryData* UInventoryRegistrySubsystem::findOrCreateInventory(E_InventoryType type, const FGuid& inventoryId, const FS_ContainerRules& rules){
    UInventoryData* existing = findInventory(inventoryId);
    return existing != nullptr ? existing : createInventory(type, inventoryId, rules);
}

void UInventoryRegistrySubsystem::destroyInventory(const FGuid& inventoryId){
    const UInventoryData* inventory = findInventory(inventoryId);
    if(inventory == nullptr){
        return;
    }
    // A Portable Container cannot hold another one (R01 3.4a), so one level is enough.
    for(const FS_InventoryEntry& entry : inventory->getEntries()){
        if(entry.state.containedInventoryId.IsValid()){
            inventories.Remove(entry.state.containedInventoryId);
        }
    }
    inventories.Remove(inventoryId);
    hosts.Remove(inventoryId);
}

void UInventoryRegistrySubsystem::registerHost(const FGuid& inventoryId, UContainerComponent* host){
    hosts.Add(inventoryId, host);
}

void UInventoryRegistrySubsystem::unregisterHost(const FGuid& inventoryId){
    hosts.Remove(inventoryId);
}

UInventoryData* UInventoryRegistrySubsystem::getContainedInventory(const FS_InventoryEntry& containerEntry, bool createIfMissing){
    const FGuid& containedId = containerEntry.state.containedInventoryId;
    if(!containedId.IsValid()){
        return nullptr;
    }
    UInventoryData* existing = findInventory(containedId);
    if(existing != nullptr || !createIfMissing){
        return existing;
    }
    // Created the first time something is put in or the container is opened, with the rules of its item definition.
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(containerEntry.itemId) : nullptr;
    return createInventory(E_InventoryType::PortableContainer, containedId, definition != nullptr ? definition->capacityRules : FS_ContainerRules());
}

bool UInventoryRegistrySubsystem::isContainerItemEmpty(const FS_InventoryEntry& entry) const{
    const UInventoryData* contained = findInventory(entry.state.containedInventoryId);
    return contained == nullptr || contained->isEmpty();
}

// ---------- Section 11: save (Doc R13's IBPI_SaveParticipant) ----------

void UInventoryRegistrySubsystem::OnWorldBeginPlay(UWorld& InWorld){
    Super::OnWorldBeginPlay(InWorld);
    USaveSubsystem* save = InWorld.GetGameInstance() != nullptr ? InWorld.GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
    if(save != nullptr && UBFL_Inventory::isServerWorld(this)){
        save->registerParticipant(this);   // Doc R13 (S11.6)
    }
}

void UInventoryRegistrySubsystem::Deinitialize(){
    const UWorld* world = GetWorld();
    USaveSubsystem* save = (world != nullptr && world->GetGameInstance() != nullptr) ? world->GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
    if(save != nullptr){
        save->unregisterParticipant(this);
    }
    Super::Deinitialize();
}

bool UInventoryRegistrySubsystem::isWorldRoot(const UInventoryData* inventory){
    return inventory->getInventoryType() == E_InventoryType::Container || (inventory->getInventoryType() == E_InventoryType::Personal && inventory->isDeathBag());
}

FName UInventoryRegistrySubsystem::getSaveKey_Implementation() const{
    return TEXT("Inventory.World");
}

E_SaveScope UInventoryRegistrySubsystem::getSaveScope_Implementation() const{
    return E_SaveScope::World;
}

int32 UInventoryRegistrySubsystem::getSnapshotVersion_Implementation() const{
    return currentSnapshotVersion;
}

APlayerState* UInventoryRegistrySubsystem::getSaveOwner_Implementation() const{
    return nullptr;   // World scope
}

// Query: never mutates.
bool UInventoryRegistrySubsystem::getSnapshotForSave_Implementation(FInstancedStruct& snapshot) const{
    const UInventoryServices* services = UInventoryServices::get(this);
    FS_WorldInventorySave record;
    record.version = currentSnapshotVersion;
    const int64 now = services != nullptr ? services->getGameTime() : 0;
    for(const TPair<FGuid, TObjectPtr<UInventoryData>>& pair : inventories){
        if(isWorldRoot(pair.Value)){
            // The World file keeps ABSOLUTE spoil times: it carries its own clock (Doc R13 3.4).
            UBFL_InventorySave::capture(this, pair.Value, false, now, record.inventories.AddDefaulted_GetRef(), record.containedInventories);
        }
    }
    snapshot = FInstancedStruct::Make(record);
    return true;
}

bool UInventoryRegistrySubsystem::migrateSnapshotStep_Implementation(int32 fromVersion, const FInstancedStruct& oldSnapshot, FInstancedStruct& newSnapshot) const{
    return false;   // no step exists yet: a file of another version is refused
}

bool UInventoryRegistrySubsystem::contributeToSlotHeader_Implementation(const FS_SaveSlotHeader& display, FS_SaveSlotHeader& contribution) const{
    return false;
}

bool UInventoryRegistrySubsystem::restoreFromSnapshot_Implementation(const FInstancedStruct& snapshot, int32 savedVersion){
    const FS_WorldInventorySave* record = snapshot.GetPtr<FS_WorldInventorySave>();
    if(!UBFL_Inventory::isServerWorld(this) || record == nullptr || savedVersion != currentSnapshotVersion || record->version != currentSnapshotVersion){
        return false;   // unreadable, or a version with no migration step: refused, nothing changed
    }
    // World inventories that the file does not know: a container keeps its (now empty) data, a stray bag goes.
    TArray<FGuid> staleBags;
    for(const TPair<FGuid, TObjectPtr<UInventoryData>>& pair : inventories){
        const FGuid& id = pair.Key;
        const bool isInFile = record->inventories.ContainsByPredicate([&id](const FS_SavedInventory& saved){
            return saved.inventoryId == id;
        });
        if(isWorldRoot(pair.Value) && !isInFile){
            if(pair.Value->isDeathBag()){
                staleBags.Add(id);
            }else{
                pair.Value->clearAll();
            }
        }
    }
    for(const FGuid& id : staleBags){
        destroyInventory(id);
    }
    const UInventoryServices* services = UInventoryServices::get(this);
    const int64 now = services != nullptr ? services->getGameTime() : 0;
    int32 restored = 0;
    for(const FS_SavedInventory& saved : record->inventories){
        restored += UBFL_InventorySave::restore(this, saved, record->containedInventories, now) != nullptr ? 1 : 0;
    }
    INVENTORY_LOG(Log, this, "Restored %d world inventories (containers and bags lying in the world)", restored);
    return true;
}