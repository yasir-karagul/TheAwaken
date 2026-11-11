#include "LocalizationFramework/LocalizationKeyValidator.h"

#if WITH_EDITOR

#include "LocalizationFramework/DA_LocalizationSettings.h"
#include "LocalizationFramework/LocalizationService.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Internationalization/StringTable.h"
#include "HAL/IConsoleManager.h"
#include "Internationalization/StringTableCore.h"

namespace{
    TSet<FName> referencedKeys;

    // Keys that code or other frameworks use directly, so no asset references them.
    // Add a key here when code, not an asset, is the only user.
    TArray<FName> codeReferencedKeys(){
        TArray<FName> keys;
        keys.Add(FName(TEXT("Format.List.Two")));
        keys.Add(FName(TEXT("Format.List.Start")));
        keys.Add(FName(TEXT("Format.List.Middle")));
        keys.Add(FName(TEXT("Format.List.End")));
        keys.Add(FName(TEXT("Format.Clock.Time")));
        keys.Add(FName(TEXT("Format.Calendar.Day")));
        keys.Add(FName(TEXT("UI.Notice.ItemsDropped")));
        keys.Add(FName(TEXT("UI.Notice.UnlocksLearned")));
        return keys;
    }

    const UDA_LocalizationSettings* loadSettingsAsset(){
        const FPrimaryAssetId settingsId = UDA_LocalizationSettings::makeAssetId();
        UAssetManager& assetManager = UAssetManager::Get();
        const FSoftObjectPath settingsPath = assetManager.GetPrimaryAssetPath(settingsId);
        if(!settingsPath.IsValid()){
            return nullptr;
        }
        return Cast<UDA_LocalizationSettings>(assetManager.GetStreamableManager().LoadSynchronous(settingsPath));
    }

    const UStringTable* findRoutedTable(const UDA_LocalizationSettings* settings, FName key){
        const FString keyString = key.ToString();
        int32 dotIndex = INDEX_NONE;
        if(!keyString.FindChar(TEXT('.'), dotIndex) || dotIndex <= 0){
            return nullptr;
        }
        const FName root(*keyString.Left(dotIndex));
        for(const FS_LocalizationTableRoute& route : settings->tableRoutes){
            if(route.keyRoot == root){
                return route.table.LoadSynchronous();
            }
        }
        return nullptr;
    }

    void addMessage(FDataValidationContext& context, const FString& message){
        context.AddError(FText::FromString(message));
    }
}

E_LocalizationLookupOutcome FLocalizationKeyValidator::findOutcome(FName key){
    if(key.IsNone()){
        return E_LocalizationLookupOutcome::EmptyKey;
    }
    const UDA_LocalizationSettings* settings = loadSettingsAsset();
    if(settings == nullptr){
        return E_LocalizationLookupOutcome::UnknownRoot;
    }
    const UStringTable* table = findRoutedTable(settings, key);
    if(table == nullptr){
        return E_LocalizationLookupOutcome::UnknownRoot;
    }
    FString defaultText;
    if(!table->GetStringTable()->GetSourceString(FTextKey(key.ToString()), defaultText) || defaultText.IsEmpty()){
        return E_LocalizationLookupOutcome::MissingKey;
    }
    return E_LocalizationLookupOutcome::Found;
}

EDataValidationResult FLocalizationKeyValidator::validateKey(FName key, const FText& ownerLabel, FDataValidationContext& context){
    referencedKeys.Add(key);
    const E_LocalizationLookupOutcome outcome = findOutcome(key);
    if(outcome == E_LocalizationLookupOutcome::Found){
        return EDataValidationResult::Valid;
    }
    addMessage(context, FString::Printf(TEXT("%s: text key '%s' cannot be resolved (%s)."), *ownerLabel.ToString(), *key.ToString(), *UEnum::GetValueAsString(outcome)));
    return EDataValidationResult::Invalid;
}

