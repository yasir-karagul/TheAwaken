#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CraftingFramework/00Enums/E_CraftingFramework.h"
#include "UCraftingStationComponent.generated.h"

class UCraftingComponent;
class UInteractionLockComponent;

// R02 3.9: makes its owner a crafting station. The owner is normally a Building piece (Doc B01).
// Exclusive (Doc B18 3.7): one player at a time. If the owner also carries Doc B18's UInteractionLockComponent, the lock
// is mirrored into it, so B18's prompt and its other interactions see the same holder.
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API UCraftingStationComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UCraftingStationComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Query. Null when the actor is not a crafting station.
    static UCraftingStationComponent* findOn(const AActor* actor);

    // Which stations this piece is. A recipe's requiredStationTag must be among them (R02 3.9).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station", meta = (Categories = "Station")) FGameplayTagContainer stationTags;

    // ---- queries ----
    UFUNCTION(BlueprintPure, Category = "Awaken|Crafting") FGuid getStationId() const;
    // True while a player uses the station. Replicated, so the interaction prompt can show "in use" (Doc B18 3.7).
    UFUNCTION(BlueprintPure, Category = "Awaken|Crafting") bool isInUse() const;
    // Server only. Null when nobody uses it.
    UCraftingComponent* getUser() const;

    // ---- commands (server) ----
    // Doc B01 passes the PieceID of the piece right after spawning it, and again after a load.
    void setStationId(const FGuid& newStationId);
    // The Exclusive lock. An empty tag = locked for this user. Interaction.FailReason.InUse when someone else has it.
    FGameplayTag tryLock(UCraftingComponent* user);
    // Does nothing unless this user holds the lock.
    void release(UCraftingComponent* user);
    // Server. Ends the session of whoever uses the station. Doc B18 calls it with OutOfRange when its lock is released;
    // the component calls it itself with StationLost when the piece goes away.
    void forceRelease(E_CraftingCloseReason reason);

protected:
    UPROPERTY(Replicated) FGuid stationId;
    UPROPERTY(Replicated) bool inUse = false;

    // Server only.
    TWeakObjectPtr<UCraftingComponent> currentUser;

    UInteractionLockComponent* findInteractionLock() const;
};
