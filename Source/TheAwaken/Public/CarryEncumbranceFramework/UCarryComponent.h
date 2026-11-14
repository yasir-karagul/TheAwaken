#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/NetSerialization.h"
#include "GameplayTagContainer.h"
#include "CarryEncumbranceFramework/01Structs/S_CarryEncumbranceFramework.h"
#include "CarryEncumbranceFramework/04Delegates/D_CarryEncumbranceFramework.h"
#include "UCarryComponent.generated.h"

class ACharacter;
class APlayerState;
class UStaticMeshComponent;

// B07 §2: the carry slots of one character. A Character component (Doc B19 3.3). Transient: never saved (B07 3.5).
// Server only mutates; the slots replicate to everyone (B07 §10). No tick: everything happens in a command or a notify.
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API UCarryComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UCarryComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // Query. The component on that character. A PlayerState is accepted too and means its current pawn. Null when missing.
    static UCarryComponent* findOn(const AActor* carrier);
    // Query. The component on the player's current pawn, or null.
    static UCarryComponent* findFor(const APlayerState* player);

    // ---- queries (B07 §4.1) ----
    // B07 IsCarryActive(): a carry slot is occupied. Combat, Abilities, Animation and Interaction all read this.
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") bool isCarryActive() const;
    // B07 GetCarriedItemsForCost(ItemID): how many units of that item are in the hands. Building reads it first (Doc B01 3.4).
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") int32 getCarriedItemsForCost(FName itemId) const;
    const TArray<FS_CarrySlotState>& getCarrySlots() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") TArray<FS_CarrySlotState> getCarrySlotsCopy() const;
    // The number of slots (tuning data, B07 3.1).
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") int32 getSlotCount() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") int32 getFreeSlotCount() const;
    // The primary slot of the thing picked up last, or INDEX_NONE when nothing is carried (C5).
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") int32 getActiveSlotIndex() const;
    // True when that slot holds a world actor (a corpse) and not an item. Doc B09 refuses to send one.
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") bool isActorCarry(int32 slotIndex) const;
    // For Doc R07's speed stack: the carry slowdown while carrying, otherwise 1 (C15).
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") float getSpeedMultiplier() const;
    // Why the last forced drop happened. Empty until one has happened on this character. For the debug view (Doc B22 3.5).
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") FGameplayTag getLastInterruptCause() const;

    // ---- commands (server only; each returns an explained result) ----
    // B07 §4.1 BeginCarry. Takes every unit of the entry or none. Refused with HandsFull when the free slots are too few.
    FS_CarryBeginResult beginCarry(const FS_InventoryEntry& entry);
    // Doc B08 BeginCarryCorpse / Doc B12 BeginCarryAlly delegate here. The actor must implement IBPI_CarryableActor (C10).
    FS_CarryBeginResult beginCarryActor(AActor* actor);
    // B07 §4.1 EndCarry. Frees the slot(s) of the thing carried in slotIndex. Dropped and Interrupted put the item on the
    // ground here; for Placed, Sent and Thrown the released entry is in the result and the caller owns it (C6).
    FS_CarryEndResult endCarry(int32 slotIndex, E_EndCarryReason reason);
    // Building's sourcing order (Doc B01 3.4): uses up to `quantity` carried units of that item, each ending
    // with the reason Placed. Returns how many were used; the caller draws any remainder from the bag (non-heavy items only).
    int32 consumeCarriedForCost(FName itemId, int32 quantity);
    // B07 3.4. Ends the carry with the reason Thrown and starts the flight from the carrier's eye.
    // A carried actor cannot be thrown (C10). slotIndex names any slot of the carried thing.
    FS_CarryEndResult throwCarried(int32 slotIndex, const FVector& direction);
    // B07 3.6. Drops everything that is carried at the carrier's feet with the reason Interrupted.
    // Returns how many things were dropped. Safe to call when nothing is carried.
    int32 interruptCarry(const FGameplayTag& causeTag);

    // ---- requests: input calls these on the owning client. On the host they run directly (C9.2) ----
    // slotIndex -1 = the active slot (the thing picked up last, C5).
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Carry") void serverDropCarried(int32 slotIndex);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Carry") void serverThrowCarried(int32 slotIndex, FVector_NetQuantizeNormal direction);

    // Raised on every machine after any change to the slots.
    UPROPERTY(BlueprintAssignable, Category = "Awaken|Carry") FD_CarryChanged onCarryChanged;

