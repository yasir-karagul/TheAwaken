// Doc R02 guide CR9.5: the automation specs of the Crafting framework (the ten Doc B22 starter tests), plus the
// automatable Verify rows of Sections 1, 4, 5 and 7 that the ten do not cover.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NativeGameplayTags.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestCharacter.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/Stubs/InventoryStubs.h"
#include "ItemInventoryFramework/03Interfaces/BPI_PlayerStateQuery.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "CraftingFramework/CraftingProgressionBridge.h"
#include "CraftingFramework/CraftingServices.h"
#include "CraftingFramework/UBFL_Crafting.h"
#include "CraftingFramework/UBFL_CraftingValidation.h"
#include "CraftingFramework/UCraftingComponent.h"
#include "CraftingFramework/UCraftingRecipeSubsystem.h"
#include "CraftingFramework/UCraftingStationComponent.h"
#include "CraftingFramework/UDA_CraftingRecipe.h"
#include "CraftingFramework/02Tags/T_CraftingFramework.h"
#include "CraftingFramework/03Interfaces/BPI_ProgressionQuery.h"
#include "CraftingFramework/Stubs/CraftingStubs.h"

// A blueprint tag that exists only for these tests.
UE_DEFINE_GAMEPLAY_TAG_STATIC(T_Test_Recipe_Unlock, "Progression.Recipe.AutomationTest");

namespace CraftingSpecHelpers{
    UCraftingRecipeSubsystem* recipesOf(const FInventoryTestFixture& fixture){
        return fixture.world()->GetGameInstance()->GetSubsystem<UCraftingRecipeSubsystem>();
    }

    // The real default is the bridge to Doc R06; tests answer unlocks from the local stub instead.
    UStubProgressionQuery* installProgressionStub(const FInventoryTestFixture& fixture){
        UStubProgressionQuery* stub = NewObject<UStubProgressionQuery>(fixture.services());
        fixture.services()->registerSeam(UBPI_ProgressionQuery::StaticClass(), stub);
        return stub;
    }

    FS_CraftingRecipeRequirement need(FName itemId, int32 quantity){
        FS_CraftingRecipeRequirement requirement;
        requirement.itemId = itemId;
        requirement.quantity = quantity;
        return requirement;
    }

    // A recipe that exists only in this fixture's recipe catalog. craftSeconds stays 0: it completes at once.
    UDA_CraftingRecipe* addRecipe(const FInventoryTestFixture& fixture, FName recipeId, const TArray<FS_CraftingRecipeRequirement>& requirements, FName outputItemId){
        UCraftingRecipeSubsystem* recipes = recipesOf(fixture);
        UDA_CraftingRecipe* recipe = NewObject<UDA_CraftingRecipe>(recipes);
        recipe->recipeId = recipeId;
        recipe->requirementList = requirements;
        recipe->outputItemId = outputItemId;
        recipes->registerRecipe(recipe);
        return recipe;
    }

    // The items and the three recipes of the designer's worked example 2 (R02 3.5).
    void addExampleContent(FInventoryTestFixture& fixture){
        recipesOf(fixture)->resetForTest();   // only the recipes this test adds
        installProgressionStub(fixture);
        fixture.addStackable(TEXT("Stick"), 50);
        fixture.addStackable(TEXT("Rock"), 50);
        fixture.addStackable(TEXT("Hatchling"), 50);
        fixture.addStackable(TEXT("Arrow"), 50);
        fixture.addInstanced(TEXT("Spear"), 5, 50.0f);
        fixture.addInstanced(TEXT("StoneSpear"), 5, 80.0f);
        addRecipe(fixture, TEXT("Hatchling"), {need(TEXT("Stick"), 1), need(TEXT("Rock"), 1)}, TEXT("Hatchling"));
        addRecipe(fixture, TEXT("Arrow"), {need(TEXT("Stick"), 2), need(TEXT("Rock"), 2)}, TEXT("Arrow"));
        addRecipe(fixture, TEXT("SpearUpgrade"), {need(TEXT("Spear"), 1), need(TEXT("Stick"), 1)}, TEXT("StoneSpear"));
    }

