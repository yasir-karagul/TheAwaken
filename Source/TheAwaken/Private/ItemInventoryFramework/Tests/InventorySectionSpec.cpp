// The automatable rows of the Verify tables of Sections 1 and 5 to 9 of the Doc R01 guide, beyond the four specs of S13.5.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/IConsoleManager.h"
#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestCharacter.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/Stubs/InventoryStubs.h"
#include "ItemInventoryFramework/ALooseItemActor.h"
#include "ItemInventoryFramework/InventoryTime.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UBFL_InventoryBag.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "Engine/World.h"

namespace InventorySectionSpecHelpers{
    FS_ItemStack stackOf(FName itemId, int32 quantity){
        FS_ItemStack stack;
        stack.itemId = itemId;
        stack.quantity = quantity;
        return stack;
    }

    FS_ItemRef refTo(FName itemId){
        FS_ItemRef ref;
        ref.itemId = itemId;
        return ref;
    }

    FS_ItemRef refToInstance(const FGuid& instanceId){
        FS_ItemRef ref;
        ref.instanceId = instanceId;
        return ref;
    }

    ALooseItemActor* spawnLoose(const FInventoryTestFixture& fixture, const FS_InventoryEntry& entry){
        ALooseItemActor* looseItem = fixture.world()->SpawnActor<ALooseItemActor>();
        looseItem->setEntry(entry);
        return looseItem;
    }

    ALooseItemActor* spawnLooseStack(const FInventoryTestFixture& fixture, FName itemId, int32 quantity, int64 spoilAtTime = 0){
        FS_InventoryEntry entry;
        entry.itemId = itemId;
        entry.totalQuantity = quantity;
        entry.spoilAtTime = spoilAtTime;
        return spawnLoose(fixture, entry);
    }

    void setFailBagStep(int32 step){
        if(IConsoleVariable* variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Awaken.Inventory.FailBagStep"))){
            variable->Set(step);
        }
    }

    // The S2.14 test items, with the fields the Verify tables need.
    void addCatalog(FInventoryTestFixture& fixture){
        UDA_ItemDefinition* unarmed = fixture.addInstanced(TEXT("Unarmed"), 0, 0.0f);
        unarmed->categoryTag = T_Item_Category_Weapon;
        unarmed->weaponType = E_WeaponType::Melee;
        UDA_ItemDefinition* axe = fixture.addInstanced(TEXT("StarterAxe"), 1, UDA_ItemDefinition::unbreakableDurability);
        axe->categoryTag = T_Item_Category_Weapon;
        axe->weaponType = E_WeaponType::Melee;
        axe->itemTags.AddTag(T_Item_Flag_Permanent);
        UDA_ItemDefinition* map = fixture.addStackable(TEXT("Map"), 1);
        map->categoryTag = T_Item_Category_Tool;
        map->itemTags.AddTag(T_Item_Flag_Permanent);
        UDA_ItemDefinition* spear = fixture.addInstanced(TEXT("Spear"), 4, 50.0f);
        spear->categoryTag = T_Item_Category_Weapon;
        spear->weaponType = E_WeaponType::Thrown;
        UDA_ItemDefinition* crossbow = fixture.addInstanced(TEXT("Crossbow"), 2, 80.0f);
        crossbow->categoryTag = T_Item_Category_Weapon;
        crossbow->weaponType = E_WeaponType::Crossbow;
        crossbow->ammoItemId = TEXT("Arrow");
        UDA_ItemDefinition* rifle = fixture.addInstanced(TEXT("Rifle"), 1, 80.0f);
        rifle->categoryTag = T_Item_Category_Weapon;
        rifle->weaponType = E_WeaponType::Firearm;
        rifle->ammoItemId = TEXT("Bullet");
        rifle->magazineSize = 6;
        fixture.addStackable(TEXT("Arrow"), 40);
        fixture.addStackable(TEXT("Bullet"), 40);
        fixture.addStackable(TEXT("Stick"), 20);
        fixture.addStackable(TEXT("Stone"), 20);
        UDA_ItemDefinition* log = fixture.addStackable(TEXT("Log"), 1);
        log->carryCapacityCost = 1;
        UDA_ItemDefinition* meat = fixture.addStackable(TEXT("RawMeat"), 10);
        meat->categoryTag = T_Item_Category_Food;
        meat->canSpoil = true;
        meat->spoilTimeHours = 24.0f;
        meat->spoiledItemId = TEXT("SpoiledMeat");
        meat->spoiledEffectiveness = 0.5f;
        FS_ConsumeEffect hunger;
        hunger.targetTag = T_Item_Category_Food;   // stands in for Survival.Meter.Hunger (owned by Doc B03)
        hunger.amount = 100.0f;
        meat->consumeEffects.Add(hunger);
        UDA_ItemDefinition* spoiled = fixture.addStackable(TEXT("SpoiledMeat"), 10);
        spoiled->categoryTag = T_Item_Category_Food;
        FS_ConsumeEffect illness;
        illness.kind = E_ConsumeEffectKind::IllnessChance;
        illness.targetTag = T_Item_Category_Material;   // stands in for Survival.IllnessSource.SpoiledFood
        illness.chance = 1.0f;
        spoiled->consumeEffects.Add(illness);
        FS_ConsumeEffect healthLoss;
        healthLoss.kind = E_ConsumeEffectKind::HealthLoss;
        healthLoss.amount = 10.0f;
        spoiled->consumeEffects.Add(healthLoss);
        UDA_ItemDefinition* clean = fixture.addStackable(TEXT("CleanWater"), 10);
        clean->categoryTag = T_Item_Category_Water;
        FS_ConsumeEffect thirst;
        thirst.targetTag = T_Item_Category_Water;   // stands in for Survival.Meter.Thirst
        thirst.amount = 100.0f;
        clean->consumeEffects.Add(thirst);
        UDA_ItemDefinition* unclean = fixture.addStackable(TEXT("UncleanWater"), 10);
        unclean->categoryTag = T_Item_Category_Water;
        unclean->effectBaseItemId = TEXT("CleanWater");
        unclean->uncleanWaterEffectiveness = 0.5f;
        unclean->consumeEffects.Add(healthLoss);
        UDA_ItemDefinition* herb = fixture.addStackable(TEXT("Herb"), 10);
        herb->categoryTag = T_Item_Category_Medicine;
        herb->healthRestoreAmount = 20.0f;
        herb->illnessReductionAmount = 10.0f;
        UDA_ItemDefinition* pouch = fixture.addInstanced(TEXT("Pouch"), 2, 0.0f);
        pouch->categoryTag = T_Item_Category_Container;
        pouch->capacityRules.defaultPerTypeLimit = 5;
    }

