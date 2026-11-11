#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentValidation.h"
#include "CosmeticEventsEnvironmentPresentationFramework/DA_EnvironmentSettings.h"
#include "CosmeticEventsEnvironmentPresentationFramework/DA_WeatherPresentation.h"
#include "CosmeticEventsEnvironmentPresentationFramework/01Structs/S_CosmeticEventsEnvironmentPresentationFramework.h"
#include "CosmeticEventsEnvironmentPresentationFramework/02Tags/T_CosmeticEventsEnvironmentPresentationFramework.h"
#include "WeatherFramework/00Enums/E_WeatherFramework.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"

namespace CosmeticEnvironmentValidation{

    FName makeRowName(const FString& tagString){
        return FName(*tagString);
    }

    FName makeUnderscoreRowName(const FString& tagString){
        return FName(*tagString.Replace(TEXT("."), TEXT("_")));
    }

    bool validateCosmeticEventTable(const UDataTable* table, TArray<FString>& errors, TArray<FString>& warnings){
        if(table == nullptr){
            errors.Add(TEXT("DT_CosmeticEvents is not set"));
            return false;
        }
        if(table->GetRowStruct() != FS_CosmeticEventRule::StaticStruct()){
            errors.Add(TEXT("DT_CosmeticEvents must use the row struct FS_CosmeticEventRule"));
            return false;
        }

        // Every declared leaf needs a row, and every row must name a declared leaf.
        TSet<FName> declaredNames;
        for(const FGameplayTag& tag : CosmeticEnvironmentTags::getCosmeticEventTags()){
            const FString tagString = tag.ToString();
            const FName dotted = makeRowName(tagString);
            const FName underscored = makeUnderscoreRowName(tagString);
            declaredNames.Add(dotted);
            declaredNames.Add(underscored);
            const bool hasRow = table->FindRow<FS_CosmeticEventRule>(dotted, TEXT("validateCosmeticEventTable"), false) != nullptr
                || table->FindRow<FS_CosmeticEventRule>(underscored, TEXT("validateCosmeticEventTable"), false) != nullptr;
            if(!hasRow){
                errors.Add(FString::Printf(TEXT("Declared cosmetic tag %s has no row in DT_CosmeticEvents"), *tagString));
            }
        }

        bool hasDefault = false;
        table->ForeachRow<FS_CosmeticEventRule>(TEXT("validateCosmeticEventTable"), [&](const FName& rowName, const FS_CosmeticEventRule& row){
            const FString name = rowName.ToString();
            if(rowName == FName("Default")){
                hasDefault = true;
            }else if(!declaredNames.Contains(rowName)){
                errors.Add(FString::Printf(TEXT("Row %s is not a declared cosmetic tag string and is not Default"), *name));
            }
            if(row.relevanceRadius <= 0.0f && !row.readsHearingDistance){
                errors.Add(FString::Printf(TEXT("Row %s: a radius of 0 is only allowed when the row reads the hearing distance"), *name));
            }
            if(row.relevanceRadius < 0.0f){
                errors.Add(FString::Printf(TEXT("Row %s: the radius is below 0"), *name));
            }
            if(row.maxPerSecondPerArea < 1){
                errors.Add(FString::Printf(TEXT("Row %s: maxPerSecondPerArea must be at least 1"), *name));
            }
        });

        if(!hasDefault){
            warnings.Add(TEXT("DT_CosmeticEvents has no Default row. An event whose tag has no row of its own is dropped with a warning"));
        }
        return errors.Num() == 0;
    }

    bool validateSettings(const UDA_EnvironmentSettings* settings, TArray<FString>& errors, TArray<FString>& warnings){
        if(settings == nullptr){
            errors.Add(TEXT("DA_EnvironmentSettings is not set"));
            return false;
        }
        if(settings->cosmeticEventTable.IsNull()){
            errors.Add(TEXT("cosmeticEventTable is not set"));
        }
        if(settings->blendSeconds <= 0.0f){
            errors.Add(TEXT("blendSeconds must be above 0 (B14 3.7: 30)"));
        }
        if(settings->idleUpdateRate <= 0.0f || settings->activeUpdateRate <= 0.0f){
            errors.Add(TEXT("idleUpdateRate and activeUpdateRate must be above 0"));
        }
        if(settings->activeUpdateRate < settings->idleUpdateRate){
            warnings.Add(TEXT("activeUpdateRate is below idleUpdateRate, so a blend would run slower than the idle loop"));
        }
        if(settings->maxUpdateDeltaSeconds <= 0.0f){
            errors.Add(TEXT("maxUpdateDeltaSeconds must be above 0"));
        }
        if(settings->lightningIntervalMinSeconds > settings->lightningIntervalMaxSeconds){
            errors.Add(TEXT("lightningIntervalMinSeconds is above lightningIntervalMaxSeconds"));
        }
        if(settings->lightningDistanceMin > settings->lightningDistanceMax){
            errors.Add(TEXT("lightningDistanceMin is above lightningDistanceMax"));
        }
        if(settings->lightningPitchMinDegrees > settings->lightningPitchMaxDegrees){
            errors.Add(TEXT("lightningPitchMinDegrees is above lightningPitchMaxDegrees"));
        }
        if(settings->nightSkyLightScale <= 0.0f){
            errors.Add(TEXT("nightSkyLightScale must be above 0: the night is dark but never black"));
        }
        if(settings->qualityPresets.Num() == 0){
            warnings.Add(TEXT("qualityPresets is empty, so the graphics preset changes nothing"));
        }
        if(settings->parameterCollection.IsNull()){
            warnings.Add(TEXT("parameterCollection (MPC_Environment) is not set, so no wetness, snow, wind or rain value reaches the materials"));
        }
        return errors.Num() == 0;
    }

    bool validateWeatherPresentations(TArray<FString>& errors, TArray<FString>& warnings){
        TArray<FPrimaryAssetId> assetIds;
        UAssetManager::Get().GetPrimaryAssetIdList(FPrimaryAssetType(UDA_WeatherPresentation::primaryAssetType), assetIds);

        TMap<E_WeatherType, int32> countByType;
        for(const FPrimaryAssetId& assetId : assetIds){
            const UDA_WeatherPresentation* presentation = Cast<UDA_WeatherPresentation>(UAssetManager::Get().GetPrimaryAssetPath(assetId).TryLoad());
            if(presentation == nullptr){
                warnings.Add(FString::Printf(TEXT("%s could not be loaded as a DA_WeatherPresentation"), *assetId.ToString()));
                continue;
            }
            countByType.FindOrAdd(presentation->weatherType)++;
        }

        const UEnum* weatherEnum = StaticEnum<E_WeatherType>();
        for(int32 index = 0; index < weatherEnum->NumEnums() - 1; index++){
            const E_WeatherType type = static_cast<E_WeatherType>(weatherEnum->GetValueByIndex(index));
            const int32* count = countByType.Find(type);
            if(count == nullptr){
                errors.Add(FString::Printf(TEXT("Weather type %s has no DA_WeatherPresentation"), *weatherEnum->GetNameStringByIndex(index)));
            }else if(*count > 1){
                errors.Add(FString::Printf(TEXT("Weather type %s has %d DA_WeatherPresentation assets, it needs exactly one"), *weatherEnum->GetNameStringByIndex(index), *count));
            }
        }
        return errors.Num() == 0;
    }
}
