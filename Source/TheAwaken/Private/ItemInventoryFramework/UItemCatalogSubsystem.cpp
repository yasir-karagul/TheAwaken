#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

void UItemCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);
    loadAllDefinitions();
    INVENTORY_LOG(Log, this, "Item catalog loaded: %d definitions", definitions.Num());
}

UItemCatalogSubsystem* UItemCatalogSubsystem::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    if(gameInstance == nullptr){
        return nullptr;
    }
    return gameInstance->GetSubsystem<UItemCatalogSubsystem>();
}

void UItemCatalogSubsystem::loadAllDefinitions(){
    UAssetManager* manager = UAssetManager::GetIfInitialized();
    if(manager == nullptr){
        return;
    }
    TArray<FPrimaryAssetId> ids;
    manager->GetPrimaryAssetIdList(UDA_ItemDefinition::getPrimaryAssetType(), ids);
    for(const FPrimaryAssetId& id : ids){
        UDA_ItemDefinition* definition = Cast<UDA_ItemDefinition>(manager->GetPrimaryAssetPath(id).TryLoad());
        if(definition != nullptr){
            registerDefinition(definition);
        }
    }
    rebuildSlotLookup();
}

void UItemCatalogSubsystem::registerDefinition(UDA_ItemDefinition* definition){
    if(definition == nullptr || definition->itemId.IsNone()){
        INVENTORY_LOG(Error, this, "Item definition %s has no itemId and was not registered", *GetNameSafe(definition));
        return;
    }
    if(definitions.Contains(definition->itemId)){
        INVENTORY_LOG(Error, this, "Duplicate itemId %s: %s was not registered", *definition->itemId.ToString(), *definition->GetName());
        return;
    }
    definitions.Add(definition->itemId, definition);
}

void UItemCatalogSubsystem::resetForTest(){
    definitions.Reset();
    rebuildSlotLookup();
}

const UDA_ItemDefinition* UItemCatalogSubsystem::getDefinition(FName itemId) const{
    const TObjectPtr<UDA_ItemDefinition>* found = definitions.Find(itemId);
    return found != nullptr ? found->Get() : nullptr;
}

const UDA_ItemDefinition* UItemCatalogSubsystem::getUnarmedDefinition() const{
    return getDefinition(UInventorySettings::get()->unarmedItemId);
}

const UDA_ItemDefinition* UItemCatalogSubsystem::findFreshItemFor(FName spoiledItemId) const{
    if(spoiledItemId.IsNone()){
        return nullptr;
    }
    // Asked only when something is eaten or validated, so a plain search over the catalog is enough.
    for(const TPair<FName, TObjectPtr<UDA_ItemDefinition>>& pair : definitions){
        if(pair.Value->canSpoil && pair.Value->spoiledItemId == spoiledItemId){
            return pair.Value;
        }
    }
    return nullptr;
}

const TMap<FName, TObjectPtr<UDA_ItemDefinition>>& UItemCatalogSubsystem::getAllDefinitions() const{
    return definitions;
}

// ---------- Section 10: the bag-screen lookup ----------

FString UItemCatalogSubsystem::makeSlotKey(const FS_InventorySlotRef& slot){
    return slot.pageLayout.ToSoftObjectPath().ToString() + TEXT("|") + slot.slotId.ToString();
}

FGameplayTag UItemCatalogSubsystem::getPageFor(const UDA_ItemDefinition* definition) const{
    const UInventorySettings* settings = UInventorySettings::get();
    if(definition != nullptr){
        for(const FS_InventoryPageMapping& mapping : settings->pageMappings){
            if(definition->categoryTag.MatchesAny(mapping.categories)){
                return mapping.pageTag;
            }
        }
    }
    return settings->fallbackPageTag;   // "Others holds everything the other four do not"
}

void UItemCatalogSubsystem::rebuildSlotLookup(){
    slotByItem.Reset();
    itemsBySlot.Reset();
    reportedMissingSlots.Reset();
    // Table 2: slot -> the item that owns it, or the two items of a declared variant pair.
    for(const TPair<FName, TObjectPtr<UDA_ItemDefinition>>& pair : definitions){
        if(pair.Value->inventorySlot.isSet()){
            itemsBySlot.FindOrAdd(makeSlotKey(pair.Value->inventorySlot)).Add(pair.Key);
        }
    }
    for(TPair<FString, TArray<FName>>& pair : itemsBySlot){
        // The item that owns the slot first, its variant second: the two halves of the pocket never swap.
        pair.Value.Sort([this](const FName& left, const FName& right){
            const bool isLeftVariant = !getDefinition(left)->slotSharedWith.IsNone();
            const bool isRightVariant = !getDefinition(right)->slotSharedWith.IsNone();
            return isLeftVariant != isRightVariant ? !isLeftVariant : left.LexicalLess(right);
        });
    }
    // Table 1: item -> page and slot.
    for(const TPair<FName, TObjectPtr<UDA_ItemDefinition>>& pair : definitions){
        if(pair.Value->inventorySlot.isSet()){
            FS_ItemSlotInfo info;
            info.pageTag = getPageFor(pair.Value);
            info.slot = pair.Value->inventorySlot;
            info.itemsInSlot = itemsBySlot.FindRef(makeSlotKey(pair.Value->inventorySlot));
            slotByItem.Add(pair.Key, info);
        }
    }
}

bool UItemCatalogSubsystem::getSlotInfo(FName itemId, FS_ItemSlotInfo& outInfo) const{
    const FS_ItemSlotInfo* found = slotByItem.Find(itemId);
    if(found != nullptr){
        outInfo = *found;
        return true;
    }
    // Fail-safe (R01 3.1b): the data check makes this impossible in a build. If it still happens, say so once,
    // do not draw the item, and never invent a slot at runtime.
    if(!reportedMissingSlots.Contains(itemId)){
        reportedMissingSlots.Add(itemId);
        INVENTORY_LOG(Error, this, "Item %s has no slot on the bag screen: it is held and usable but is not drawn", *itemId.ToString());
    }
    return false;
}

TArray<FName> UItemCatalogSubsystem::getItemsOnPage(FGameplayTag pageTag) const{
    TArray<FName> items;
    for(const TPair<FName, FS_ItemSlotInfo>& pair : slotByItem){
        if(pair.Value.pageTag == pageTag){
            items.Add(pair.Key);
        }
    }
    return items;
}

int32 UItemCatalogSubsystem::getDisplayQuantity(const UInventoryData* inventory, FName itemId) const{
    return inventory != nullptr ? inventory->getAvailableQuantity(itemId) : 0;   // held minus reserved: what every reader sees
}

int32 UItemCatalogSubsystem::getStagedQuantity(const UInventoryData* inventory, FName itemId) const{
    return inventory != nullptr ? inventory->getTotalQuantity(itemId) - inventory->getAvailableQuantity(itemId) : 0;
}

FS_InventorySlotRef UItemCatalogSubsystem::getStagingSlot(FName itemId) const{
    const UDA_ItemDefinition* definition = getDefinition(itemId);
    if(definition == nullptr){
        return FS_InventorySlotRef();
    }
    return definition->stagingSlot.isSet() ? definition->stagingSlot : definition->inventorySlot;
}
