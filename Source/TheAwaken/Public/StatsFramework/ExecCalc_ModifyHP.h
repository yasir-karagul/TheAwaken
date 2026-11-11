#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ExecCalc_ModifyHP.generated.h"

// S2 step 2. Its only job is to forward the set-by-caller "delta" into the incomingHP meta attribute.
UCLASS()
class THEAWAKEN_API UExecCalc_ModifyHP : public UGameplayEffectExecutionCalculation{
    GENERATED_BODY()
public:
    virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& executionParams, FGameplayEffectCustomExecutionOutput& outExecutionOutput) const override;
};
