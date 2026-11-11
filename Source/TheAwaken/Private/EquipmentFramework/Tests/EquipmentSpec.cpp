#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/IConsoleManager.h"
#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UBFL_InventoryBag.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "EquipmentFramework/UEquipmentComponent.h"
#include "EquipmentFramework/UBFL_Equipment.h"
#include "EquipmentFramework/02Tags/T_EquipmentFramework.h"
#include "EquipmentFramework/03Interfaces/BPI_ArmorStatsSink.h"
#include "EquipmentFramework/Stubs/EquipmentStubs.h"

namespace{
    // Puts a fresh copy of an armor item into the player's bag and returns its instance id.
    FGuid giveArmor(AInventoryTestPlayerState* player, const UDA_ItemDefinition& definition){
        const FS_InventoryEntry entry = UBFL_Inventory::makeFreshInstance(player, definition);
        UPlayerInventoryComponent::findFor(player)->getPersonalInventory()->addInstance(entry);
        return entry.instanceId;
    }

    void setStackCap(int32 cap){
        IConsoleVariable* variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Awaken.Equipment.StackCap"));
        if(variable != nullptr){
            variable->Set(cap);
        }
    }
}

BEGIN_DEFINE_SPEC(FEquipmentSpec, "Awaken.Equipment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FEquipmentSpec)

void FEquipmentSpec::Define(){
    BeforeEach([this](){
        FInventoryTestFixture::expectForeignStartupErrors(*this);
    });

    // ---- Spec 1 — the armor stack (tier 1) ----
    Describe("Armor stack", [this](){
        It("moves a piece out of the bag, refuses at the cap and never loses a piece", [this](){
            FInventoryTestFixture fixture;
            const UDA_ItemDefinition* vest = fixture.addArmor(TEXT("IronVest"), 2, 10.0f, 100.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            const UStubArmorStatsSink* stats = Cast<UStubArmorStatsSink>(fixture.services()->getSeam(UBPI_ArmorStatsSink::StaticClass()));

            const FGuid first = giveArmor(player, *vest);
            const FGuid second = giveArmor(player, *vest);
            TestTrue(TEXT("equip first"), equipment->equipArmor(first).succeeded());
            TestTrue(TEXT("equip second"), equipment->equipArmor(second).succeeded());
            TestEqual(TEXT("the bag holds none: worn is not in the bag"), bag->getInstanceCount(TEXT("IronVest")), 0);
            TestEqual(TEXT("two worn"), equipment->getArmorStack().Num(), 2);
            TestEqual(TEXT("aggregate is the sum"), equipment->getAggregatedArmorStats().armorValue, 20.0f);
            TestEqual(TEXT("Stats was told"), stats->lastArmorValue, 20.0f);

            setStackCap(2);
            const FGuid third = giveArmor(player, *vest);
            TestTrue(TEXT("refused at the cap"), equipment->equipArmor(third).failReason == T_Equipment_FailReason_StackFull);
            TestTrue(TEXT("the refused piece is still in the bag"), bag->findInstance(third) != nullptr);
            setStackCap(1);
            TestEqual(TEXT("lowering the cap takes nothing off"), equipment->getArmorStack().Num(), 2);
            setStackCap(-1);

            TestTrue(TEXT("unequip position 0"), equipment->unequipArmor(0).succeeded());
            TestTrue(TEXT("the same instance is back in the bag"), bag->findInstance(first) != nullptr);
            TestTrue(TEXT("the stack closed up"), equipment->getArmorStack()[0].instanceId == second && equipment->getArmorStack()[0].stackIndex == 0);
            // The bag now holds two vests, which is the per-type limit of this test item.
            TestTrue(TEXT("refused when the bag is at its limit"), equipment->unequipArmor(0).failReason == T_Interaction_FailReason_BagFull);
            TestEqual(TEXT("so the piece stays worn"), equipment->getArmorStack().Num(), 1);
        });
    });

    // ---- Spec 2 — armor wear and the reference damage step (tier 1) ----
    Describe("Armor damage", [this](){
        It("wears the last piece first, passes the rest down and destroys a used-up piece", [this](){
            FInventoryTestFixture fixture;
            const UDA_ItemDefinition* vest = fixture.addArmor(TEXT("IronVest"), 5, 10.0f, 100.0f);
            const UDA_ItemDefinition* pad = fixture.addArmor(TEXT("LeatherPad"), 5, 4.0f, 30.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            const FGuid vestId = giveArmor(player, *vest);
            const FGuid padId = giveArmor(player, *pad);
            equipment->equipArmor(vestId);
            equipment->equipArmor(padId);

            TestEqual(TEXT("absorbs 20"), equipment->applyDamageToArmor(20.0f).absorbedAmount, 20.0f);
            TestEqual(TEXT("the pad, stacked last, took it"), equipment->getArmorStack()[1].remainingDurability, 10.0f);
            TestEqual(TEXT("the vest is untouched"), equipment->getArmorStack()[0].remainingDurability, 100.0f);

            const FS_ArmorDamageResult second = equipment->applyDamageToArmor(25.0f);
            TestEqual(TEXT("absorbs 25 across two pieces"), second.absorbedAmount, 25.0f);
            TestEqual(TEXT("one piece broke"), second.brokenPieces, 1);
            TestEqual(TEXT("one piece left"), equipment->getArmorStack().Num(), 1);
            TestEqual(TEXT("the vest took the remaining 15"), equipment->getArmorStack()[0].remainingDurability, 85.0f);
            TestEqual(TEXT("aggregate dropped"), equipment->getAggregatedArmorStats().armorValue, 10.0f);
            TestTrue(TEXT("a broken piece is destroyed, not returned"), bag->findInstance(padId) == nullptr);
            // The break effect goes to Doc B21's real UCosmeticEventSubsystem (no recording stub exists): see EquipmentSectionSpec for the message check.

            TestEqual(TEXT("absorbs only what is left"), equipment->applyDamageToArmor(500.0f).absorbedAmount, 85.0f);
            TestEqual(TEXT("the stack is empty"), equipment->getArmorStack().Num(), 0);
        });
        It("clothes first, then armor times one minus penetration, never below zero", [this](){
            FInventoryTestFixture fixture;
            const UDA_ItemDefinition* vest = fixture.addArmor(TEXT("IronVest"), 5, 10.0f, 100.0f);
            fixture.addClothes(TEXT("SoldierVest"), 3, 5.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            UPlayerInventoryComponent::findFor(player)->getPersonalInventory()->addStack(TEXT("SoldierVest"), 1);
            equipment->equipClothes(TEXT("SoldierVest"));
            equipment->equipArmor(giveArmor(player, *vest));

            const FS_GearAttackResult hit = UBFL_Equipment::applyAttackToGear(player, 30.0f, 0.5f);
            TestEqual(TEXT("clothes took 5"), hit.absorbedByClothes, 5.0f);
            TestEqual(TEXT("armor took 10 x 0.5"), hit.absorbedByArmor, 5.0f);
            TestEqual(TEXT("20 goes on to health"), hit.damageToHealth, 20.0f);
            TestEqual(TEXT("the vest wore by what it absorbed"), equipment->getArmorStack()[0].remainingDurability, 95.0f);

            const FS_GearAttackResult weak = UBFL_Equipment::applyAttackToGear(player, 3.0f, 0.0f);
            TestEqual(TEXT("never below zero"), weak.damageToHealth, 0.0f);
            TestEqual(TEXT("clothes never wear out"), equipment->getClothesProtection().protection, 5.0f);
        });
    });

    // ---- Spec 3 — clothes (tier 1) ----
    Describe("Clothes", [this](){
        It("takes one unit from the stack and returns the old outfit to the bag", [this](){
            FInventoryTestFixture fixture;
            fixture.addClothes(TEXT("SoldierVest"), 3, 5.0f);
            fixture.addClothes(TEXT("WoolClothes"), 3, 1.0f, 4.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            bag->addStack(TEXT("SoldierVest"), 2);
            bag->addStack(TEXT("WoolClothes"), 1);

            TestTrue(TEXT("a new player wears the default clothes"), equipment->getWornClothes().isDefault());
            TestTrue(TEXT("wear"), equipment->equipClothes(TEXT("SoldierVest")).succeeded());
            TestEqual(TEXT("one unit left the stack"), bag->getTotalQuantity(TEXT("SoldierVest")), 1);
            TestTrue(TEXT("the same outfit again is refused"), equipment->equipClothes(TEXT("SoldierVest")).failReason == T_Equipment_FailReason_AlreadyWorn);

            TestTrue(TEXT("change"), equipment->equipClothes(TEXT("WoolClothes")).succeeded());
            TestEqual(TEXT("the old outfit came back"), bag->getTotalQuantity(TEXT("SoldierVest")), 2);
            TestEqual(TEXT("none of the new one left in the bag"), bag->getTotalQuantity(TEXT("WoolClothes")), 0);
            TestEqual(TEXT("warmth follows the clothes"), equipment->getTotalWarmth(), 4.0f);
        });
    });

    // ---- Spec 4 — death bag and recovery (tier 2: several frameworks' pieces together) ----
    Describe("Bag lifecycle", [this](){
        It("armor leaves with the bag and returns in the same order; clothes stay on", [this](){
            FInventoryTestFixture fixture;
            const UDA_ItemDefinition* vest = fixture.addArmor(TEXT("IronVest"), 5, 10.0f, 100.0f);
            const UDA_ItemDefinition* pad = fixture.addArmor(TEXT("LeatherPad"), 5, 4.0f, 30.0f);
            fixture.addClothes(TEXT("SoldierVest"), 3, 5.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            const UStubArmorStatsSink* stats = Cast<UStubArmorStatsSink>(fixture.services()->getSeam(UBPI_ArmorStatsSink::StaticClass()));
            UPlayerInventoryComponent::findFor(player)->getPersonalInventory()->addStack(TEXT("SoldierVest"), 1);
            equipment->equipClothes(TEXT("SoldierVest"));
            const FGuid vestId = giveArmor(player, *vest);
            const FGuid padId = giveArmor(player, *pad);
            equipment->equipArmor(vestId);
            equipment->equipArmor(padId);
            equipment->applyDamageToArmor(10.0f);   // the pad is now at 20

            const FS_BagDetachResult detached = UBFL_InventoryBag::detachBag(player, FVector::ZeroVector);
            TestTrue(TEXT("detached"), detached.succeeded());
            TestEqual(TEXT("no armor while Bagless"), equipment->getArmorStack().Num(), 0);
            TestEqual(TEXT("Stats was told zero"), stats->lastArmorValue, 0.0f);
            TestTrue(TEXT("clothes stay on the player"), equipment->getWornClothes().itemId == FName(TEXT("SoldierVest")));
            TestTrue(TEXT("a Bagless player cannot equip"), equipment->equipArmor(vestId).failReason == T_Inventory_FailReason_NoBag);

            TestTrue(TEXT("recovered"), UBFL_InventoryBag::reattachBag(player, detached.bagId).succeeded());
            TestEqual(TEXT("both pieces are worn again"), equipment->getArmorStack().Num(), 2);
            TestTrue(TEXT("in the positions they occupied"), equipment->getArmorStack()[0].instanceId == vestId && equipment->getArmorStack()[1].instanceId == padId);
            TestEqual(TEXT("durability travelled with the piece"), equipment->getArmorStack()[1].remainingDurability, 20.0f);
            TestEqual(TEXT("aggregate restored"), equipment->getAggregatedArmorStats().armorValue, 14.0f);
        });
    });
}

#endif
