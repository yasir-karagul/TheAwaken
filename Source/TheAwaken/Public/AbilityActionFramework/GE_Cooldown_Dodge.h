#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Cooldown_Dodge.generated.h"

// A7 step 2. Duration is the set-by-caller value cooldownSeconds. Grants Action.Cooldown.Dodge.
UCLASS()
class THEAWAKEN_API UGE_Cooldown_Dodge : public UGameplayEffect{
    GENERATED_BODY()
public:
    UGE_Cooldown_Dodge();
};
