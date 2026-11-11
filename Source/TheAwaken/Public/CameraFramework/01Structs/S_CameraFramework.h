#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Camera/CameraShakeBase.h"
#include "S_CameraFramework.generated.h"

// One screen-effect channel while it is being driven (step 9). Runtime state only, never saved.
USTRUCT(BlueprintType)
struct FS_ScreenEffectParameterState{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FGameplayTag effectTag;
    // The unscaled value. The intensity setting is applied only when the value is pushed to the material (step 9 item 5).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float current = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float target = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float startValue = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float blendTime = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    float elapsed = 0.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    int32 winningPriority = 0;
    // For the debug view: which mapping row set the current target.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
    FName winningRowName;
};

// The four settings the menu feeds in (Doc B17 3.13). Step 5.
USTRUCT(BlueprintType)
struct FS_CameraComfortSettings{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float fieldOfView = 90.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float shakeScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float headBobScale = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float screenEffectIntensity = 1.0f;
};

// One entry of shakeByTag in the config: the shake asset and its base scale (step 3).
USTRUCT(BlueprintType)
struct FS_CameraShakeEntry{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftClassPtr<UCameraShakeBase> shakeClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float baseScale = 1.0f;
};

// One row of the shake table in the config (step 7, PROPOSED).
// The row with the highest minMagnitude that the event reaches wins, which is how LightHit and HeavyHit are chosen.
USTRUCT(BlueprintType)
struct FS_CameraShakeRow{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FGameplayTag eventTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float minMagnitude = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(Categories="Camera.Shake"))
    FGameplayTag shakeTag;
    // Events farther than this from the local pawn are ignored (Open items, row 6).
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float hitRadius = 0.0f;
    // PROPOSED: true for explosions. The shake is scaled down with distance using explosionFalloffCurve of the config.
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool scaleByDistance = false;
};
