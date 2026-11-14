// Doc B07 guide C10.5: the automation specs of the Carry & Encumbrance framework, plus the automatable Verify rows of
// Sections 4 to 8 that the six specs do not cover.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestCharacter.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/Stubs/InventoryStubs.h"
#include "ItemInventoryFramework/03Interfaces/BPI_LooseItemSpawner.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "SaveFramework/03Interfaces/BPI_SaveParticipant.h"
#include "CarryEncumbranceFramework/AThrownItemActor.h"
#include "CarryEncumbranceFramework/CarryServices.h"
#include "CarryEncumbranceFramework/UBFL_Carry.h"
#include "CarryEncumbranceFramework/UCarryComponent.h"
#include "CarryEncumbranceFramework/02Tags/T_CarryEncumbranceFramework.h"
#include "CarryEncumbranceFramework/03Interfaces/BPI_ThrowImpactSink.h"
#include "CarryEncumbranceFramework/Stubs/CarryStubs.h"

namespace CarrySpecHelpers{
    // One unit of a stackable heavy item, as R01 hands it to the seam.
    FS_InventoryEntry makeUnit(FName itemId){
        FS_InventoryEntry entry;
        entry.itemId = itemId;
        entry.totalQuantity = 1;
        return entry;
    }

    // A player with a character, and that character's carry component.
    UCarryComponent* makeCarrier(FInventoryTestFixture& fixture, AInventoryTestPlayerState*& outPlayer){
        outPlayer = fixture.makePlayer();
        fixture.makeCharacterFor(outPlayer);
        return UCarryComponent::findFor(outPlayer);
    }

    AThrownItemActor* findThrown(const FInventoryTestFixture& fixture){
        AThrownItemActor* thrown = nullptr;
        for(TActorIterator<AThrownItemActor> iterator(fixture.world()); iterator; ++iterator){
            thrown = *iterator;
        }
        return thrown;
    }

    void setSlotCountOverride(int32 count){
        if(IConsoleVariable* variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Awaken.Carry.SlotCount"))){
            variable->Set(count);
        }
    }
}

using namespace CarrySpecHelpers;

