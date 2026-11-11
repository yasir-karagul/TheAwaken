#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GE_Dodge_Invulnerable.generated.h"

// A7 step 3. Duration is the set-by-caller value invulnerabilitySeconds. Grants Action.State.DodgeInvulnerable.
UCLASS()
class THEAWAKEN_API UGE_Dodge_Invulnerable : public UGameplayEffect{
    GENERATED_BODY()
public:
    UGE_Dodge_Invulnerable();
};
