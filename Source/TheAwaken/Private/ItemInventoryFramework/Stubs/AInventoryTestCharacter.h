// TEMPORARY STUB — owners: Doc B19 (class table) and Doc R07 (the real Character). The real Character carries the same component.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AInventoryTestCharacter.generated.h"

class UHeldItemComponent;

UCLASS()
class AInventoryTestCharacter : public ACharacter{
    GENERATED_BODY()

public:
    AInventoryTestCharacter();

    UPROPERTY(VisibleAnywhere) TObjectPtr<UHeldItemComponent> heldItemComponent;
};
