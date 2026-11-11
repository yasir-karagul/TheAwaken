#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "EquipmentFramework/01Structs/S_EquipmentFramework.h"
#include "EquipmentFramework/04Delegates/D_EquipmentFramework.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "SaveFramework/03Interfaces/BPI_SaveParticipant.h"
#include "UEquipmentComponent.generated.h"

class APlayerState;

// R05 §2: the armor stack and the worn clothes of one player. A PlayerState component (Doc B19 3.2).
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API UEquipmentComponent : public UActorComponent, public IBPI_SaveParticipant{
    GENERATED_BODY()

public:
    UEquipmentComponent();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Query. Null when the component is missing.
    static UEquipmentComponent* findFor(const APlayerState* player);

    // ---- queries (R05 §5) ----
    const TArray<FS_EquippedArmorState>& getArmorStack() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") TArray<FS_EquippedArmorState> getArmorStackCopy() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") FS_WornClothesState getWornClothes() const;
    // Cached: rebuilt only on equip, unequip or a break. Never per frame.
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") FS_AggregatedArmorStats getAggregatedArmorStats() const;
    // Protection and warmth of the worn clothes. Never changes with damage.
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") FS_ClothesProtection getClothesProtection() const;
    // Armor warmth + clothes warmth. For the cold-exposure Illness source (R05 3.5; Doc B14 3.6, Doc B03).
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") float getTotalWarmth() const;
    // The damage the whole stack can still absorb. For the HUD's wear display and for the damage step.
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") float getTotalRemainingDurability() const;
    // The cap for NEW equips (R05 3.2). A stack that already holds more keeps its pieces.
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") int32 getArmorStackCap() const;


    // ---- commands (server only; each returns an explained result) ----
    // R05 §5 EquipArmor: bag -> next free position of the stack. Refused at the cap (S2-F22).
    FS_InventoryResult equipArmor(const FGuid& instanceId);
    // R05 §5 UnequipArmor: stack -> bag. Refused when the bag is at its limit, so nothing is lost (S2-F22).
    FS_InventoryResult unequipArmor(int32 stackIndex);

    // R05 §5 ApplyDamageToArmor. Server only. Called by Doc R03 at step 4, for attack damage only.
    // damageAmount is what the armor is asked to absorb. Returns what it really absorbed and how many pieces broke.
    FS_ArmorDamageResult applyDamageToArmor(float damageAmount);

    // R05 §5 ReleaseEquippedGear. Server. Empties the stack and returns the pieces; the caller (R01) stores them in the bag record.
    FS_GearReleaseResult releaseEquippedGear();
    // Server. Puts recovered pieces back on, in the order they were worn. The cap is not applied.
    bool reEquipGear(const TArray<FS_GearSlotRecord>& gear);
    // Server. Destroys the worn armor without returning it anywhere (E13: Doc B06's "world bag wins" case).
    void discardWornArmor();
    // Server. Fresh character / Start Again: no armor, default clothes (E12).
    void resetForNewCharacter();
    // Raised on every machine after any change (stack, wear, clothes).
    UPROPERTY(BlueprintAssignable, Category = "Awaken|Equipment") FD_EquipmentChanged onEquipmentChanged;

protected:
    // Replicated to everyone (R05 §8, Doc02 §8).
    UPROPERTY(ReplicatedUsing = OnRep_armorStack) TArray<FS_EquippedArmorState> armorStack;
    UPROPERTY(ReplicatedUsing = OnRep_wornClothes) FS_WornClothesState wornClothes;

    UFUNCTION()
    void OnRep_armorStack(const TArray<FS_EquippedArmorState>& previousStack);
    UFUNCTION()
    void OnRep_wornClothes(const FS_WornClothesState& previousClothes);

    bool isServerSide() const;
    APlayerState* getOwnerPlayerState() const;

    // Rebuilds both caches from the item definitions. Runs on every machine.
    void recalculate();
    // Server. Sends the aggregate to Stats (R08) and raises Equipment.StatsRecalculated (server only).
    void pushStats() const;
    // After a change to WHICH pieces or clothes are worn: recalculate, push, replicate now, notify.
    void finishChange();
    // After a change to wear only: replicate now, notify. The aggregate is untouched.
    void finishWearChange();
    void renumberStack();

    void raiseArmorMessage(const FGameplayTag& channel, const FS_EquippedArmorState& piece) const;
    void raiseClothesMessage() const;

    // The bag entry a worn piece turns back into (same instance id, current durability).
    static FS_InventoryEntry makeEntry(const FS_EquippedArmorState& piece);
    static FS_InventoryResult refusal(const FGameplayTag& reason);
    static FS_InventoryResult accepted();

    // Not replicated: every machine rebuilds them from the replicated state.
    FS_AggregatedArmorStats cachedArmorStats;
    FS_ClothesProtection cachedClothes;

public:
    // R05 §5 EquipClothes: one unit leaves the bag's stack; the previous outfit returns to the bag.
    FS_InventoryResult equipClothes(FName itemId);
    // R05 amendment: clothes taken from the container the player has open. The previous outfit drops on the ground.
    FS_InventoryResult equipClothesFromContainer(FName itemId);

protected:
    // Shared checks of the two commands. Empty tag = allowed.
    FGameplayTag checkClothesChange(FName itemId) const;
    void applyClothesChange(FName itemId);

public:
    // ---- Section 8: IBPI_SaveParticipant. Doc R13's real interface exists, so it is implemented directly
    // (the guide's byte-blob stub contract is not used). Key Equipment.Character ----
    inline static constexpr int32 currentSnapshotVersion = 1;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual FName getSaveKey_Implementation() const override;
    virtual E_SaveScope getSaveScope_Implementation() const override;
    virtual int32 getSnapshotVersion_Implementation() const override;
    virtual APlayerState* getSaveOwner_Implementation() const override;
    // Query: never mutates.
    virtual bool getSnapshotForSave_Implementation(FInstancedStruct& snapshot) const override;
    virtual bool restoreFromSnapshot_Implementation(const FInstancedStruct& snapshot, int32 savedVersion) override;
    // Chained migration, one step per version (R13 3.11). No step exists yet: another version is refused.
    virtual bool migrateSnapshotStep_Implementation(int32 fromVersion, const FInstancedStruct& oldSnapshot, FInstancedStruct& newSnapshot) const override;
    virtual bool contributeToSlotHeader_Implementation(const FS_SaveSlotHeader& display, FS_SaveSlotHeader& contribution) const override;

public:
    // ---- requests: the UI calls these on the owning client. On the host they run directly ----
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Equipment") void serverEquipArmor(FGuid instanceId);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Equipment") void serverUnequipArmor(int32 stackIndex);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Equipment") void serverEquipClothes(FName itemId, bool fromOpenContainer);

protected:
    // Rate, state and hands, through R01's gate. False = refused and the requester has been told.
    bool passRequestGate(const TCHAR* operation) const;
    void replyIfRefused(const FS_InventoryResult& result, const TCHAR* operation) const;
};