BEGIN_DEFINE_SPEC(FCarrySpec, "Awaken.Carry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FCarrySpec)

void FCarrySpec::Define(){
    BeforeEach([this](){
        FInventoryTestFixture::expectForeignStartupErrors(*this);
    });

    // ---- Section 1 Verify 4 — defaults (tier 1) ----
    Describe("Defaults", [this](){
        It("a fresh slot is free, and a carry ends the five actions", [this](){
            FInventoryTestFixture fixture;   // not needed by the checks; it raises the startup lines BeforeEach expects
            TestFalse(TEXT("a fresh slot is not occupied"), FS_CarrySlotState().isOccupied());
            const TArray<FGameplayTag> actions = CarryTags::getActionsEndedByCarry();
            TestEqual(TEXT("five actions end"), actions.Num(), 5);
            TestTrue(TEXT("Reload is one"), actions.Contains(T_Action_Reload.GetTag()));
        });

        It("the real carry service and the impact stub are registered", [this](){
            FInventoryTestFixture fixture;
            TestTrue(TEXT("R01's carry seam is UCarryServices"), Cast<UCarryServices>(fixture.services()->getSeam(UBPI_CarryService::StaticClass())) != nullptr);
            TestNotNull(TEXT("an impact sink is registered"), fixture.services()->getSeam(UBPI_ThrowImpactSink::StaticClass()));
        });
    });

    // ---- Spec 1 — slots and capacity (tier 1) ----
    Describe("Carry slots", [this](){
        It("fills one slot per unit of cost, refuses what the hands cannot take, and never takes an ordinary item", [this](){
            FInventoryTestFixture fixture;
            fixture.addHeavy(TEXT("Log"), 1);
            fixture.addHeavy(TEXT("Trunk"), 2);
            fixture.addStackable(TEXT("Stick"), 10);
            AInventoryTestPlayerState* player = nullptr;
            UCarryComponent* carry = makeCarrier(fixture, player);

            TestFalse(TEXT("hands start empty"), carry->isCarryActive());
            TestEqual(TEXT("no active slot"), carry->getActiveSlotIndex(), static_cast<int32>(INDEX_NONE));
            TestEqual(TEXT("full speed"), carry->getSpeedMultiplier(), 1.0f);
            TestTrue(TEXT("an ordinary item is not carried"), carry->beginCarry(makeUnit(TEXT("Stick"))).failReason == T_Carry_FailReason_NotCarryable);

            TestEqual(TEXT("the first log goes to slot 0"), carry->beginCarry(makeUnit(TEXT("Log"))).slotIndex, 0);
            TestTrue(TEXT("carrying"), carry->isCarryActive());
            TestTrue(TEXT("slower while carrying"), carry->getSpeedMultiplier() < 1.0f);
            TestTrue(TEXT("a trunk needs two free slots"), carry->beginCarry(makeUnit(TEXT("Trunk"))).failReason == T_Interaction_FailReason_HandsFull);
            TestEqual(TEXT("the second log goes to slot 1"), carry->beginCarry(makeUnit(TEXT("Log"))).slotIndex, 1);
            TestTrue(TEXT("a third log is refused with HandsFull"), carry->beginCarry(makeUnit(TEXT("Log"))).failReason == T_Interaction_FailReason_HandsFull);
            TestEqual(TEXT("two logs for a cost"), carry->getCarriedItemsForCost(TEXT("Log")), 2);
            TestEqual(TEXT("the log picked up last is active"), carry->getActiveSlotIndex(), 1);
            TestTrue(TEXT("R01's seam now answers from the real component"), fixture.services()->isCarryActive(player));
            TestTrue(TEXT("the shared refusal line is HandsFull"), UBFL_Carry::getHandsBlockReason(player) == T_Interaction_FailReason_HandsFull);

            carry->interruptCarry(T_Carry_InterruptCause_Downed);
            TestFalse(TEXT("no refusal line with empty hands"), UBFL_Carry::getHandsBlockReason(player).IsValid());
            TestEqual(TEXT("a trunk fills both slots"), carry->beginCarry(makeUnit(TEXT("Trunk"))).slotIndex, 0);
            TestEqual(TEXT("no slot is free"), carry->getFreeSlotCount(), 0);
            TestEqual(TEXT("but it is ONE trunk"), carry->getCarriedItemsForCost(TEXT("Trunk")), 1);
            TestTrue(TEXT("both slots share the carry id"), carry->getCarrySlots()[0].carryId == carry->getCarrySlots()[1].carryId);
        });

        It("refuses a Downed body, an oversized item, and empty hands of 0 slots; a carry ends the five actions once", [this](){
            FInventoryTestFixture fixture;
            fixture.addHeavy(TEXT("Log"), 1);
            fixture.addHeavy(TEXT("Trunk"), 2);
            AInventoryTestPlayerState* player = nullptr;
            UCarryComponent* carry = makeCarrier(fixture, player);
            UStubActionService* actions = fixture.stub<UStubActionService, UBPI_ActionService>();

            carry->beginCarry(makeUnit(TEXT("Log")));
            TestEqual(TEXT("the first carry ended five actions"), actions != nullptr ? actions->cancelledActions.Num() : -1, 5);
            carry->beginCarry(makeUnit(TEXT("Log")));
            TestEqual(TEXT("a second log ends nothing more"), actions != nullptr ? actions->cancelledActions.Num() : -1, 5);
            carry->interruptCarry(T_Carry_InterruptCause_Sleep);

            fixture.stub<UStubPlayerStateQuery, UBPI_PlayerStateQuery>()->incapacitatedPlayers.Add(player);
            TestTrue(TEXT("a Downed player cannot begin a carry (C17)"), carry->beginCarry(makeUnit(TEXT("Log"))).failReason == T_Interaction_FailReason_NotAllowedInState);
            fixture.stub<UStubPlayerStateQuery, UBPI_PlayerStateQuery>()->incapacitatedPlayers.Remove(player);

            setSlotCountOverride(1);
            TestEqual(TEXT("the cvar sets one slot"), carry->getSlotCount(), 1);
            TestTrue(TEXT("a trunk can never fit one slot"), carry->beginCarry(makeUnit(TEXT("Trunk"))).failReason == T_Interaction_FailReason_HandsFull);
            setSlotCountOverride(0);
            TestTrue(TEXT("0 slots take nothing"), carry->beginCarry(makeUnit(TEXT("Log"))).failReason == T_Interaction_FailReason_HandsFull);
            setSlotCountOverride(-1);
            TestEqual(TEXT("back to the project setting"), carry->getSlotCount(), 2);
        });
    });

    // ---- Spec 2 — ending a carry (tier 1) ----
    Describe("Ending a carry", [this](){
        It("drops a loose item, hands a placed or sent item to the caller, and frees every slot of a two-slot item", [this](){
            FInventoryTestFixture fixture;
            fixture.addHeavy(TEXT("Log"), 1);
            fixture.addHeavy(TEXT("Trunk"), 2);
            AInventoryTestPlayerState* player = nullptr;
            UCarryComponent* carry = makeCarrier(fixture, player);
            const UStubLooseItemSpawner* spawner = fixture.stub<UStubLooseItemSpawner, UBPI_LooseItemSpawner>();

            FS_InventoryEntry spoilingLog = makeUnit(TEXT("Log"));
            spoilingLog.spoilAtTime = 4242;
            carry->beginCarry(spoilingLog);
            TestTrue(TEXT("drop"), carry->endCarry(0, E_EndCarryReason::Dropped).succeeded());
            TestEqual(TEXT("one loose item"), spawner->spawnedEntries.Num(), 1);
            TestTrue(TEXT("it is the log"), spawner->spawnedEntries[0].itemId == FName(TEXT("Log")) && spawner->spawnedEntries[0].totalQuantity == 1);
            TestEqual(TEXT("the entry travelled through the hands unchanged"), spawner->spawnedEntries[0].spoilAtTime, static_cast<int64>(4242));
            TestFalse(TEXT("hands empty"), carry->isCarryActive());
            TestTrue(TEXT("an empty slot is refused"), carry->endCarry(0, E_EndCarryReason::Dropped).failReason == T_Carry_FailReason_SlotEmpty);
            TestTrue(TEXT("a slot that does not exist is refused"), carry->endCarry(7, E_EndCarryReason::Dropped).failReason == T_Carry_FailReason_SlotEmpty);

            carry->beginCarry(makeUnit(TEXT("Log")));
            carry->beginCarry(makeUnit(TEXT("Log")));
            carry->endCarry(0, E_EndCarryReason::Dropped);
            TestTrue(TEXT("dropping one of two keeps carrying"), carry->isCarryActive());
            carry->beginCarry(makeUnit(TEXT("Log")));
            TestEqual(TEXT("Building uses what is carried, never more"), carry->consumeCarriedForCost(TEXT("Log"), 3), 2);
            TestEqual(TEXT("a placed item is not dropped"), spawner->spawnedEntries.Num(), 2);

            carry->beginCarry(makeUnit(TEXT("Log")));
            const FS_CarryEndResult sent = carry->endCarry(0, E_EndCarryReason::Sent);
            TestTrue(TEXT("a sent item is handed to the caller"), sent.succeeded() && sent.entry.itemId == FName(TEXT("Log")));
            TestEqual(TEXT("and is not dropped either"), spawner->spawnedEntries.Num(), 2);

            carry->beginCarry(makeUnit(TEXT("Trunk")));
            TestTrue(TEXT("ending the second slot of a trunk"), carry->endCarry(1, E_EndCarryReason::Dropped).succeeded());
            TestEqual(TEXT("frees both slots"), carry->getFreeSlotCount(), 2);
            TestEqual(TEXT("and drops one trunk"), spawner->spawnedEntries.Num(), 3);
        });
    });

    // ---- Spec 3 — interrupts and "never saved" (tier 1) ----
    Describe("Interrupts", [this](){
        It("drops everything with the cause, loses nothing, and the component is not a save participant", [this](){
            FInventoryTestFixture fixture;
            fixture.addHeavy(TEXT("Log"), 1);
            fixture.addHeavy(TEXT("Trunk"), 2);
            AInventoryTestPlayerState* player = nullptr;
            UCarryComponent* carry = makeCarrier(fixture, player);
            const UStubLooseItemSpawner* spawner = fixture.stub<UStubLooseItemSpawner, UBPI_LooseItemSpawner>();

            TestEqual(TEXT("nothing to interrupt"), carry->interruptCarry(T_Carry_InterruptCause_Sleep), 0);
            carry->beginCarry(makeUnit(TEXT("Log")));
            carry->beginCarry(makeUnit(TEXT("Log")));
            TestEqual(TEXT("both are dropped"), carry->interruptCarry(T_Carry_InterruptCause_Downed), 2);
            TestEqual(TEXT("as loose items"), spawner->spawnedEntries.Num(), 2);
            TestTrue(TEXT("at the feet"), spawner->spawnLocations.Num() == 2 && spawner->spawnLocations[0].Equals(carry->GetOwner()->GetActorLocation()));
            TestFalse(TEXT("hands empty"), carry->isCarryActive());
            TestTrue(TEXT("the cause is remembered"), carry->getLastInterruptCause() == T_Carry_InterruptCause_Downed);
            TestTrue(TEXT("the freed slot says why"), carry->getCarrySlots()[0].lastEndReason == E_EndCarryReason::Interrupted && carry->getCarrySlots()[0].lastInterruptCause == T_Carry_InterruptCause_Downed);
            TestFalse(TEXT("carry state is never saved (B07 3.5)"), carry->GetClass()->ImplementsInterface(UBPI_SaveParticipant::StaticClass()));

            carry->beginCarry(makeUnit(TEXT("Trunk")));
            TestEqual(TEXT("a two-slot trunk is ONE thing dropped"), carry->interruptCarry(T_Carry_InterruptCause_Swim), 1);

            carry->beginCarry(makeUnit(TEXT("Log")));
            UBFL_Carry::handlePlayerLeft(player);
            TestTrue(TEXT("a leaving player drops with the cause PlayerLeft"), !carry->isCarryActive() && carry->getLastInterruptCause() == T_Carry_InterruptCause_PlayerLeft);

            carry->beginCarry(makeUnit(TEXT("Log")));
            const int32 droppedBefore = spawner->spawnedEntries.Num();
            carry->GetOwner()->Destroy();
            TestEqual(TEXT("a removed pawn drops the log instead of deleting it (safety net)"), spawner->spawnedEntries.Num(), droppedBefore + 1);
        });
    });

    // ---- Spec 4 — the real seam inside R01's grant (tier 2: two frameworks together) ----
    Describe("Heavy grant", [this](){
        It("goes to free slots, the surplus lands loose, and nothing enters the bag", [this](){
            FInventoryTestFixture fixture;
            fixture.addHeavy(TEXT("Log"), 1);
            AInventoryTestPlayerState* player = nullptr;
            UCarryComponent* carry = makeCarrier(fixture, player);
            const UStubLooseItemSpawner* spawner = fixture.stub<UStubLooseItemSpawner, UBPI_LooseItemSpawner>();

            FS_ItemStack stack;
            stack.itemId = TEXT("Log");
            stack.quantity = 3;
            const FS_GrantResult granted = UBFL_Inventory::grantOrDrop(player, stack);
            TestEqual(TEXT("two in the hands"), granted.quantityCarried, 2);
            TestEqual(TEXT("one on the ground"), granted.quantityDropped, 1);
            TestEqual(TEXT("none stored"), granted.quantityStored, 0);
            TestEqual(TEXT("the bag holds no log"), UPlayerInventoryComponent::findFor(player)->getPersonalInventory()->getTotalQuantity(TEXT("Log")), 0);
            TestEqual(TEXT("the component agrees"), carry->getCarriedItemsForCost(TEXT("Log")), 2);
            TestEqual(TEXT("one loose item was spawned"), spawner->spawnedEntries.Num(), 1);
        });
    });

    // ---- Spec 5 — throw (tier 2: the component, the pipeline actor and R01's loose-item seam) ----
    Describe("Throw", [this](){
        It("leaves the hands at once and lands as a loose item with the same entry", [this](){
            FInventoryTestFixture fixture;
            fixture.addHeavy(TEXT("Stone"), 1, 20.0f);
            AInventoryTestPlayerState* player = nullptr;
            UCarryComponent* carry = makeCarrier(fixture, player);
            const UStubLooseItemSpawner* spawner = fixture.stub<UStubLooseItemSpawner, UBPI_LooseItemSpawner>();

            TestTrue(TEXT("empty hands cannot throw"), carry->throwCarried(0, FVector::ForwardVector).failReason == T_Carry_FailReason_SlotEmpty);
            carry->beginCarry(makeUnit(TEXT("Stone")));
            TestTrue(TEXT("a zero direction is refused"), carry->throwCarried(0, FVector::ZeroVector).failReason == T_Carry_FailReason_InvalidThrow);
            TestTrue(TEXT("and the stone is still in the hands"), carry->isCarryActive());

            TestTrue(TEXT("throw"), carry->throwCarried(0, FVector::ForwardVector).succeeded());
            TestFalse(TEXT("hands empty at once"), carry->isCarryActive());
            TestEqual(TEXT("not a loose item while it flies"), spawner->spawnedEntries.Num(), 0);

            AThrownItemActor* thrown = findThrown(fixture);
            TestNotNull(TEXT("one item is in flight"), thrown);
            if(thrown != nullptr){
                TestTrue(TEXT("it carries the entry"), thrown->getEntry().itemId == FName(TEXT("Stone")));
                thrown->forceLand();
                TestEqual(TEXT("it landed as a loose item"), spawner->spawnedEntries.Num(), 1);
            }
        });

        It("a sink that says 'do not land' keeps the item from landing", [this](){
            FInventoryTestFixture fixture;
            fixture.addHeavy(TEXT("Stone"), 1, 20.0f);
            const UStubLooseItemSpawner* spawner = fixture.stub<UStubLooseItemSpawner, UBPI_LooseItemSpawner>();
            UStubThrowImpactSink* sink = fixture.stub<UStubThrowImpactSink, UBPI_ThrowImpactSink>();
            TestNotNull(TEXT("the stub impact sink is registered"), sink);
            if(sink == nullptr){
                return;
            }
            sink->letItemLand = false;
            FS_InventoryEntry stone = makeUnit(TEXT("Stone"));
            // The thrower and the target are two plain actors: only the hit matters here.
            AActor* thrower = fixture.world()->SpawnActor<AStubCarryableActor>();
            AActor* target = fixture.world()->SpawnActor<AStubCarryableActor>(FVector(500.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
            AThrownItemActor* thrown = UBFL_Carry::throwEntry(thrower, stone, FVector::ZeroVector, FVector::ForwardVector, 900.0f, 20.0f);
            TestNotNull(TEXT("the pipeline spawned the flight"), thrown);
            if(thrown == nullptr){
                return;
            }
            thrown->forceLand();   // no hit: lands
            TestEqual(TEXT("a flight that hits nothing still lands"), spawner->spawnedEntries.Num(), 1);

            AThrownItemActor* second = UBFL_Carry::throwEntry(thrower, stone, FVector::ZeroVector, FVector::ForwardVector, 900.0f, 20.0f);
            UProjectileMovementComponent* movement = second != nullptr ? second->FindComponentByClass<UProjectileMovementComponent>() : nullptr;
            if(movement != nullptr){
                // Simulate the stop on the target through the projectile's own event, as the engine raises it.
                const FHitResult hit(target, nullptr, target->GetActorLocation(), FVector::BackwardVector);
                movement->OnProjectileStop.Broadcast(hit);
            }
            TestEqual(TEXT("the sink saw the target"), sink->hitTargets.Num(), 1);
            TestEqual(TEXT("with the impact damage"), sink->totalDamage, 20.0f);
            TestEqual(TEXT("and nothing landed"), spawner->spawnedEntries.Num(), 1);
        });
    });

    // ---- Spec 6 — carrying an actor (tier 2) ----
    Describe("Actor carry", [this](){
        It("fills slots like an item, can only be put down, and is told when it is", [this](){
            FInventoryTestFixture fixture;
            fixture.addHeavy(TEXT("Log"), 1);
            AInventoryTestPlayerState* player = nullptr;
            UCarryComponent* carry = makeCarrier(fixture, player);
            const UStubLooseItemSpawner* spawner = fixture.stub<UStubLooseItemSpawner, UBPI_LooseItemSpawner>();
            AStubCarryableActor* dummy = fixture.world()->SpawnActor<AStubCarryableActor>();

            TestTrue(TEXT("an actor without the interface is not carryable"), carry->beginCarryActor(fixture.world()->SpawnActor<AActor>()).failReason == T_Carry_FailReason_NotCarryable);
            TestTrue(TEXT("carry the dummy"), carry->beginCarryActor(dummy).succeeded());
            TestTrue(TEXT("carrying"), carry->isCarryActive() && carry->isActorCarry(0));
            TestEqual(TEXT("the actor was told"), dummy->beganCount, 1);
            TestTrue(TEXT("it is attached to the carrier"), dummy->GetAttachParentActor() == carry->GetOwner());
            TestEqual(TEXT("it is never a building material"), carry->getCarriedItemsForCost(TEXT("Dummy")), 0);
            TestTrue(TEXT("it cannot be thrown"), carry->throwCarried(0, FVector::ForwardVector).failReason == T_Carry_FailReason_CannotThrow);
            TestTrue(TEXT("it cannot be sent"), carry->endCarry(0, E_EndCarryReason::Sent).failReason == T_Interaction_FailReason_NotAllowedInState);
            TestTrue(TEXT("it cannot be placed"), carry->endCarry(0, E_EndCarryReason::Placed).failReason == T_Interaction_FailReason_NotAllowedInState);
            TestTrue(TEXT("still carried after the refusals"), carry->isCarryActive());

            AInventoryTestPlayerState* otherPlayer = nullptr;
            UCarryComponent* otherCarry = makeCarrier(fixture, otherPlayer);
            TestTrue(TEXT("a body in somebody's hands cannot be taken"), otherCarry->beginCarryActor(dummy).failReason == T_Carry_FailReason_NotCarryable);

            TestTrue(TEXT("put it down"), carry->endCarry(0, E_EndCarryReason::Dropped).succeeded());
            TestEqual(TEXT("the actor was told why"), dummy->endedCount, 1);
            TestTrue(TEXT("with the reason"), dummy->lastReason == E_EndCarryReason::Dropped);
            TestTrue(TEXT("it is detached"), dummy->GetAttachParentActor() == nullptr);
            TestEqual(TEXT("no loose item: it is itself the object"), spawner->spawnedEntries.Num(), 0);

            carry->beginCarry(makeUnit(TEXT("Log")));
            dummy->slotCost = 2;
            TestTrue(TEXT("a two-slot body with one hand full is HandsFull"), carry->beginCarryActor(dummy).failReason == T_Interaction_FailReason_HandsFull);
            carry->interruptCarry(T_Carry_InterruptCause_Downed);

            dummy->slotCost = 1;
            carry->beginCarryActor(dummy);
            dummy->Destroy();
            TestFalse(TEXT("a carried actor destroyed by something else frees its slot"), carry->isCarryActive());
        });
    });
}

#endif
