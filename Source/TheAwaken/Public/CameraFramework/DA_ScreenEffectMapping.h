#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DA_ScreenEffectMapping.generated.h"

class UCurveFloat;

// One asset is one mapping row (spec 6.1, step 3). All values are authored in the editor, none are compiled in (Doc00 2.3, 2.7).
// The Asset Manager must scan the primary asset type "ScreenEffectMapping" (see the hand-off notes).
UCLASS(BlueprintType)
class UDA_ScreenEffectMapping : public UPrimaryDataAsset{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag sourceMessageTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag sourceFilterTag;   // PROPOSED: for example the Illness meter
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(Categories="ScreenEffect"))
    FGameplayTag effectTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UCurveFloat> valueCurve;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float defaultBlendTime = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 priority = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float pulseSeconds = 0.0f;      // PROPOSED: above 0 clears the effect after this long
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float maxPawnDistance = 0.0f;   // PROPOSED: above 0 ignores a Cosmetic.Received event farther than this from the local pawn

    // The type name the Asset Manager scans for.
    static FPrimaryAssetType getAssetType();

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& context) const override;
#endif
};