protected:
    // Replicated to everyone (B07 §10, Doc02 §8).
    UPROPERTY(ReplicatedUsing = OnRep_carrySlots) TArray<FS_CarrySlotState> carrySlots;

    UFUNCTION()
    void OnRep_carrySlots(const TArray<FS_CarrySlotState>& previousSlots);

    bool isServerSide() const;
    // The PlayerState of the carrier, or null for an NPC.
    APlayerState* getCarrierPlayerState() const;

    // ---- slot helpers ----
    // Server. Makes the array as long as the slot count. It never shrinks, so lowering the count takes nothing away.
    void ensureSlots();
    // Server. Fills `cost` free slots with one carried thing and returns its primary slot. The caller has checked the room.
    int32 fillSlots(const FGuid& carryId, FName itemId, int32 cost, AActor* actor);
    // True when no lower slot holds the same carried thing.
    static bool isPrimarySlot(const TArray<FS_CarrySlotState>& slots, int32 slotIndex);

    // ---- the state tag (B07 3.2) ----
    // Server. Sets Carry.State.Active on the carrier's Ability System Component as a replicated loose tag (C11).
    void applyCarryStateTag(bool isActive) const;

    // ---- begin / end ----
    // What must be true of the carrier's body. Empty tag = allowed (C17).
    FGameplayTag checkCanBegin() const;
    // Server. After slots were filled: the state tag, the ended actions, the message, the cosmetic event.
    void afterBegin(int32 slotIndex, FName itemId, bool wasActive);
    FS_CarryEndResult endCarryInternal(int32 slotIndex, E_EndCarryReason reason, const FGameplayTag& interruptCause);
    // In front of the carrier for a chosen drop, at the feet for a forced one (B07 3.6).
    FVector getDropLocation(bool atFeet) const;

    // ---- safety nets (C13) ----
    UFUNCTION()
    void onMovementModeChanged(ACharacter* character, EMovementMode previousMovementMode, uint8 previousCustomMode);
    // The carried actor was destroyed by something else while it was in the hands: free its slots.
    UFUNCTION()
    void onCarriedActorDestroyed(AActor* destroyedActor);

    // ---- requests ----
    // Rate and state, through R01's gate. False = refused and the requester has been told.
    bool passRequestGate(const TCHAR* operation) const;
    void replyIfRefused(const FGameplayTag& failReason, const TCHAR* operation) const;

    // ---- messages and visuals: run on every machine ----
    void raiseBegan(int32 slotIndex, FName itemId) const;
    void raiseEnded(int32 slotIndex, FName itemId, E_EndCarryReason reason, const FGameplayTag& interruptCause);
    // Rebuilds the meshes shown in the hands from the slots (C2).
    void refreshVisuals();
    // After any change, on the server: replicate now, redraw, notify.
    void finishChange();

    static FS_CarryBeginResult refusedBegin(const FGameplayTag& reason);
    static FS_CarryEndResult refusedEnd(const FGameplayTag& reason);

    // Server only, never replicated: the full entry of each carried item, by carryId (C2).
    // It keeps what the slot does not show: a spoil time, an instance id, a durability.
    UPROPERTY() TMap<FGuid, FS_InventoryEntry> carriedEntries;
    int32 nextSequence = 0;
    FGameplayTag lastInterruptCause;

    // Local on each machine: one mesh per slot, made on first use. Never replicated.
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> slotVisuals;
};
