#include "StatsFramework/GE_ModifyHP.h"
#include "StatsFramework/ExecCalc_ModifyHP.h"

UGE_ModifyHP::UGE_ModifyHP(){
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayEffectExecutionDefinition execution;
    execution.CalculationClass = UExecCalc_ModifyHP::StaticClass();
    Executions.Add(execution);
}