    // A player with a bag and a character, and that character's crafting component.
    UCraftingComponent* makeCrafter(FInventoryTestFixture& fixture, AInventoryTestPlayerState*& outPlayer, UInventoryData*& outBag){
        outPlayer = fixture.makePlayer();
        fixture.makeCharacterFor(outPlayer);
        outBag = UPlayerInventoryComponent::findFor(outPlayer)->getPersonalInventory();
        return UCraftingComponent::findFor(outPlayer);
    }

    // Puts one spear with that durability into the bag and returns its instance id.
    FGuid giveSpear(const FInventoryTestFixture& fixture, AInventoryTestPlayerState* player, UInventoryData* bag, float durability){
        FS_InventoryEntry spear = UBFL_Inventory::makeFreshInstance(player, *fixture.catalog()->getDefinition(TEXT("Spear")));
        spear.state.durability = durability;
        bag->addInstance(spear);
        return spear.instanceId;
    }

    FS_ItemRef item(FName itemId){
        FS_ItemRef ref;
        ref.itemId = itemId;
        return ref;
    }

    // The button of that recipe, or null when the recipe is not listed.
    const FS_AvailableRecipeOption* findOption(const TArray<FS_AvailableRecipeOption>& list, FName recipeId){
        return list.FindByPredicate([recipeId](const FS_AvailableRecipeOption& option){
            return option.recipeId == recipeId;
        });
    }

    bool hasState(const TArray<FS_AvailableRecipeOption>& list, FName recipeId, E_RecipeListState state){
        const FS_AvailableRecipeOption* option = findOption(list, recipeId);
        return option != nullptr && option->state == state;
    }
}

using namespace CraftingSpecHelpers;

BEGIN_DEFINE_SPEC(FCraftingSpec, "Awaken.Crafting", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FCraftingSpec)

