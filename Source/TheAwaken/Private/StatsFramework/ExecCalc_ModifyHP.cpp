#include "StatsFramework/ExecCalc_ModifyHP.h"
#include "GameplayEffect.h"
#include "StatsFramework/AS_CoreVitals.h"

void UExecCalc_ModifyHP::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& executionParams, FGameplayEffectCustomExecutionOutput& outExecutionOutput) const{
    const FGameplayEffectSpec& spec = executionParams.GetOwningSpec();
    const float delta = spec.GetSetByCallerMagnitude(FName("delta"), false, 0.0f);
    if(delta == 0.0f){
        return;
    }
    outExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(UAS_CoreVitals::incomingHPAttribute(), EGameplayModOp::Additive, delta));
}
