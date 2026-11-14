#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CarryEncumbranceFramework/01Structs/S_CarryEncumbranceFramework.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UBFL_Carry.generated.h"

class AActor;
class APlayerState;
class AThrownItemActor;

// What other frameworks call. Teammates need not know the component: every function accepts the Character OR its PlayerState.
UCLASS()
class THEAWAKEN_API UBFL_Carry : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // ---- queries. An actor without the component carries nothing ----
    // B07 IsCarryActive(). Combat's attack check, Animation's carry pose, the squeeze and climb checks.
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") static bool isCarryActive(const AActor* carrier);
    // The one refusal line for "your hands are full": Interaction.FailReason.HandsFull while carrying, otherwise an empty tag.
    // Dialogue, the bag screen, crafting, climbing and the sleep registration all return this (B07 3.6).
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") static FGameplayTag getHandsBlockReason(const AActor* carrier);
    // B07 GetCarriedItemsForCost(ItemID) (Doc B01 3.4).
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") static int32 getCarriedItemsForCost(const AActor* carrier, FName itemId);
    // For Doc R07's speed stack.
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") static float getSpeedMultiplier(const AActor* carrier);

    // ---- commands, server only ----
    // Doc B01: use carried units for a cost. Each ends with the reason Placed. Returns how many were used.
    static int32 consumeCarriedForCost(AActor* carrier, FName itemId, int32 quantity);
    // B07 3.6. A forced drop of everything the carrier holds. causeTag is one of Carry.InterruptCause.*.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Carry") static int32 interruptCarry(AActor* carrier, FGameplayTag causeTag);
    // Cleanup for Session.PlayerLeft (Doc B20 3.6): a carried item is dropped where the leaving player stands.
    static void handlePlayerLeft(APlayerState* player);

    // B07 3.4, the generic throw pipeline (C7). Server only. The entry must already be out of wherever it was held:
    // Carry passes what endCarry released, Doc R03 passes what UBFL_Inventory::takeOutOfBag returned.
    // direction is normalized here. Returns null when the actor could not be spawned; the caller then still owns the entry.
    static AThrownItemActor* throwEntry(AActor* thrower, const FS_InventoryEntry& entry, const FVector& origin, const FVector& direction, float speed, float impactDamage);
};
