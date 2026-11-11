// Doc R05 guide E8 Verify / E10.5 "Save round trip" (tier 2): R05's half of Doc B22's "R13 Save round trip".
// Calls Doc R13's IBPI_SaveParticipant functions directly on the equipment and inventory components.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/IConsoleManager.h"
#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "EquipmentFramework/UEquipmentComponent.h"
#include "EquipmentFramework/03Interfaces/BPI_ArmorStatsSink.h"
#include "EquipmentFramework/Stubs/EquipmentStubs.h"
#include "StructUtils/InstancedStruct.h"

namespace EquipmentSaveSpecHelpers{
    FGuid giveArmor(AInventoryTestPlayerState* player, const UDA_ItemDefinition& definition){
        const FS_InventoryEntry entry = UBFL_Inventory::makeFreshInstance(player, definition);
        UPlayerInventoryComponent::findFor(player)->getPersonalInventory()->addInstance(entry);
        return entry.instanceId;
    }

    void setStackCap(int32 cap){
        if(IConsoleVariable* variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Awaken.Equipment.StackCap"))){
            variable->Set(cap);
        }
    }
}

BEGIN_DEFINE_SPEC(FEquipmentSaveSpec, "Awaken.Equipment.Save", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FEquipmentSaveSpec)

void FEquipmentSaveSpec::Define(){
    using namespace EquipmentSaveSpecHelpers;

    BeforeEach([this](){
        FInventoryTestFixture::expectForeignStartupErrors(*this);
    });

    It("round trip: same pieces, order, ids, durability and clothes; nothing counted twice; unknown skipped; newer refused; cap ignored", [this](){
        FInventoryTestFixture fixture;
        const UDA_ItemDefinition* vest = fixture.addArmor(TEXT("IronVest"), 14, 10.0f, 100.0f);
        const UDA_ItemDefinition* pad = fixture.addArmor(TEXT("LeatherPad"), 14, 4.0f, 30.0f);
        fixture.addClothes(TEXT("WoolClothes"), 3, 1.0f, 4.0f);
        fixture.addClothes(TEXT("SoldierVest"), 3, 5.0f);
        AInventoryTestPlayerState* player = fixture.makePlayer();
        UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
        UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
        const UStubArmorStatsSink* stats = Cast<UStubArmorStatsSink>(fixture.services()->getSeam(UBPI_ArmorStatsSink::StaticClass()));

        // ---- step 1 ----
        const FGuid vestId = giveArmor(player, *vest);
        const FGuid padId = giveArmor(player, *pad);
        const FGuid spareId = giveArmor(player, *vest);
        equipment->equipArmor(vestId);
        equipment->equipArmor(padId);
        equipment->applyDamageToArmor(18.0f);   // the pad, stacked last, goes from 30 to 12
        inventory->getPersonalInventory()->addStack(TEXT("WoolClothes"), 1);
        equipment->equipClothes(TEXT("WoolClothes"));

        // ---- step 2 ----
        FInstancedStruct equipmentSnapshot;
        FInstancedStruct inventorySnapshot;
        TestTrue(TEXT("Equipment captured"), IBPI_SaveParticipant::Execute_getSnapshotForSave(equipment, equipmentSnapshot));
        TestTrue(TEXT("Inventory captured"), IBPI_SaveParticipant::Execute_getSnapshotForSave(inventory, inventorySnapshot));
        equipment->discardWornArmor();
        inventory->getPersonalInventory()->addStack(TEXT("SoldierVest"), 1);
        equipment->equipClothes(TEXT("SoldierVest"));
        inventory->getPersonalInventory()->clearAll();

        // ---- steps 3 and 4 ----
        TestTrue(TEXT("Inventory restored"), IBPI_SaveParticipant::Execute_restoreFromSnapshot(inventory, inventorySnapshot, 1));
        TestTrue(TEXT("Equipment restored"), IBPI_SaveParticipant::Execute_restoreFromSnapshot(equipment, equipmentSnapshot, 1));
        const TArray<FS_EquippedArmorState>& stack = equipment->getArmorStack();
        TestTrue(TEXT("two pieces, same order, same instance ids"), stack.Num() == 2 && stack[0].instanceId == vestId && stack[1].instanceId == padId);
        TestTrue(TEXT("durabilities 100 and 12"), stack.Num() == 2 && stack[0].remainingDurability == 100.0f && stack[1].remainingDurability == 12.0f);
        TestTrue(TEXT("WoolClothes again"), equipment->getWornClothes().itemId == FName(TEXT("WoolClothes")));
        TestEqual(TEXT("aggregate 14"), equipment->getAggregatedArmorStats().armorValue, 14.0f);
        TestEqual(TEXT("warmth 4"), equipment->getTotalWarmth(), 4.0f);
        TestEqual(TEXT("Stats received 14"), stats->lastArmorValue, 14.0f);
        const UInventoryData* bag = inventory->getPersonalInventory();
        TestEqual(TEXT("the bag holds exactly one vest"), bag->getInstanceCount(TEXT("IronVest")), 1);
        TestTrue(TEXT("it is the spare one"), bag->findInstance(spareId) != nullptr);
        TestTrue(TEXT("the worn pieces are not in the bag"), bag->findInstance(vestId) == nullptr && bag->findInstance(padId) == nullptr);

        // ---- step 5: an item that no longer exists ----
        FInstancedStruct patched = equipmentSnapshot;
        patched.GetMutablePtr<FS_EquipmentSave>()->armorStack[1].itemId = TEXT("NoSuchArmor");
        AddExpectedError(TEXT("Skipped saved armor NoSuchArmor"), EAutomationExpectedErrorFlags::Contains, 1);
        TestTrue(TEXT("the load continues"), IBPI_SaveParticipant::Execute_restoreFromSnapshot(equipment, patched, 1));
        TestTrue(TEXT("the vest is restored at index 0"), equipment->getArmorStack().Num() == 1 && equipment->getArmorStack()[0].instanceId == vestId);

        // ---- step 6: a newer version ----
        FInstancedStruct newer = equipmentSnapshot;
        newer.GetMutablePtr<FS_EquipmentSave>()->version = 99;
        TestFalse(TEXT("version 99 is refused"), IBPI_SaveParticipant::Execute_restoreFromSnapshot(equipment, newer, 99));
        TestEqual(TEXT("and the worn state is unchanged"), equipment->getArmorStack().Num(), 1);

        // ---- step 7: the cap is not applied to restored pieces ----
        setStackCap(1);
        TestTrue(TEXT("restored with the cap at 1"), IBPI_SaveParticipant::Execute_restoreFromSnapshot(equipment, equipmentSnapshot, 1));
        setStackCap(-1);
        TestEqual(TEXT("both pieces are worn"), equipment->getArmorStack().Num(), 2);
    });
}

#endif
