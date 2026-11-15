#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CraftingServices.generated.h"

class AActor;
class UInventoryServices;

// The Crafting framework's game-instance services: the facade for its outgoing seams. Crafting keeps no seam map of its
// own; it uses R01's UInventoryServices (like R05 and B07), so a teammate replaces the progression answer with one line:
// UInventoryServices::get(world)->registerSeam(UBPI_ProgressionQuery::StaticClass(), myProgressionObject);
UCLASS()
class THEAWAKEN_API UCraftingServices : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static UCraftingServices* get(const UObject* worldContext);

    // ---- facade ----
    // Query. Doc R06. An empty tag means "no blueprint needed" and is always unlocked (R02 3.9).
    bool isUnlocked(const FGameplayTag& unlockTag) const;
    // Command. Doc B21. Raised through B21's UCosmeticEventSubsystem (no IBPI_CosmeticEventSink exists in this project).
    void raiseCosmeticEvent(const FGameplayTag& eventTag, const FVector& location, AActor* instigator) const;

private:
    UInventoryServices* getRegistry() const;
};
