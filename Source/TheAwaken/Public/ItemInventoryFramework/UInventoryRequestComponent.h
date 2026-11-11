#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ItemInventoryFramework/04Delegates/D_ItemInventoryFramework.h"
#include "UInventoryRequestComponent.generated.h"

class ALooseItemActor;
class APlayerState;

// The only door between a client and the inventory rules (Doc02 §19.6). A PlayerState component:
// the owning client may call its server requests, and no other client can.
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API UInventoryRequestComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UInventoryRequestComponent();

    virtual void BeginPlay() override;

    static UInventoryRequestComponent* findFor(const APlayerState* player);

    // ---- requests: the UI or the interaction system calls these on the owning client ----
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverBindHotbarSlot(int32 slotIndex, FName itemId);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverSelectHotbarSlot(int32 slotIndex);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverHoldFromBag(const FS_ItemRef& ref);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverPutAway();
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverUseConsumable(FName itemId);
    // Bagless: slotIndex is the hotbar key, 1 to 3.
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverUseHoldSlot(int32 slotIndex);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverDropHoldSlot(int32 slotIndex);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverDropFromBag(const FS_ItemRef& ref, int32 quantity);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverPickUp(ALooseItemActor* looseItem);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverOpenContainer(AActor* containerActor);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverOpenPortableContainer(FGuid instanceId);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverCloseContainer();
    // Between the bag and the container the player has open.
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverTransfer(const FS_ItemRef& ref, int32 quantity, bool toContainer);
    // A container without a screen: a quick take or store from a stack, with no view (R01 3.4).
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverQuickTransfer(AActor* containerActor, const FS_ItemRef& ref, int32 quantity, bool toContainer);

    // ---- the shared gate. Public: Equipment (Doc R05) reuses it, so one rate limit covers both frameworks ----
    // Rate limit + player state + hands. An empty tag = allowed (Doc02 §19.6).
    FGameplayTag gate(bool needsHands, bool allowWhenIncapacitated);
    // A world container in range with a clear line, or a Portable Container that is in the player's own bag.
    bool isContainerReachable(APlayerState* player, const FGuid& containerId) const;
    // Logs the reason tag and tells only the requester (Doc02 §6, Doc B22 §3.6).
    void refuse(const FGameplayTag& reason, const TCHAR* operation);

    UFUNCTION(Client, Reliable)
    void clientRefused(FGameplayTag reason);

    // Raised on the requester's machine. Doc B17 shows the one-line refusal from the reason tag (3.14).
    UPROPERTY(BlueprintAssignable, Category = "Awaken|Inventory") FD_InventoryRequestRefused onRequestRefused;

protected:
    APlayerState* getOwnerPlayerState() const;
    // Distance from the pawn's eye (inclusive) and a clear line to the target.
    bool isInReach(const APlayerState* player, const AActor* target, float range) const;
    // Tells the requester only when the result is a refusal.
    void reply(const FS_InventoryResult& result, const TCHAR* operation);
    // The shared body of the two transfer requests.
    void transferWith(const FGuid& containerId, const FS_ItemRef& ref, int32 quantity, bool toContainer, const TCHAR* operation);

    float requestTokens = 0.0f;
    double lastTokenTime = 0.0;
};
