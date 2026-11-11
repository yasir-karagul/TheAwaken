#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ItemInventoryFramework/04Delegates/D_ItemInventoryFramework.h"
#include "UContainerViewComponent.generated.h"

class APlayerState;
class UInventoryData;

// R01 §7: "a Container replicates to the players who have it open".
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API UContainerViewComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UContainerViewComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    static UContainerViewComponent* findFor(const APlayerState* player);

    // ---- queries ----
    // An invalid id while nothing is open.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FGuid getViewedContainerId() const;
    // Host: the real inventory. Owning client: the local read-only copy. Null while nothing is open.
    UInventoryData* getViewedInventory() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") TArray<FS_InventoryEntry> getViewedEntries() const;

    // ---- commands (server) ----
    // Starts viewing an inventory of the registry: a world container, or the contents of a Portable Container.
    FS_InventoryResult openContainer(const FGuid& containerId);
    void closeContainer();

    // Raised on the host and the owning client when a view opens (a valid id) or closes (an invalid id).
    UPROPERTY(BlueprintAssignable, Category = "Awaken|Inventory") FD_ContainerViewChanged onViewChanged;

protected:
    UFUNCTION()
    void OnRep_viewedContainerId();

    bool isServerSide() const;
    void handleViewedChanged(UInventoryData* inventory);
    void handleMirrorReceived();
    // Runs twice a second, only while a view is open (Doc02 §5): closes the view when the player has walked away.
    void checkReach();

    // Both replicate to the owner only.
    UPROPERTY(ReplicatedUsing = OnRep_viewedContainerId) FGuid viewedContainerId;
    UPROPERTY(Replicated) FS_InventoryEntryList replicatedView;

    UPROPERTY() TObjectPtr<UInventoryData> viewedInventory;   // host
    UPROPERTY() TObjectPtr<UInventoryData> clientView;        // owning client
    FDelegateHandle changedHandle;
    FTimerHandle reachTimer;
};
