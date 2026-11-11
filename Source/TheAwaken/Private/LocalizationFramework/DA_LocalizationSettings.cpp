#include "LocalizationFramework/DA_LocalizationSettings.h"
#include "LocalizationFramework/LocalizationKeyValidator.h"

FPrimaryAssetId UDA_LocalizationSettings::makeAssetId(){
    return FPrimaryAssetId(FPrimaryAssetType(TEXT("LocalizationSettings")), FName(TEXT("DA_LocalizationSettings")));
}

FPrimaryAssetId UDA_LocalizationSettings::GetPrimaryAssetId() const{
    return FPrimaryAssetId(FPrimaryAssetType(TEXT("LocalizationSettings")), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UDA_LocalizationSettings::IsDataValid(FDataValidationContext& context) const{
    const EDataValidationResult baseResult = Super::IsDataValid(context);
    const EDataValidationResult settingsResult = FLocalizationKeyValidator::validateSettings(this, context);
    return CombineDataValidationResults(baseResult, settingsResult);
}
#endif
