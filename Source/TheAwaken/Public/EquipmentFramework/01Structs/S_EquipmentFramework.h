#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "S_EquipmentFramework.generated.h"

class APlayerState;

// E1.3 (moved here from 04Delegates, see D_EquipmentFramework.h). Raised on every machine that holds the state,
// after any change to the armor stack, a piece's wear, or the clothes.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FD_EquipmentChanged, APlayerState*, player);

// ---------- Worn state (R05 §6) ----------

// One worn armor piece. stackIndex is its place in the stack: 0 = first worn, the highest = last stacked (E2).
// itemId is the E3 addition: other players need it to draw the piece.
USTRUCT(BlueprintType)
struct FS_EquippedArmorState{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 stackIndex = 0;
    UPROPERTY(BlueprintReadOnly) FGuid instanceId;
    UPROPERTY(BlueprintReadOnly) float remainingDurability = 0.0f;
    UPROPERTY(BlueprintReadOnly) FName itemId;
};

// The outfit. An item TYPE id, not an instance: clothes are stackable and carry no state (R05 §6, Index A87).
// An empty itemId means the default clothes, which are built into the character and are not an item (R01 3.8).
USTRUCT(BlueprintType)
struct FS_WornClothesState{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName itemId;

    bool isDefault() const{
        return itemId.IsNone();
    }
};

// ---------- Query results (R05 §5) ----------

// getAggregatedArmorStats(): cached, rebuilt only on equip, unequip or a break.
USTRUCT(BlueprintType)
struct FS_AggregatedArmorStats{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float armorValue = 0.0f;
    UPROPERTY(BlueprintReadOnly) float warmth = 0.0f;
    UPROPERTY(BlueprintReadOnly) int32 pieceCount = 0;
};

// getClothesProtection(): never changes with damage.
USTRUCT(BlueprintType)
struct FS_ClothesProtection{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float protection = 0.0f;
    UPROPERTY(BlueprintReadOnly) float warmth = 0.0f;
};

// ---------- Command results ----------

USTRUCT(BlueprintType)
struct FS_ArmorDamageResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float absorbedAmount = 0.0f;
    UPROPERTY(BlueprintReadOnly) int32 brokenPieces = 0;
};

// The reference version of Doc R03 3.7 step 4 for a player target (E5).
USTRUCT(BlueprintType)
struct FS_GearAttackResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float damageToHealth = 0.0f;
    UPROPERTY(BlueprintReadOnly) float absorbedByClothes = 0.0f;
    UPROPERTY(BlueprintReadOnly) float absorbedByArmor = 0.0f;
    UPROPERTY(BlueprintReadOnly) int32 brokenPieces = 0;
};

// ---------- Content layout data (R05 3.2, visual only) ----------

USTRUCT(BlueprintType)
struct FS_ArmorStackPosition{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Equipment.ArmorStack")) FGameplayTag positionTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName socketName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FTransform offset;
};

// ---------- Save record (R05 §9) ----------

USTRUCT()
struct FS_EquipmentSave{
    GENERATED_BODY()

    UPROPERTY() int32 version = 1;
    UPROPERTY() TArray<FS_EquippedArmorState> armorStack;
    UPROPERTY() FS_WornClothesState clothes;
};

// ---------- Message payloads (R05 §7, Doc02 §11) ----------

// Equipment.ArmorEquipped / ArmorUnequipped / ArmorBroken
USTRUCT(BlueprintType)
struct FS_ArmorChangedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<APlayerState> player = nullptr;
    UPROPERTY(BlueprintReadOnly) int32 stackIndex = 0;
    UPROPERTY(BlueprintReadOnly) FGuid instanceId;
    UPROPERTY(BlueprintReadOnly) FName itemId;
};

// Equipment.ClothesChanged
USTRUCT(BlueprintType)
struct FS_ClothesChangedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<APlayerState> player = nullptr;
    UPROPERTY(BlueprintReadOnly) FName itemId;
};

// Equipment.StatsRecalculated
USTRUCT(BlueprintType)
struct FS_ArmorStatsMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<APlayerState> player = nullptr;
    UPROPERTY(BlueprintReadOnly) float aggregatedArmorValue = 0.0f;
};
