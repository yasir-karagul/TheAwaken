#include "CosmeticEventsEnvironmentPresentationFramework/DA_WeatherPresentation.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentMath.h"
#include "Misc/DataValidation.h"

FPrimaryAssetId UDA_WeatherPresentation::GetPrimaryAssetId() const{
    return FPrimaryAssetId(FPrimaryAssetType(primaryAssetType), GetFName());
}

FS_WeatherLook UDA_WeatherPresentation::getTargetLook(float strength) const{
    if(!scalesWithStrength){
        return look;
    }
    return UCosmeticEnvironmentMathLibrary::scaleLookByStrength(lookAtZero, look, strength);
}

#if WITH_EDITOR
EDataValidationResult UDA_WeatherPresentation::IsDataValid(FDataValidationContext& context) const{
    EDataValidationResult result = Super::IsDataValid(context);
    const bool isStrengthType = weatherType == E_WeatherType::Rain || weatherType == E_WeatherType::Storm
        || weatherType == E_WeatherType::Snow || weatherType == E_WeatherType::Blizzard;
    if(isStrengthType && !scalesWithStrength){
        context.AddWarning(FText::FromString(TEXT("A rain or snow type normally scales with strength")));
    }
    if(look.lightScale < 0.0f){
        context.AddError(FText::FromString(TEXT("lightScale is below 0")));
        result = EDataValidationResult::Invalid;
    }
    return result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : result;
}
#endif
