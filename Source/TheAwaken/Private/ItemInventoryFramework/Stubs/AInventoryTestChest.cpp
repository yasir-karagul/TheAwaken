// TEMPORARY STUB.
#include "ItemInventoryFramework/Stubs/AInventoryTestChest.h"
#include "ItemInventoryFramework/UContainerComponent.h"
#include "Components/StaticMeshComponent.h"

AInventoryTestChest::AInventoryTestChest(){
    bReplicates = true;
    mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(mesh);
    container = CreateDefaultSubobject<UContainerComponent>(TEXT("Container"));
}