EDataValidationResult FLocalizationKeyValidator::validateSettings(const UDA_LocalizationSettings* settings, FDataValidationContext& context){
    EDataValidationResult result = EDataValidationResult::Valid;

    // 1. Languages: no duplicates, and the default is one of them.
    bool hasDefault = false;
    TSet<FName> seenCultures;
    for(const FS_SupportedCultureEntry& entry : settings->supportedCultures){
        if(seenCultures.Contains(entry.cultureName)){
            addMessage(context, FString::Printf(TEXT("Culture '%s' is listed twice."), *entry.cultureName.ToString()));
            result = EDataValidationResult::Invalid;
        }
        seenCultures.Add(entry.cultureName);
        if(entry.cultureName == settings->defaultCulture){
            hasDefault = true;
        }
    }
    if(!hasDefault){
        addMessage(context, TEXT("defaultCulture is not in supportedCultures."));
        result = EDataValidationResult::Invalid;
    }

    // 2. Routes: one table per root, and every entry has default text and sits in the right table.
    TSet<FName> seenRoots;
    TSet<const UStringTable*> checkedTables;
    for(const FS_LocalizationTableRoute& route : settings->tableRoutes){
        if(route.keyRoot.IsNone() || seenRoots.Contains(route.keyRoot)){
            addMessage(context, FString::Printf(TEXT("Table route root '%s' is empty or listed twice."), *route.keyRoot.ToString()));
            result = EDataValidationResult::Invalid;
            continue;
        }
        seenRoots.Add(route.keyRoot);
        const UStringTable* table = route.table.LoadSynchronous();
        if(table == nullptr){
            addMessage(context, FString::Printf(TEXT("Table route '%s' has no String Table."), *route.keyRoot.ToString()));
            result = EDataValidationResult::Invalid;
            continue;
        }
        if(checkedTables.Contains(table)){
            continue;
        }
        checkedTables.Add(table);

        auto visitEntry = [&](const FString& entryKey, const FString& sourceString){
            if(sourceString.IsEmpty()){
                addMessage(context, FString::Printf(TEXT("Key '%s' has an empty default text."), *entryKey));
                result = EDataValidationResult::Invalid;
            }
            if(findRoutedTable(settings, FName(*entryKey)) != table){
                addMessage(context, FString::Printf(TEXT("Key '%s' is in a table its root does not route to."), *entryKey));
                result = EDataValidationResult::Invalid;
            }
            return true;
        };
        table->GetStringTable()->EnumerateSourceStrings(visitEntry);
    }

    // 3. DT_ReasonText: set, no duplicate reason, every key resolves.
    const UDataTable* reasonTable = settings->reasonTextTable.LoadSynchronous();
    if(reasonTable == nullptr){
        addMessage(context, TEXT("reasonTextTable (DT_ReasonText) is not set."));
        return EDataValidationResult::Invalid;
    }
    TArray<FS_ReasonTextRow*> rows;
    reasonTable->GetAllRows<FS_ReasonTextRow>(TEXT("FLocalizationKeyValidator::validateSettings"), rows);
    TSet<FGameplayTag> seenReasons;
    for(const FS_ReasonTextRow* row : rows){
        if(row == nullptr){
            continue;
        }
        if(!row->reasonTag.IsValid() || seenReasons.Contains(row->reasonTag)){
            addMessage(context, FString::Printf(TEXT("DT_ReasonText has an empty or duplicate reason tag near key '%s'."), *row->textKey.ToString()));
            result = EDataValidationResult::Invalid;
        }
        seenReasons.Add(row->reasonTag);
        if(validateKey(row->textKey, FText::FromString(TEXT("DT_ReasonText")), context) != EDataValidationResult::Valid){
            result = EDataValidationResult::Invalid;
        }
    }
    return result;
}

void FLocalizationKeyValidator::reportUnusedKeys(){
    const UDA_LocalizationSettings* settings = loadSettingsAsset();
    if(settings == nullptr){
        UE_LOG(LogAwakenLocalization, Warning, TEXT("No DA_LocalizationSettings. Nothing to report."));
        return;
    }
    TSet<FName> usedKeys = referencedKeys;
    for(const FName& codeKey : codeReferencedKeys()){
        usedKeys.Add(codeKey);
    }
    int32 unusedCount = 0;
    TSet<const UStringTable*> checkedTables;
    for(const FS_LocalizationTableRoute& route : settings->tableRoutes){
        const UStringTable* table = route.table.LoadSynchronous();
        if(table == nullptr || checkedTables.Contains(table)){
            continue;
        }
        checkedTables.Add(table);
        auto visitEntry = [&](const FString& entryKey, const FString& sourceString){
            if(!usedKeys.Contains(FName(*entryKey))){
                UE_LOG(LogAwakenLocalization, Log, TEXT("Unused key: %s"), *entryKey);
                unusedCount++;
            }
            return true;
        };
        table->GetStringTable()->EnumerateSourceStrings(visitEntry);
    }
    UE_LOG(LogAwakenLocalization, Log, TEXT("%d unused keys listed. Nothing was deleted."), unusedCount);
    // Start the next validate-then-report cycle from a clean set.
    referencedKeys.Reset();
}

static FAutoConsoleCommand reportUnusedKeysCommand(
    TEXT("Awaken.Localization.ReportUnusedKeys"),
    TEXT("Lists text keys no validated asset references. Run Validate Data first. The list is cleared after each report."),
    FConsoleCommandDelegate::CreateStatic(&FLocalizationKeyValidator::reportUnusedKeys));

#endif