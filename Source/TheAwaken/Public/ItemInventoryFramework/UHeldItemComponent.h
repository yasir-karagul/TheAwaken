#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ItemInventoryFramework/04Delegates/D_ItemInventoryFramework.h"
#include "UHeldItemComponent.generated.h"

class APlayerState;
class UDA_ItemDefinition;

// R01 3.5: what the character has in the hand. A Character component, owned by this framework.
// Combat (R03), Equipment (R05) and Animation (R07) read it. It is never saved.
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API UHeldItemComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UHeldItemComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Query. The component on the player's current pawn, or null.
    static UHeldItemComponent* findFor(const APlayerState* player);

    // ---- queries ----
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FS_HeldItem getHeldState() const;
    // What Combat reads: the wielded weapon, or the Unarmed entry when nothing is wielded. Never null once the catalog is loaded.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") const UDA_ItemDefinition* getHeldDefinition() const;
    // What Animation shows in the hand: any held item, wielded or not. Null for an empty hand.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") const UDA_ItemDefinition* getShownDefinition() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") bool isUnarmed() const;

    // ---- commands (server) ----
    // A hotbar key was pressed: take what that slot resolves to. An empty slot empties the hand.
    FS_InventoryResult selectHotbarSlot(int32 slotIndex);
    // A direct pick from the bag screen (R01 3.5).
    FS_InventoryResult holdFromBag(const FS_ItemRef& ref);
    // Empties the hand: Combat then reads Unarmed.
    void putAway();
    // The bag changed: if the held entry is gone, the next copy takes over, or the hand empties.
    void refreshFromInventory();

    // Raised on every machine when what is in the hand changes.
    UPROPERTY(BlueprintAssignable, Category = "Awaken|Inventory") FD_HeldItemChanged onHeldItemChanged;

protected:
    UFUNCTION()
    void OnRep_held();

    bool isServerSide() const;
    APlayerState* getPlayerState() const;
    void setHeld(const FS_HeldItem& newHeld);

    // Replicated to everyone: other players must see what is in the hand (Doc02 §8).
    UPROPERTY(ReplicatedUsing = OnRep_held) FS_HeldItem held;
};
