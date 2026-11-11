#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

// Roots owned by the spec: ScreenEffect.* and Camera.Shake.*. New leaves sit under roots that already exist (Doc02 2).
// Tag strings stay Domain.Noun.Verb; only the C++ variable takes the T_ form (Doc02 20.2).

// Spec section 5
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_ScreenEffect_BloodCoverage);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_ScreenEffect_IllnessBlur);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_ScreenEffect_FatigueVignette);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_ScreenEffect_WetnessDroplets);
// PROPOSED in the spec (Index A115)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_ScreenEffect_BagBackdrop);
// PROPOSED here
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_ScreenEffect_LowHealth);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_ScreenEffect_HitFlash);

// Spec section 5
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Camera_Shake_LightHit);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Camera_Shake_HeavyHit);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Camera_Shake_Explosion);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Camera_Shake_Footstep);

// PROPOSED here: keys for the eye heights of step 6
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Camera_Stance_Standing);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Camera_Stance_Crouched);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Camera_Stance_Swimming);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Camera_Stance_Riding);

// PROPOSED here (step 6)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Camera_FailReason_OriginOutsideTolerance);
