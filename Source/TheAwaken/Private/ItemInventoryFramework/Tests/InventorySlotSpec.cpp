// Doc R01 guide S13.5: "Slot data check / Slot lookup / Page mapping" and "Fresh and spoiled share a slot" (tier 1).
// The rows of Section 10's Verify table. Real content is used through TRANSIENT COPIES, so a test never changes an asset.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UBFL_InventoryValidation.h"
#include "ItemInventoryFramework/UDA_InventoryPageLayout.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UDA_StackLayout.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "Engine/AssetManager.h"
#include "UObject/Package.h"

namespace InventorySlotSpecHelpers{
    // Every real item definition (the S2.14 content), copied into the transient package and registered in the fixture's catalog.
    TMap<FName, UDA_ItemDefinition*> loadContentCopies(FInventoryTestFixture& fixture){
        TMap<FName, UDA_ItemDefinition*> copies;
        UAssetManager& manager = UAssetManager::Get();
        TArray<FPrimaryAssetId> ids;
        manager.GetPrimaryAssetIdList(UDA_ItemDefinition::getPrimaryAssetType(), ids);
        for(const FPrimaryAssetId& id : ids){
            const UDA_ItemDefinition* original = Cast<UDA_ItemDefinition>(manager.GetPrimaryAssetPath(id).TryLoad());
            if(original != nullptr){
                UDA_ItemDefinition* copy = DuplicateObject<UDA_ItemDefinition>(original, GetTransientPackage());
                fixture.catalog()->registerDefinition(copy);
                copies.Add(copy->itemId, copy);
            }
        }
        fixture.catalog()->rebuildSlotLookup();
        return copies;
    }

    bool anyErrorContains(const TArray<FString>& errors, const TCHAR* text){
        return errors.ContainsByPredicate([text](const FString& error){
            return error.Contains(text);
        });
    }
}

