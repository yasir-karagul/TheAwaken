// TEMPORARY STUB — owner: Doc R08. Delete when the real framework registers its seam.
#include "EquipmentFramework/Stubs/EquipmentStubs.h"

void UStubArmorStatsSink::applyArmorAggregate_Implementation(APlayerState* player, float totalArmorValue){
    lastArmorValue = totalArmorValue;
    callCount++;
}
