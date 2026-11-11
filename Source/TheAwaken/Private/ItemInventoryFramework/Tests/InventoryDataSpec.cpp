#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"

BEGIN_DEFINE_SPEC(FInventoryDataSpec, "Awaken.Inventory.Data", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FInventoryDataSpec)

void FInventoryDataSpec::Define(){
    BeforeEach([this](){
        FInventoryTestFixture::expectForeignStartupErrors(*this);
    });

    Describe("Per-type limit", [this](){
        It("stores what the limit allows, refuses when nothing fits, and never loses a unit", [this](){
            FInventoryTestFixture fixture;
            fixture.addStackable(TEXT("Stick"), 20);
            const UDA_ItemDefinition* spear = fixture.addInstanced(TEXT("Spear"), 4, 50.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            int32 changes = 0;
            bag->onChanged.AddLambda([&changes](UInventoryData* changed){
                changes++;
            });

            TestEqual(TEXT("5 sticks fit"), bag->addStack(TEXT("Stick"), 5).quantity, 5);
            TestEqual(TEXT("30 more: only 15 fit"), bag->addStack(TEXT("Stick"), 30).quantity, 15);
            TestTrue(TEXT("one more is refused with BagFull"), bag->addStack(TEXT("Stick"), 1).failReason == T_Interaction_FailReason_BagFull);
            TestEqual(TEXT("a refusal raises no change"), changes, 2);
            TestTrue(TEXT("an instanced item is not a stack"), bag->addStack(TEXT("Spear"), 1).failReason == T_Inventory_FailReason_InvalidRequest);

            for(int32 index = 0; index < 4; index++){
                FS_InventoryEntry copy;
                copy.itemId = spear->itemId;
                copy.instanceId = FGuid::NewGuid();
                copy.state.durability = spear->durabilityMax;
                TestTrue(TEXT("a spear fits"), bag->addInstance(copy).succeeded());
            }
            FS_InventoryEntry fifth;
            fifth.itemId = spear->itemId;
            fifth.instanceId = FGuid::NewGuid();
            TestTrue(TEXT("the fifth spear is refused"), bag->addInstance(fifth).failReason == T_Interaction_FailReason_BagFull);
            TestEqual(TEXT("four copies"), bag->getInstanceCount(TEXT("Spear")), 4);

            TestTrue(TEXT("cannot remove more than is held"), bag->removeStack(TEXT("Stick"), 25).failReason == T_Inventory_FailReason_InsufficientQuantity);
            TestTrue(TEXT("remove all"), bag->removeStack(TEXT("Stick"), 20).succeeded());
            TestTrue(TEXT("the empty stack is gone"), bag->findStack(TEXT("Stick")) == nullptr);
        });
    });
    Describe("Queries and refusals", [this](){
        It("answers canFit from the limit, refuses unknown items and duplicate instance ids", [this](){
            FInventoryTestFixture fixture;
            fixture.addStackable(TEXT("Stick"), 20);
            const UDA_ItemDefinition* spear = fixture.addInstanced(TEXT("Spear"), 4, 50.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();

            FS_ItemStack fifty;
            fifty.itemId = TEXT("Stick");
            fifty.quantity = 50;
            TestEqual(TEXT("canFit on an empty bag is the limit"), bag->canFit(fifty), 20);
            TestTrue(TEXT("an unknown item is NotFound"), bag->addStack(TEXT("NoSuchItem"), 1).failReason == T_Inventory_FailReason_NotFound);

            FS_InventoryEntry copy;
            copy.itemId = spear->itemId;
            copy.instanceId = FGuid::NewGuid();
            TestTrue(TEXT("the first copy fits"), bag->addInstance(copy).succeeded());
            TestTrue(TEXT("the same instance id twice is InvalidRequest"), bag->addInstance(copy).failReason == T_Inventory_FailReason_InvalidRequest);
            TestEqual(TEXT("one copy"), bag->getInstanceCount(TEXT("Spear")), 1);
        });
    });

    Describe("Container rules", [this](){
        It("refuses an item the allowedTags do not name", [this](){
            FInventoryTestFixture fixture;
            fixture.addStackable(TEXT("Stick"), 20);
            FS_ContainerRules rules;
            rules.allowedTags.AddTag(T_Item_Category_Food);
            UInventoryData* chest = fixture.makeContainer(rules);
            TestTrue(TEXT("a stick is not food"), chest->addStack(TEXT("Stick"), 1).failReason == T_Inventory_FailReason_ItemNotAllowed);
            TestTrue(TEXT("nothing was stored"), chest->isEmpty());
        });

        It("holds one item id of the singleItemTag group at a time", [this](){
            FInventoryTestFixture fixture;
            UDA_ItemDefinition* clean = fixture.addStackable(TEXT("CleanWater"), 10);
            clean->categoryTag = T_Item_Category_Water;
            UDA_ItemDefinition* unclean = fixture.addStackable(TEXT("UncleanWater"), 10);
            unclean->categoryTag = T_Item_Category_Water;
            FS_ContainerRules rules;
            rules.singleItemTag = T_Item_Category_Water;
            UInventoryData* barrel = fixture.makeContainer(rules);

            TestTrue(TEXT("clean water goes in"), barrel->addStack(TEXT("CleanWater"), 2).succeeded());
            TestTrue(TEXT("more clean water goes in"), barrel->addStack(TEXT("CleanWater"), 1).succeeded());
            TestTrue(TEXT("unclean water is refused while clean water is there"), barrel->addStack(TEXT("UncleanWater"), 1).failReason == T_Inventory_FailReason_ItemNotAllowed);
            TestTrue(TEXT("the clean water leaves"), barrel->removeStack(TEXT("CleanWater"), 3).succeeded());
            TestTrue(TEXT("then unclean water is accepted"), barrel->addStack(TEXT("UncleanWater"), 1).succeeded());
        });

        It("refuses with ContainerFull, not BagFull, at a container's limit", [this](){
            FInventoryTestFixture fixture;
            fixture.addStackable(TEXT("Stick"), 20);
            FS_ContainerRules rules;
            rules.perItemLimits.Add(TEXT("Stick"), 3);
            UInventoryData* chest = fixture.makeContainer(rules);
            TestEqual(TEXT("the per-item limit wins"), chest->addStack(TEXT("Stick"), 5).quantity, 3);
            TestTrue(TEXT("full"), chest->addStack(TEXT("Stick"), 1).failReason == T_Inventory_FailReason_ContainerFull);
        });
    });

    Describe("Spoil time on merge", [this](){
        It("keeps the earliest spoil time when two stacks merge", [this](){
            FInventoryTestFixture fixture;
            UDA_ItemDefinition* meat = fixture.addStackable(TEXT("RawMeat"), 10);
            meat->canSpoil = true;
            meat->spoilTimeHours = 24.0f;
            meat->spoiledItemId = TEXT("SpoiledMeat");
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            int32 changes = 0;
            bag->onChanged.AddLambda([&changes](UInventoryData* changed){
                changes++;
            });

            bag->addStack(TEXT("RawMeat"), 3, 1000);
            bag->addStack(TEXT("RawMeat"), 2, 400);
            const FS_InventoryEntry* stack = bag->findStack(TEXT("RawMeat"));
            TestTrue(TEXT("one stack"), stack != nullptr && bag->getEntries().Num() == 1);
            TestEqual(TEXT("of 5"), stack != nullptr ? stack->totalQuantity : 0, 5);
            TestEqual(TEXT("with the earliest time"), stack != nullptr ? stack->spoilAtTime : 0, static_cast<int64>(400));
            TestEqual(TEXT("one change per accepted command"), changes, 2);
        });
    });
}

#endif
