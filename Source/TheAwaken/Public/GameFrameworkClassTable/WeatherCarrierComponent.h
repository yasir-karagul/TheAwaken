// Public/GameFrameworkClassTable/WeatherCarrierComponent.h
#pragma once
#include "Components/ActorComponent.h"
#include "WeatherFramework/01Structs/S_WeatherFramework.h"
#include "WeatherCarrierComponent.generated.h"

UCLASS(ClassGroup=(Awaken), meta=(BlueprintSpawnableComponent))
class THEAWAKEN_API UWeatherCarrierComponent : public UActorComponent{
    GENERATED_BODY()
public:
    UWeatherCarrierComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // COND_None: every player must see the same weather (Doc02 8, 19.1)
    UPROPERTY(ReplicatedUsing=OnRep_weatherState)
    FS_WeatherState weatherState;

    UPROPERTY(ReplicatedUsing=OnRep_waterBodies)
    TArray<FS_WaterBodyState> waterBodies;

    UFUNCTION()
    void OnRep_weatherState(const FS_WeatherState& previous);

    UFUNCTION()
    void OnRep_waterBodies(const TArray<FS_WaterBodyState>& previous);
};