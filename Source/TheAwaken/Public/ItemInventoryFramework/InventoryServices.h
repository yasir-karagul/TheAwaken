#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Math/RandomStream.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "InventoryServices.generated.h"

class APlayerState;

UCLASS()
class THEAWAKEN_API UInventoryServices : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static UInventoryServices* get(const UObject* worldContext);

    // ---- the seam registry ----
    // Command. Replaces whatever was registered for that interface.
    void registerSeam(UClass* interfaceClass, UObject* implementation);
    // Query. Null when nothing is registered.
    UObject* getSeam(const UClass* interfaceClass) const;

    // ---- facade: one short call per outside framework ----
    int64 getGameTime() const;                                                                                      // Doc R12
    bool isIncapacitated(const APlayerState* player) const;                                                         // Docs B02, R03, R11, R17
    bool isCarryActive(const APlayerState* player) const;                                                           // Doc B07
    bool beginCarry(APlayerState* player, const FS_InventoryEntry& entry) const;                                    // Doc B07
    void modifyMeter(APlayerState* player, const FGameplayTag& meterTag, float delta, const FGameplayTag& sourceTag) const;   // Doc B03
    void registerIllnessSource(APlayerState* player, const FGameplayTag& sourceTag, float durationHours) const;     // Doc B03
    bool isIllnessActive(const APlayerState* player) const;                                                         // Docs B03, B13
    void healPlayer(APlayerState* player, float amount, const FGameplayTag& sourceTag) const;                       // Doc R08
    void damagePlayer(APlayerState* player, float amount, const FGameplayTag& damageTypeTag) const;                 // Doc R03
    void cancelAction(APlayerState* player, const FGameplayTag& actionTag) const;                                   // Doc R04
    AActor* spawnLooseItem(APlayerState* instigatorPlayer, const FS_InventoryEntry& entry, const FVector& location) const;   // Doc02 19.7
    FS_GearReleaseResult releaseEquippedGear(APlayerState* player) const;                                           // Doc R05
    bool reEquipGear(APlayerState* player, const TArray<FS_GearSlotRecord>& gear) const;                            // Doc R05
    UObject* getBagService() const;                                                                                 // Docs B02, R13
    void raiseItemDroppedCosmetic(APlayerState* player, const FVector& location) const;                             // Doc B21 (I20)

    // True with the given probability. Uses one stream so a fixed seed repeats a test (Doc B22 3.7).
    bool rollChance(float chance) const;

private:
    template<typename InterfaceClass>
    UObject* find() const{
        return getSeam(InterfaceClass::StaticClass());
    }

    UPROPERTY() TMap<TObjectPtr<UClass>, TObjectPtr<UObject>> seams;
    mutable FRandomStream randomStream;
};
