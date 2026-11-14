// TEMPORARY STUB — owners: Doc B19 (class table) and Doc R07 (the real Character). The real Character carries the same component.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AInventoryTestCharacter.generated.h"

class UCarryComponent;
class UHeldItemComponent;

UCLASS()
class AInventoryTestCharacter : public ACharacter{
    GENERATED_BODY()

public:
    AInventoryTestCharacter();

    UPROPERTY(VisibleAnywhere) TObjectPtr<UHeldItemComponent> heldItemComponent;
    // Doc B07 guide C3.8. The real Character (AAwakenCharacter) carries the same component.
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCarryComponent> carryComponent;
};
