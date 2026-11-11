#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_State_SprintExhausted.generated.h"

// A8 step 4. Infinite. Grants Action.State.SprintExhausted (the restart latch).
UCLASS()
class THEAWAKEN_API UGE_State_SprintExhausted : public UGameplayEffect{
    GENERATED_BODY()
public:
    UGE_State_SprintExhausted();
};
