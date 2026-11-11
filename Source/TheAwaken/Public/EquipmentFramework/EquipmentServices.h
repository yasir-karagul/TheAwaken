#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ItemInventoryFramework/03Interfaces/BPI_EquipmentGear.h"
#include "EquipmentServices.generated.h"

class APlayerState;
class UInventoryServices;

UCLASS()
class THEAWAKEN_API UEquipmentServices : public UGameInstanceSubsystem, public IBPI_EquipmentGear{
    GENERATED_BODY()

public:
    // ---- IBPI_EquipmentGear: the seam Doc R01 calls from detachBag / reattachBag (E7.2) ----
    virtual FS_GearReleaseResult releaseEquippedGear_Implementation(APlayerState* player) override;
    virtual bool reEquipGear_Implementation(APlayerState* player, const TArray<FS_GearSlotRecord>& gear) override;

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static UEquipmentServices* get(const UObject* worldContext);

    // ---- facade (Command) ----
    void applyArmorAggregate(APlayerState* player, float totalArmorValue) const;                                        // Doc R08
    void raiseCosmeticEvent(const FGameplayTag& eventTag, const FVector& location, AActor* instigator, float magnitude) const;   // Doc B21

private:
    UInventoryServices* getRegistry() const;
};
