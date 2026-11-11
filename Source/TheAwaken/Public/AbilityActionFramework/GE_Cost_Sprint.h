#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Cost_Sprint.generated.h"

// A8 step 1. Infinite periodic drain through UMMC_StaminaCost. Applied by hand in GA_Sprint, never as the ability's cost effect.
UCLASS()
class THEAWAKEN_API UGE_Cost_Sprint : public UGameplayEffect{
    GENERATED_BODY()
public:
    UGE_Cost_Sprint();
};
