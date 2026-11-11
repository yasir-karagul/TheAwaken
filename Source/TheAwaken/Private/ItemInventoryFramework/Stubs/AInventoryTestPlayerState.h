// TEMPORARY STUB — owner: Doc B19 Game Framework Class Table. The real PlayerState carries the same components.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AInventoryTestPlayerState.generated.h"

class UContainerViewComponent;
class UEquipmentComponent;
class UInventoryRequestComponent;
class UPlayerInventoryComponent;

UCLASS()
class AInventoryTestPlayerState : public APlayerState{
    GENERATED_BODY()

public:
    AInventoryTestPlayerState();

    UPROPERTY(VisibleAnywhere) TObjectPtr<UPlayerInventoryComponent> inventoryComponent;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInventoryRequestComponent> requestComponent;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UContainerViewComponent> containerViewComponent;
    // Doc R05 guide E3.7.
    UPROPERTY(VisibleAnywhere) TObjectPtr<UEquipmentComponent> equipmentComponent;
};
