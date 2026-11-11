// TEMPORARY STUB — owners: Docs B19, R07.
#include "ItemInventoryFramework/Stubs/AInventoryTestCharacter.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"

AInventoryTestCharacter::AInventoryTestCharacter(){
    heldItemComponent = CreateDefaultSubobject<UHeldItemComponent>(TEXT("HeldItem"));
}
