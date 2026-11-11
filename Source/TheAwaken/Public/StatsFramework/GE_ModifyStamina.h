#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_ModifyStamina.generated.h"

// R08 section 10. Instant effect, one additive modifier on currentStamina through UMMC_StaminaDelta (gap 30).
UCLASS()
class THEAWAKEN_API UGE_ModifyStamina : public UGameplayEffect{
    GENERATED_BODY()
public:
    UGE_ModifyStamina();
};
