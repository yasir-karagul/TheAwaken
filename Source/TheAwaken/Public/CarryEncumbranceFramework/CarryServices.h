#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ItemInventoryFramework/03Interfaces/BPI_CarryService.h"
#include "CarryServices.generated.h"

class AActor;
class APlayerState;
class UInventoryServices;

// The Carry framework's game-instance services. It is the REAL IBPI_CarryService that Doc R01 calls through its seam
// registry (C1, C4.2), and the facade for Carry's own outgoing seams. Carry keeps no seam map of its own: it uses R01's
// UInventoryServices, so a teammate replaces a stub with one line:
// UInventoryServices::get(world)->registerSeam(UBPI_ThrowImpactSink::StaticClass(), myCombatObject);
UCLASS()
class THEAWAKEN_API UCarryServices : public UGameInstanceSubsystem, public IBPI_CarryService{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static UCarryServices* get(const UObject* worldContext);

    // ---- IBPI_CarryService: the seam Doc R01 calls from grantOrDrop, pickUpLooseItem and the request gate ----
    virtual bool isCarryActive_Implementation(APlayerState* player) override;
    virtual bool beginCarry_Implementation(APlayerState* player, const FS_InventoryEntry& entry) override;

    // ---- facade (Command) ----
    // Doc R03. Returns false when the item must not land.
    bool applyThrowImpact(AActor* target, AActor* thrower, FS_InventoryEntry& entry, float damage, const FVector& location) const;
    // Doc B21. Raised through B21's UCosmeticEventSubsystem (no IBPI_CosmeticEventSink exists in this project, C20).
    void raiseCosmeticEvent(const FGameplayTag& eventTag, const FVector& location, AActor* instigator) const;

private:
    UInventoryServices* getRegistry() const;
};