BEGIN_DEFINE_SPEC(FInventorySlotSpec, "Awaken.Inventory.Slots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FInventorySlotSpec)

void FInventorySlotSpec::Define(){
    using namespace InventorySlotSpecHelpers;


    Describe("Page mapping", [this](){
        BeforeEach([this](){
            FInventoryTestFixture::expectForeignStartupErrors(*this);
        });

        It("derives the page from the category, and every unlisted category lands on Others", [this](){
            FInventoryTestFixture fixture;
            TMap<FName, UDA_ItemDefinition*> items = loadContentCopies(fixture);
            const UItemCatalogSubsystem* catalog = fixture.catalog();
            TestTrue(TEXT("the content loaded"), items.Num() >= 16);
            TestTrue(TEXT("Spear -> Weapons"), catalog->getPageFor(items.FindRef(TEXT("Spear"))) == T_Inventory_Page_Weapons);
            TestTrue(TEXT("Map -> Tools"), catalog->getPageFor(items.FindRef(TEXT("Map"))) == T_Inventory_Page_Tools);
            TestTrue(TEXT("RawMeat -> Consumables"), catalog->getPageFor(items.FindRef(TEXT("RawMeat"))) == T_Inventory_Page_Consumables);
            TestTrue(TEXT("Stick -> Materials"), catalog->getPageFor(items.FindRef(TEXT("Stick"))) == T_Inventory_Page_Materials);
            TestTrue(TEXT("IronVest (armor) -> Others"), catalog->getPageFor(items.FindRef(TEXT("IronVest"))) == T_Inventory_Page_Others);
            TestTrue(TEXT("Pouch (container) -> Others"), catalog->getPageFor(items.FindRef(TEXT("Pouch"))) == T_Inventory_Page_Others);
            UDA_ItemDefinition* clothing = fixture.addInstanced(TEXT("TestShirt"), 1, 0.0f);
            clothing->categoryTag = T_Item_Category_Clothing;   // a category no mapping row names
            TestTrue(TEXT("a clothing item -> Others"), catalog->getPageFor(clothing) == T_Inventory_Page_Others);
            TestTrue(TEXT("a null definition -> Others"), catalog->getPageFor(nullptr) == T_Inventory_Page_Others);
        });
    });

    Describe("Slot lookup", [this](){
        BeforeEach([this](){
            FInventoryTestFixture::expectForeignStartupErrors(*this);
        });

        It("answers page, slot and the items of a shared slot, owner first", [this](){
            FInventoryTestFixture fixture;
            loadContentCopies(fixture);
            const UItemCatalogSubsystem* catalog = fixture.catalog();
            FS_ItemSlotInfo stick;
            TestTrue(TEXT("Stick has a slot"), catalog->getSlotInfo(TEXT("Stick"), stick));
            TestTrue(TEXT("on Materials"), stick.pageTag == T_Inventory_Page_Materials);
            TestTrue(TEXT("slot M1"), stick.slot.slotId == FName(TEXT("M1")));
            TestTrue(TEXT("alone in it"), stick.itemsInSlot.Num() == 1 && stick.itemsInSlot[0] == FName(TEXT("Stick")));

            FS_ItemSlotInfo spoiled;
            TestTrue(TEXT("SpoiledMeat has a slot"), catalog->getSlotInfo(TEXT("SpoiledMeat"), spoiled));
            TestTrue(TEXT("slot C1"), spoiled.slot.slotId == FName(TEXT("C1")));
            TestTrue(TEXT("[RawMeat, SpoiledMeat], the fresh one first"), spoiled.itemsInSlot.Num() == 2 && spoiled.itemsInSlot[0] == FName(TEXT("RawMeat")) && spoiled.itemsInSlot[1] == FName(TEXT("SpoiledMeat")));
        });

        It("reports an item without a slot once, and never invents one", [this](){
            FInventoryTestFixture fixture;
            loadContentCopies(fixture);
            AddExpectedError(TEXT("Item Log has no slot on the bag screen"), EAutomationExpectedErrorFlags::Contains, 1);
            FS_ItemSlotInfo info;
            TestFalse(TEXT("Log (heavy) has no slot"), fixture.catalog()->getSlotInfo(TEXT("Log"), info));
            TestFalse(TEXT("asked again: still no slot, no second line"), fixture.catalog()->getSlotInfo(TEXT("Log"), info));
        });

        It("shows the available quantity whatever the arrival order, and the staged part apart", [this](){
            FS_ItemStack sticks;
            sticks.itemId = TEXT("Stick");
            sticks.quantity = 3;
            FS_ItemStack spears;
            spears.itemId = TEXT("Spear");
            spears.quantity = 2;
            int32 firstStick = 0;
            int32 firstSpear = 0;
            for(int32 run = 0; run < 2; run++){
                FInventoryTestFixture fixture;
                loadContentCopies(fixture);
                AInventoryTestPlayerState* player = fixture.makePlayer();
                if(run == 0){
                    UBFL_Inventory::grantOrDrop(player, sticks);
                    UBFL_Inventory::grantOrDrop(player, spears);
                }else{
                    UBFL_Inventory::grantOrDrop(player, spears);
                    UBFL_Inventory::grantOrDrop(player, sticks);
                }
                const UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
                const int32 stickCount = fixture.catalog()->getDisplayQuantity(bag, TEXT("Stick"));
                const int32 spearCount = fixture.catalog()->getDisplayQuantity(bag, TEXT("Spear"));
                if(run == 0){
                    firstStick = stickCount;
                    firstSpear = spearCount;
                }else{
                    TestEqual(TEXT("the same stick count in either order"), stickCount, firstStick);
                    TestEqual(TEXT("the same spear count in either order"), spearCount, firstSpear);
                }
            }
            TestEqual(TEXT("3 sticks"), firstStick, 3);
            TestEqual(TEXT("2 spears"), firstSpear, 2);

            FInventoryTestFixture fixture;
            loadContentCopies(fixture);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            sticks.quantity = 5;
            UBFL_Inventory::grantOrDrop(player, sticks);
            FS_ItemRef stickRef;
            stickRef.itemId = TEXT("Stick");
            UBFL_Inventory::reserveForStaging(player, stickRef, 2, FGuid::NewGuid());
            const UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            TestEqual(TEXT("display 3"), fixture.catalog()->getDisplayQuantity(bag, TEXT("Stick")), 3);
            TestEqual(TEXT("staged 2"), fixture.catalog()->getStagedQuantity(bag, TEXT("Stick")), 2);
        });

        It("keeps fresh and spoiled as two piles of one slot, and spoiling moves the count", [this](){
            FInventoryTestFixture fixture;
            loadContentCopies(fixture);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            fixture.setGameTime(0);
            bag->addStack(TEXT("RawMeat"), 3, 3600);
            bag->addStack(TEXT("SpoiledMeat"), 1, 0);
            TestEqual(TEXT("3 fresh"), fixture.catalog()->getDisplayQuantity(bag, TEXT("RawMeat")), 3);
            TestEqual(TEXT("1 spoiled"), fixture.catalog()->getDisplayQuantity(bag, TEXT("SpoiledMeat")), 1);
            bag->evaluateSpoilage(3600);
            TestEqual(TEXT("0 fresh after spoiling"), fixture.catalog()->getDisplayQuantity(bag, TEXT("RawMeat")), 0);
            TestEqual(TEXT("4 spoiled"), fixture.catalog()->getDisplayQuantity(bag, TEXT("SpoiledMeat")), 4);
            bag->addStack(TEXT("RawMeat"), 2, 90000);
            TestEqual(TEXT("fresh is never merged into spoiled"), fixture.catalog()->getDisplayQuantity(bag, TEXT("SpoiledMeat")), 4);
        });
    });

    Describe("Stack layout", [this](){   // no fixture: reads one asset
        It("keeps a pile as full as it can look beyond its layout", [this](){
            const UDA_StackLayout* row = LoadObject<UDA_StackLayout>(nullptr, TEXT("/Game/Awaken/Inventory/DA_Stack_Row.DA_Stack_Row"));
            TestNotNull(TEXT("DA_Stack_Row exists"), row);
            if(row != nullptr){
                TestEqual(TEXT("4 units for a quantity of 7"), row->getVisibleUnitCount(7), 4);
                TestEqual(TEXT("0 for nothing"), row->getVisibleUnitCount(0), 0);
            }
        });

    });

    Describe("Slot data check", [this](){
        BeforeEach([this](){
            FInventoryTestFixture::expectForeignStartupErrors(*this);
        });

        It("finds no error in the starter content", [this](){
            FInventoryTestFixture fixture;
            loadContentCopies(fixture);
            TArray<FString> errors;
            const int32 count = UBFL_InventoryValidation::validateCatalog(fixture.world(), errors);
            for(const FString& error : errors){
                AddInfo(error);
            }
            TestEqual(TEXT("0 errors on the S2.14 content"), count, 0);
        });

        It("catches each broken rule of R01 3.1a", [this](){
            FInventoryTestFixture fixture;
            TMap<FName, UDA_ItemDefinition*> items = loadContentCopies(fixture);
            TArray<FString> errors;
            UDA_ItemDefinition* stone = items.FindRef(TEXT("Stone"));
            UDA_ItemDefinition* stick = items.FindRef(TEXT("Stick"));
            UDA_ItemDefinition* spoiled = items.FindRef(TEXT("SpoiledMeat"));
            UDA_ItemDefinition* spear = items.FindRef(TEXT("Spear"));
            if(stone == nullptr || stick == nullptr || spoiled == nullptr || spear == nullptr){
                AddError(TEXT("the starter content is incomplete"));
                return;
            }

            const FS_InventorySlotRef stoneSlot = stone->inventorySlot;
            stone->inventorySlot = stick->inventorySlot;
            UBFL_InventoryValidation::validateCatalog(fixture.world(), errors);
            TestTrue(TEXT("two unrelated items in one slot"), anyErrorContains(errors, TEXT("is used by 2 items")));
            stone->inventorySlot = stoneSlot;

            stick->categoryTag = T_Item_Category_Tool;
            UBFL_InventoryValidation::validateCatalog(fixture.world(), errors);
            TestTrue(TEXT("a category change without a slot change"), anyErrorContains(errors, TEXT("its category puts it on page Inventory.Page.Tools")));
            stick->categoryTag = T_Item_Category_Material;

            stone->inventorySlot.slotId = TEXT("M99");
            UBFL_InventoryValidation::validateCatalog(fixture.world(), errors);
            TestTrue(TEXT("a deleted slot"), anyErrorContains(errors, TEXT("Stone: slot 'M99' does not exist in its layout")));
            stone->inventorySlot = stoneSlot;

            FS_ConsumeEffect meter;
            meter.targetTag = T_Survival_Meter_Illness;
            meter.amount = 10.0f;
            spoiled->consumeEffects.Add(meter);
            UBFL_InventoryValidation::validateCatalog(fixture.world(), errors);
            TestTrue(TEXT("a variant with its own meter entry"), anyErrorContains(errors, TEXT("must not list ModifyMeter entries")));
            spoiled->consumeEffects.Pop();

            spear->durabilityMax = UDA_ItemDefinition::unbreakableDurability;
            spear->itemTags.AddTag(T_Item_Flag_Permanent);
            UBFL_InventoryValidation::validateCatalog(fixture.world(), errors);
            TestTrue(TEXT("a second unbreakable item"), anyErrorContains(errors, TEXT("2 items are unbreakable")));
            spear->durabilityMax = 50.0f;
            spear->itemTags.RemoveTag(T_Item_Flag_Permanent);

            UDA_StackLayout* bigHeap = DuplicateObject<UDA_StackLayout>(stone->stackLayout.LoadSynchronous(), GetTransientPackage());
            bigHeap->pileFootprint = FVector2D(500.0, 500.0);
            const TSoftObjectPtr<UDA_StackLayout> stoneLayout = stone->stackLayout;
            stone->stackLayout = bigHeap;
            UBFL_InventoryValidation::validateCatalog(fixture.world(), errors);
            TestTrue(TEXT("a pile that does not fit its pocket"), anyErrorContains(errors, TEXT("does not fit inside slot 'M2'")));
            stone->stackLayout = stoneLayout;

            UInventorySettings* settings = GetMutableDefault<UInventorySettings>();
            const TArray<FS_InventoryPageMapping> savedMappings = settings->pageMappings;
            settings->pageMappings.RemoveAll([](const FS_InventoryPageMapping& mapping){
                return mapping.pageTag == T_Inventory_Page_Others;
            });
            UBFL_InventoryValidation::validateCatalog(fixture.world(), errors);
            settings->pageMappings = savedMappings;
            TestTrue(TEXT("no layout for the fallback page"), anyErrorContains(errors, TEXT("The fallback page (Others) has no layout")));

            TestEqual(TEXT("everything undone: 0 errors again"), UBFL_InventoryValidation::validateCatalog(fixture.world(), errors), 0);
        });
    });
}

#endif
