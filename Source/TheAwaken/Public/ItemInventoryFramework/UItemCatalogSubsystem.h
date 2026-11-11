#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UItemCatalogSubsystem.generated.h"

class UDA_ItemDefinition;
class UInventoryData;

// Looks an item definition up by its id. Exists on every machine; the data is static content (Doc00 §3).
UCLASS()
class THEAWAKEN_API UItemCatalogSubsystem : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static UItemCatalogSubsystem* get(const UObject* worldContext);

    // ---- queries ----
    // Null when no item has that id (a removed item in an old save, a typo in a command). Never crashes.
    const UDA_ItemDefinition* getDefinition(FName itemId) const;
    // The always-available Unarmed entry (R01 3.10).
    const UDA_ItemDefinition* getUnarmedDefinition() const;
    // The fresh item whose spoiledItemId is this id, or null (R01 3.2a).
    const UDA_ItemDefinition* findFreshItemFor(FName spoiledItemId) const;
    const TMap<FName, TObjectPtr<UDA_ItemDefinition>>& getAllDefinitions() const;

    // ---- commands ----
    // Adds one definition. Used by the loader and by tests.
    void registerDefinition(UDA_ItemDefinition* definition);
    // Tests only: forget every definition.
    void resetForTest();

private:
    void loadAllDefinitions();

    UPROPERTY() TMap<FName, TObjectPtr<UDA_ItemDefinition>> definitions;

public:
    // ---- Section 10: the bag-screen lookup (R01 3.1b). Presentation only ----
    // The page of an item, derived from its category. The fallback page for a null definition.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FGameplayTag getPageFor(const UDA_ItemDefinition* definition) const;
    // Table 1. False, with one Error line naming the item, when the item has no slot: it is still held and usable, only not drawn.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") bool getSlotInfo(FName itemId, FS_ItemSlotInfo& outInfo) const;
    // Every item id drawn on a page.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") TArray<FName> getItemsOnPage(FGameplayTag pageTag) const;
    // What the slot of an item shows: the available quantity of a stack, or the number of free copies (R01 3.1b, 3.6).
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") int32 getDisplayQuantity(const UInventoryData* inventory, FName itemId) const;
    // What the staging widget shows for an item: the reserved quantity (Doc R02 3.3).
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") int32 getStagedQuantity(const UInventoryData* inventory, FName itemId) const;
    // The slot of the staging widget: the item's stagingSlot, or its inventory slot when none is set.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FS_InventorySlotRef getStagingSlot(FName itemId) const;

    // Command. Builds both tables. Runs when the item data loads; a test calls it after adding its definitions.
    void rebuildSlotLookup();

private:
    static FString makeSlotKey(const FS_InventorySlotRef& slot);

    TMap<FName, FS_ItemSlotInfo> slotByItem;       // table 1
    TMap<FString, TArray<FName>> itemsBySlot;      // table 2
    mutable TSet<FName> reportedMissingSlots;
};
