// Doc R01 guide S11 Verify / S13.5 "Save round trip (R01's part)", tier 2.
// Calls Doc R13's IBPI_SaveParticipant functions directly on the component (Character) and the registry (World).
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestCharacter.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/ALooseItemActor.h"
#include "ItemInventoryFramework/InventoryTime.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UBFL_InventoryBag.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "Engine/World.h"
#include "StructUtils/InstancedStruct.h"

namespace InventorySaveSpecHelpers{
    FS_InventoryEntry freshInstance(const UDA_ItemDefinition* definition){
        FS_InventoryEntry entry;
        entry.itemId = definition->itemId;
        entry.totalQuantity = 1;
        entry.instanceId = FGuid::NewGuid();
        entry.state.durability = definition->durabilityMax;
        if(definition->isPortableContainer()){
            entry.state.containedInventoryId = FGuid::NewGuid();
        }
        return entry;
    }

    // The items of step 1.
    void addItems(FInventoryTestFixture& fixture){
        fixture.addStackable(TEXT("Stick"), 20);
        fixture.addStackable(TEXT("Stone"), 20);
        fixture.addStackable(TEXT("Arrow"), 40);
        fixture.addStackable(TEXT("Herb"), 10);
        UDA_ItemDefinition* spear = fixture.addInstanced(TEXT("Spear"), 4, 50.0f);
        spear->categoryTag = T_Item_Category_Weapon;
        spear->weaponType = E_WeaponType::Thrown;
        UDA_ItemDefinition* crossbow = fixture.addInstanced(TEXT("Crossbow"), 2, 80.0f);
        crossbow->categoryTag = T_Item_Category_Weapon;
        crossbow->weaponType = E_WeaponType::Crossbow;
        crossbow->ammoItemId = TEXT("Arrow");
        UDA_ItemDefinition* pouch = fixture.addInstanced(TEXT("Pouch"), 2, 0.0f);
        pouch->categoryTag = T_Item_Category_Container;
        pouch->capacityRules.defaultPerTypeLimit = 5;
        UDA_ItemDefinition* meat = fixture.addStackable(TEXT("RawMeat"), 10);
        meat->canSpoil = true;
        meat->spoilTimeHours = 24.0f;
        meat->spoiledItemId = TEXT("SpoiledMeat");
        fixture.addStackable(TEXT("SpoiledMeat"), 10);
    }

    bool snapshotNames(const FInstancedStruct& snapshot, FName itemId){
        TArray<const FS_SavedInventory*> inventories;
        if(const FS_PlayerInventorySave* character = snapshot.GetPtr<FS_PlayerInventorySave>()){
            inventories.Add(&character->bag);
            for(const FS_SavedInventory& contained : character->containedInventories){
                inventories.Add(&contained);
            }
        }
        if(const FS_WorldInventorySave* world = snapshot.GetPtr<FS_WorldInventorySave>()){
            for(const FS_SavedInventory& saved : world->inventories){
                inventories.Add(&saved);
            }
            for(const FS_SavedInventory& contained : world->containedInventories){
                inventories.Add(&contained);
            }
        }
        for(const FS_SavedInventory* saved : inventories){
            if(saved->entries.ContainsByPredicate([itemId](const FS_InventoryEntry& entry){ return entry.itemId == itemId; })){
                return true;
            }
        }
        return false;
    }
}

BEGIN_DEFINE_SPEC(FInventorySaveSpec, "Awaken.Inventory.Save", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FInventorySaveSpec)

