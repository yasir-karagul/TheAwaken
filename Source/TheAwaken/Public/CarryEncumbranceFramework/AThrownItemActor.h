#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "AThrownItemActor.generated.h"

class APlayerState;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

// B07 3.4: one item in flight. The server simulates it; clients receive its movement (Doc02 19.7).
// While it flies it is NOT a loose item (it cannot be picked up). When it stops it turns into an ordinary loose item and removes itself.
UCLASS()
class THEAWAKEN_API AThrownItemActor : public AActor{
    GENERATED_BODY()

public:
    AThrownItemActor();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;

    // Command, server. Called once, between the deferred spawn and its finish.
    void launch(AActor* newThrower, const FS_InventoryEntry& newEntry, const FVector& velocity, float newImpactDamage);
    // Command, server. Ends the flight where the actor is now. Used by the flight timer and by tests.
    void forceLand();

    FS_InventoryEntry getEntry() const;

protected:
    UFUNCTION()
    void onProjectileStopped(const FHitResult& impactResult);
    UFUNCTION()
    void OnRep_itemId();

    // Server. The one way a flight ends.
    void land(AActor* hitActor, const FVector& location);
    void refreshMesh();

    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> collision;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> movement;

    // Replicated to everyone so each machine can show the right mesh. The rest of the entry stays on the server.
    UPROPERTY(ReplicatedUsing = OnRep_itemId) FName itemId;

    UPROPERTY() FS_InventoryEntry entry;
    TWeakObjectPtr<AActor> thrower;
    TWeakObjectPtr<APlayerState> throwerPlayer;
    float impactDamage = 0.0f;
    bool hasLanded = false;
    FTimerHandle flightTimer;
};
