#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestCharacter.h"
#include "ItemInventoryFramework/Stubs/InventoryStubs.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/InventoryTime.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

FInventoryTestFixture::FInventoryTestFixture(){
    gameInstance = NewObject<UGameInstance>(GEngine);
    gameInstance->AddToRoot();
    gameInstance->InitializeStandalone();   // a world context, an empty game world, and the game-instance subsystems
    testWorld = gameInstance->GetWorld();
    testWorld->InitializeActorsForPlay(FURL());
    testWorld->GetWorldSettings()->NotifyBeginPlay();   // actors spawned from here on begin play at once

    catalog()->resetForTest();   // only the items this test adds
    // Tests never spawn real loose actors: the recording stub answers instead.
    services()->registerSeam(UBPI_LooseItemSpawner::StaticClass(), NewObject<UStubLooseItemSpawner>(services()));
}

FInventoryTestFixture::~FInventoryTestFixture(){
    if(gameInstance != nullptr){
        gameInstance->Shutdown();
    }
    if(testWorld != nullptr){
        GEngine->DestroyWorldContext(testWorld);
        testWorld->DestroyWorld(false);
    }
    if(gameInstance != nullptr){
        gameInstance->RemoveFromRoot();
    }
}

void FInventoryTestFixture::expectForeignStartupErrors(FAutomationTestBase& test){
    const UAssetManager* manager = UAssetManager::GetIfInitialized();
    const FPrimaryAssetId environmentSettings(FPrimaryAssetType(TEXT("EnvironmentSettings")), FName(TEXT("DA_EnvironmentSettings_Default")));
    if(manager != nullptr && manager->GetPrimaryAssetPath(environmentSettings).IsValid()){
        return;
    }
    test.AddExpectedError(TEXT("DA_EnvironmentSettings_Default is missing"), EAutomationExpectedErrorFlags::Contains, 0);
}

UWorld* FInventoryTestFixture::world() const{
    return testWorld;
}

UInventoryServices* FInventoryTestFixture::services() const{
    return gameInstance->GetSubsystem<UInventoryServices>();
}

UItemCatalogSubsystem* FInventoryTestFixture::catalog() const{
    return gameInstance->GetSubsystem<UItemCatalogSubsystem>();
}

UInventoryRegistrySubsystem* FInventoryTestFixture::registry() const{
    return testWorld->GetSubsystem<UInventoryRegistrySubsystem>();
}

UDA_ItemDefinition* FInventoryTestFixture::addStackable(FName itemId, int32 limit){
    UDA_ItemDefinition* definition = NewObject<UDA_ItemDefinition>(gameInstance);
    definition->itemId = itemId;
    definition->itemKind = E_ItemKind::Stackable;
    definition->categoryTag = T_Item_Category_Material;
    definition->perTypeLimit = limit;
    catalog()->registerDefinition(definition);
    return definition;
}

UDA_ItemDefinition* FInventoryTestFixture::addInstanced(FName itemId, int32 limit, float durability){
    UDA_ItemDefinition* definition = NewObject<UDA_ItemDefinition>(gameInstance);
    definition->itemId = itemId;
    definition->itemKind = E_ItemKind::Instanced;
    definition->categoryTag = T_Item_Category_Tool;
    definition->perTypeLimit = limit;
    definition->durabilityMax = durability;
    catalog()->registerDefinition(definition);
    return definition;
}

AInventoryTestPlayerState* FInventoryTestFixture::makePlayer(){
    AInventoryTestPlayerState* player = testWorld->SpawnActor<AInventoryTestPlayerState>();
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    inventory->setCharacterId(FGuid::NewGuid());
    inventory->createFreshBag();
    return player;
}

UInventoryData* FInventoryTestFixture::makeContainer(const FS_ContainerRules& rules){
    return registry()->createInventory(E_InventoryType::Container, FGuid::NewGuid(), rules);
}

void FInventoryTestFixture::setGameTime(int64 gameTime){
    stub<UStubGameClock, UBPI_GameClock>()->gameTime = gameTime;
}

void FInventoryTestFixture::advanceHours(float hours){
    stub<UStubGameClock, UBPI_GameClock>()->gameTime += InventoryTime::hoursToGameSeconds(hours);
}

AInventoryTestCharacter* FInventoryTestFixture::makeCharacterFor(AInventoryTestPlayerState* player){
    AInventoryTestCharacter* character = testWorld->SpawnActor<AInventoryTestCharacter>();
    character->SetPlayerState(player);   // also tells the PlayerState which pawn is its own
    return character;
}

UDA_ItemDefinition* FInventoryTestFixture::addArmor(FName itemId, int32 limit, float armorValue, float durability, float warmth){
    UDA_ItemDefinition* definition = addInstanced(itemId, limit, durability);
    definition->categoryTag = T_Item_Category_Armor;
    definition->armorValue = armorValue;
    definition->warmth = warmth;
    return definition;
}

UDA_ItemDefinition* FInventoryTestFixture::addClothes(FName itemId, int32 limit, float protection, float warmth){
    UDA_ItemDefinition* definition = addStackable(itemId, limit);
    definition->categoryTag = T_Item_Category_Clothing;
    definition->clothesProtection = protection;
    definition->warmth = warmth;
    return definition;
}
