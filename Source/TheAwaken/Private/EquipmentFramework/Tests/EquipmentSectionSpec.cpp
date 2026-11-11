// The automatable rows of the Verify tables of Sections E1 and E3 to E7 of the Doc R05 guide, beyond the four specs of E10.5.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/IConsoleManager.h"
#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/Stubs/InventoryStubs.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UBFL_InventoryBag.h"
#include "ItemInventoryFramework/UContainerViewComponent.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/03Interfaces/BPI_EquipmentGear.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "EquipmentFramework/EquipmentServices.h"
#include "EquipmentFramework/UBFL_Equipment.h"
#include "EquipmentFramework/UEquipmentComponent.h"
#include "EquipmentFramework/02Tags/T_EquipmentFramework.h"
#include "EquipmentFramework/03Interfaces/BPI_ArmorStatsSink.h"
#include "EquipmentFramework/Stubs/EquipmentStubs.h"
#include "GameFramework/GameplayMessageSubsystem.h"

namespace EquipmentSectionSpecHelpers{
    FGuid giveArmor(AInventoryTestPlayerState* player, const UDA_ItemDefinition& definition){
        const FS_InventoryEntry entry = UBFL_Inventory::makeFreshInstance(player, definition);
        UPlayerInventoryComponent::findFor(player)->getPersonalInventory()->addInstance(entry);
        return entry.instanceId;
    }

    void setCvar(const TCHAR* name, int32 value){
        if(IConsoleVariable* variable = IConsoleManager::Get().FindConsoleVariable(name)){
            variable->Set(value);
        }
    }
}

BEGIN_DEFINE_SPEC(FEquipmentSectionSpec, "Awaken.Equipment.Sections", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FEquipmentSectionSpec)

