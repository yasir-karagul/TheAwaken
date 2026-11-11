// Public/AINPCFramework/DA_NPCProfile.h (TEMPORARY)
#pragma once
#include "Engine/DataAsset.h"
#include "DA_NPCProfile.generated.h"

class UDA_AbilityLoadout;
class UDA_CoreVitalsDefinition;

// TEMPORARY until Doc B12 is built. Add these fields to the real asset. canBlock is a PROPOSED name (B12 names only CanDodge).
UCLASS(BlueprintType)
class THEAWAKEN_API UDA_NPCProfile : public UPrimaryDataAsset{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly)
    float maxHP = 100.0f;
    UPROPERTY(EditDefaultsOnly)
    float armor = 0.0f;
    UPROPERTY(EditDefaultsOnly)
    bool usesStamina = false;
    UPROPERTY(EditDefaultsOnly)
    bool essential = false;
    UPROPERTY(EditDefaultsOnly)
    bool canDodge = false;
    UPROPERTY(EditDefaultsOnly)
    bool canBlock = false;
    UPROPERTY(EditDefaultsOnly)
    TObjectPtr<UDA_AbilityLoadout> abilityLoadout;
    // Used for maxStamina when usesStamina is true.
    UPROPERTY(EditDefaultsOnly)
    TObjectPtr<UDA_CoreVitalsDefinition> vitalsDefinition;
};
