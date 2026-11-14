#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CarryEncumbranceFramework/00Enums/E_CarryEncumbranceFramework.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "S_CarryEncumbranceFramework.generated.h"

class AActor;

// ---------- Delegates (guide C1.4) ----------
// Declared here, not in a delegate-only header: UHT does not register a dynamic delegate that sits alone in a header
// (the same fallback as R01 S1.9 and R05). D_CarryEncumbranceFramework.h includes this file.
// Raised on every machine that holds the state, after any change to the carry slots of this character.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FD_CarryChanged, AActor*, carrier);

// ---------- Carry state (B07 §7) ----------

// One carry slot. It holds exactly one heavy item, or nothing: there is no quantity (Index A103).
// Everything after itemId is the C3 addition.
USTRUCT(BlueprintType)
struct FS_CarrySlotState{
    GENERATED_BODY()

    // The item in this slot. For a carried actor it is the label the actor gives (for example Corpse).
    UPROPERTY(BlueprintReadOnly) FName itemId;
    // One id per carried thing. An item whose cost is 2 fills two slots that share this id (C4). Invalid = the slot is free.
    UPROPERTY(BlueprintReadOnly) FGuid carryId;
    // Pickup order. The occupied slot with the highest number is the active slot (C5).
    UPROPERTY(BlueprintReadOnly) int32 sequence = 0;
    // True when the slot holds a world actor (a corpse, a wounded ally) and not an item (C10).
    UPROPERTY(BlueprintReadOnly) bool holdsActor = false;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> carriedActor = nullptr;
    // Only meaningful once the slot is free again: why its last carry ended. A client reads these two
    // to raise Carry.Ended from replicated state (B07 §9).
    UPROPERTY(BlueprintReadOnly) E_EndCarryReason lastEndReason = E_EndCarryReason::Dropped;
    UPROPERTY(BlueprintReadOnly) FGameplayTag lastInterruptCause;

    bool isOccupied() const{
        return carryId.IsValid();
    }
};

// ---------- Command results ----------

USTRUCT(BlueprintType)
struct FS_CarryBeginResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FGameplayTag failReason;
    // The primary slot of the carried thing (of the last one, when several units were taken at once).
    UPROPERTY(BlueprintReadOnly) int32 slotIndex = INDEX_NONE;

    bool succeeded() const{
        return !failReason.IsValid();
    }
};

USTRUCT(BlueprintType)
struct FS_CarryEndResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FGameplayTag failReason;
    // The item that left the hands. For Placed, Sent and Thrown the caller now owns it (C6).
    UPROPERTY(BlueprintReadOnly) FS_InventoryEntry entry;
    // Set instead of entry when the slot held an actor.
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> actor = nullptr;

    bool succeeded() const{
        return !failReason.IsValid();
    }
};

// ---------- Message payloads (B07 §9, Doc02 §11) ----------

// Carry.Began
USTRUCT(BlueprintType)
struct FS_CarryBeganMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> carrier = nullptr;
    UPROPERTY(BlueprintReadOnly) FName itemId;
    UPROPERTY(BlueprintReadOnly) int32 slotIndex = INDEX_NONE;
};

// Carry.Ended
USTRUCT(BlueprintType)
struct FS_CarryEndedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> carrier = nullptr;
    UPROPERTY(BlueprintReadOnly) int32 slotIndex = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) E_EndCarryReason reason = E_EndCarryReason::Dropped;
    UPROPERTY(BlueprintReadOnly) FName itemId;
    // Empty unless reason is Interrupted (C14).
    UPROPERTY(BlueprintReadOnly) FGameplayTag interruptCause;
};