void FEquipmentSectionSpec::Define(){
    using namespace EquipmentSectionSpecHelpers;

    Describe("E1 types and tags", [this](){
        It("default clothes and the stack position tags", [this](){
            TestTrue(TEXT("fresh clothes are the default"), FS_WornClothesState().isDefault());
            TestTrue(TEXT("index 0 -> Position01"), EquipmentTags::getStackPositionTag(0) == T_Equipment_ArmorStack_Position01);
            TestTrue(TEXT("index 13 -> Position14"), EquipmentTags::getStackPositionTag(13) == T_Equipment_ArmorStack_Position14);
            TestFalse(TEXT("index 14 -> no tag"), EquipmentTags::getStackPositionTag(14).IsValid());
        });
    });

    Describe("E3 to E7", [this](){
        BeforeEach([this](){
            FInventoryTestFixture::expectForeignStartupErrors(*this);
        });

        It("E3: the cap, its override, a zero aggregate at start, and the real gear seam", [this](){
            FInventoryTestFixture fixture;
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            const UStubArmorStatsSink* stats = Cast<UStubArmorStatsSink>(fixture.services()->getSeam(UBPI_ArmorStatsSink::StaticClass()));
            TestEqual(TEXT("cap 14"), equipment->getArmorStackCap(), 14);
            setCvar(TEXT("Awaken.Equipment.StackCap"), 3);
            TestEqual(TEXT("override 3"), equipment->getArmorStackCap(), 3);
            setCvar(TEXT("Awaken.Equipment.StackCap"), -1);
            TestEqual(TEXT("back to 14"), equipment->getArmorStackCap(), 14);
            TestTrue(TEXT("a fresh player reported 0 to Stats"), stats->callCount >= 1 && stats->lastArmorValue == 0.0f);
            TestTrue(TEXT("R01's gear seam is the real UEquipmentServices"), Cast<UEquipmentServices>(fixture.services()->getSeam(UBPI_EquipmentGear::StaticClass())) != nullptr);
        });

        It("E4: refusals that change nothing", [this](){
            FInventoryTestFixture fixture;
            const UDA_ItemDefinition* vest = fixture.addArmor(TEXT("IronVest"), 14, 10.0f, 100.0f);
            const UDA_ItemDefinition* spear = fixture.addInstanced(TEXT("Spear"), 4, 50.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();

            const FGuid spearId = giveArmor(player, *spear);
            TestTrue(TEXT("a spear is NotArmor"), equipment->equipArmor(spearId).failReason == T_Equipment_FailReason_NotArmor);
            TestTrue(TEXT("and stays in the bag"), bag->findInstance(spearId) != nullptr);
            const FGuid vestId = giveArmor(player, *vest);
            TestTrue(TEXT("equip once"), equipment->equipArmor(vestId).succeeded());
            TestTrue(TEXT("the same instance twice is NotFound"), equipment->equipArmor(vestId).failReason == T_Inventory_FailReason_NotFound);
            TestTrue(TEXT("a bad position is InvalidStackIndex"), equipment->unequipArmor(5).failReason == T_Equipment_FailReason_InvalidStackIndex);
            const FGuid staged = giveArmor(player, *vest);
            FS_ItemRef ref;
            ref.instanceId = staged;
            UBFL_Inventory::reserveForStaging(player, ref, 1, FGuid::NewGuid());
            TestTrue(TEXT("a staged piece is ReservedQuantity"), equipment->equipArmor(staged).failReason == T_Inventory_FailReason_ReservedQuantity);
        });

        It("E5: wear-only changes tell nobody; a break is announced once; nothing for empty, negative or full penetration", [this](){
            FInventoryTestFixture fixture;
            const UDA_ItemDefinition* vest = fixture.addArmor(TEXT("IronVest"), 14, 10.0f, 100.0f);
            const UDA_ItemDefinition* pad = fixture.addArmor(TEXT("LeatherPad"), 14, 4.0f, 30.0f);
            fixture.addClothes(TEXT("SoldierVest"), 3, 5.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            const UStubArmorStatsSink* stats = Cast<UStubArmorStatsSink>(fixture.services()->getSeam(UBPI_ArmorStatsSink::StaticClass()));
            int32 broken = 0;
            FGameplayMessageListenerHandle listener = UGameplayMessageSubsystem::Get(fixture.world()).RegisterListener<FS_ArmorChangedMessage>(T_Equipment_ArmorBroken, [&broken](FGameplayTag channel, const FS_ArmorChangedMessage& message){
                broken++;
            });

            TestEqual(TEXT("an empty stack absorbs nothing"), equipment->applyDamageToArmor(10.0f).absorbedAmount, 0.0f);
            equipment->equipArmor(giveArmor(player, *vest));
            equipment->equipArmor(giveArmor(player, *pad));
            TestEqual(TEXT("a negative hit absorbs nothing"), equipment->applyDamageToArmor(-5.0f).absorbedAmount, 0.0f);
            const int32 callsBefore = stats->callCount;
            equipment->applyDamageToArmor(20.0f);
            TestEqual(TEXT("wear only: Stats is not called again"), stats->callCount, callsBefore);
            TestEqual(TEXT("and no break"), broken, 0);
            equipment->applyDamageToArmor(25.0f);
            TestEqual(TEXT("one break, announced once"), broken, 1);
            TestTrue(TEXT("Stats got the new aggregate"), stats->callCount > callsBefore && stats->lastArmorValue == 10.0f);

            const float before = equipment->getArmorStack()[0].remainingDurability;
            const FS_GearAttackResult full = UBFL_Equipment::applyAttackToGear(player, 30.0f, 1.0f);
            TestTrue(TEXT("full penetration: armor absorbs 0 and does not wear"), full.absorbedByArmor == 0.0f && equipment->getArmorStack()[0].remainingDurability == before);

            UPlayerInventoryComponent::findFor(player)->getPersonalInventory()->addStack(TEXT("SoldierVest"), 1);
            equipment->equipClothes(TEXT("SoldierVest"));
            for(int32 hit = 0; hit < 100; hit++){
                UBFL_Equipment::applyAttackToGear(player, 3.0f, 0.0f);
            }
            TestEqual(TEXT("clothes after 100 hits: same protection"), equipment->getClothesProtection().protection, 5.0f);
            listener.Unregister();
        });

        It("E6: clothes refusals, a full return drops, and clothes from an open container", [this](){
            FInventoryTestFixture fixture;
            fixture.addArmor(TEXT("IronVest"), 14, 10.0f, 100.0f);
            fixture.addClothes(TEXT("SoldierVest"), 3, 5.0f);
            fixture.addClothes(TEXT("BeachOutfit"), 3, 0.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            const UStubLooseItemSpawner* spawner = fixture.stub<UStubLooseItemSpawner, UBPI_LooseItemSpawner>();
            int32 changed = 0;
            FGameplayMessageListenerHandle listener = UGameplayMessageSubsystem::Get(fixture.world()).RegisterListener<FS_ClothesChangedMessage>(T_Equipment_ClothesChanged, [&changed](FGameplayTag channel, const FS_ClothesChangedMessage& message){
                changed++;
            });

            TestEqual(TEXT("a new player: protection 0"), equipment->getClothesProtection().protection, 0.0f);
            TestTrue(TEXT("armor is NotClothes"), equipment->equipClothes(TEXT("IronVest")).failReason == T_Equipment_FailReason_NotClothes);
            TestTrue(TEXT("none in the bag"), equipment->equipClothes(TEXT("SoldierVest")).failReason == T_Inventory_FailReason_InsufficientQuantity);

            bag->addStack(TEXT("SoldierVest"), 3);
            TestTrue(TEXT("wear a SoldierVest"), equipment->equipClothes(TEXT("SoldierVest")).succeeded());
            TestEqual(TEXT("ClothesChanged raised"), changed, 1);
            bag->addStack(TEXT("SoldierVest"), 1);   // back at the limit of 3
            bag->addStack(TEXT("BeachOutfit"), 1);
            const int32 droppedBefore = spawner->spawnedEntries.Num();
            TestTrue(TEXT("change while SoldierVest is at its limit"), equipment->equipClothes(TEXT("BeachOutfit")).succeeded());
            TestTrue(TEXT("the returned vest dropped as a loose item"), spawner->spawnedEntries.Num() == droppedBefore + 1 && spawner->spawnedEntries.Last().itemId == FName(TEXT("SoldierVest")));

            TestTrue(TEXT("no container open: NotFound"), equipment->equipClothesFromContainer(TEXT("SoldierVest")).failReason == T_Inventory_FailReason_NotFound);
            UInventoryData* chest = fixture.makeContainer(FS_ContainerRules());
            chest->addStack(TEXT("SoldierVest"), 1);
            UContainerViewComponent::findFor(player)->openContainer(chest->getInventoryId());
            const int32 bagVests = bag->getTotalQuantity(TEXT("SoldierVest"));
            TestTrue(TEXT("wear from the chest"), equipment->equipClothesFromContainer(TEXT("SoldierVest")).succeeded());
            TestEqual(TEXT("the chest lost one"), chest->getTotalQuantity(TEXT("SoldierVest")), 0);
            TestTrue(TEXT("the old outfit lies on the ground"), spawner->spawnedEntries.Last().itemId == FName(TEXT("BeachOutfit")));
            TestEqual(TEXT("the bag is unchanged"), bag->getTotalQuantity(TEXT("SoldierVest")), bagVests);

            UBFL_InventoryBag::detachBag(player, FVector::ZeroVector);
            TestTrue(TEXT("Bagless: NoBag"), equipment->equipClothes(TEXT("BeachOutfit")).failReason == T_Inventory_FailReason_NoBag);
            TestTrue(TEXT("and still wears the SoldierVest"), equipment->getWornClothes().itemId == FName(TEXT("SoldierVest")));
            listener.Unregister();
        });

        It("E7: a failed step 2 moves nothing; recovery ignores the cap; reset for a new character", [this](){
            FInventoryTestFixture fixture;
            const UDA_ItemDefinition* vest = fixture.addArmor(TEXT("IronVest"), 14, 10.0f, 100.0f);
            fixture.addClothes(TEXT("SoldierVest"), 3, 5.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            for(int32 index = 0; index < 3; index++){
                equipment->equipArmor(giveArmor(player, *vest));
            }
            UPlayerInventoryComponent::findFor(player)->getPersonalInventory()->addStack(TEXT("SoldierVest"), 1);
            equipment->equipClothes(TEXT("SoldierVest"));

            setCvar(TEXT("Awaken.Inventory.FailBagStep"), 2);
            TestTrue(TEXT("step 2 fails"), UBFL_InventoryBag::detachBag(player, FVector::ZeroVector).failReason == T_Inventory_FailReason_GearReleaseFailed);
            setCvar(TEXT("Awaken.Inventory.FailBagStep"), 0);
            TestEqual(TEXT("the player still wears all the armor"), equipment->getArmorStack().Num(), 3);
            TestTrue(TEXT("and still has the bag"), UPlayerInventoryComponent::findFor(player)->hasBag());

            const FS_BagDetachResult detached = UBFL_InventoryBag::detachBag(player, FVector::ZeroVector);
            setCvar(TEXT("Awaken.Equipment.StackCap"), 1);
            TestTrue(TEXT("recovered"), UBFL_InventoryBag::reattachBag(player, detached.bagId).succeeded());
            TestEqual(TEXT("all three are worn again despite the cap of 1"), equipment->getArmorStack().Num(), 3);
            const FGuid fourth = giveArmor(player, *vest);
            TestTrue(TEXT("a fourth equip is StackFull"), equipment->equipArmor(fourth).failReason == T_Equipment_FailReason_StackFull);
            setCvar(TEXT("Awaken.Equipment.StackCap"), -1);

            equipment->resetForNewCharacter();
            TestEqual(TEXT("reset: no armor"), equipment->getArmorStack().Num(), 0);
            TestTrue(TEXT("reset: default clothes"), equipment->getWornClothes().isDefault());
            TestEqual(TEXT("reset: aggregate 0"), equipment->getAggregatedArmorStats().armorValue, 0.0f);
        });
    });
}

#endif
