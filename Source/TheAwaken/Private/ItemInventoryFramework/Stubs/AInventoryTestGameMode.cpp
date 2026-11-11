// TEMPORARY STUB — owners: Docs B19, B05, B20.
#include "ItemInventoryFramework/Stubs/AInventoryTestGameMode.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestCharacter.h"
#include "ItemInventoryFramework/UBFL_InventoryBag.h"
#include "EquipmentFramework/UEquipmentComponent.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "GameFramework/PlayerController.h"

AInventoryTestGameMode::AInventoryTestGameMode(){
    PlayerStateClass = AInventoryTestPlayerState::StaticClass();
    DefaultPawnClass = AInventoryTestCharacter::StaticClass();
}

void AInventoryTestGameMode::PostLogin(APlayerController* NewPlayer){
    Super::PostLogin(NewPlayer);
    UBFL_InventoryBag::startNewCharacter(NewPlayer->PlayerState, FGuid::NewGuid());
    // Doc R05 guide E7.4: wherever startNewCharacter runs, the equipment is reset right after it.
    UEquipmentComponent* equipment = UEquipmentComponent::findFor(NewPlayer->PlayerState);
    if(equipment != nullptr){
        equipment->resetForNewCharacter();
    }
}
