// TEMPORARY STUB — owners: Docs B19, R07.
#include "ItemInventoryFramework/Stubs/AInventoryTestCharacter.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "CarryEncumbranceFramework/UCarryComponent.h"
#include "CraftingFramework/UCraftingComponent.h"

AInventoryTestCharacter::AInventoryTestCharacter(){
    heldItemComponent = CreateDefaultSubobject<UHeldItemComponent>(TEXT("HeldItem"));
    carryComponent = CreateDefaultSubobject<UCarryComponent>(TEXT("Carry"));
    craftingComponent = CreateDefaultSubobject<UCraftingComponent>(TEXT("Crafting"));
}
