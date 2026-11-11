#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CameraFramework/01Structs/S_CameraFramework.h"
#include "DA_FirstPersonCameraConfig.generated.h"

class UCurveFloat;

// PROPOSED (Open items, row 2). One asset holds every tunable of the camera. It is assigned on the UScreenEffectComponent
// of the PlayerController, and the camera component on the Character reads it from there. Authored in the editor only.
UCLASS(BlueprintType)
class UDA_FirstPersonCameraConfig : public UDataAsset{
    GENERATED_BODY()
public:
    // Camera.Stance.* tag to eye height above the pawn position (the capsule centre for an ACharacter).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(Categories="Camera.Stance"))
    TMap<FGameplayTag, float> eyeHeightByStance;

    // Largest allowed distance between a client's origin and the host's eye (Doc02 19.6). A distance equal to it passes.
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float eyeTolerance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(Categories="Camera.Shake"))
    TMap<FGameplayTag, FS_CameraShakeEntry> shakeByTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float bagViewBlendTime = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float bagViewFov = 0.0f;

    // Near clip for the bag and for the owner's body (renamed from bagNearClip; spec 3.5 rule 2, PROPOSED).
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float firstPersonNearClip = 0.0f;

    // The bone hidden from the owner's own camera, and whether the hidden head still casts the owner's shadow (spec 3.5 rules 1 and 3, PROPOSED).
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName headBoneName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool headCastsShadow = false;

    // Multiplier on sway and bob while the bag animates or is open.
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float bagSwayScale = 1.0f;

    // Locomotion state name (the entry names of R07's E_LocomotionState) to a Camera.Stance.* tag. Every state needs an entry.
    // Keyed by name because that enum lives in Doc R07 and is not visible to this framework; see the hand-off notes.
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TMap<FName, FGameplayTag> stanceByLocomotionState;

    // Rows of event tag, minimum magnitude, Camera.Shake.* tag and hit radius (step 7).
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FS_CameraShakeRow> shakeTable;
    // Scale for a row with scaleByDistance: input is distance divided by the row's hitRadius (0 to 1), output is the scale.
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UCurveFloat> explosionFalloffCurve;

    // Lowest value the intensity setting can take, above zero (step 5). Designer-confirmed (B04 ripple edit A89, B17 3.13).
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float screenEffectIntensityFloor = 0.0f;

    // ScreenEffect.* tag to the scalar name in M_ScreenEffects (step 9). Explicit, because the names do not follow a rule.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(Categories="ScreenEffect"))
    TMap<FGameplayTag, FName> materialParameterByEffect;

    // The Camera.Stance.* tag for a locomotion state name, or an empty tag when the state has no entry.
    UFUNCTION(BlueprintPure)
    FGameplayTag getStanceForLocomotionState(FName locomotionState) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& context) const override;
#endif
};
