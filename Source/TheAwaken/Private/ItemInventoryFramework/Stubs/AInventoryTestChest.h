// TEMPORARY STUB — a chest to test with. Real containers are content: any actor with a UContainerComponent.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AInventoryTestChest.generated.h"

class UContainerComponent;
class UStaticMeshComponent;

UCLASS()
class AInventoryTestChest : public AActor{
    GENERATED_BODY()

public:
    AInventoryTestChest();

    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UContainerComponent> container;
};