void FCraftingSpec::Define(){
    BeforeEach([this](){
        FInventoryTestFixture::expectForeignStartupErrors(*this);
    });

    // ---- Section 1 Verify 3 and 4 — defaults and the seam (tier 1) ----
    Describe("Defaults", [this](){
        It("fresh structs, the default progression bridge, and an empty tag is always unlocked", [this](){
            FInventoryTestFixture fixture;
            const FS_AvailableRecipeOption option;
            TestTrue(TEXT("a fresh option is red"), option.state == E_RecipeListState::Missing);
            TestEqual(TEXT("and misses nothing"), option.getMissingCount(), 0);
            TestFalse(TEXT("a fresh session is closed"), FS_CraftingSessionState().isOpen);
            TestNotNull(TEXT("the R06 bridge is registered"), Cast<UCraftingProgressionBridge>(fixture.services()->getSeam(UBPI_ProgressionQuery::StaticClass())));
            const UCraftingServices* services = UCraftingServices::get(fixture.world());
            TestTrue(TEXT("an empty tag is unlocked"), services != nullptr && services->isUnlocked(FGameplayTag()));
            TestFalse(TEXT("a real tag is locked with no Progression component"), services != nullptr && services->isUnlocked(T_Test_Recipe_Unlock));
            installProgressionStub(fixture)->unlockedTags.AddTag(T_Test_Recipe_Unlock);
            TestTrue(TEXT("the stub answers once told"), services != nullptr && services->isUnlocked(T_Test_Recipe_Unlock));
        });
    });

    // ---- Doc B22 "Craft button states" (tier 1) ----
    Describe("Craft button states", [this](){
        It("1 stick and 1 rock: hatchling green, arrow red, and the server refuses the red one", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 10);
            bag->addStack(TEXT("Rock"), 10);

            TestEqual(TEXT("an empty crafting area lists nothing"), crafting->getAvailableRecipes().Num(), 0);
            crafting->stageIngredient(item(TEXT("Stick")), 1);
            crafting->stageIngredient(item(TEXT("Rock")), 1);
            const TArray<FS_AvailableRecipeOption> list = crafting->getAvailableRecipes();
            TestTrue(TEXT("hatchling is green"), hasState(list, TEXT("Hatchling"), E_RecipeListState::Craftable));
            TestTrue(TEXT("arrow is red"), hasState(list, TEXT("Arrow"), E_RecipeListState::Missing));
            TestTrue(TEXT("spear upgrade is red"), hasState(list, TEXT("SpearUpgrade"), E_RecipeListState::Missing));
            TestTrue(TEXT("green comes first"), list.Num() == 3 && list[0].recipeId == FName(TEXT("Hatchling")));
            TestTrue(TEXT("RequestCraft(arrow) is refused"), crafting->requestCraft(TEXT("Arrow")).failReason == T_Crafting_FailReason_InsufficientStaged);
            TestEqual(TEXT("and nothing was consumed"), bag->getTotalQuantity(TEXT("Stick")), 10);
        });
    });

    // ---- Doc B22 "Craft leftovers" (tier 1) ----
    Describe("Craft leftovers", [this](){
        It("3 sticks and 4 rocks: craft arrow, 1 stick and 2 rocks stay staged, hatchling green, arrow red", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 10);
            bag->addStack(TEXT("Rock"), 10);
            crafting->stageIngredient(item(TEXT("Stick")), 3);
            crafting->stageIngredient(item(TEXT("Rock")), 4);

            TArray<FS_AvailableRecipeOption> list = crafting->getAvailableRecipes();
            TestTrue(TEXT("both are green"), hasState(list, TEXT("Hatchling"), E_RecipeListState::Craftable) && hasState(list, TEXT("Arrow"), E_RecipeListState::Craftable));
            TestTrue(TEXT("craft arrow"), crafting->requestCraft(TEXT("Arrow")).succeeded());
            TestEqual(TEXT("one arrow was made"), bag->getTotalQuantity(TEXT("Arrow")), 1);
            TestEqual(TEXT("one recipe's worth of sticks left the bag"), bag->getTotalQuantity(TEXT("Stick")), 8);
            TestEqual(TEXT("1 stick stays staged"), crafting->getStagedQuantity(TEXT("Stick")), 1);
            TestEqual(TEXT("2 rocks stay staged"), crafting->getStagedQuantity(TEXT("Rock")), 2);
            list = crafting->getAvailableRecipes();
            TestTrue(TEXT("hatchling is still green"), hasState(list, TEXT("Hatchling"), E_RecipeListState::Craftable));
            TestTrue(TEXT("arrow is now red"), hasState(list, TEXT("Arrow"), E_RecipeListState::Missing));
        });
    });

    // ---- Doc B22 "Extra item refusal" (tier 1) ----
    Describe("Extra item refusal", [this](){
        It("stick, rock and spear: hatchling and spear upgrade are white, and the server refuses a white one", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 10);
            bag->addStack(TEXT("Rock"), 10);
            giveSpear(fixture, player, bag, 50.0f);
            crafting->stageIngredient(item(TEXT("Stick")), 1);
            crafting->stageIngredient(item(TEXT("Rock")), 1);
            crafting->stageIngredient(item(TEXT("Spear")), 1);

            const TArray<FS_AvailableRecipeOption> list = crafting->getAvailableRecipes();
            TestTrue(TEXT("hatchling is white"), hasState(list, TEXT("Hatchling"), E_RecipeListState::ExtraItems));
            TestTrue(TEXT("spear upgrade is white"), hasState(list, TEXT("SpearUpgrade"), E_RecipeListState::ExtraItems));
            TestTrue(TEXT("arrow is red: red wins over white"), hasState(list, TEXT("Arrow"), E_RecipeListState::Missing));
            const FS_AvailableRecipeOption* hatchling = findOption(list, TEXT("Hatchling"));
            TestTrue(TEXT("the extra item is named"), hatchling != nullptr && hatchling->extraItems.Contains(FName(TEXT("Spear"))));
            // A client that claims the button was green sends the same request: the server checks again.
            TestTrue(TEXT("refused with ExtraIngredients"), crafting->requestCraft(TEXT("Hatchling")).failReason == T_Crafting_FailReason_ExtraIngredients);
            TestEqual(TEXT("nothing was made"), bag->getTotalQuantity(TEXT("Hatchling")), 0);
        });
    });

    // ---- Doc B22 "Craft list cap and order" (tier 1) ----
    Describe("Craft list cap and order", [this](){
        It("20 red candidates show 15, fewest missing first; 16 green show all 16 and no red", [this](){
            FInventoryTestFixture fixture;
            recipesOf(fixture)->resetForTest();
            installProgressionStub(fixture);
            fixture.addStackable(TEXT("Stick"), 50);
            fixture.addStackable(TEXT("Rock"), 50);
            fixture.addStackable(TEXT("Thing"), 50);
            // 20 recipes that all use a stick and need 1 to 20 rocks. Registered in reverse to prove the order is by rule.
            for(int32 rocks = 20; rocks >= 1; rocks--){
                addRecipe(fixture, FName(*FString::Printf(TEXT("Red%02d"), rocks)), {need(TEXT("Stick"), 1), need(TEXT("Rock"), rocks)}, TEXT("Thing"));
            }
            TMap<FName, int32> staged;
            staged.Add(TEXT("Stick"), 1);
            TArray<FS_AvailableRecipeOption> list = UBFL_Crafting::buildRecipeList(fixture.world(), staged, FGameplayTagContainer());
            TestEqual(TEXT("the cap is 15"), list.Num(), 15);
            TestTrue(TEXT("fewest missing first"), list.Num() == 15 && list[0].recipeId == FName(TEXT("Red01")) && list[14].recipeId == FName(TEXT("Red15")));

            // 16 recipes that need only a stick: all green.
            for(int32 index = 1; index <= 16; index++){
                addRecipe(fixture, FName(*FString::Printf(TEXT("Green%02d"), index)), {need(TEXT("Stick"), 1)}, TEXT("Thing"));
            }
            list = UBFL_Crafting::buildRecipeList(fixture.world(), staged, FGameplayTagContainer());
            TestEqual(TEXT("all 16 green are listed, above the cap"), list.Num(), 16);
            TestFalse(TEXT("and no red is shown"), list.ContainsByPredicate([](const FS_AvailableRecipeOption& option){
                return option.state == E_RecipeListState::Missing;
            }));
        });
    });

    // ---- Doc B22 "Locked blueprint" (tier 1) ----
    Describe("Locked blueprint", [this](){
        It("a locked recipe is never listed, even when fully satisfied, and RequestCraft is refused", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            const_cast<UDA_CraftingRecipe*>(recipesOf(fixture)->getRecipe(TEXT("SpearUpgrade")))->requiredBlueprintTag = T_Test_Recipe_Unlock;
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 10);
            giveSpear(fixture, player, bag, 50.0f);
            crafting->stageIngredient(item(TEXT("Stick")), 1);
            crafting->stageIngredient(item(TEXT("Spear")), 1);

            TestTrue(TEXT("not in the list"), findOption(crafting->getAvailableRecipes(), TEXT("SpearUpgrade")) == nullptr);
            TestTrue(TEXT("refused with BlueprintLocked"), crafting->requestCraft(TEXT("SpearUpgrade")).failReason == T_Crafting_FailReason_BlueprintLocked);

            fixture.stub<UStubProgressionQuery, UBPI_ProgressionQuery>()->unlockedTags.AddTag(T_Test_Recipe_Unlock);
            TestTrue(TEXT("unlocked: listed and green"), hasState(crafting->getAvailableRecipes(), TEXT("SpearUpgrade"), E_RecipeListState::Craftable));
        });
    });

    // ---- Doc B22 "Stage and unstage amounts" (tier 1) ----
    Describe("Stage and unstage amounts", [this](){
        It("staging reserves without removing, opens the session at the first unit, and closing returns everything", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 10);

            TestFalse(TEXT("no session before the first staged ingredient"), crafting->isSessionOpen());
            TestTrue(TEXT("one unit"), crafting->stageIngredient(item(TEXT("Stick")), 1).succeeded());
            TestTrue(TEXT("the session is open"), crafting->isSessionOpen());
            TestEqual(TEXT("staged 1"), crafting->getStagedQuantity(TEXT("Stick")), 1);
            TestEqual(TEXT("the bag still holds all 10"), bag->getTotalQuantity(TEXT("Stick")), 10);
            TestEqual(TEXT("9 are available"), bag->getAvailableQuantity(TEXT("Stick")), 9);
            // "The whole stack" is what the request passes after the server counted the available units.
            TestTrue(TEXT("the rest of the stack"), crafting->stageIngredient(item(TEXT("Stick")), bag->getAvailableQuantity(TEXT("Stick"))).succeeded());
            TestEqual(TEXT("staged 10"), crafting->getStagedQuantity(TEXT("Stick")), 10);
            TestFalse(TEXT("an eleventh is refused"), crafting->stageIngredient(item(TEXT("Stick")), 1).succeeded());

            TestTrue(TEXT("a click returns one unit"), crafting->unstageIngredient(item(TEXT("Stick")), 1).succeeded());
            TestEqual(TEXT("staged 9"), crafting->getStagedQuantity(TEXT("Stick")), 9);
            crafting->unstageIngredient(item(TEXT("Stick")), 9);
            TestEqual(TEXT("staged 0"), crafting->getStagedQuantity(TEXT("Stick")), 0);
            TestTrue(TEXT("an empty crafting area does not end the session"), crafting->isSessionOpen());

            crafting->stageIngredient(item(TEXT("Stick")), 4);
            crafting->closeCraftingSession(E_CraftingCloseReason::BagClosed);
            TestFalse(TEXT("closed"), crafting->isSessionOpen());
            TestEqual(TEXT("everything is available again"), bag->getAvailableQuantity(TEXT("Stick")), 10);
        });
    });

    // ---- Doc B22 "Station recipe" (tier 2: the session, the station component and the rule together) ----
    Describe("Station recipe", [this](){
        It("is red away from the station, green next to it, and the station takes one player at a time", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            const_cast<UDA_CraftingRecipe*>(recipesOf(fixture)->getRecipe(TEXT("Arrow")))->requiredStationTag = T_Station_CraftingTable;
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 10);
            bag->addStack(TEXT("Rock"), 10);
            crafting->stageIngredient(item(TEXT("Stick")), 2);
            crafting->stageIngredient(item(TEXT("Rock")), 2);

            const FS_AvailableRecipeOption* away = findOption(crafting->getAvailableRecipes(), TEXT("Arrow"));
            TestTrue(TEXT("red away from the station, and the card says why"), away != nullptr && away->state == E_RecipeListState::Missing && away->isStationMissing);
            TestTrue(TEXT("refused with StationRequired"), crafting->requestCraft(TEXT("Arrow")).failReason == T_Crafting_FailReason_StationRequired);
            crafting->closeCraftingSession(E_CraftingCloseReason::BagClosed);

            AStubCraftingStation* table = fixture.world()->SpawnActor<AStubCraftingStation>();
            table->stationComponent->stationTags.AddTag(T_Station_CraftingTable);
            TestTrue(TEXT("enter the station"), crafting->enterStation(table->stationComponent).succeeded());
            TestTrue(TEXT("the station is in use"), table->stationComponent->isInUse());
            TestFalse(TEXT("entering starts no session"), crafting->isSessionOpen());

            AInventoryTestPlayerState* otherPlayer = nullptr;
            UInventoryData* otherBag = nullptr;
            UCraftingComponent* other = makeCrafter(fixture, otherPlayer, otherBag);
            TestTrue(TEXT("a second player is refused"), other->enterStation(table->stationComponent).failReason == T_Interaction_FailReason_InUse);

            crafting->stageIngredient(item(TEXT("Stick")), 2);
            crafting->stageIngredient(item(TEXT("Rock")), 2);
            TestTrue(TEXT("the reservation is keyed by the station id"), bag->getReservations().Num() > 0 && bag->getReservations()[0].owningStationId == table->stationComponent->getStationId());
            TestTrue(TEXT("green next to the station"), hasState(crafting->getAvailableRecipes(), TEXT("Arrow"), E_RecipeListState::Craftable));
            TestTrue(TEXT("and it can be made"), crafting->requestCraft(TEXT("Arrow")).succeeded());

            crafting->stageIngredient(item(TEXT("Rock")), 2);
            table->Destroy();   // the station is destroyed (R02 3.8)
            TestFalse(TEXT("the session ended with the station"), crafting->isSessionOpen());
            TestFalse(TEXT("and the station context with it"), crafting->getSessionState().hasStation);
            TestEqual(TEXT("nothing is still reserved"), bag->getAvailableQuantity(TEXT("Rock")), bag->getTotalQuantity(TEXT("Rock")));
        });
    });

    // ---- Section 4 / 7 Verify — station id, forceRelease, pawn removed (tier 2) ----
    Describe("Station lifecycle", [this](){
        It("keeps a given id, frees itself on forceRelease and when the user's pawn is removed", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            AStubCraftingStation* table = fixture.world()->SpawnActor<AStubCraftingStation>();
            table->stationComponent->stationTags.AddTag(T_Station_CraftingTable);
            TestTrue(TEXT("an id was made at BeginPlay"), table->stationComponent->getStationId().IsValid());
            TestFalse(TEXT("not in use"), table->stationComponent->isInUse());
            const FGuid pieceId = FGuid::NewGuid();
            table->stationComponent->setStationId(pieceId);
            TestTrue(TEXT("Building's id is kept"), table->stationComponent->getStationId() == pieceId);

            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Rock"), 5);
            crafting->enterStation(table->stationComponent);
            crafting->stageIngredient(item(TEXT("Rock")), 2);
            table->stationComponent->forceRelease(E_CraftingCloseReason::OutOfRange);
            TestFalse(TEXT("OutOfRange ends the session"), crafting->isSessionOpen());
            TestFalse(TEXT("and frees the station"), table->stationComponent->isInUse());
            TestEqual(TEXT("the rocks are available"), bag->getAvailableQuantity(TEXT("Rock")), 5);

            crafting->enterStation(table->stationComponent);
            crafting->stageIngredient(item(TEXT("Rock")), 3);
            crafting->GetOwner()->Destroy();   // the pawn goes away and nobody closed the session first (K12)
            TestFalse(TEXT("the station is free again"), table->stationComponent->isInUse());
            TestEqual(TEXT("the reservations were released"), bag->getAvailableQuantity(TEXT("Rock")), 5);
        });
    });

    // ---- Doc B22 "Damage with the bag open" (tier 2: Crafting, R01's reservations and the player-state seam) ----
    Describe("Damage with the bag open", [this](){
        It("a hit changes nothing; Downed closes the session, cancels a running craft and consumes nothing", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            const_cast<UDA_CraftingRecipe*>(recipesOf(fixture)->getRecipe(TEXT("Hatchling")))->craftSeconds = 5.0f;
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 10);
            bag->addStack(TEXT("Rock"), 10);
            crafting->stageIngredient(item(TEXT("Stick")), 1);
            crafting->stageIngredient(item(TEXT("Rock")), 1);

            fixture.services()->damagePlayer(player, 10.0f, FGameplayTag());   // a hit that does not Down the player
            TestTrue(TEXT("the session stays"), crafting->isSessionOpen());
            TestEqual(TEXT("the reservation stays"), crafting->getStagedQuantity(TEXT("Stick")), 1);

            TestTrue(TEXT("a craft starts"), crafting->requestCraft(TEXT("Hatchling")).succeeded());
            TestTrue(TEXT("and is running"), crafting->isCraftInProgress());
            TestTrue(TEXT("a second press is refused"), crafting->requestCraft(TEXT("Hatchling")).failReason == T_Crafting_FailReason_CraftInProgress);
            TestTrue(TEXT("staging is refused while it runs"), crafting->stageIngredient(item(TEXT("Stick")), 1).failReason == T_Crafting_FailReason_CraftInProgress);
            TestTrue(TEXT("unstaging is refused while it runs"), crafting->unstageIngredient(item(TEXT("Stick")), 1).failReason == T_Crafting_FailReason_CraftInProgress);

            // Health reached zero: Doc B02 marks the player and closes the session.
            fixture.stub<UStubPlayerStateQuery, UBPI_PlayerStateQuery>()->incapacitatedPlayers.Add(player);
            UBFL_Crafting::closeSession(crafting->GetOwner(), E_CraftingCloseReason::Downed);
            TestFalse(TEXT("the bag closed"), crafting->isSessionOpen());
            TestFalse(TEXT("the craft was cancelled"), crafting->isCraftInProgress());
            TestEqual(TEXT("nothing was made"), bag->getTotalQuantity(TEXT("Hatchling")), 0);
            TestEqual(TEXT("nothing was consumed"), bag->getTotalQuantity(TEXT("Stick")), 10);
            TestEqual(TEXT("every reservation was released"), bag->getAvailableQuantity(TEXT("Stick")), 10);
            TestTrue(TEXT("a Downed player cannot stage"), crafting->stageIngredient(item(TEXT("Stick")), 1).failReason == T_Interaction_FailReason_NotAllowedInState);
        });
    });

    // ---- Section 5 Verify — who may craft at all (tier 2) ----
    Describe("Bag refused", [this](){
        It("an unknown recipe, an empty crafting area and a Bagless player are each refused with their reason", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 5);
            TestTrue(TEXT("closing with nothing open does nothing"), !crafting->isSessionOpen());
            crafting->closeCraftingSession(E_CraftingCloseReason::BagClosed);
            TestTrue(TEXT("an unknown recipe is NotFound"), crafting->requestCraft(TEXT("NoSuchRecipe")).failReason == T_Inventory_FailReason_NotFound);
            TestTrue(TEXT("nothing staged is InsufficientStaged"), crafting->requestCraft(TEXT("Hatchling")).failReason == T_Crafting_FailReason_InsufficientStaged);
            UPlayerInventoryComponent::findFor(player)->releaseInventory();
            TestTrue(TEXT("Bagless is BagRequired"), crafting->stageIngredient(item(TEXT("Stick")), 1).failReason == T_Crafting_FailReason_BagRequired);
        });
    });

    // ---- Weapon upgrade, R02 3.10 (tier 2) ----
    Describe("Weapon upgrade", [this](){
        It("uses the most worn spear and makes the new weapon at full condition", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 10);
            const FGuid goodSpear = giveSpear(fixture, player, bag, 40.0f);
            const FGuid wornSpear = giveSpear(fixture, player, bag, 12.0f);

            crafting->stageIngredient(item(TEXT("Stick")), 1);
            TestTrue(TEXT("stage a spear by its type"), crafting->stageIngredient(item(TEXT("Spear")), 1).succeeded());
            TestTrue(TEXT("the most worn one was staged"), bag->findInstance(wornSpear) != nullptr && bag->findInstance(wornSpear)->reservedQuantity == 1);
            TestTrue(TEXT("craft the upgrade"), crafting->requestCraft(TEXT("SpearUpgrade")).succeeded());
            TestTrue(TEXT("the worn spear is gone"), bag->findInstance(wornSpear) == nullptr);
            TestTrue(TEXT("the good spear is untouched"), bag->findInstance(goodSpear) != nullptr && bag->findInstance(goodSpear)->reservedQuantity == 0);

            const FS_InventoryEntry* upgraded = bag->getEntries().FindByPredicate([](const FS_InventoryEntry& entry){
                return entry.itemId == FName(TEXT("StoneSpear"));
            });
            TestNotNull(TEXT("a stone spear was made"), upgraded);
            if(upgraded != nullptr){
                TestEqual(TEXT("at full condition, not the old one's 12"), upgraded->state.durability, 80.0f);
            }
        });
    });

    // ---- Doc B22 "No firearm recipe" (tier 1) ----
    Describe("No firearm recipe", [this](){
        It("the data check rejects a recipe that makes a firearm, its ammunition, or uses an unknown item", [this](){
            FInventoryTestFixture fixture;
            recipesOf(fixture)->resetForTest();
            fixture.addStackable(TEXT("Scrap"), 50);
            fixture.addStackable(TEXT("Bullet"), 50);
            UDA_ItemDefinition* pistol = fixture.addInstanced(TEXT("Pistol"), 2, 100.0f);
            pistol->weaponType = E_WeaponType::Firearm;
            pistol->ammoItemId = TEXT("Bullet");

            TArray<FString> errors;
            addRecipe(fixture, TEXT("GoodRecipe"), {need(TEXT("Scrap"), 2)}, TEXT("Scrap"));
            TestEqual(TEXT("a normal recipe passes"), UBFL_CraftingValidation::validateRecipes(fixture.world(), errors), 0);

            addRecipe(fixture, TEXT("MakePistol"), {need(TEXT("Scrap"), 5)}, TEXT("Pistol"));
            addRecipe(fixture, TEXT("MakeBullet"), {need(TEXT("Scrap"), 1)}, TEXT("Bullet"));
            addRecipe(fixture, TEXT("Typo"), {need(TEXT("Scrapp"), 1)}, TEXT("Scrap"));
            errors.Reset();
            TestEqual(TEXT("three errors: the firearm, the ammunition, the unknown ingredient"), UBFL_CraftingValidation::validateRecipes(fixture.world(), errors), 3);
        });
    });
}

#endif
