#include "CosmeticEventsEnvironmentPresentationFramework/DA_EnvironmentSettings.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentValidation.h"
#include "Engine/AssetManager.h"
#include "Misc/DataValidation.h"

FPrimaryAssetId UDA_EnvironmentSettings::GetPrimaryAssetId() const{
    return FPrimaryAssetId(FPrimaryAssetType("EnvironmentSettings"), GetFName());
}

UDA_EnvironmentSettings* UDA_EnvironmentSettings::loadDefault(){
    const FPrimaryAssetId assetId(FPrimaryAssetType("EnvironmentSettings"), FName("DA_EnvironmentSettings_Default"));
    return Cast<UDA_EnvironmentSettings>(UAssetManager::Get().GetPrimaryAssetPath(assetId).TryLoad());
}

#if WITH_EDITOR
EDataValidationResult UDA_EnvironmentSettings::IsDataValid(FDataValidationContext& context) const{
    EDataValidationResult result = Super::IsDataValid(context);

    TArray<FString> errors;
    TArray<FString> warnings;
    CosmeticEnvironmentValidation::validateSettings(this, errors, warnings);
    CosmeticEnvironmentValidation::validateCosmeticEventTable(cosmeticEventTable.LoadSynchronous(), errors, warnings);
    CosmeticEnvironmentValidation::validateWeatherPresentations(errors, warnings);

    for(const FString& warning : warnings){
        context.AddWarning(FText::FromString(warning));
    }
    for(const FString& error : errors){
        context.AddError(FText::FromString(error));
    }
    if(errors.Num() > 0){
        return EDataValidationResult::Invalid;
    }
    return result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : result;
}
#endif
