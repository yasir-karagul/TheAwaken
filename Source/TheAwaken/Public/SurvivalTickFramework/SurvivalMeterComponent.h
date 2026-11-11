// Public/SurvivalTickFramework/SurvivalMeterComponent.h (TEMPORARY)
#pragma once
#include "Components/ActorComponent.h"
#include "SurvivalMeterComponent.generated.h"

// TEMPORARY until Doc B03 is built. The three queries below are what the Stats framework reads (S5).
// B03 must call UVitalsRegenComponent::reevaluate() when a hunger or thirst tier changes, when the blood amount
// changes and when the Illness meter crosses its lowest tier.
UCLASS(ClassGroup=(Awaken), meta=(BlueprintSpawnableComponent))
class THEAWAKEN_API USurvivalMeterComponent : public UActorComponent{
    GENERATED_BODY()
public:
    USurvivalMeterComponent();

    virtual bool isHungryOrThirsty() const;
    virtual bool hasBlood() const;
    virtual bool isIll() const;
};
