#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ALooseItemActor.generated.h"

class UStaticMeshComponent;

// Doc02 19.7: a loose physical object. The server simulates it; clients receive its position.
// When the shared base class of Doc02 19.7 exists, derive this class from it (I7).
UCLASS()
class THEAWAKEN_API ALooseItemActor : public AActor{
    GENERATED_BODY()

public:
    ALooseItemActor();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Command, server. Called right after spawning.
    void setEntry(const FS_InventoryEntry& newEntry);
    // Command, server. Part of the stack was taken. The actor destroys itself when nothing is left.
    void reduceQuantity(int32 takenQuantity);

    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FS_InventoryEntry getEntry() const;

protected:
    UFUNCTION()
    void OnRep_entry();

    void refreshMesh();

    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> mesh;
    // Replicated to everyone: anyone may see and pick up a loose item.
    UPROPERTY(ReplicatedUsing = OnRep_entry) FS_InventoryEntry entry;
};