void FInventorySaveSpec::Define(){
    using namespace InventorySaveSpecHelpers;

    BeforeEach([this](){
        FInventoryTestFixture::expectForeignStartupErrors(*this);
    });

    It("round trip: ids, durability, loaded flag, pouch contents, hotbar and spoil times; no reservation, no held item", [this](){
        FInventoryTestFixture fixture;
        addItems(fixture);
        fixture.setGameTime(100000);
        const int64 hour = InventoryTime::hoursToGameSeconds(1.0f);
        AInventoryTestPlayerState* player = fixture.makePlayer();
        fixture.makeCharacterFor(player);
        UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
        UHeldItemComponent* heldItem = UHeldItemComponent::findFor(player);
        UInventoryData* bag = inventory->getPersonalInventory();
        const UItemCatalogSubsystem* catalog = fixture.catalog();

        // ---- step 1 ----
        bag->addStack(TEXT("Stick"), 12);
        FS_InventoryEntry spearA = freshInstance(catalog->getDefinition(TEXT("Spear")));
        FS_InventoryEntry spearB = freshInstance(catalog->getDefinition(TEXT("Spear")));
        spearB.state.durability = 7.0f;
        bag->addInstance(spearA);
        bag->addInstance(spearB);
        FS_InventoryEntry crossbow = freshInstance(catalog->getDefinition(TEXT("Crossbow")));
        crossbow.state.isLoaded = true;
        bag->addInstance(crossbow);
        const FS_InventoryEntry pouch = freshInstance(catalog->getDefinition(TEXT("Pouch")));
        bag->addInstance(pouch);
        fixture.registry()->getContainedInventory(pouch, true)->addStack(TEXT("Stone"), 3);
        bag->addStack(TEXT("RawMeat"), 4, fixture.services()->getGameTime() + 10 * hour);
        inventory->bindHotbarSlot(1, TEXT("Spear"));
        inventory->bindHotbarSlot(4, TEXT("Stick"));
        FS_ItemRef stickRef;
        stickRef.itemId = TEXT("Stick");
        UBFL_Inventory::reserveForStaging(player, stickRef, 2, FGuid::NewGuid());
        FS_ItemRef spearRef;
        spearRef.instanceId = spearA.instanceId;
        heldItem->holdFromBag(spearRef);
        TestFalse(TEXT("a spear is in the hand before the save"), heldItem->getHeldState().isEmpty());
        UInventoryData* chest = fixture.makeContainer(FS_ContainerRules());
        chest->addStack(TEXT("Stone"), 5);
        const int64 chestMeatSpoilAt = fixture.services()->getGameTime() + 10 * hour;
        chest->addStack(TEXT("RawMeat"), 2, chestMeatSpoilAt);

        // ---- step 2 ----
        FInstancedStruct characterBytes;
        FInstancedStruct worldBytes;
        TestTrue(TEXT("Character captured"), IBPI_SaveParticipant::Execute_getSnapshotForSave(inventory, characterBytes));
        TestTrue(TEXT("World captured"), IBPI_SaveParticipant::Execute_getSnapshotForSave(fixture.registry(), worldBytes));
        const FS_PlayerInventorySave* captured = characterBytes.GetPtr<FS_PlayerInventorySave>();
        TestTrue(TEXT("no reservation in the file"), captured != nullptr && !captured->bag.entries.ContainsByPredicate([](const FS_InventoryEntry& entry){ return entry.reservedQuantity != 0; }));

        // ---- step 3: empty everything, so the restore must really write ----
        UBFL_Inventory::releaseAllStaged(player);
        bag->clearAll();
        chest->clearAll();
        inventory->setHotbarBindings(TArray<FS_HotbarBinding>());
        fixture.advanceHours(6.0f);

        // ---- step 4: World, then Character ----
        TestTrue(TEXT("World restored"), IBPI_SaveParticipant::Execute_restoreFromSnapshot(fixture.registry(), worldBytes, 1));
        TestTrue(TEXT("Character restored"), IBPI_SaveParticipant::Execute_restoreFromSnapshot(inventory, characterBytes, 1));
        UInventoryData* restored = inventory->getPersonalInventory();
        TestNotNull(TEXT("the bag is back"), restored);
        if(restored == nullptr){
            return;
        }
        TestEqual(TEXT("12 sticks"), restored->getTotalQuantity(TEXT("Stick")), 12);
        TestEqual(TEXT("all 12 available: no reservation"), restored->getAvailableQuantity(TEXT("Stick")), 12);
        const FS_InventoryEntry* worn = restored->findInstance(spearB.instanceId);
        TestTrue(TEXT("the same spear instance, durability 7"), worn != nullptr && worn->state.durability == 7.0f);
        TestTrue(TEXT("the other spear instance"), restored->findInstance(spearA.instanceId) != nullptr);
        const FS_InventoryEntry* bow = restored->findInstance(crossbow.instanceId);
        TestTrue(TEXT("the crossbow is still loaded"), bow != nullptr && bow->state.isLoaded);
        const FS_InventoryEntry* restoredPouch = restored->findInstance(pouch.instanceId);
        const UInventoryData* pouchContents = restoredPouch != nullptr ? fixture.registry()->getContainedInventory(*restoredPouch, false) : nullptr;
        TestTrue(TEXT("the pouch holds 3 stones"), pouchContents != nullptr && pouchContents->getTotalQuantity(TEXT("Stone")) == 3);
        TestTrue(TEXT("key 1 is Spear"), inventory->getBoundItemId(1) == FName(TEXT("Spear")));
        TestTrue(TEXT("key 4 is Stick"), inventory->getBoundItemId(4) == FName(TEXT("Stick")));
        TestTrue(TEXT("the hand is empty: the held item is not saved"), heldItem->getHeldState().isEmpty());
        const FS_InventoryEntry* bagMeat = restored->findStack(TEXT("RawMeat"));
        TestTrue(TEXT("the bag's meat has 10 hours left from now"), bagMeat != nullptr && bagMeat->spoilAtTime == fixture.services()->getGameTime() + 10 * hour);
        const FS_InventoryEntry* chestMeat = chest->findStack(TEXT("RawMeat"));
        TestTrue(TEXT("the chest's meat keeps its absolute spoil time (4 hours left)"), chestMeat != nullptr && chestMeat->spoilAtTime == chestMeatSpoilAt);
        TestEqual(TEXT("the chest's 5 stones"), chest->getTotalQuantity(TEXT("Stone")), 5);

        // ---- step 5: an item that no longer exists ----
        FInstancedStruct patched = characterBytes;
        FS_PlayerInventorySave* patchedRecord = patched.GetMutablePtr<FS_PlayerInventorySave>();
        for(FS_InventoryEntry& entry : patchedRecord->bag.entries){
            if(entry.itemId == FName(TEXT("Stick"))){
                entry.itemId = TEXT("NoSuchItem");
            }
        }
        AddExpectedError(TEXT("Skipped saved item 'NoSuchItem'"), EAutomationExpectedErrorFlags::Contains, 1);
        TestTrue(TEXT("the load continues"), IBPI_SaveParticipant::Execute_restoreFromSnapshot(inventory, patched, 1));
        TestEqual(TEXT("the unknown entry is skipped"), inventory->getPersonalInventory()->getTotalQuantity(TEXT("Stick")), 0);
        TestTrue(TEXT("the others are restored"), inventory->getPersonalInventory()->findInstance(spearB.instanceId) != nullptr);

        // ---- step 6: a version with no migration step ----
        UInventoryData* before = inventory->getPersonalInventory();
        FInstancedStruct newer = characterBytes;
        newer.GetMutablePtr<FS_PlayerInventorySave>()->version = 99;
        TestFalse(TEXT("version 99 is refused"), IBPI_SaveParticipant::Execute_restoreFromSnapshot(inventory, newer, 99));
        TestTrue(TEXT("and the bag is unchanged"), inventory->getPersonalInventory() == before);
    });

    It("Bagless: no Character capture, the death bag is in the World file, the world bag wins, hold slots are never saved", [this](){
        FInventoryTestFixture fixture;
        addItems(fixture);
        AInventoryTestPlayerState* player = fixture.makePlayer();
        fixture.makeCharacterFor(player);
        UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
        inventory->getPersonalInventory()->addStack(TEXT("Stick"), 4);
        const FGuid bagId = inventory->getPersonalInventory()->getInventoryId();

        // step 8 begins: the Character file while holding the bag
        FInstancedStruct olderCharacter;
        TestTrue(TEXT("captured with the bag"), IBPI_SaveParticipant::Execute_getSnapshotForSave(inventory, olderCharacter));

        // step 7: Bagless capture
        TestTrue(TEXT("detached"), UBFL_InventoryBag::detachBag(player, FVector::ZeroVector).succeeded());
        AddExpectedError(TEXT("No inventory snapshot for"), EAutomationExpectedErrorFlags::Contains, 1);
        FInstancedStruct baglessCharacter;
        TestFalse(TEXT("a Bagless player is never captured"), IBPI_SaveParticipant::Execute_getSnapshotForSave(inventory, baglessCharacter));
        FInstancedStruct world;
        IBPI_SaveParticipant::Execute_getSnapshotForSave(fixture.registry(), world);
        const FS_WorldInventorySave* worldRecord = world.GetPtr<FS_WorldInventorySave>();
        TestTrue(TEXT("the World file holds the bag as a death bag"), worldRecord != nullptr && worldRecord->inventories.ContainsByPredicate([&bagId](const FS_SavedInventory& saved){
            return saved.inventoryId == bagId && saved.isDeathBag;
        }));

        // step 9: an item in a hold slot is in neither blob
        FS_InventoryEntry herb;
        herb.itemId = TEXT("Herb");
        herb.totalQuantity = 1;
        ALooseItemActor* looseItem = fixture.world()->SpawnActor<ALooseItemActor>();
        looseItem->setEntry(herb);
        TestEqual(TEXT("the herb goes into a hold slot"), UBFL_Inventory::pickUpLooseItem(player, looseItem).quantity, 1);
        FInstancedStruct worldWithHeld;
        IBPI_SaveParticipant::Execute_getSnapshotForSave(fixture.registry(), worldWithHeld);
        TestFalse(TEXT("not in the World blob"), snapshotNames(worldWithHeld, TEXT("Herb")));
        TestFalse(TEXT("not in the Character blob"), snapshotNames(olderCharacter, TEXT("Herb")));

        // step 8: the world bag wins
        TestTrue(TEXT("World restored"), IBPI_SaveParticipant::Execute_restoreFromSnapshot(fixture.registry(), world, 1));
        TestTrue(TEXT("older Character restored"), IBPI_SaveParticipant::Execute_restoreFromSnapshot(inventory, olderCharacter, 1));
        TestFalse(TEXT("the player is Bagless"), inventory->hasBag());
        int32 deathBags = 0;
        for(const TPair<FGuid, TObjectPtr<UInventoryData>>& pair : fixture.registry()->getAllInventories()){
            deathBags += pair.Value->isDeathBag() ? 1 : 0;
        }
        TestEqual(TEXT("one death bag, not two"), deathBags, 1);
    });
}

#endif
