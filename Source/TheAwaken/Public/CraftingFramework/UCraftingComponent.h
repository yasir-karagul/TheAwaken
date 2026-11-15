#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CraftingFramework/01Structs/S_CraftingFramework.h"
#include "CraftingFramework/04Delegates/D_CraftingFramework.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UCraftingComponent.generated.h"

class APlayerState;
class UCraftingStationComponent;
class UInventoryData;

// R02 §2: the crafting session of one player. A Character component (decision D5). Never saved (R02 §9).
// Holds no list of staged items: the bag's reserved quantities are the one truth (K4).
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API UCraftingComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UCraftingComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // Query. The component on that character. A PlayerState is accepted too and means its current pawn. Null when missing.
    static UCraftingComponent* findOn(const AActor* crafter);
    // Query. The component on the player's current pawn, or null.
    static UCraftingComponent* findFor(const APlayerState* player);

    // ---- queries: valid on the server and on the owning client ----
    UFUNCTION(BlueprintPure, Category = "Awaken|Crafting") FS_CraftingSessionState getSessionState() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Crafting") bool isSessionOpen() const;
    // A craft was requested and its animation has not ended yet (R02 3.7).
    UFUNCTION(BlueprintPure, Category = "Awaken|Crafting") bool isCraftInProgress() const;
    // What is in the crafting area: item id -> reserved quantity, read from the bag (K4).
    TMap<FName, int32> getStagedQuantities() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Crafting") int32 getStagedQuantity(FName itemId) const;
    // R02 §4.1 GetAvailableRecipes(): the capped, ordered craft button list for what is staged right now (Section 3).
    UFUNCTION(BlueprintPure, Category = "Awaken|Crafting") TArray<FS_AvailableRecipeOption> getAvailableRecipes() const;

    // ---- commands (server only; each returns an explained result) ----
    // The player interacted with a crafting station (Doc B18). Locks it and remembers it; nothing is staged yet (K8).
    FS_InventoryResult enterStation(UCraftingStationComponent* newStation);
    // R02 §4.1 OpenCraftingSession. Called by the first stageIngredient of a bag that has no session (R02 3.2, K9).
    FS_InventoryResult openCraftingSession();
    // R02 §4.1 StageIngredient. Wraps ReserveForStaging: the quantity moves from available to reserved.
    // For an instanced item named by item id, the most worn free copy is taken (R02 3.10).
    FS_InventoryResult stageIngredient(const FS_ItemRef& ref, int32 quantity);
    // R02 §4.1 UnstageIngredient. Wraps ReleaseStagedItem: the quantity is available again.
    FS_InventoryResult unstageIngredient(const FS_ItemRef& ref, int32 quantity);
    // R02 §4.1 RequestCraft(RecipeID). Server. Starts one craft. Succeeds only for a recipe that is green at this moment.
    FS_InventoryResult requestCraft(FName recipeId);
    // R02 §4.1 CloseCraftingSession(Reason). Releases every reservation of the session and leaves the station (R02 3.8).
    // Safe to call when no session is open.
    void closeCraftingSession(E_CraftingCloseReason reason);

    // ---- requests: the bag screen calls these on the owning client. On the host they run directly (CR8.2) ----
    // TEMPORARY entry point (owner: Doc B18): the player pressed interact on a crafting station.
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Crafting") void serverUseStation(AActor* stationActor);
    // One drag or Alt plus click: wholeStack false = one unit. With Shift: wholeStack true (R02 3.3). The server counts (K10).
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Crafting") void serverStageIngredient(const FS_ItemRef& ref, bool wholeStack);
    // A click on a staged item returns one unit; with Shift the whole staged stack (R02 3.3, S12-P4).
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Crafting") void serverUnstageIngredient(const FS_ItemRef& ref, bool wholeStack);
    // A press on a craft button. Only a recipe that is green on the SERVER is made (R02 3.7).
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Crafting") void serverRequestCraft(FName recipeId);
    // The bag screen closed (K7). Ends the session and leaves the station.
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Crafting") void serverCloseBag();

    // Raised on the server and on the owning client after any change to the session state.
    UPROPERTY(BlueprintAssignable, Category = "Awaken|Crafting") FD_CraftingChanged onCraftingChanged;

protected:
    // Replicated to the owning player only (Doc02 §8).
    UPROPERTY(ReplicatedUsing = OnRep_sessionState) FS_CraftingSessionState sessionState;

    UFUNCTION()
    void OnRep_sessionState(const FS_CraftingSessionState& previousState);

    // Crafting.SessionClosed is local to the crafting player (R02 §7). The payload cannot be rebuilt from state.
    UFUNCTION(Client, Reliable)
    void clientSessionClosed(const TArray<FS_ItemStack>& returnedItems, E_CraftingCloseReason reason);
    // Crafting.ItemCrafted: "server, then local to the crafting player" (R02 §7).
    UFUNCTION(Client, Reliable)
    void clientItemCrafted(FName recipeId, FName outputItemId, int32 outputQuantity);

    bool isServerSide() const;
    APlayerState* getCrafterPlayerState() const;
    // True on the machine whose player this character is.
    bool isLocalCrafter() const;

    // Bag, hands and state. Empty tag = allowed.
    FGameplayTag checkPlayerCanCraft() const;
    // Server. After any change to sessionState: replicate now, notify.
    void pushState();
    // Server. Unlocks the station and forgets it.
    void releaseStation();
    void raiseSessionOpened() const;
    void raiseItemCrafted(FName recipeId, FName outputItemId, int32 outputQuantity) const;

    // Server. The crafting time has passed: consume one recipe's worth and grant the output.
    void completeCraft();
    // Server. Stops a running craft. Nothing is consumed (R02 3.7).
    void cancelCraft();

    // R01 3.2a: a staged stack reached its spoil time and its reservation is gone. Bound while a session is open.
    void onStagedItemLost(const FGuid& lostStationId, FName itemId);
    void bindStagedItemLost();
    void unbindStagedItemLost();

    // Through R01's gate, so one rate limit covers bag and crafting. False = refused and the requester has been told.
    bool passRequestGate(bool needsHands, bool allowWhenIncapacitated, const TCHAR* operation) const;
    void replyIfRefused(const FGameplayTag& failReason, const TCHAR* operation) const;

    static FS_InventoryResult refusal(const FGameplayTag& reason);

    // Server only.
    TWeakObjectPtr<UCraftingStationComponent> station;
    // The one timer of this framework. It exists only while a craft runs (Doc02 §5).
    FTimerHandle craftTimer;
    FDelegateHandle stagedItemLostHandle;
    TWeakObjectPtr<UInventoryData> boundBag;
};
