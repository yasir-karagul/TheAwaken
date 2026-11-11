// Public/ItemInventoryFramework/HeldItemComponent.h (TEMPORARY)
#pragma once
#include "Components/ActorComponent.h"
#include "HeldItemComponent.generated.h"

// TEMPORARY until Doc R01 is built. applyReload is a PROPOSED request to R01 (Ability guide gap 4).
// R01's real component must call UAbilityLibrary::cancelAction(player, Action.Reload) on the server whenever the held item changes (A6 step 5).
UCLASS(ClassGroup=(Awaken), meta=(BlueprintSpawnableComponent))
class THEAWAKEN_API UHeldItemComponent : public UActorComponent{
    GENERATED_BODY()
public:
    UHeldItemComponent();

    virtual UObject* getHeldItem() const;
    virtual void applyReload(AActor* player, UObject* weaponInstance);
};
