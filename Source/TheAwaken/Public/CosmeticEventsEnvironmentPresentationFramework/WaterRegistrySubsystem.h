#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "TimerManager.h"
#include "CosmeticEventsEnvironmentPresentationFramework/00Enums/E_CosmeticEventsEnvironmentPresentationFramework.h"
#include "WeatherFramework/01Structs/S_WeatherFramework.h"
#include "WaterRegistrySubsystem.generated.h"

class AAwakenWaterBody;
class UDA_EnvironmentSettings;
class UDA_GroundTypeMap;

// The list of loaded water bodies and the five queries of B21 C.2 (gap G7: no document names where the list lives, so it is this small world
// subsystem). Local on every machine, never replicated. Any framework can call the queries: R07 (swim), B12 (NPC currents), B17 (bag and map), B03.
UCLASS()
class THEAWAKEN_API UWaterRegistrySubsystem : public UWorldSubsystem{
    GENERATED_BODY()

public:
    static UWaterRegistrySubsystem* get(const UObject* worldContext);

    virtual bool ShouldCreateSubsystem(UObject* outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& world) override;

    // Called by AAwakenWaterBody in BeginPlay and EndPlay (streaming can unload a body).
    void registerBody(AAwakenWaterBody* body);
    void unregisterBody(AAwakenWaterBody* body);

    AAwakenWaterBody* findBodyById(const FGuid& bodyId) const;

    // The loaded body whose swim volume contains the point. Null when the point is in no water.
    AAwakenWaterBody* findBodyAt(const FVector& location) const;

    // ---- The five queries (C.2) ----

    // True when any loaded body's swim volume contains the point. Cheap: no allocation, no tick. It stays true on solid ice, because it answers only
    // "inside the swim volume". A caller that decides Swim must check isFrozenWalkable first, and the bag-open check of B17 decides for itself
    // whether solid ice counts (gap G8).
    UFUNCTION(BlueprintPure, Category="Water")
    bool isLocationInWater(const FVector& location) const;

    // Returns false and leaves kind untouched when the point is not in water.
    bool getWaterKindAt(const FVector& location, E_WaterKind& kind) const;

    // Zero unless the body is a river or a stream. Reads replicated values only, so the host and the owning client get the same vector.
    // The slow-down above about 85 percent is R07's, not this query's.
    UFUNCTION(BlueprintPure, Category="Water")
    FVector getFlowAt(const FVector& location) const;

    // True when the point is in a body that can freeze (river, stream, lake: never the Sea) and the season is Winter, which includes the
    // Snowy block (B14 3.7). Agrees with Weather's isWaterFreezing(bodyId).
    UFUNCTION(BlueprintPure, Category="Water")
    bool isFreezing(const FVector& location) const;

    // True when the point is on a body whose ice collision is on. Immersion illness uses the same condition (A108, S6-F19).
    UFUNCTION(BlueprintPure, Category="Water")
    bool isFrozenWalkable(const FVector& location) const;

    // The Winter or Snowy test alone, for a caller that already knows the body.
    bool isFreezingSeason() const;

    // ---- The ground type map ----

    // A point in water returns the tag of its kind (a Stream returns the river tag), otherwise the grid is sampled. Local, read-only.
    UFUNCTION(BlueprintPure, Category="Water")
    FGameplayTag getGroundTypeAt(const FVector& location) const;

    const TArray<TWeakObjectPtr<AAwakenWaterBody>>& getBodies() const{
        return bodies;
    }

    UDA_EnvironmentSettings* getSettings() const{
        return settings;
    }

#if !UE_BUILD_SHIPPING
    void drawDebug() const;
#endif

private:
    void onWaterFroze(FGameplayTag channel, const FS_WeatherWaterMessage& message);
    void onWaterThawed(FGameplayTag channel, const FS_WeatherWaterMessage& message);

    UPROPERTY()
    TObjectPtr<UDA_EnvironmentSettings> settings;

    UPROPERTY()
    TObjectPtr<UDA_GroundTypeMap> groundMap;

    TArray<TWeakObjectPtr<AAwakenWaterBody>> bodies;

    FGameplayMessageListenerHandle frozeListener;
    FGameplayMessageListenerHandle thawedListener;

#if !UE_BUILD_SHIPPING
    FTimerHandle debugTimerHandle;
#endif
};
