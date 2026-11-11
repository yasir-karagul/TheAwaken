#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EquipmentFramework/01Structs/S_EquipmentFramework.h"
#include "UBFL_Equipment.generated.h"

class APlayerState;

UCLASS()
class THEAWAKEN_API UBFL_Equipment : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // ---- queries. A player without the component answers zeros ----
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") static FS_AggregatedArmorStats getAggregatedArmorStats(const APlayerState* player);
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") static FS_ClothesProtection getClothesProtection(const APlayerState* player);
    // For Doc B14 / Doc B03: how warm the player's armor and clothes are. The size of the effect is their data.
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") static float getTotalWarmth(const APlayerState* player);

    // ---- commands, server only ----
    // Doc R03 step 4: wear the armor down by what it absorbed.
    static FS_ArmorDamageResult applyDamageToArmor(APlayerState* player, float damageAmount);
    // REFERENCE version of Doc R03 3.7 step 4 for a player target (E5). damage is the attack damage after Block.
    static FS_GearAttackResult applyAttackToGear(APlayerState* player, float damage, float armorPenetrationPercent);
};
