#include "ItemInventoryFramework/UBFL_InventoryValidation.h"
#include "ItemInventoryFramework/UDA_InventoryPageLayout.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UDA_StackLayout.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace{
    struct FSlottedItem{
        const UDA_ItemDefinition* item = nullptr;
        const FS_InventorySlotDef* slot = nullptr;
    };
}

int32 UBFL_InventoryValidation::validateCatalog(const UObject* worldContext, TArray<FString>& outErrors){
    outErrors.Reset();
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(worldContext);
    const UInventorySettings* settings = UInventorySettings::get();
    if(catalog == nullptr){
        outErrors.Add(TEXT("The item catalog is not available."));
        return outErrors.Num();
    }

    // ---- the page mapping: no item may map to a page that has no layout ----
    TMap<FGameplayTag, const UDA_InventoryPageLayout*> layoutByPage;
    for(const FS_InventoryPageMapping& mapping : settings->pageMappings){
        const UDA_InventoryPageLayout* layout = mapping.layout.LoadSynchronous();
        if(!mapping.pageTag.IsValid() || layout == nullptr){
            outErrors.Add(FString::Printf(TEXT("Page mapping '%s' has no page tag or no layout."), *mapping.pageTag.ToString()));
            continue;
        }
        if(layout->pageTag != mapping.pageTag){
            outErrors.Add(FString::Printf(TEXT("Layout %s says page %s but is mapped to page %s."), *layout->GetName(), *layout->pageTag.ToString(), *mapping.pageTag.ToString()));
        }
        layoutByPage.Add(mapping.pageTag, layout);
    }
    if(!layoutByPage.Contains(settings->fallbackPageTag)){
        outErrors.Add(TEXT("The fallback page (Others) has no layout: an item of an unlisted category would have no page."));
    }

    int32 unbreakableCount = 0;
    TMap<FString, TArray<FSlottedItem>> itemsBySlot;
    for(const TPair<FName, TObjectPtr<UDA_ItemDefinition>>& pair : catalog->getAllDefinitions()){
        const UDA_ItemDefinition* item = pair.Value;
        const FString name = item->itemId.ToString();
        unbreakableCount += item->isUnbreakable() ? 1 : 0;

#if WITH_EDITOR
        // The item's own check (S2.12, and the armor and clothes rules the R05 guide adds to it).
        FDataValidationContext context;
        if(item->IsDataValid(context) == EDataValidationResult::Invalid){
            TArray<FText> warnings;
            TArray<FText> errors;
            context.SplitIssues(warnings, errors);
            for(const FText& error : errors){
                outErrors.Add(error.ToString());
            }
        }
#endif

        // ---- a reference to something that does not exist (Doc02 19.8) ----
        if(item->canSpoil){
            const UDA_ItemDefinition* spoiled = catalog->getDefinition(item->spoiledItemId);
            if(spoiled == nullptr || !spoiled->isStackable()){
                outErrors.Add(FString::Printf(TEXT("%s: spoiledItemId '%s' does not exist or is not stackable."), *name, *item->spoiledItemId.ToString()));
            }
        }
        if(!item->effectBaseItemId.IsNone() && catalog->getDefinition(item->effectBaseItemId) == nullptr){
            outErrors.Add(FString::Printf(TEXT("%s: effectBaseItemId '%s' does not exist."), *name, *item->effectBaseItemId.ToString()));
        }
        if(!item->ammoItemId.IsNone()){
            const UDA_ItemDefinition* ammo = catalog->getDefinition(item->ammoItemId);
            if(ammo == nullptr || !ammo->isStackable()){
                outErrors.Add(FString::Printf(TEXT("%s: ammoItemId '%s' does not exist or is not stackable."), *name, *item->ammoItemId.ToString()));
            }
        }
        // A variant derives its meter values from its base item; its own list holds only the illness chance and the HP loss.
        const bool isVariant = !item->effectBaseItemId.IsNone() || catalog->findFreshItemFor(item->itemId) != nullptr;
        const bool hasOwnMeterEntry = item->consumeEffects.ContainsByPredicate([](const FS_ConsumeEffect& effect){
            return effect.kind == E_ConsumeEffectKind::ModifyMeter;
        });
        if(isVariant && hasOwnMeterEntry){
            outErrors.Add(FString::Printf(TEXT("%s: a spoiled or unclean variant must not list ModifyMeter entries; they are derived from its base item."), *name));
        }

        // ---- the slot (R01 3.1a, 3.1b) ----
        const bool isBagItem = item->itemId != settings->unarmedItemId && !item->isHeavy();
        if(!isBagItem || !item->inventorySlot.isSet()){
            continue;   // a bag item without a slot is reported by its own check above
        }
        const UDA_InventoryPageLayout* pageLayout = layoutByPage.FindRef(catalog->getPageFor(item));
        const UDA_InventoryPageLayout* slotLayout = item->inventorySlot.pageLayout.LoadSynchronous();
        if(slotLayout != pageLayout){
            outErrors.Add(FString::Printf(TEXT("%s: its category puts it on page %s, but its slot is in another layout. After a category change the slot must point into the layout of the new page."), *name, *catalog->getPageFor(item).ToString()));
        }
        const FS_InventorySlotDef* slot = slotLayout != nullptr ? slotLayout->findSlot(item->inventorySlot.slotId) : nullptr;
        if(slot == nullptr){
            outErrors.Add(FString::Printf(TEXT("%s: slot '%s' does not exist in its layout (deleted or renumbered?)."), *name, *item->inventorySlot.slotId.ToString()));
            continue;
        }
        FSlottedItem slotted;
        slotted.item = item;
        slotted.slot = slot;
        itemsBySlot.FindOrAdd(item->inventorySlot.pageLayout.ToSoftObjectPath().ToString() + TEXT("|") + item->inventorySlot.slotId.ToString()).Add(slotted);
    }

    // ---- no two items share a slot unless they are declared variants; a full pile fits inside its pocket ----
    for(const TPair<FString, TArray<FSlottedItem>>& pair : itemsBySlot){
        const TArray<FSlottedItem>& sharing = pair.Value;
        const bool isDeclaredPair = sharing.Num() == 2 && (sharing[0].item->slotSharedWith == sharing[1].item->itemId || sharing[1].item->slotSharedWith == sharing[0].item->itemId);
        if(sharing.Num() > 1 && !isDeclaredPair){
            outErrors.Add(FString::Printf(TEXT("Slot %s is used by %d items (%s, %s, ...) that are not a declared variant pair."), *pair.Key, sharing.Num(), *sharing[0].item->itemId.ToString(), *sharing[1].item->itemId.ToString()));
        }
        for(const FSlottedItem& slotted : sharing){
            const UDA_StackLayout* stackLayout = slotted.item->stackLayout.LoadSynchronous();
            // A shared pocket is split into halves (PROPOSED in R01 3.2a), so each pile has half the width.
            const double pocketWidth = slotted.slot->size.X / sharing.Num();
            if(stackLayout != nullptr && (stackLayout->pileFootprint.X > pocketWidth || stackLayout->pileFootprint.Y > slotted.slot->size.Y)){
                outErrors.Add(FString::Printf(TEXT("%s: a full pile of layout %s does not fit inside slot '%s'."), *slotted.item->itemId.ToString(), *stackLayout->GetName(), *slotted.slot->slotId.ToString()));
            }
        }
    }
    for(const TPair<FName, TObjectPtr<UDA_ItemDefinition>>& pair : catalog->getAllDefinitions()){
        const UDA_ItemDefinition* item = pair.Value;
        if(item->slotSharedWith.IsNone()){
            continue;
        }
        const UDA_ItemDefinition* partner = catalog->getDefinition(item->slotSharedWith);
        if(partner == nullptr || partner->inventorySlot.slotId != item->inventorySlot.slotId || partner->inventorySlot.pageLayout != item->inventorySlot.pageLayout){
            outErrors.Add(FString::Printf(TEXT("%s: slotSharedWith '%s' does not exist or does not use the same slot."), *item->itemId.ToString(), *item->slotSharedWith.ToString()));
        }
    }

    // ---- the special items (R01 3.1, 3.8, 3.10) ----
    if(unbreakableCount != 1){
        outErrors.Add(FString::Printf(TEXT("%d items are unbreakable. Exactly one item in the whole catalog is: the starter Axe."), unbreakableCount));
    }
    const UDA_ItemDefinition* unarmed = catalog->getUnarmedDefinition();
    if(unarmed == nullptr || unarmed->weaponType != E_WeaponType::Melee || unarmed->durabilityMax != 0.0f){
        outErrors.Add(TEXT("The Unarmed entry is missing, is not a Melee weapon, or has durability."));
    }
    for(const FS_ItemStack& stack : settings->startingKit){
        if(catalog->getDefinition(stack.itemId) == nullptr){
            outErrors.Add(FString::Printf(TEXT("The starting kit names '%s', which does not exist."), *stack.itemId.ToString()));
        }
    }
    return outErrors.Num();
}
