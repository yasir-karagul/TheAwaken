#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_ModifyHP.generated.h"

// R08 section 10. Instant effect that runs UExecCalc_ModifyHP. Configured in the constructor.
UCLASS()
class THEAWAKEN_API UGE_ModifyHP : public UGameplayEffect{
    GENERATED_BODY()
public:
    UGE_ModifyHP();
};
