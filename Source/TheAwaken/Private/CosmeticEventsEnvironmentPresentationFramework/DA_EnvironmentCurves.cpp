#include "CosmeticEventsEnvironmentPresentationFramework/DA_EnvironmentCurves.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Engine/AssetManager.h"
#include "Misc/DataValidation.h"

FPrimaryAssetId UDA_EnvironmentCurves::GetPrimaryAssetId() const{
    return FPrimaryAssetId(FPrimaryAssetType("EnvironmentCurves"), GetFName());
}

UDA_EnvironmentCurves* UDA_EnvironmentCurves::loadDefault(){
    const FPrimaryAssetId assetId(FPrimaryAssetType("EnvironmentCurves"), FName("DA_EnvironmentCurves_Default"));
    return Cast<UDA_EnvironmentCurves>(UAssetManager::Get().GetPrimaryAssetPath(assetId).TryLoad());
}

float UDA_EnvironmentCurves::sampleSunIntensity(float dayFraction) const{
    return sunIntensity ? sunIntensity->GetFloatValue(dayFraction) : 0.0f;
}

FLinearColor UDA_EnvironmentCurves::sampleSunColor(float dayFraction) const{
    return sunColor ? sunColor->GetLinearColorValue(dayFraction) : FLinearColor::White;
}

float UDA_EnvironmentCurves::sampleMoonIntensity(float dayFraction) const{
    return moonIntensity ? moonIntensity->GetFloatValue(dayFraction) : 0.0f;
}

FLinearColor UDA_EnvironmentCurves::sampleMoonColor(float dayFraction) const{
    return moonColor ? moonColor->GetLinearColorValue(dayFraction) : FLinearColor::White;
}

FLinearColor UDA_EnvironmentCurves::sampleSkyTint(float dayFraction) const{
    return skyTint ? skyTint->GetLinearColorValue(dayFraction) : FLinearColor::White;
}

float UDA_EnvironmentCurves::sampleExposureBias(float dayFraction) const{
    return exposureBias ? exposureBias->GetFloatValue(dayFraction) : 0.0f;
}

float UDA_EnvironmentCurves::sampleNightFactor(float dayFraction) const{
    return nightFactor ? FMath::Clamp(nightFactor->GetFloatValue(dayFraction), 0.0f, 1.0f) : 0.0f;
}

FLinearColor UDA_EnvironmentCurves::getSeasonGrade(E_Season season, bool isSnowyBlock) const{
    const E_Season key = isSnowyBlock ? E_Season::Winter : season;
    const FLinearColor* grade = seasonGrade.Find(key);
    return grade ? *grade : FLinearColor::White;
}

float UDA_EnvironmentCurves::getNightMinimum() const{
    if(moonIntensity == nullptr || skyTint == nullptr){
        return 0.0f;
    }
    // 96 samples: one every quarter of an hour, which is finer than any curve a designer draws.
    float lowest = TNumericLimits<float>::Max();
    for(int32 step = 0; step <= 96; step++){
        const float fraction = static_cast<float>(step) / 96.0f;
        lowest = FMath::Min(lowest, moonIntensity->GetFloatValue(fraction));
        lowest = FMath::Min(lowest, skyTint->GetLinearColorValue(fraction).GetLuminance());
    }
    return lowest;
}

#if WITH_EDITOR
EDataValidationResult UDA_EnvironmentCurves::IsDataValid(FDataValidationContext& context) const{
    EDataValidationResult result = Super::IsDataValid(context);
    bool isInvalid = false;
    if(sunIntensity == nullptr || sunColor == nullptr || moonIntensity == nullptr || moonColor == nullptr
        || skyTint == nullptr || exposureBias == nullptr || nightFactor == nullptr){
        context.AddError(FText::FromString(TEXT("Every curve of DA_EnvironmentCurves must be set")));
        isInvalid = true;
    }else if(getNightMinimum() <= 0.0f){
        context.AddError(FText::FromString(TEXT("The night minimum of the moon and sky curves is 0. The night must be dark but never black (B21 B.1)")));
        isInvalid = true;
    }
    if(seasonGrade.Num() < 4){
        context.AddWarning(FText::FromString(TEXT("seasonGrade does not hold all four seasons. A missing season uses no grade")));
    }
    if(isInvalid){
        return EDataValidationResult::Invalid;
    }
    return result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : result;
}
#endif
