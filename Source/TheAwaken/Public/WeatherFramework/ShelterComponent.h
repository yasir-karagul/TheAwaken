#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeatherFramework/01Structs/S_WeatherFramework.h"
#include "ShelterComponent.generated.h"

class UDA_WeatherTuning;

UCLASS(ClassGroup=(Awaken), meta=(BlueprintSpawnableComponent))
class THEAWAKEN_API UShelterComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UShelterComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Query. Valid on the host for every player and on the owning client for its own player.
    const FS_ShelterState& getShelterState() const;

    // Command, host only. Called by the weather subsystem about once per second.
    void evaluate();

    // Query. The share of upward traces blocked above a point. Shared with the rain collector.
    static float getRoofFraction(const UWorld* world, const FVector& location, const UDA_WeatherTuning& tuning, const AActor* ignoredActor);

private:
    bool isNearWarmth(const FVector& location, const UDA_WeatherTuning& tuning) const;

    // COND_OwnerOnly: a player's shelter is nobody else's business (Doc02 8).
    UPROPERTY(Replicated)
    FS_ShelterState shelterState;
};