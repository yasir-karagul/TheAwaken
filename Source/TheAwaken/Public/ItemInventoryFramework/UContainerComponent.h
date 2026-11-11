#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UContainerComponent.generated.h"

class UInventoryData;

// R01 3.4: a world-fixed Container. The actor is only the loaded view; the contents live in the registry.
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API UContainerComponent : public UActorComponent{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FGuid getContainerId() const;
    // Host only. Null on a client: a client sees a container through its view component (Section 12).
    UInventoryData* getInventory() const;

    // Designer-placed: set in the editor and never changed (Doc02 19.3, kind 1).
    // Player-built: leave empty, it is created when the container is built (kind 2).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container") FGuid containerId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container") FS_ContainerRules rules;
    // Some containers have no screen and give a quick take or store from a stack (R01 3.4). Read by the UI.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container") bool hasScreen = true;
};
