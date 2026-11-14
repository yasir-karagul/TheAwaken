// TEMPORARY STUB — owners: Docs B19, R07.
#include "ItemInventoryFramework/Stubs/AInventoryTestCharacter.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "CarryEncumbranceFramework/UCarryComponent.h"

AInventoryTestCharacter::AInventoryTestCharacter(){
    heldItemComponent = CreateDefaultSubobject<UHeldItemComponent>(TEXT("HeldItem"));
    carryComponent = CreateDefaultSubobject<UCarryComponent>(TEXT("Carry"));
}
