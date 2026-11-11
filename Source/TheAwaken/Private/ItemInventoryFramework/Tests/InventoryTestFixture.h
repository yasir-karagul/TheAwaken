#pragma once

#include "CoreMinimal.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ItemInventoryFramework/InventoryServices.h"

class AInventoryTestPlayerState;
class UDA_ItemDefinition;
class UGameInstance;
class UInventoryData;
class UInventoryRegistrySubsystem;
class UItemCatalogSubsystem;
class UWorld;

// A private game world for one automated test. Everything it creates is destroyed with it.
struct FInventoryTestFixture{
    FInventoryTestFixture();
    ~FInventoryTestFixture();

    // Doc B21's subsystems log an Error at every game-instance start while DA_EnvironmentSettings_Default does not exist.
    // That is another framework's missing content, not an inventory failure: while the asset is missing, a spec marks
    // those exact lines as expected (call it in BeforeEach). Once the asset exists, this adds nothing.
    static void expectForeignStartupErrors(class FAutomationTestBase& test);

    UWorld* world() const;
    UInventoryServices* services() const;
    UItemCatalogSubsystem* catalog() const;
    UInventoryRegistrySubsystem* registry() const;

    // The stub that stands in for a framework, so a test can read what was asked of it.
    template<typename StubClass, typename InterfaceClass>
    StubClass* stub() const{
        return Cast<StubClass>(services()->getSeam(InterfaceClass::StaticClass()));
    }

    // Item definitions that exist only in this fixture's catalog.
    UDA_ItemDefinition* addStackable(FName itemId, int32 limit);
    UDA_ItemDefinition* addInstanced(FName itemId, int32 limit, float durability);

    // A player with a character id and an empty bag.
    AInventoryTestPlayerState* makePlayer();
    // A world container without an actor: the data only.
    UInventoryData* makeContainer(const FS_ContainerRules& rules);

    // The stub game clock.
    void setGameTime(int64 gameTime);
    void advanceHours(float hours);


    // A stub Character linked to the player, so the held item can be tested without a controller.
    class AInventoryTestCharacter* makeCharacterFor(AInventoryTestPlayerState* player);

    UDA_ItemDefinition* addArmor(FName itemId, int32 limit, float armorValue, float durability, float warmth = 0.0f);
    UDA_ItemDefinition* addClothes(FName itemId, int32 limit, float protection, float warmth = 0.0f);
private:
    UGameInstance* gameInstance = nullptr;
    UWorld* testWorld = nullptr;
};
