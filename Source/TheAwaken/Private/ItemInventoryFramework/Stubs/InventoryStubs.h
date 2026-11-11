// TEMPORARY STUBS — the owner of each is named on its class. Delete a class when the real framework registers its seam.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameFramework/PlayerState.h"
#include "ItemInventoryFramework/03Interfaces/BPI_GameClock.h"
#include "ItemInventoryFramework/03Interfaces/BPI_PlayerStateQuery.h"
#include "ItemInventoryFramework/03Interfaces/BPI_CarryService.h"
#include "ItemInventoryFramework/03Interfaces/BPI_SurvivalService.h"
#include "ItemInventoryFramework/03Interfaces/BPI_HealthService.h"
#include "ItemInventoryFramework/03Interfaces/BPI_ActionService.h"
#include "ItemInventoryFramework/03Interfaces/BPI_LooseItemSpawner.h"
#include "ItemInventoryFramework/03Interfaces/BPI_BagActorService.h"
#include "InventoryStubs.generated.h"

// TEMPORARY STUB — owner: Doc R12 Time. The clock moves only when a test or a dev command moves it (I24).
UCLASS()
class UStubGameClock : public UObject, public IBPI_GameClock{
    GENERATED_BODY()

public:
    UPROPERTY() int64 gameTime = 0;

    virtual int64 getGameTime_Implementation() override;
};

// TEMPORARY STUB — owners: Docs B02, R03, R11, R17. A set of players marked as unable to act.
UCLASS()
class UStubPlayerStateQuery : public UObject, public IBPI_PlayerStateQuery{
    GENERATED_BODY()

public:
    TSet<TWeakObjectPtr<APlayerState>> incapacitatedPlayers;

    virtual bool isIncapacitated_Implementation(APlayerState* player) override;
};

// TEMPORARY STUB — owner: Doc B07 Carry. A set of players marked as carrying; it accepts a heavy item only when told to.
UCLASS()
class UStubCarryService : public UObject, public IBPI_CarryService{
    GENERATED_BODY()

public:
    TSet<TWeakObjectPtr<APlayerState>> carryingPlayers;
    UPROPERTY() bool acceptsNewCarry = false;
    UPROPERTY() TArray<FS_InventoryEntry> carriedEntries;

    virtual bool isCarryActive_Implementation(APlayerState* player) override;
    virtual bool beginCarry_Implementation(APlayerState* player, const FS_InventoryEntry& entry) override;
};

// TEMPORARY STUB — owner: Doc B03 Survival Tick. Adds up what each meter received.
UCLASS()
class UStubSurvivalService : public UObject, public IBPI_SurvivalService{
    GENERATED_BODY()

public:
    UPROPERTY() TMap<FGameplayTag, float> meterTotals;
    UPROPERTY() TArray<FGameplayTag> registeredSources;
    UPROPERTY() bool illnessActive = true;

    virtual void modifyMeter_Implementation(APlayerState* player, FGameplayTag meterTag, float delta, FGameplayTag sourceTag) override;
    virtual void registerIllnessSource_Implementation(APlayerState* player, FGameplayTag sourceTag, float durationHours) override;
    virtual bool isIllnessActive_Implementation(APlayerState* player) override;
};

// TEMPORARY STUB — owners: Doc R08 Stats (heal) and Doc R03 Combat (damage).
UCLASS()
class UStubHealthService : public UObject, public IBPI_HealthService{
    GENERATED_BODY()

public:
    UPROPERTY() float totalHealed = 0.0f;
    UPROPERTY() float totalDamaged = 0.0f;
    UPROPERTY() FGameplayTag lastDamageType;

    virtual void healPlayer_Implementation(APlayerState* player, float amount, FGameplayTag sourceTag) override;
    virtual void damagePlayer_Implementation(APlayerState* player, float amount, FGameplayTag damageTypeTag) override;
};

// TEMPORARY STUB — owner: Doc R04 Ability & Action.
UCLASS()
class UStubActionService : public UObject, public IBPI_ActionService{
    GENERATED_BODY()

public:
    UPROPERTY() TArray<FGameplayTag> cancelledActions;

    virtual void cancelAction_Implementation(APlayerState* player, FGameplayTag actionTag) override;
};

// A spawner that only remembers. Tests use it; Section 5 registers the real spawner for play.
UCLASS()
class UStubLooseItemSpawner : public UObject, public IBPI_LooseItemSpawner{
    GENERATED_BODY()

public:
    UPROPERTY() TArray<FS_InventoryEntry> spawnedEntries;
    UPROPERTY() TArray<FVector> spawnLocations;

    virtual AActor* spawnLooseItem_Implementation(APlayerState* instigatorPlayer, const FS_InventoryEntry& entry, FVector location) override;
};

// TEMPORARY STUB — owners: Doc B02 (actor, registry) and Doc R13 (record). Records only: no actor, nothing saved.
UCLASS()
class UStubBagActorService : public UObject, public IBPI_BagActorService{
    GENERATED_BODY()

public:
    UPROPERTY() TMap<FGuid, FS_BagInfo> bags;

    virtual bool createBag_Implementation(FGuid bagId, FGuid ownerCharacterId, FVector location) override;
    virtual void setBagContents_Implementation(FGuid bagId, const FS_BagContents& contents) override;
    virtual FS_BagInfo findBag_Implementation(FGuid bagId) override;
    virtual FS_BagInfo findBagOwnedBy_Implementation(FGuid ownerCharacterId) override;
    virtual void deleteBag_Implementation(FGuid bagId) override;
    virtual TArray<FGuid> removeBagsOwnedBy_Implementation(FGuid ownerCharacterId) override;
};
