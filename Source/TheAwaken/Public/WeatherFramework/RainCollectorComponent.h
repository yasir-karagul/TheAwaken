#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RainCollectorComponent.generated.h"

UCLASS(ClassGroup=(Awaken), meta=(BlueprintSpawnableComponent))
class THEAWAKEN_API URainCollectorComponent : public UActorComponent{
    GENERATED_BODY()

public:
    URainCollectorComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

    // Command, host only. Called by the weather subsystem with the fill units earned since the last call.
    void collect(float units);

private:
    // The fraction of a unit not yet added. Transient, never saved.
    float pendingUnits = 0.0f;
};
