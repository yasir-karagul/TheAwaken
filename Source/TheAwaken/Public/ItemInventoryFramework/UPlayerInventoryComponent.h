#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ItemInventoryFramework/04Delegates/D_ItemInventoryFramework.h"
#include "SaveFramework/03Interfaces/BPI_SaveParticipant.h"
#include "UPlayerInventoryComponent.generated.h"

class APlayerState;
class UInventoryData;

// The player's link to their bag (R01 §2, decision D5). A PlayerState component, so it survives the pawn.
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API UPlayerInventoryComponent : public UActorComponent, public IBPI_SaveParticipant{
    GENERATED_BODY()

public:
    // ---- Section 11: IBPI_SaveParticipant. Doc R13's real interface exists, so this implements it directly
    // (S11.1: the stub interface is not created once R13's BPI_SaveParticipant.h exists). Key Inventory.Character ----
    inline static constexpr int32 currentSnapshotVersion = 1;

    virtual FName getSaveKey_Implementation() const override;
    virtual E_SaveScope getSaveScope_Implementation() const override;
    virtual int32 getSnapshotVersion_Implementation() const override;
    virtual APlayerState* getSaveOwner_Implementation() const override;
    // Query: never mutates. False while Bagless or not the host: R13 then keeps the previous record (Doc R13 3.3).
    virtual bool getSnapshotForSave_Implementation(FInstancedStruct& snapshot) const override;
    virtual bool restoreFromSnapshot_Implementation(const FInstancedStruct& snapshot, int32 savedVersion) override;
    // Chained migration, one step per version (Doc R13 3.11). No step exists yet: an older file is refused.
    virtual bool migrateSnapshotStep_Implementation(int32 fromVersion, const FInstancedStruct& oldSnapshot, FInstancedStruct& newSnapshot) const override;
    virtual bool contributeToSlotHeader_Implementation(const FS_SaveSlotHeader& display, FS_SaveSlotHeader& contribution) const override;

    UPlayerInventoryComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Query. Null when the component is missing.
    static UPlayerInventoryComponent* findFor(const APlayerState* player);

    // ---- queries ----
    // R01 §5 HasBag: true while the Personal Inventory exists. On a machine that is neither the host nor the owner it answers false.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") bool hasBag() const;
    // The bag. Null while Bagless. On the host: the real data. On the owning client: a read-only copy.
    UInventoryData* getPersonalInventory() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") TArray<FS_InventoryEntry> getBagEntries() const;
    // Held minus reserved: the number every reader shows (R01 3.6).
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") int32 getAvailableQuantity(FName itemId) const;
    // The character that owns this player's bags (Index A93). Doc R13 owns the real id (I23).
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FGuid getCharacterId() const;

    // ---- commands (server) ----
    // A new, empty bag. Does nothing when the player already has one.
    void createFreshBag();
    // Makes this inventory the player's bag.
    void attachInventory(UInventoryData* inventory);
    // Takes the bag away and returns it. The player is Bagless afterwards. The inventory stays in the registry.
    UInventoryData* releaseInventory();
    void setCharacterId(const FGuid& newCharacterId);

    // Server -> the owning client: an item was dropped because the bag had no room (Inventory.GrantDropped).
    UFUNCTION(Client, Reliable)
    void clientGrantDropped(FName itemId, int32 quantity);

    // Raised on the host and on the owning client after any change to the bag, the hotbar or the hold slots.
    UPROPERTY(BlueprintAssignable, Category = "Awaken|Inventory") FD_PlayerInventoryChanged onInventoryChanged;

protected:
    bool isServerSide() const;
    APlayerState* getOwnerPlayerState() const;
    void notifyChanged();
    // Host: the real bag changed.
    void handleBagChanged(UInventoryData* inventory);
    // Owning client: a batch of mirror changes arrived.
    void handleMirrorReceived();

    UFUNCTION()
    void OnRep_bagAttached();

    // All four replicate to the owner only (R01 §7).
    UPROPERTY(ReplicatedUsing = OnRep_bagAttached) bool bagAttached = false;
    UPROPERTY(Replicated) FGuid bagInventoryId;
    UPROPERTY(Replicated) FS_InventoryEntryList replicatedBag;
    UPROPERTY(Replicated) FGuid characterId;

    // Host: the bag in the registry. Clients: null.
    UPROPERTY() TObjectPtr<UInventoryData> personalInventory;
    // Owning client: the local read-only copy, rebuilt from the mirror.
    UPROPERTY() TObjectPtr<UInventoryData> clientBagView;
    FDelegateHandle bagChangedHandle;

public:
    // ---- hotbar (R01 3.5) ----
    // R01 §5 BindHotbarSlot. Server. An empty itemId clears the slot. Replaces any earlier binding on that slot.
    FS_InventoryResult bindHotbarSlot(int32 slotIndex, FName itemId);
    // R01 §5 ResolveHotbarSlot: the entry the slot uses now: the pooled stack, or the last used instance.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FS_ResolvedHotbarSlot resolveHotbarSlot(int32 slotIndex) const;
    // R01 §5 GetUsableHotbarSlotCount: the full hotbar with the bag, 3 while Bagless.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") int32 getUsableHotbarSlotCount() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FName getBoundItemId(int32 slotIndex) const;
    // With the bag: any slot 0 to 9. While Bagless: only keys 1, 2 and 3 (R01 3.8).
    bool isHotbarSlotUsable(int32 slotIndex) const;
    const TArray<FS_HotbarBinding>& getHotbarBindings() const;
    // Server. Puts a whole set of bindings back: bag recovery and restore from a save.
    void setHotbarBindings(const TArray<FS_HotbarBinding>& newBindings);
    // The copy of an instanced item that would be used now: the last used one that is still in the bag and not staged.
    FGuid findUsableInstance(FName itemId) const;
    // Server. Remembers that this copy was just taken in the hand.
    void noteInstanceUsed(const FGuid& instanceId);

protected:
    UFUNCTION()
    void OnRep_hotbar(const TArray<FS_HotbarBinding>& previousBindings);

    void raiseHotbarChanged(int32 slotIndex) const;

    // Entry i is slot i. Replicated to the owner only (R01 §7).
    UPROPERTY(ReplicatedUsing = OnRep_hotbar) TArray<FS_HotbarBinding> hotbar;
    // The most recent is last. Owner only (the hover card marks the copy that will be used next, Doc B17 3.6). Not saved (I9).
    UPROPERTY(Replicated) TArray<FGuid> recentInstances;

public:
    // ---- Bagless hold slots (R01 3.8). Hold index 0, 1, 2 is hotbar key 1, 2, 3 ----
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") TArray<FS_BaglessHoldSlot> getHoldSlots() const;
    // INDEX_NONE when all three are full.
    int32 findFreeHoldSlot() const;
    // Server. Puts ONE unit into an empty hold slot and freezes its spoil clock.
    bool putInHoldSlot(int32 holdIndex, const FS_InventoryEntry& unit);
    // Server. Takes the item out and rebuilds its spoil time from the clock. False when the slot is empty.
    bool takeFromHoldSlot(int32 holdIndex, FS_InventoryEntry& outEntry);
    // Server. Destroys what the hold slots hold. Answers how many items were destroyed.
    int32 clearHoldSlots();

    // ---- starting kit (R01 3.8) ----
    bool isKitGranted() const;
    void setKitGranted(bool newValue);

protected:
    UFUNCTION()
    void OnRep_holdSlots();

    // Owner only. Never saved: the host keeps them in the player's held state while the session runs (Index A110, S2-F30).
    UPROPERTY(ReplicatedUsing = OnRep_holdSlots) TArray<FS_BaglessHoldSlot> holdSlots;
    // Host only. Saved with the Character (Section 11), so a kit is never granted twice.
    UPROPERTY() bool kitGranted = false;
};
