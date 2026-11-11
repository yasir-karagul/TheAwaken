#include "CameraFramework/DA_FirstPersonCameraConfig.h"

#include "CameraFramework/02Tags/T_CameraFramework.h"
#include "Curves/CurveFloat.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FGameplayTag UDA_FirstPersonCameraConfig::getStanceForLocomotionState(FName locomotionState) const{
    const FGameplayTag* stance = stanceByLocomotionState.Find(locomotionState);
    return stance != nullptr ? *stance : FGameplayTag();
}

#if WITH_EDITOR
EDataValidationResult UDA_FirstPersonCameraConfig::IsDataValid(FDataValidationContext& context) const{
    EDataValidationResult result = EDataValidationResult::Valid;
    if(Super::IsDataValid(context) == EDataValidationResult::Invalid){
        result = EDataValidationResult::Invalid;
    }
    const auto fail = [&context, &result](const FString& message){
        context.AddError(FText::FromString(message));
        result = EDataValidationResult::Invalid;
    };

    // Every stance has an eye height.
    const FGameplayTag stances[] = {
        T_Camera_Stance_Standing.GetTag(),
        T_Camera_Stance_Crouched.GetTag(),
        T_Camera_Stance_Swimming.GetTag(),
        T_Camera_Stance_Riding.GetTag()
    };
    for(const FGameplayTag& stance : stances){
        if(!eyeHeightByStance.Contains(stance)){
            fail(FString::Printf(TEXT("CameraConfig: eyeHeightByStance has no height for %s."), *stance.ToString()));
        }
    }

    // Every locomotion state has a stance entry, and the entry is a stance that has a height.
    // The eleven names are Doc R07 section 5 (E_LocomotionState). Update this list when R07 adds a state.
    static const TCHAR* locomotionStates[] = {
        TEXT("Idle"), TEXT("Walk"), TEXT("Sprint"), TEXT("Crouch"), TEXT("Jump"), TEXT("Dodge"),
        TEXT("Climb"), TEXT("Swim"), TEXT("Dive"), TEXT("Squeeze"), TEXT("RopeRide")
    };
    for(const TCHAR* stateName : locomotionStates){
        const FGameplayTag* stance = stanceByLocomotionState.Find(FName(stateName));
        if(stance == nullptr || !stance->IsValid()){
            fail(FString::Printf(TEXT("CameraConfig: stanceByLocomotionState has no stance for the locomotion state %s."), stateName));
        }else if(!eyeHeightByStance.Contains(*stance)){
            fail(FString::Printf(TEXT("CameraConfig: the stance %s of locomotion state %s has no eye height."), *stance->ToString(), stateName));
        }
    }

    if(eyeTolerance <= 0.0f){
        fail(TEXT("CameraConfig: eyeTolerance must be above zero."));
    }
    if(headBoneName.IsNone()){
        fail(TEXT("CameraConfig: headBoneName is empty. It names the bone hidden from the owner's own camera."));
    }
    if(firstPersonNearClip <= 0.0f){
        fail(TEXT("CameraConfig: firstPersonNearClip must be above zero."));
    }
    // Designer-confirmed rule: the setting must never be able to hide illness, tiredness or low health.
    if(screenEffectIntensityFloor <= 0.0f){
        fail(TEXT("CameraConfig: screenEffectIntensityFloor must be above zero, so the intensity setting can never hide the warning effects."));
    }

    // Shake entries and rows must point at something.
    for(const TPair<FGameplayTag, FS_CameraShakeEntry>& entry : shakeByTag){
        if(entry.Value.shakeClass.IsNull()){
            fail(FString::Printf(TEXT("CameraConfig: shakeByTag entry %s has no shake class."), *entry.Key.ToString()));
        }
    }
    for(int32 index = 0; index < shakeTable.Num(); ++index){
        const FS_CameraShakeRow& row = shakeTable[index];
        if(!row.eventTag.IsValid() || !row.shakeTag.IsValid()){
            fail(FString::Printf(TEXT("CameraConfig: shakeTable row %d needs an event tag and a shake tag."), index));
        }else if(!shakeByTag.Contains(row.shakeTag)){
            fail(FString::Printf(TEXT("CameraConfig: shakeTable row %d uses %s, which is not in shakeByTag."), index, *row.shakeTag.ToString()));
        }
        if(row.hitRadius <= 0.0f){
            fail(FString::Printf(TEXT("CameraConfig: shakeTable row %d needs a hit radius above zero."), index));
        }
    }

    // A scalar name for every channel the framework drives.
    const FGameplayTag channels[] = {
        T_ScreenEffect_BloodCoverage.GetTag(),
        T_ScreenEffect_IllnessBlur.GetTag(),
        T_ScreenEffect_FatigueVignette.GetTag(),
        T_ScreenEffect_WetnessDroplets.GetTag(),
        T_ScreenEffect_LowHealth.GetTag(),
        T_ScreenEffect_HitFlash.GetTag(),
        T_ScreenEffect_BagBackdrop.GetTag()
    };
    for(const FGameplayTag& channel : channels){
        const FName* name = materialParameterByEffect.Find(channel);
        if(name == nullptr || name->IsNone()){
            fail(FString::Printf(TEXT("CameraConfig: materialParameterByEffect has no scalar name for %s."), *channel.ToString()));
        }
    }
    return result;
}
#endif
