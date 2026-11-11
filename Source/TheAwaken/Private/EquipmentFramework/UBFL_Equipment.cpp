#include "EquipmentFramework/UBFL_Equipment.h"
#include "EquipmentFramework/UEquipmentComponent.h"
#include "GameFramework/PlayerState.h"

FS_AggregatedArmorStats UBFL_Equipment::getAggregatedArmorStats(const APlayerState* player){
    const UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
    return equipment != nullptr ? equipment->getAggregatedArmorStats() : FS_AggregatedArmorStats();
}

FS_ClothesProtection UBFL_Equipment::getClothesProtection(const APlayerState* player){
    const UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
    return equipment != nullptr ? equipment->getClothesProtection() : FS_ClothesProtection();
}

float UBFL_Equipment::getTotalWarmth(const APlayerState* player){
    const UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
    return equipment != nullptr ? equipment->getTotalWarmth() : 0.0f;
}

FS_ArmorDamageResult UBFL_Equipment::applyDamageToArmor(APlayerState* player, float damageAmount){
    UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
    return equipment != nullptr ? equipment->applyDamageToArmor(damageAmount) : FS_ArmorDamageResult();
}

FS_GearAttackResult UBFL_Equipment::applyAttackToGear(APlayerState* player, float damage, float armorPenetrationPercent){
    FS_GearAttackResult result;
    result.damageToHealth = FMath::Max(0.0f, damage);
    UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
    if(equipment == nullptr || damage <= 0.0f){
        return result;
    }
    // 1. Clothes first: they reduce the damage the armor receives, and never wear out (R05 3.6 and amendment).
    const float afterClothes = FMath::Max(0.0f, damage - equipment->getClothesProtection().protection);
    result.absorbedByClothes = damage - afterClothes;

    // 2. Armor: up to armor x (1 - penetration) (Doc R03 3.7), and never more than the stack can still take.
    const float penetration = FMath::Clamp(armorPenetrationPercent, 0.0f, 1.0f);
    const float armorReduction = FMath::Min(afterClothes, equipment->getAggregatedArmorStats().armorValue * (1.0f - penetration));
    const FS_ArmorDamageResult worn = equipment->applyDamageToArmor(armorReduction);
    result.absorbedByArmor = worn.absorbedAmount;
    result.brokenPieces = worn.brokenPieces;

    // 3. The rest goes on to health. Never below zero. MinimumAttackDamage is Doc R03's rule, applied by R03 after this.
    result.damageToHealth = FMath::Max(0.0f, afterClothes - worn.absorbedAmount);
    return result;
}
