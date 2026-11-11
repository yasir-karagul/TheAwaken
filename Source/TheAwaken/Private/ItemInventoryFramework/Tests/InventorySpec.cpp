#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/IConsoleManager.h"
#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestCharacter.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/Stubs/InventoryStubs.h"
#include "ItemInventoryFramework/ALooseItemActor.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UBFL_InventoryBag.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "Engine/World.h"

namespace InventorySpecHelpers{
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

    // A real loose actor in the fixture's world, holding a stack.
    ALooseItemActor* spawnLooseStack(const FInventoryTestFixture& fixture, FName itemId, int32 quantity, int64 spoilAtTime = 0){
        FS_InventoryEntry entry;
        entry.itemId = itemId;
        entry.totalQuantity = quantity;
        entry.spoilAtTime = spoilAtTime;
        ALooseItemActor* looseItem = fixture.world()->SpawnActor<ALooseItemActor>();
        looseItem->setEntry(entry);
        return looseItem;
    }

    void setFailBagStep(int32 step){
        IConsoleVariable* variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Awaken.Inventory.FailBagStep"));
        if(variable != nullptr){
            variable->Set(step);
        }
    }
}

BEGIN_DEFINE_SPEC(FInventorySpec, "Awaken.Inventory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FInventorySpec)

void FInventorySpec::Define(){
    using namespace InventorySpecHelpers;

    BeforeEach([this](){
        FInventoryTestFixture::expectForeignStartupErrors(*this);
    });

    // ---- Spec 1 — Reserve and consume; staged items are reserved (tier 1; Doc B22 §3.7) ----
    Describe("Reserve and consume", [this](){
        It("a reserved amount cannot be spent twice, cannot leave the bag, and comes back on release", [this](){
            FInventoryTestFixture fixture;
            fixture.addStackable(TEXT("Stick"), 20);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            bag->addStack(TEXT("Stick"), 10);
            const FGuid station = FGuid::NewGuid();

            TestTrue(TEXT("stage 4"), UBFL_Inventory::reserveForStaging(player, refTo(TEXT("Stick")), 4, station).succeeded());
            TestEqual(TEXT("nothing left the bag"), bag->getTotalQuantity(TEXT("Stick")), 10);
            TestEqual(TEXT("every reader sees 6"), bag->getAvailableQuantity(TEXT("Stick")), 6);
            TestTrue(TEXT("cannot stage more than is available"), UBFL_Inventory::reserveForStaging(player, refTo(TEXT("Stick")), 7, station).failReason == T_Inventory_FailReason_InsufficientQuantity);
            TestTrue(TEXT("a staged unit cannot be removed"), bag->removeStack(TEXT("Stick"), 7).failReason == T_Inventory_FailReason_ReservedQuantity);
            TestTrue(TEXT("or dropped"), UBFL_Inventory::dropFromBag(player, refTo(TEXT("Stick")), 7).failReason == T_Inventory_FailReason_ReservedQuantity);

            TestTrue(TEXT("the craft consumes 3"), UBFL_Inventory::consumeStaged(player, station, refTo(TEXT("Stick")), 3).succeeded());
            TestEqual(TEXT("7 held"), bag->getTotalQuantity(TEXT("Stick")), 7);
            TestEqual(TEXT("6 still available"), bag->getAvailableQuantity(TEXT("Stick")), 6);
            TestTrue(TEXT("cannot consume what was not staged"), UBFL_Inventory::consumeStaged(player, station, refTo(TEXT("Stick")), 5).failReason == T_Inventory_FailReason_InsufficientQuantity);

            TestEqual(TEXT("a failed or closed craft releases the rest"), UBFL_Inventory::releaseAllStaged(player), 1);
            TestEqual(TEXT("all 7 available again"), bag->getAvailableQuantity(TEXT("Stick")), 7);
        });
    });

    // ---- Spec 2 — grantOrDrop and the pickup at the limit (tier 1) ----
    Describe("Grant and pickup", [this](){
        It("a grant drops the surplus and never destroys it; a pickup at the limit is refused, not dropped again", [this](){
            FInventoryTestFixture fixture;
            fixture.addStackable(TEXT("Stick"), 20);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            const UStubLooseItemSpawner* spawner = fixture.stub<UStubLooseItemSpawner, UBPI_LooseItemSpawner>();

            const FS_GrantResult grant = UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("Stick"), 30));
            TestEqual(TEXT("the limit is stored"), grant.quantityStored, 20);
            TestEqual(TEXT("the rest is dropped"), grant.quantityDropped, 10);
            TestTrue(TEXT("as one loose stack of 10"), spawner->spawnedEntries.Num() == 1 && spawner->spawnedEntries[0].totalQuantity == 10);

            ALooseItemActor* looseItem = spawnLooseStack(fixture, TEXT("Stick"), 3);
            TestTrue(TEXT("a pickup at the limit is refused"), UBFL_Inventory::pickUpLooseItem(player, looseItem).failReason == T_Interaction_FailReason_BagFull);
            TestEqual(TEXT("the item stays where it lies"), looseItem->getEntry().totalQuantity, 3);
            TestEqual(TEXT("and nothing new was dropped"), spawner->spawnedEntries.Num(), 1);

            bag->removeStack(TEXT("Stick"), 2);
            TestEqual(TEXT("a partly fitting stack is taken in part"), UBFL_Inventory::pickUpLooseItem(player, looseItem).quantity, 2);
            TestEqual(TEXT("the rest stays"), looseItem->getEntry().totalQuantity, 1);
        });
    });

    // ---- Spec 3 — Spoiled food (tier 2: the clock, the bag and three other frameworks' seams together) ----
    Describe("Spoiled food", [this](){
        It("fresh gives 100, spoiled gives 50 and costs HP, and a merged stack keeps the earliest time", [this](){
            FInventoryTestFixture fixture;
            // Any registered tag works as a meter key for the stub; Doc B03 owns the real meter tags.
            const FGameplayTag hungerMeter = T_Survival_Meter_Illness;
            UDA_ItemDefinition* meat = fixture.addStackable(TEXT("RawMeat"), 10);
            meat->canSpoil = true;
            meat->spoilTimeHours = 24.0f;
            meat->spoiledItemId = TEXT("SpoiledMeat");
            FS_ConsumeEffect hunger;
            hunger.targetTag = hungerMeter;
            hunger.amount = 100.0f;
            meat->consumeEffects.Add(hunger);
            UDA_ItemDefinition* spoiled = fixture.addStackable(TEXT("SpoiledMeat"), 10);
            FS_ConsumeEffect illness;
            illness.kind = E_ConsumeEffectKind::IllnessChance;
            illness.targetTag = T_Item_Category_Food;   // stands in for Survival.IllnessSource.SpoiledFood
            illness.chance = 1.0f;                      // certain, so the test does not depend on the seed
            spoiled->consumeEffects.Add(illness);
            FS_ConsumeEffect healthLoss;
            healthLoss.kind = E_ConsumeEffectKind::HealthLoss;
            healthLoss.amount = 10.0f;
            spoiled->consumeEffects.Add(healthLoss);

            AInventoryTestPlayerState* player = fixture.makePlayer();
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            const UStubSurvivalService* survival = fixture.stub<UStubSurvivalService, UBPI_SurvivalService>();
            const UStubHealthService* health = fixture.stub<UStubHealthService, UBPI_HealthService>();

            fixture.setGameTime(0);
            UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("RawMeat"), 2));
            fixture.advanceHours(10.0f);
            UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("RawMeat"), 2));
            TestEqual(TEXT("a merged stack keeps the earliest spoil time"), bag->findStack(TEXT("RawMeat"))->spoilAtTime, static_cast<int64>(24 * 3600));

            fixture.advanceHours(13.0f);   // hour 23
            TestTrue(TEXT("still fresh"), UBFL_Inventory::useConsumable(player, TEXT("RawMeat")).succeeded());
            TestEqual(TEXT("fresh meat gives 100"), survival->meterTotals.FindRef(hungerMeter), 100.0f);
            TestEqual(TEXT("and costs no HP"), health->totalDamaged, 0.0f);

            fixture.advanceHours(1.0f);    // hour 24: exactly at the boundary the server's answer is "spoiled"
            TestFalse(TEXT("the fresh item is gone"), UBFL_Inventory::useConsumable(player, TEXT("RawMeat")).succeeded());
            TestEqual(TEXT("the stack became the spoiled item"), bag->getTotalQuantity(TEXT("SpoiledMeat")), 3);

            TestTrue(TEXT("spoiled food can still be eaten"), UBFL_Inventory::useConsumable(player, TEXT("SpoiledMeat")).succeeded());
            TestEqual(TEXT("it gives half"), survival->meterTotals.FindRef(hungerMeter), 150.0f);
            TestEqual(TEXT("it costs HP every time"), health->totalDamaged, 10.0f);
            TestTrue(TEXT("through DamageType.Illness"), health->lastDamageType == T_DamageType_Illness);
            TestEqual(TEXT("the illness source was registered"), survival->registeredSources.Num(), 1);
        });
    });

    // ---- Spec 4 — Bag lifecycle (tier 2; this is R01's side of Doc B22's "Respawn and bag") ----
    Describe("Bag lifecycle", [this](){
        It("detach is all or nothing; Bagless holds one item per slot; recovery restores bag, hotbar and hand", [this](){
            FInventoryTestFixture fixture;
            fixture.addStackable(TEXT("Stick"), 20);
            UDA_ItemDefinition* spear = fixture.addInstanced(TEXT("Spear"), 4, 50.0f);
            spear->categoryTag = T_Item_Category_Weapon;
            spear->weaponType = E_WeaponType::Thrown;
            AInventoryTestPlayerState* player = fixture.makePlayer();
            fixture.makeCharacterFor(player);
            UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
            UHeldItemComponent* heldItem = UHeldItemComponent::findFor(player);
            UInventoryData* bag = inventory->getPersonalInventory();
            bag->addStack(TEXT("Stick"), 5);
            UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("Spear"), 1));
            const FGuid spearId = inventory->findUsableInstance(TEXT("Spear"));
            inventory->bindHotbarSlot(1, TEXT("Spear"));
            heldItem->selectHotbarSlot(1);
            TestTrue(TEXT("the spear is wielded"), heldItem->getHeldState().isWielded);

            setFailBagStep(1);
            TestTrue(TEXT("a failed step 1 is reported"), UBFL_InventoryBag::detachBag(player, FVector::ZeroVector).failReason == T_Inventory_FailReason_BagCreateFailed);
            TestTrue(TEXT("and nothing moved"), inventory->hasBag() && heldItem->getHeldState().instanceId == spearId);
            setFailBagStep(0);

            const FS_BagDetachResult detached = UBFL_InventoryBag::detachBag(player, FVector::ZeroVector);
            TestTrue(TEXT("detached"), detached.succeeded());
            TestFalse(TEXT("the player is Bagless"), inventory->hasBag());
            TestEqual(TEXT("three usable keys"), UBFL_InventoryBag::getUsableHotbarSlotCount(player), 3);
            TestTrue(TEXT("the hand is empty"), heldItem->getHeldState().isEmpty());
            TestTrue(TEXT("the bag lies in the world, locked"), fixture.registry()->findInventory(bag->getInventoryId())->isDeathBag());
            TestTrue(TEXT("a death bag cannot be taken from"), UBFL_Inventory::transferEntry(bag, fixture.makeContainer(FS_ContainerRules()), refTo(TEXT("Stick")), 1).failReason == T_Inventory_FailReason_DeathBagLocked);

            TestEqual(TEXT("a grant to a Bagless player drops"), UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("Stick"), 2)).quantityDropped, 2);
            ALooseItemActor* looseItem = spawnLooseStack(fixture, TEXT("Stick"), 5);
            TestEqual(TEXT("a pickup takes one item into a hold slot"), UBFL_Inventory::pickUpLooseItem(player, looseItem).quantity, 1);
            TestEqual(TEXT("the rest of the stack stays"), looseItem->getEntry().totalQuantity, 4);
            TestTrue(TEXT("binding is refused while Bagless"), inventory->bindHotbarSlot(1, TEXT("Stick")).failReason == T_Inventory_FailReason_NoBag);

            TestTrue(TEXT("recovered"), UBFL_InventoryBag::reattachBag(player, detached.bagId).succeeded());
            TestTrue(TEXT("the bag is back"), inventory->hasBag());
            TestEqual(TEXT("the held stick was added to it"), inventory->getPersonalInventory()->getTotalQuantity(TEXT("Stick")), 6);
            TestTrue(TEXT("key 1 is the spear again"), inventory->getBoundItemId(1) == FName(TEXT("Spear")));
            TestTrue(TEXT("the same spear is in the hand, wielded"), heldItem->getHeldState().instanceId == spearId && heldItem->getHeldState().isWielded);
            TestTrue(TEXT("a second recovery is refused"), UBFL_InventoryBag::reattachBag(player, detached.bagId).failReason == T_Inventory_FailReason_AlreadyHasBag);
        });
    });

    // ---- Hotbar key and spoiled food (tier 1; S13.5 list) ----
    Describe("Hotbar key and spoiled food", [this](){
        It("a key bound to fresh food never resolves to the spoiled item, and the binding stays", [this](){
            FInventoryTestFixture fixture;
            UDA_ItemDefinition* meat = fixture.addStackable(TEXT("RawMeat"), 10);
            meat->canSpoil = true;
            meat->spoilTimeHours = 24.0f;
            meat->spoiledItemId = TEXT("SpoiledMeat");
            fixture.addStackable(TEXT("SpoiledMeat"), 10);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
            fixture.setGameTime(0);
            UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("RawMeat"), 2));
            TestTrue(TEXT("bound"), inventory->bindHotbarSlot(1, TEXT("RawMeat")).succeeded());
            TestTrue(TEXT("usable while fresh"), inventory->resolveHotbarSlot(1).isUsable);

            fixture.advanceHours(25.0f);
            inventory->getPersonalInventory()->evaluateSpoilage(fixture.services()->getGameTime());   // any request on the bag
            const FS_ResolvedHotbarSlot resolved = inventory->resolveHotbarSlot(1);
            TestFalse(TEXT("the slot is not usable"), resolved.isUsable);
            TestTrue(TEXT("it never resolves to the spoiled item"), resolved.itemId == FName(TEXT("RawMeat")));
            TestTrue(TEXT("the binding is unchanged"), inventory->getBoundItemId(1) == FName(TEXT("RawMeat")));
            TestEqual(TEXT("the meat is SpoiledMeat now"), inventory->getAvailableQuantity(TEXT("SpoiledMeat")), 2);
        });
    });
}

#endif
