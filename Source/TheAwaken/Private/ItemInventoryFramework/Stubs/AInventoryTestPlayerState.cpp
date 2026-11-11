// TEMPORARY STUB — owner: Doc B19.
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/UContainerViewComponent.h"
#include "EquipmentFramework/UEquipmentComponent.h"
#include "ItemInventoryFramework/UInventoryRequestComponent.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"

AInventoryTestPlayerState::AInventoryTestPlayerState(){
    inventoryComponent = CreateDefaultSubobject<UPlayerInventoryComponent>(TEXT("Inventory"));
    requestComponent = CreateDefaultSubobject<UInventoryRequestComponent>(TEXT("InventoryRequests"));
    containerViewComponent = CreateDefaultSubobject<UContainerViewComponent>(TEXT("ContainerView"));
    equipmentComponent = CreateDefaultSubobject<UEquipmentComponent>(TEXT("Equipment"));
}