    FGuid addInstance(FInventoryTestFixture& fixture, UInventoryData* inventory, FName itemId, float durability = -2.0f){
        FS_InventoryEntry entry = UBFL_Inventory::makeFreshInstance(fixture.world(), *fixture.catalog()->getDefinition(itemId));
        if(durability > -2.0f){
            entry.state.durability = durability;
        }
        inventory->addInstance(entry);
        return entry.instanceId;
    }
}

BEGIN_DEFINE_SPEC(FInventorySectionSpec, "Awaken.Inventory.Sections", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FInventorySectionSpec)

void FInventorySectionSpec::Define(){
    using namespace InventorySectionSpecHelpers;


    // ---------------- Section 1 ----------------
    Describe("S1 types", [this](){
        It("default values", [this](){
            const FS_InventoryEntry entry;
            TestFalse(TEXT("a fresh entry is not instanced"), entry.isInstanced());
            TestEqual(TEXT("and has nothing available"), entry.getAvailableQuantity(), 0);
            TestTrue(TEXT("a fresh result succeeded"), FS_InventoryResult().succeeded());
            TestTrue(TEXT("a fresh gear release succeeded"), FS_GearReleaseResult().succeeded);
            FS_InventoryEntry left;
            left.itemId = TEXT("Stick");
            FS_InventoryEntry right;
            right.itemId = TEXT("Stick");
            right.totalQuantity = 3;
            TestTrue(TEXT("same id, no instance: the same line"), left.isSameLineAs(right));
        });

        It("the mirror equals the source after changes", [this](){
            TArray<FS_InventoryEntry> source;
            for(const TCHAR* id : {TEXT("Stick"), TEXT("Stone"), TEXT("Arrow")}){
                FS_InventoryEntry& entry = source.AddDefaulted_GetRef();
                entry.itemId = id;
                entry.totalQuantity = 5;
            }
            FS_InventoryEntryList mirror;
            mirror.syncFrom(source);
            source[0].totalQuantity = 9;
            source.RemoveAt(2);
            mirror.syncFrom(source);
            TestTrue(TEXT("toEntries equals the new array"), mirror.toEntries() == source);
        });
    });

    // ---------------- Section 5 ----------------
    Describe("S5 grant, pickup, drop, transfer", [this](){
        BeforeEach([this](){
            FInventoryTestFixture::expectForeignStartupErrors(*this);
        });

        It("grants instances, heavy items and unknown items by the rules", [this](){
            FInventoryTestFixture fixture;
            addCatalog(fixture);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UStubLooseItemSpawner* spawner = fixture.stub<UStubLooseItemSpawner, UBPI_LooseItemSpawner>();
            UStubCarryService* carry = fixture.stub<UStubCarryService, UBPI_CarryService>();

            const FS_GrantResult spears = UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("Spear"), 6));
            TestEqual(TEXT("4 spears stored"), spears.quantityStored, 4);
            TestEqual(TEXT("2 dropped"), spears.quantityDropped, 2);
            TestTrue(TEXT("two loose spears, each its own instance at full durability"), spawner->spawnedEntries.Num() == 2
                && spawner->spawnedEntries[0].instanceId != spawner->spawnedEntries[1].instanceId && spawner->spawnedEntries[0].state.durability == 50.0f);

            const FS_GrantResult logs = UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("Log"), 2));
            TestTrue(TEXT("heavy, carry refusing: dropped 2, carried 0"), logs.quantityDropped == 2 && logs.quantityCarried == 0);
            TestEqual(TEXT("the bag holds no log"), UPlayerInventoryComponent::findFor(player)->getAvailableQuantity(TEXT("Log")), 0);
            carry->acceptsNewCarry = true;
            const FS_GrantResult carried = UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("Log"), 2));
            TestTrue(TEXT("carry accepting: carried 2, dropped 0"), carried.quantityCarried == 2 && carried.quantityDropped == 0);

            AddExpectedError(TEXT("grantOrDrop did nothing"), EAutomationExpectedErrorFlags::Contains, 1);
            const FS_GrantResult unknown = UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("NoSuchItem"), 1));
            TestTrue(TEXT("an unknown item: nothing happens"), unknown.quantityStored == 0 && unknown.quantityDropped == 0);
        });

        It("picks up by the rules of 3.11", [this](){
            FInventoryTestFixture fixture;
            addCatalog(fixture);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            bag->addStack(TEXT("Stick"), 15);
            ALooseItemActor* sticks = spawnLooseStack(fixture, TEXT("Stick"), 10);
            TestEqual(TEXT("5 of 10 taken"), UBFL_Inventory::pickUpLooseItem(player, sticks).quantity, 5);
            TestEqual(TEXT("5 stay"), sticks->getEntry().totalQuantity, 5);

            FS_InventoryEntry spear = UBFL_Inventory::makeFreshInstance(fixture.world(), *fixture.catalog()->getDefinition(TEXT("Spear")));
            spear.state.durability = 12.0f;
            UBFL_Inventory::pickUpLooseItem(player, spawnLoose(fixture, spear));
            const FS_InventoryEntry* picked = bag->findInstance(spear.instanceId);
            TestTrue(TEXT("the same spear, durability 12"), picked != nullptr && picked->state.durability == 12.0f);

            TestTrue(TEXT("a heavy log with busy hands is HandsFull"), UBFL_Inventory::pickUpLooseItem(player, spawnLooseStack(fixture, TEXT("Log"), 1)).failReason == T_Interaction_FailReason_HandsFull);
        });

        It("drops and transfers by the rules of 3.4a, 3.7 and 3.10", [this](){
            FInventoryTestFixture fixture;
            addCatalog(fixture);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            const FGuid axe = addInstance(fixture, bag, TEXT("StarterAxe"));
            bag->addStack(TEXT("Map"), 1);
            TestTrue(TEXT("the Axe is never dropped"), UBFL_Inventory::dropFromBag(player, refToInstance(axe), 1).failReason == T_Inventory_FailReason_Permanent);
            TestTrue(TEXT("nor the map"), UBFL_Inventory::dropFromBag(player, refTo(TEXT("Map")), 1).failReason == T_Inventory_FailReason_Permanent);

            const FGuid pouch = addInstance(fixture, bag, TEXT("Pouch"));
            UInventoryData* pouchContents = fixture.registry()->getContainedInventory(*bag->findInstance(pouch), true);
            pouchContents->addStack(TEXT("Stick"), 1);
            TestTrue(TEXT("a pouch holding a stick is not dropped"), UBFL_Inventory::dropFromBag(player, refToInstance(pouch), 1).failReason == T_Inventory_FailReason_ContainerNotEmpty);

            FS_ContainerRules small;
            small.perItemLimits.Add(TEXT("Stick"), 4);
            UInventoryData* chest = fixture.makeContainer(small);
            TestTrue(TEXT("a pouch holding something does not go into a chest"), UBFL_Inventory::transferEntry(bag, chest, refToInstance(pouch), 1).failReason == T_Inventory_FailReason_ContainerNotEmpty);
            const FGuid secondPouch = addInstance(fixture, bag, TEXT("Pouch"));
            TestTrue(TEXT("never a pouch inside a pouch"), UBFL_Inventory::transferEntry(bag, pouchContents, refToInstance(secondPouch), 1).failReason == T_Inventory_FailReason_NestedContainer);
            TestTrue(TEXT("an empty pouch goes into a chest"), UBFL_Inventory::transferEntry(bag, chest, refToInstance(secondPouch), 1).succeeded());
            pouchContents->removeStack(TEXT("Stick"), 1);
            TestTrue(TEXT("an emptied pouch drops"), UBFL_Inventory::dropFromBag(player, refToInstance(pouch), 1).succeeded());
            TestTrue(TEXT("the Axe never goes into a chest"), UBFL_Inventory::transferEntry(bag, chest, refToInstance(axe), 1).failReason == T_Inventory_FailReason_Permanent);

            bag->addStack(TEXT("Stick"), 10);
            const FS_InventoryResult moved = UBFL_Inventory::transferEntry(bag, chest, refTo(TEXT("Stick")), 10);
            TestEqual(TEXT("room for 4 of 10: 4 moved"), moved.quantity, 4);
            TestEqual(TEXT("6 stay in the bag"), bag->getTotalQuantity(TEXT("Stick")), 6);
            TestTrue(TEXT("a full chest is ContainerFull"), UBFL_Inventory::transferEntry(bag, chest, refTo(TEXT("Stick")), 1).failReason == T_Inventory_FailReason_ContainerFull);

            UInventoryData* openChest = fixture.makeContainer(FS_ContainerRules());
            TestEqual(TEXT("bag -> chest, 6 sticks"), UBFL_Inventory::transferEntry(bag, openChest, refTo(TEXT("Stick")), 6).quantity, 6);
            TestTrue(TEXT("totals add up"), bag->getTotalQuantity(TEXT("Stick")) == 0 && openChest->getTotalQuantity(TEXT("Stick")) == 6);
            openChest->setDeathBag(true);
            TestTrue(TEXT("a death bag is locked"), UBFL_Inventory::transferEntry(openChest, bag, refTo(TEXT("Stick")), 1).failReason == T_Inventory_FailReason_DeathBagLocked);
        });
    });

    // ---------------- Section 6 ----------------
    Describe("S6 hotbar and held item", [this](){
        BeforeEach([this](){
            FInventoryTestFixture::expectForeignStartupErrors(*this);
        });

        It("binds a type, shares a pool, and resolves the last used copy", [this](){
            FInventoryTestFixture fixture;
            addCatalog(fixture);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            fixture.makeCharacterFor(player);
            UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
            UHeldItemComponent* held = UHeldItemComponent::findFor(player);
            UInventoryData* bag = inventory->getPersonalInventory();
            TestEqual(TEXT("ten slots"), inventory->getHotbarBindings().Num(), 10);
            TestEqual(TEXT("ten usable"), inventory->getUsableHotbarSlotCount(), 10);
            TestTrue(TEXT("Combat reads Unarmed"), UBFL_Inventory::getHeldItem(player) == fixture.catalog()->getDefinition(TEXT("Unarmed")));

            TestTrue(TEXT("bind without holding any"), inventory->bindHotbarSlot(1, TEXT("Stick")).succeeded());
            TestFalse(TEXT("not usable yet"), inventory->resolveHotbarSlot(1).isUsable);
            UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("Stick"), 4));
            TestTrue(TEXT("usable with 4"), inventory->resolveHotbarSlot(1).isUsable && inventory->resolveHotbarSlot(1).availableQuantity == 4);
            inventory->bindHotbarSlot(2, TEXT("Stick"));
            bag->removeStack(TEXT("Stick"), 3);
            TestTrue(TEXT("one shared pool"), inventory->resolveHotbarSlot(1).availableQuantity == 1 && inventory->resolveHotbarSlot(2).availableQuantity == 1);
            inventory->bindHotbarSlot(1, TEXT("Stone"));
            TestTrue(TEXT("rebinding replaces"), inventory->getBoundItemId(1) == FName(TEXT("Stone")));
            TestTrue(TEXT("slot 12 does not exist"), inventory->bindHotbarSlot(12, TEXT("Stick")).failReason == T_Inventory_FailReason_InvalidSlot);
            TestTrue(TEXT("a heavy item cannot be bound"), inventory->bindHotbarSlot(1, TEXT("Log")).failReason == T_Inventory_FailReason_InvalidRequest);

            const FGuid spearA = addInstance(fixture, bag, TEXT("Spear"));
            const FGuid spearB = addInstance(fixture, bag, TEXT("Spear"));
            held->holdFromBag(refToInstance(spearA));
            held->holdFromBag(refToInstance(spearB));
            inventory->bindHotbarSlot(3, TEXT("Spear"));
            TestTrue(TEXT("the last used copy (B)"), inventory->resolveHotbarSlot(3).instanceId == spearB);
            UStubActionService* action = fixture.stub<UStubActionService, UBPI_ActionService>();
            action->cancelledActions.Reset();
            held->putAway();
            held->selectHotbarSlot(3);
            TestTrue(TEXT("spear B wielded"), held->getHeldState().instanceId == spearB && held->getHeldState().isWielded);
            TestTrue(TEXT("Combat reads the Spear"), UBFL_Inventory::getHeldItem(player) == fixture.catalog()->getDefinition(TEXT("Spear")));
            TestTrue(TEXT("a swap cancels a reload"), action->cancelledActions.Contains(T_Action_Reload.GetTag()));

            UBFL_Inventory::applyDurabilityLoss(player, spearB, 1000.0f);
            TestTrue(TEXT("B broke and is gone"), bag->findInstance(spearB) == nullptr);
            TestTrue(TEXT("the hand moved to A"), held->getHeldState().instanceId == spearA);
            TestTrue(TEXT("the slot resolves to A"), inventory->resolveHotbarSlot(3).instanceId == spearA);
            UBFL_Inventory::applyDurabilityLoss(player, spearA, 1000.0f);
            TestTrue(TEXT("both broken: the hand is empty"), held->getHeldState().isEmpty());
            TestTrue(TEXT("Combat reads Unarmed again"), UBFL_Inventory::getHeldItem(player) == fixture.catalog()->getDefinition(TEXT("Unarmed")));
            TestFalse(TEXT("slot 3 is not usable"), inventory->resolveHotbarSlot(3).isUsable);
            UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("Spear"), 1));
            TestTrue(TEXT("a new spear: usable again without rebinding"), inventory->resolveHotbarSlot(3).isUsable);
        });

        It("holds a non-weapon unwielded, follows a transfer, and never breaks the Axe", [this](){
            FInventoryTestFixture fixture;
            addCatalog(fixture);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            fixture.makeCharacterFor(player);
            UHeldItemComponent* held = UHeldItemComponent::findFor(player);
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            bag->addStack(TEXT("Herb"), 2);
            held->holdFromBag(refTo(TEXT("Herb")));
            TestTrue(TEXT("a herb is held, not wielded"), held->getHeldState().itemId == FName(TEXT("Herb")) && !held->getHeldState().isWielded);
            TestTrue(TEXT("Combat still reads Unarmed"), UBFL_Inventory::getHeldItem(player) == fixture.catalog()->getDefinition(TEXT("Unarmed")));
            TestTrue(TEXT("Animation shows the herb"), held->getShownDefinition() == fixture.catalog()->getDefinition(TEXT("Herb")));

            const FGuid spearA = addInstance(fixture, bag, TEXT("Spear"));
            const FGuid spearB = addInstance(fixture, bag, TEXT("Spear"));
            held->holdFromBag(refToInstance(spearA));
            UBFL_Inventory::transferEntry(bag, fixture.makeContainer(FS_ContainerRules()), refToInstance(spearA), 1);
            TestTrue(TEXT("the held spear left: the hand moves to the other one"), held->getHeldState().instanceId == spearB);

            const FGuid axe = addInstance(fixture, bag, TEXT("StarterAxe"));
            UBFL_Inventory::applyDurabilityLoss(player, axe, 1000.0f);
            const FS_InventoryEntry* axeEntry = bag->findInstance(axe);
            TestTrue(TEXT("the Axe never breaks"), axeEntry != nullptr && axeEntry->state.durability == UDA_ItemDefinition::unbreakableDurability);
        });

        It("reloads and fires a crossbow and a firearm", [this](){
            FInventoryTestFixture fixture;
            addCatalog(fixture);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            const FGuid crossbow = addInstance(fixture, bag, TEXT("Crossbow"));
            TestTrue(TEXT("no arrows: NoAmmo"), UBFL_Inventory::applyReload(player, crossbow).failReason == T_Inventory_FailReason_NoAmmo);
            bag->addStack(TEXT("Arrow"), 5);
            TestTrue(TEXT("reloaded"), UBFL_Inventory::applyReload(player, crossbow).succeeded());
            TestTrue(TEXT("loaded, 4 arrows left"), bag->findInstance(crossbow)->state.isLoaded && bag->getTotalQuantity(TEXT("Arrow")) == 4);
            TestTrue(TEXT("a loaded crossbow cannot reload"), UBFL_Inventory::applyReload(player, crossbow).failReason == T_Inventory_FailReason_CannotReload);
            TestTrue(TEXT("a shot unloads it"), UBFL_Inventory::spendLoadedShot(player, crossbow).succeeded() && !bag->findInstance(crossbow)->state.isLoaded);
            TestTrue(TEXT("nothing left to fire"), UBFL_Inventory::spendLoadedShot(player, crossbow).failReason == T_Inventory_FailReason_NoAmmo);

            const FGuid rifle = addInstance(fixture, bag, TEXT("Rifle"));
            bag->addStack(TEXT("Bullet"), 4);
            TestEqual(TEXT("4 rounds loaded"), UBFL_Inventory::applyReload(player, rifle).quantity, 4);
            TestTrue(TEXT("loadedRounds 4, no bullets left"), bag->findInstance(rifle)->state.loadedRounds == 4 && bag->getTotalQuantity(TEXT("Bullet")) == 0);
        });
    });

    // ---------------- Section 7 ----------------
    Describe("S7 staging", [this](){
        BeforeEach([this](){
            FInventoryTestFixture::expectForeignStartupErrors(*this);
        });

        It("reserves, consumes and releases by station", [this](){
            FInventoryTestFixture fixture;
            addCatalog(fixture);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
            UInventoryData* bag = inventory->getPersonalInventory();
            bag->addStack(TEXT("Stick"), 10);
            const FGuid stationA = FGuid::NewGuid();
            const FGuid stationB = FGuid::NewGuid();
            UBFL_Inventory::reserveForStaging(player, refTo(TEXT("Stick")), 4, stationA);
            TestEqual(TEXT("the entry holds 4 reserved"), bag->findStack(TEXT("Stick"))->reservedQuantity, 4);
            inventory->bindHotbarSlot(1, TEXT("Stick"));
            TestEqual(TEXT("the hotbar sees 6"), inventory->resolveHotbarSlot(1).availableQuantity, 6);
            TestEqual(TEXT("a transfer moves only 6"), UBFL_Inventory::transferEntry(bag, fixture.makeContainer(FS_ContainerRules()), refTo(TEXT("Stick")), 7).quantity, 6);
            UBFL_Inventory::releaseAllStaged(player);
            bag->addStack(TEXT("Stick"), 5);
            UBFL_Inventory::reserveForStaging(player, refTo(TEXT("Stick")), 2, stationA);
            UBFL_Inventory::reserveForStaging(player, refTo(TEXT("Stick")), 3, stationB);
            TestEqual(TEXT("release station A: 2"), UBFL_Inventory::releaseStagedReservation(player, stationA), 2);
            TestEqual(TEXT("3 still reserved"), bag->findStack(TEXT("Stick"))->reservedQuantity, 3);
            TestEqual(TEXT("release one unit"), UBFL_Inventory::releaseStagedItem(player, refTo(TEXT("Stick")), 1).quantity, 1);
            TestEqual(TEXT("releaseAll"), UBFL_Inventory::releaseAllStaged(player), 2);
            TestEqual(TEXT("a second releaseAll releases 0"), UBFL_Inventory::releaseAllStaged(player), 0);
        });

        It("stages the most worn free copy, never the wielded one, the Axe, the map or a full pouch", [this](){
            FInventoryTestFixture fixture;
            addCatalog(fixture);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            fixture.makeCharacterFor(player);
            UHeldItemComponent* held = UHeldItemComponent::findFor(player);
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            const FGuid x = addInstance(fixture, bag, TEXT("Spear"), 10.0f);
            addInstance(fixture, bag, TEXT("Spear"), 40.0f);
            const FGuid z = addInstance(fixture, bag, TEXT("Spear"), 25.0f);
            held->holdFromBag(refToInstance(x));
            const FS_InventoryResult staged = UBFL_Inventory::reserveForStaging(player, refTo(TEXT("Spear")), 1, FGuid());
            TestTrue(TEXT("Z, the most worn one not in the hand"), staged.instanceId == z);
            TestTrue(TEXT("the wielded spear is refused"), UBFL_Inventory::reserveForStaging(player, refToInstance(x), 1, FGuid()).failReason == T_Inventory_FailReason_ItemWielded);
            TestTrue(TEXT("a staged spear is not dropped"), UBFL_Inventory::dropFromBag(player, refToInstance(z), 1).failReason == T_Inventory_FailReason_ReservedQuantity);
            TestTrue(TEXT("nor transferred"), UBFL_Inventory::transferEntry(bag, fixture.makeContainer(FS_ContainerRules()), refToInstance(z), 1).failReason == T_Inventory_FailReason_ReservedQuantity);
            TestTrue(TEXT("nor held"), held->holdFromBag(refToInstance(z)).failReason == T_Inventory_FailReason_ReservedQuantity);

            const FGuid axe = addInstance(fixture, bag, TEXT("StarterAxe"));
            bag->addStack(TEXT("Map"), 1);
            TestTrue(TEXT("the Axe is never staged"), UBFL_Inventory::reserveForStaging(player, refToInstance(axe), 1, FGuid()).failReason == T_Inventory_FailReason_Permanent);
            TestTrue(TEXT("nor the map"), UBFL_Inventory::reserveForStaging(player, refTo(TEXT("Map")), 1, FGuid()).failReason == T_Inventory_FailReason_Permanent);
            const FGuid pouch = addInstance(fixture, bag, TEXT("Pouch"));
            fixture.registry()->getContainedInventory(*bag->findInstance(pouch), true)->addStack(TEXT("Stick"), 1);
            TestTrue(TEXT("a pouch holding a stick is not staged"), UBFL_Inventory::reserveForStaging(player, refToInstance(pouch), 1, FGuid()).failReason == T_Inventory_FailReason_ContainerNotEmpty);
        });
    });

    // ---------------- Section 8 ----------------
    Describe("S8 spoilage and consumables", [this](){
        BeforeEach([this](){
            FInventoryTestFixture::expectForeignStartupErrors(*this);
        });

        It("spoils lazily, clamps the start time, releases staged food and goes over the limit", [this](){
            FInventoryTestFixture fixture;
            addCatalog(fixture);
            fixture.setGameTime(0);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("RawMeat"), 3));
            TestEqual(TEXT("spoils at 86400"), bag->findStack(TEXT("RawMeat"))->spoilAtTime, static_cast<int64>(86400));
            bag->clearAll();

            fixture.setGameTime(InventoryTime::hoursToGameSeconds(40.0f));
            UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("RawMeat"), 1), fixture.services()->getGameTime() - InventoryTime::hoursToGameSeconds(30.0f));
            TestEqual(TEXT("stored as RawMeat"), bag->getTotalQuantity(TEXT("RawMeat")), 1);
            bag->evaluateSpoilage(fixture.services()->getGameTime());
            TestEqual(TEXT("spoiled at the next request"), bag->getTotalQuantity(TEXT("SpoiledMeat")), 1);
            bag->clearAll();
            UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("RawMeat"), 1), fixture.services()->getGameTime() + 999999);
            TestEqual(TEXT("a future start is clamped to now"), bag->findStack(TEXT("RawMeat"))->spoilAtTime, fixture.services()->getGameTime() + static_cast<int64>(86400));
            bag->clearAll();

            const FGuid station = FGuid::NewGuid();
            FGuid lostStation;
            bag->onStagedItemLost.AddLambda([&lostStation](const FGuid& stationId, FName itemId){
                lostStation = stationId;
            });
            bag->addStack(TEXT("SpoiledMeat"), 10);
            UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("RawMeat"), 2));
            UBFL_Inventory::reserveForStaging(player, refTo(TEXT("RawMeat")), 2, station);
            fixture.advanceHours(25.0f);
            bag->evaluateSpoilage(fixture.services()->getGameTime());
            TestTrue(TEXT("the craft heard that its stack spoiled"), lostStation == station);
            TestEqual(TEXT("no reservation left"), bag->getReservations().Num(), 0);
            TestEqual(TEXT("the spoiled stack went over its limit: 12"), bag->getTotalQuantity(TEXT("SpoiledMeat")), 12);
            TestEqual(TEXT("none reserved"), bag->findStack(TEXT("SpoiledMeat"))->reservedQuantity, 0);
        });

        It("applies water, medicine and the illness switch", [this](){
            FInventoryTestFixture fixture;
            addCatalog(fixture);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            UStubSurvivalService* survival = fixture.stub<UStubSurvivalService, UBPI_SurvivalService>();
            UStubHealthService* health = fixture.stub<UStubHealthService, UBPI_HealthService>();
            bag->addStack(TEXT("UncleanWater"), 1);
            bag->addStack(TEXT("Herb"), 1);
            bag->addStack(TEXT("Stick"), 1);
            bag->addStack(TEXT("SpoiledMeat"), 1);
            TestTrue(TEXT("drink unclean water"), UBFL_Inventory::useConsumable(player, TEXT("UncleanWater")).succeeded());
            TestEqual(TEXT("thirst 50"), survival->meterTotals.FindRef(T_Item_Category_Water), 50.0f);
            TestEqual(TEXT("10 damage"), health->totalDamaged, 10.0f);
            TestTrue(TEXT("through DamageType.Illness"), health->lastDamageType == T_DamageType_Illness);
            TestTrue(TEXT("use the herb"), UBFL_Inventory::useConsumable(player, TEXT("Herb")).succeeded());
            TestEqual(TEXT("healed 20"), health->totalHealed, 20.0f);
            TestEqual(TEXT("Illness meter -10"), survival->meterTotals.FindRef(T_Survival_Meter_Illness), -10.0f);
            TestTrue(TEXT("a stick is not consumable"), UBFL_Inventory::useConsumable(player, TEXT("Stick")).failReason == T_Inventory_FailReason_NotConsumable);

            survival->illnessActive = false;
            survival->registeredSources.Reset();
            const float damagedBefore = health->totalDamaged;
            UBFL_Inventory::useConsumable(player, TEXT("SpoiledMeat"));
            TestEqual(TEXT("spoiled meat still feeds: hunger 50"), survival->meterTotals.FindRef(T_Item_Category_Food), 50.0f);
            TestEqual(TEXT("illness off: no damage"), health->totalDamaged, damagedBefore);
            TestEqual(TEXT("illness off: no illness source"), survival->registeredSources.Num(), 0);
        });
    });

    // ---------------- Section 9 ----------------
    Describe("S9 bag lifecycle", [this](){
        BeforeEach([this](){
            FInventoryTestFixture::expectForeignStartupErrors(*this);
        });

        It("grants the kit once and starts a new character cleanly", [this](){
            FInventoryTestFixture fixture;
            addCatalog(fixture);
            AInventoryTestPlayerState* player = fixture.world()->SpawnActor<AInventoryTestPlayerState>();
            UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
            UBFL_InventoryBag::startNewCharacter(player, FGuid::NewGuid());
            TestTrue(TEXT("the kit: the Axe and the map"), inventory->getAvailableQuantity(TEXT("StarterAxe")) == 1 && inventory->getAvailableQuantity(TEXT("Map")) == 1);
            TestTrue(TEXT("a second grant is refused"), UBFL_InventoryBag::grantStartingKit(player).failReason == T_Inventory_FailReason_KitAlreadyGranted);

            const FS_BagDetachResult detached = UBFL_InventoryBag::detachBag(player, FVector::ZeroVector);
            UBFL_InventoryBag::startNewCharacter(player, FGuid::NewGuid());
            TestTrue(TEXT("the old bag is gone"), UBFL_InventoryBag::reattachBag(player, detached.bagId).failReason == T_Inventory_FailReason_AlreadyHasBag);
            UBFL_InventoryBag::detachBag(player, FVector::ZeroVector);
            TestTrue(TEXT("the old bag id answers NotFound"), UBFL_InventoryBag::reattachBag(player, detached.bagId).failReason == T_Inventory_FailReason_NotFound);

            UInventorySettings* settings = GetMutableDefault<UInventorySettings>();
            const TArray<FS_ItemStack> savedKit = settings->startingKit;
            settings->startingKit.Reset();
            AddExpectedError(TEXT("The starting kit of"), EAutomationExpectedErrorFlags::Contains, 1);
            AInventoryTestPlayerState* other = fixture.world()->SpawnActor<AInventoryTestPlayerState>();
            UBFL_InventoryBag::startNewCharacter(other, FGuid::NewGuid());
            TestFalse(TEXT("an empty kit is not marked granted"), UPlayerInventoryComponent::findFor(other)->isKitGranted());
            settings->startingKit = savedKit;
            TestTrue(TEXT("fixed data: the kit works"), UBFL_InventoryBag::grantStartingKit(other).succeeded());
        });

        It("detaches atomically and keeps the Bagless rules", [this](){
            FInventoryTestFixture fixture;
            addCatalog(fixture);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            fixture.makeCharacterFor(player);
            UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
            UHeldItemComponent* held = UHeldItemComponent::findFor(player);
            UInventoryData* bag = inventory->getPersonalInventory();
            bag->addStack(TEXT("Stick"), 5);
            UBFL_Inventory::reserveForStaging(player, refTo(TEXT("Stick")), 2, FGuid::NewGuid());
            const FGuid spear = addInstance(fixture, bag, TEXT("Spear"));
            held->holdFromBag(refToInstance(spear));

            setFailBagStep(2);
            TestTrue(TEXT("step 2 fails"), UBFL_InventoryBag::detachBag(player, FVector::ZeroVector).failReason == T_Inventory_FailReason_GearReleaseFailed);
            setFailBagStep(0);
            TestTrue(TEXT("nothing moved"), inventory->hasBag() && held->getHeldState().instanceId == spear);
            UStubBagActorService* bags = fixture.stub<UStubBagActorService, UBPI_BagActorService>();
            TestEqual(TEXT("no record left"), bags->bags.Num(), 0);

            const FS_BagDetachResult detached = UBFL_InventoryBag::detachBag(player, FVector::ZeroVector);
            TestTrue(TEXT("detached"), detached.succeeded());
            TestEqual(TEXT("nothing reserved in the bag record"), bag->findStack(TEXT("Stick"))->reservedQuantity, 0);
            TestTrue(TEXT("no personal inventory"), inventory->getPersonalInventory() == nullptr);
            TestEqual(TEXT("a record with the hotbar snapshot"), bags->bags.Num(), 1);

            for(const FName itemId : {FName(TEXT("Spear")), FName(TEXT("Stone")), FName(TEXT("Herb"))}){
                if(itemId == FName(TEXT("Spear"))){
                    UBFL_Inventory::pickUpLooseItem(player, spawnLoose(fixture, UBFL_Inventory::makeFreshInstance(fixture.world(), *fixture.catalog()->getDefinition(itemId))));
                }else{
                    UBFL_Inventory::pickUpLooseItem(player, spawnLooseStack(fixture, itemId, 1));
                }
            }
            ALooseItemActor* fourth = spawnLooseStack(fixture, TEXT("Stick"), 1);
            TestTrue(TEXT("a fourth pickup is HandsFull"), UBFL_Inventory::pickUpLooseItem(player, fourth).failReason == T_Interaction_FailReason_HandsFull);
            TestEqual(TEXT("and stays on the ground"), fourth->getEntry().totalQuantity, 1);
            held->selectHotbarSlot(1);
            TestTrue(TEXT("key 1 shows the spear, not wielded"), held->getHeldState().itemId == FName(TEXT("Spear")) && !held->getHeldState().isWielded);
            TestTrue(TEXT("Combat reads Unarmed"), UBFL_Inventory::getHeldItem(player) == fixture.catalog()->getDefinition(TEXT("Unarmed")));
            TestTrue(TEXT("key 5 is locked"), held->selectHotbarSlot(5).failReason == T_Inventory_FailReason_InvalidSlot);
            TestTrue(TEXT("stage: NoBag"), UBFL_Inventory::reserveForStaging(player, refTo(TEXT("Stick")), 1, FGuid()).failReason == T_Inventory_FailReason_NoBag);
            TestTrue(TEXT("drop from bag: NoBag"), UBFL_Inventory::dropFromBag(player, refTo(TEXT("Stick")), 1).failReason == T_Inventory_FailReason_NoBag);
            TestTrue(TEXT("eat from bag: NoBag"), UBFL_Inventory::useConsumable(player, TEXT("Herb")).failReason == T_Inventory_FailReason_NoBag);
            TestTrue(TEXT("reload: NoBag"), UBFL_Inventory::applyReload(player, spear).failReason == T_Inventory_FailReason_NoBag);
            TestTrue(TEXT("drop a hold slot"), UBFL_InventoryBag::dropHoldSlot(player, 1).succeeded());
            TestFalse(TEXT("the slot is free"), inventory->getHoldSlots()[1].isOccupied);
            TestEqual(TEXT("discard: the other two are destroyed"), UBFL_InventoryBag::discardBaglessItems(player), 2);

            AInventoryTestPlayerState* stranger = fixture.makePlayer();
            UInventoryData* strangerBag = UPlayerInventoryComponent::findFor(stranger)->releaseInventory();
            fixture.registry()->destroyInventory(strangerBag->getInventoryId());
            TestTrue(TEXT("another character's bag: NotBagOwner"), UBFL_InventoryBag::reattachBag(stranger, detached.bagId).failReason == T_Inventory_FailReason_NotBagOwner);
        });

        It("keeps food fresh in the hand and eats spoiled food as spoiled", [this](){
            FInventoryTestFixture fixture;
            addCatalog(fixture);
            fixture.setGameTime(0);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UStubSurvivalService* survival = fixture.stub<UStubSurvivalService, UBPI_SurvivalService>();
            UStubHealthService* health = fixture.stub<UStubHealthService, UBPI_HealthService>();
            UBFL_InventoryBag::detachBag(player, FVector::ZeroVector);
            UBFL_Inventory::pickUpLooseItem(player, spawnLooseStack(fixture, TEXT("RawMeat"), 1, InventoryTime::hoursToGameSeconds(10.0f)));
            fixture.advanceHours(20.0f);
            TestTrue(TEXT("eaten from the hold slot"), UBFL_InventoryBag::useConsumableFromHoldSlot(player, 0).succeeded());
            TestEqual(TEXT("still fresh: hunger 100"), survival->meterTotals.FindRef(T_Item_Category_Food), 100.0f);

            UBFL_Inventory::pickUpLooseItem(player, spawnLooseStack(fixture, TEXT("RawMeat"), 1, 1));   // already past its time on the ground
            UBFL_InventoryBag::useConsumableFromHoldSlot(player, 0);
            TestEqual(TEXT("spoiled: hunger +50"), survival->meterTotals.FindRef(T_Item_Category_Food), 150.0f);
            TestEqual(TEXT("and the HP loss"), health->totalDamaged, 10.0f);
        });
    });
}

#endif
