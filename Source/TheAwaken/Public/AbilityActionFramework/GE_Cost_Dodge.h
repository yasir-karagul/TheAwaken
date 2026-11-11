#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Cost_Dodge.generated.h"

// A7 step 1. Instant cost on currentStamina through UMMC_StaminaCost. The engine's default cost check uses it.
UCLASS()
class THEAWAKEN_API UGE_Cost_Dodge : public UGameplayEffect{
    GENERATED_BODY()
public:
    UGE_Cost_Dodge();
};
