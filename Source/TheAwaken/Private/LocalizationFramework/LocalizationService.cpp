#include "LocalizationFramework/LocalizationService.h"
#include "LocalizationFramework/DA_LocalizationSettings.h"
#include "LocalizationFramework/02Tags/T_LocalizationFramework.h"
#include "Engine/AssetManager.h"
#include "Engine/DataTable.h"
#include "Engine/GameInstance.h"
#include "Internationalization/StringTable.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/StringTableRegistry.h"
#include "Kismet/KismetInternationalizationLibrary.h"

DEFINE_LOG_CATEGORY(LogAwakenLocalization);

bool ULocalizationService::ShouldCreateSubsystem(UObject* outer) const{
    // Doc R10 section 6: a dedicated server resolves no text.
    if(IsRunningDedicatedServer()){
        return false;
    }
    return Super::ShouldCreateSubsystem(outer);
}

void ULocalizationService::Initialize(FSubsystemCollectionBase& collection){
    Super::Initialize(collection);
    collection.InitializeDependency(UGameplayMessageSubsystem::StaticClass());

    if(!loadSettings()){
        UE_LOG(LogAwakenLocalization, Error, TEXT("[%s] DA_LocalizationSettings was not found. All text will show as [Key]."), *roleName());
        activeCulture = readStartupCulture();
        return;
    }

    buildRouteTable();
    buildReasonTable();

    // The engine has already applied the saved or platform culture before this runs.
    // Make sure it is one the game offers, so the first menu frame is in a valid language.
    const FName engineCulture = readStartupCulture();
    const FName startupCulture = resolveEffectiveCulture(engineCulture);
    if(startupCulture != engineCulture){
        UKismetInternationalizationLibrary::SetCurrentCulture(startupCulture.ToString(), false);
    }
    activeCulture = startupCulture;
    UE_LOG(LogAwakenLocalization, Log, TEXT("[%s] Localization ready: language '%s', %d cultures, %d table routes, %d reason rows."), *roleName(), *activeCulture.ToString(), settings->supportedCultures.Num(), routedTables.Num(), reasonKeys.Num());
}

void ULocalizationService::Deinitialize(){
    routedTables.Reset();
    reasonKeys.Reset();
    Super::Deinitialize();
}

FString ULocalizationService::roleName() const{
    const UGameInstance* gameInstance = GetGameInstance();
    const UWorld* world = gameInstance != nullptr ? gameInstance->GetWorld() : nullptr;
    if(world == nullptr){
        return TEXT("Offline");   // the Main Menu: no world exists yet
    }
    if(world->GetNetMode() == NM_Client){
        return TEXT("Client");
    }
    if(world->GetNetMode() == NM_Standalone){
        return TEXT("Offline");
    }
    return TEXT("Host");
}

bool ULocalizationService::loadSettings(){
    const FPrimaryAssetId settingsId = UDA_LocalizationSettings::makeAssetId();
    UAssetManager& assetManager = UAssetManager::Get();
    const FSoftObjectPath settingsPath = assetManager.GetPrimaryAssetPath(settingsId);
    if(!settingsPath.IsValid()){
        return false;
    }
    // Synchronous on purpose: the Main Menu needs text on its first frame (Doc R10 section 3.5).
    settings = Cast<UDA_LocalizationSettings>(assetManager.GetStreamableManager().LoadSynchronous(settingsPath));
    return settings != nullptr;
}

void ULocalizationService::buildRouteTable(){
    routedTables.Reset();
    for(const FS_LocalizationTableRoute& route : settings->tableRoutes){
        UStringTable* table = route.table.LoadSynchronous();
        if(route.keyRoot.IsNone() || table == nullptr){
            UE_LOG(LogAwakenLocalization, Warning, TEXT("[%s] A table route has no root or no String Table: '%s'."), *roleName(), *route.keyRoot.ToString());
            continue;
        }
        routedTables.Add(route.keyRoot, table);
    }
}

void ULocalizationService::buildReasonTable(){
    reasonKeys.Reset();
    reasonTable = settings->reasonTextTable.LoadSynchronous();
    if(reasonTable == nullptr){
        UE_LOG(LogAwakenLocalization, Warning, TEXT("[%s] DT_ReasonText is not set. Refusal reasons will show as [Tag]."), *roleName());
        return;
    }
    TArray<FS_ReasonTextRow*> rows;
    reasonTable->GetAllRows<FS_ReasonTextRow>(TEXT("ULocalizationService::buildReasonTable"), rows);
    for(const FS_ReasonTextRow* row : rows){
        if(row != nullptr && row->reasonTag.IsValid() && !row->textKey.IsNone()){
            reasonKeys.Add(row->reasonTag, row->textKey);
        }
    }
}

FName ULocalizationService::readStartupCulture() const{
    return FName(*FInternationalization::Get().GetCurrentLanguage()->GetName());
}

// ---- Commands ----

FS_LocalizationResult ULocalizationService::setCulture(FName cultureName){
    FS_LocalizationResult result;
    result.cultureName = cultureName;

    // 1. Validate everything first. Fail-fast, no partial application (Doc02 section 6).
    if(!isCultureSupported(cultureName) || !FInternationalization::Get().GetCulture(cultureName.ToString()).IsValid()){
        result.failReason = T_Localization_FailReason_UnknownCulture.GetTag();
        UE_LOG(LogAwakenLocalization, Warning, TEXT("[%s] setCulture('%s') refused: %s"), *roleName(), *cultureName.ToString(), *result.failReason.ToString());
        return result;
    }

    // 2. Same language: nothing changes and nothing is broadcast.
    if(cultureName == activeCulture){
        result.success = true;
        return result;
    }

    // 3. Apply. Outside the editor the choice is also saved to the local settings file.
    const FName previousCulture = activeCulture;
    // In the editor (PIE included) the culture is process-wide, so it is not persisted there.
    const bool saveToConfig = !GIsEditor;
    if(!UKismetInternationalizationLibrary::SetCurrentCulture(cultureName.ToString(), saveToConfig)){
        result.failReason = T_Localization_FailReason_UnknownCulture.GetTag();
        UE_LOG(LogAwakenLocalization, Warning, TEXT("[%s] setCulture('%s') refused by the engine: %s"), *roleName(), *cultureName.ToString(), *result.failReason.ToString());
        return result;
    }
    activeCulture = cultureName;

    // 4. Broadcast on this machine only (Doc02 sections 11 and 19.2).
    UGameInstance* gameInstance = GetGameInstance();
    UGameplayMessageSubsystem* messageSubsystem = gameInstance != nullptr ? gameInstance->GetSubsystem<UGameplayMessageSubsystem>() : nullptr;
    if(messageSubsystem != nullptr){
        FS_LocalizationCultureChanged message;
        message.cultureName = cultureName;
        message.previousCultureName = previousCulture;
        messageSubsystem->BroadcastMessage(T_Localization_CultureChanged.GetTag(), message);
    }

    UE_LOG(LogAwakenLocalization, Log, TEXT("[%s] Language changed from '%s' to '%s'."), *roleName(), *previousCulture.ToString(), *cultureName.ToString());
    result.success = true;
    return result;
}

// ---- Culture queries ----

FName ULocalizationService::getActiveCulture() const{
    return activeCulture;
}

FName ULocalizationService::getDefaultCulture() const{
    if(settings == nullptr){
        return FName(TEXT("en"));
    }
    return settings->defaultCulture;
}

const FS_SupportedCultureEntry* ULocalizationService::findEntry(FName cultureName) const{
    if(settings == nullptr){
        return nullptr;
    }
    for(const FS_SupportedCultureEntry& entry : settings->supportedCultures){
        if(entry.cultureName == cultureName){
#if UE_BUILD_SHIPPING
            if(entry.isDevelopmentOnly){
                return nullptr;
            }
#endif
            return &entry;
        }
    }
    return nullptr;
}

bool ULocalizationService::isCultureSupported(FName cultureName) const{
    return findEntry(cultureName) != nullptr;
}

TArray<FS_SupportedCultureEntry> ULocalizationService::getAvailableCultures() const{
    TArray<FS_SupportedCultureEntry> available;
    if(settings == nullptr){
        return available;
    }
    for(const FS_SupportedCultureEntry& entry : settings->supportedCultures){
        if(isCultureSupported(entry.cultureName)){
            available.Add(entry);
        }
    }
    return available;
}

FName ULocalizationService::resolveEffectiveCulture(FName requestedCulture) const{
    if(isCultureSupported(requestedCulture)){
        return requestedCulture;
    }
    // Regional parent chain: "zh-Hans-CN" -> "zh-Hans" -> "zh".
    FString candidate = requestedCulture.ToString();
    int32 separatorIndex = INDEX_NONE;
    while(candidate.FindLastChar(TEXT('-'), separatorIndex)){
        candidate.LeftInline(separatorIndex);
        const FName parentCulture(*candidate);
        if(isCultureSupported(parentCulture)){
            return parentCulture;
        }
    }
    return getDefaultCulture();
}

bool ULocalizationService::hasVoice(FName cultureName) const{
    const FS_SupportedCultureEntry* entry = findEntry(cultureName);
    return entry != nullptr && entry->hasVoice;
}


namespace{
    ETextGender toEngineGender(E_LocalizationGender gender){
        switch(gender){
            case E_LocalizationGender::Feminine:{
                return ETextGender::Feminine;
            }
            case E_LocalizationGender::Neuter:{
                return ETextGender::Neuter;
            }
            default:{
                return ETextGender::Masculine;
            }
        }
    }
}

// ---- Lookup ----

const UStringTable* ULocalizationService::findTableForKey(FName key) const{
    // The root is the part before the first dot: "UI.Pause.Continue" -> "UI".
    const FString keyString = key.ToString();
    int32 dotIndex = INDEX_NONE;
    if(!keyString.FindChar(TEXT('.'), dotIndex) || dotIndex <= 0){
        return nullptr;
    }
    const FName root(*keyString.Left(dotIndex));
    const TObjectPtr<UStringTable>* table = routedTables.Find(root);
    if(table == nullptr){
        return nullptr;
    }
    return table->Get();
}

E_LocalizationLookupOutcome ULocalizationService::classifyKey(FName key, const UStringTable*& foundTable) const{
    foundTable = nullptr;
    if(key.IsNone()){
        return E_LocalizationLookupOutcome::EmptyKey;
    }
    foundTable = findTableForKey(key);
    if(foundTable == nullptr){
        return E_LocalizationLookupOutcome::UnknownRoot;
    }
    // The default-language text is the String Table's source string. Empty counts as missing.
    FString defaultText;
    if(!foundTable->GetStringTable()->GetSourceString(FTextKey(key.ToString()), defaultText) || defaultText.IsEmpty()){
        return E_LocalizationLookupOutcome::MissingKey;
    }
    return E_LocalizationLookupOutcome::Found;
}

FText ULocalizationService::makeMissingText(FName key) const{
    // Never blank (Doc R10 section 3.1). Culture-invariant so it is never gathered for translation.
    return FText::AsCultureInvariant(FString::Printf(TEXT("[%s]"), *key.ToString()));
}

void ULocalizationService::logMissingOnce(FName key, E_LocalizationLookupOutcome outcome) const{
    if(loggedMissingKeys.Contains(key)){
        return;
    }
    loggedMissingKeys.Add(key);
    UE_LOG(LogAwakenLocalization, Warning, TEXT("[%s] Missing text for key '%s' (%s)."), *roleName(), *key.ToString(), *UEnum::GetValueAsString(outcome));
}

FS_LocalizationLookupResult ULocalizationService::lookupText(FName key) const{
    FS_LocalizationLookupResult result;
    result.key = key;

    const UStringTable* table = nullptr;
    result.outcome = classifyKey(key, table);
    if(result.outcome != E_LocalizationLookupOutcome::Found){
        result.text = makeMissingText(key);
        logMissingOnce(key, result.outcome);
        return result;
    }

    // A text object tied to the table and key. It re-resolves by itself when the language
    // changes, which is why bound widgets refresh without any code (Doc R10 section 3.5).
    result.text = FText::FromStringTable(table->GetStringTableId(), key.ToString());
    return result;
}

FText ULocalizationService::getText(FName key) const{
    return lookupText(key).text;
}

bool ULocalizationService::hasKey(FName key) const{
    const UStringTable* table = nullptr;
    return classifyKey(key, table) == E_LocalizationLookupOutcome::Found;
}

FS_LocalizationResult ULocalizationService::checkKey(FName key) const{
    FS_LocalizationResult result;
    result.cultureName = activeCulture;
    if(hasKey(key)){
        result.success = true;
        return result;
    }
    result.failReason = T_Localization_FailReason_MissingKey.GetTag();
    return result;
}

FText ULocalizationService::getReasonText(FGameplayTag reasonTag) const{
    const FName* textKey = reasonKeys.Find(reasonTag);
    if(textKey == nullptr){
        logMissingOnce(reasonTag.GetTagName(), E_LocalizationLookupOutcome::MissingKey);
        return FText::AsCultureInvariant(FString::Printf(TEXT("[%s]"), *reasonTag.ToString()));
    }
    return getText(*textKey);
}

// ---- Formatting ----

FText ULocalizationService::formatTextNamed(FName key, const FFormatNamedArguments& namedArguments) const{
    const FS_LocalizationLookupResult lookup = lookupText(key);
    if(lookup.outcome != E_LocalizationLookupOutcome::Found){
        return lookup.text;
    }
    return FText::Format(lookup.text, namedArguments);
}

FText ULocalizationService::formatText(FName key, const TArray<FS_LocalizationArgument>& arguments) const{
    FFormatNamedArguments namedArguments;
    for(const FS_LocalizationArgument& argument : arguments){
        const FString argumentName = argument.name.ToString();
        switch(argument.kind){
            case E_LocalizationArgumentKind::Text:{
                namedArguments.Add(argumentName, FFormatArgumentValue(argument.textValue));
                break;
            }
            case E_LocalizationArgumentKind::Integer:{
                namedArguments.Add(argumentName, FFormatArgumentValue(argument.integerValue));
                break;
            }
            case E_LocalizationArgumentKind::Float:{
                namedArguments.Add(argumentName, FFormatArgumentValue(argument.floatValue));
                break;
            }
            case E_LocalizationArgumentKind::Gender:{
                namedArguments.Add(argumentName, FFormatArgumentValue(toEngineGender(argument.genderValue)));
                break;
            }
            case E_LocalizationArgumentKind::Name:{
                // A player or host name inside a sentence (R10 3.6, Index A108, Q-05): shown as the platform wrote it, wrapped to keep its own direction.
                namedArguments.Add(argumentName, FFormatArgumentValue(wrapNameForSentence(argument.textValue)));
                break;
            }
        }
    }
    return formatTextNamed(key, namedArguments);
}

FText ULocalizationService::wrapNameForSentence(const FText& name){
    // U+2068 FIRST STRONG ISOLATE and U+2069 POP DIRECTIONAL ISOLATE. The name keeps its own direction whatever language the sentence is in,
    // and it does not change the order of the words around it. The text itself is not changed (a platform name is never translated).
    FString wrapped;
    wrapped.AppendChar(static_cast<TCHAR>(0x2068));
    wrapped += name.ToString();
    wrapped.AppendChar(static_cast<TCHAR>(0x2069));
    return FText::AsCultureInvariant(wrapped);
}

FText ULocalizationService::joinListPair(FName key, const FText& first, const FText& second) const{
    FFormatNamedArguments namedArguments;
    namedArguments.Add(TEXT("First"), first);
    namedArguments.Add(TEXT("Second"), second);
    return formatTextNamed(key, namedArguments);
}

FText ULocalizationService::formatList(const TArray<FText>& items) const{
    // Built from four format keys, the same way CLDR defines lists, so each language
    // chooses its own separators and its own "and". The code never decides a form.
    const int32 count = items.Num();
    if(count == 0){
        return FText::GetEmpty();
    }
    if(count == 1){
        return items[0];
    }
    if(count == 2){
        return joinListPair(FName(TEXT("Format.List.Two")), items[0], items[1]);
    }
    FText tail = joinListPair(FName(TEXT("Format.List.End")), items[count - 2], items[count - 1]);
    for(int32 index = count - 3; index >= 1; index--){
        tail = joinListPair(FName(TEXT("Format.List.Middle")), items[index], tail);
    }
    return joinListPair(FName(TEXT("Format.List.Start")), items[0], tail);
}

FText ULocalizationService::formatRealDateTime(const FDateTime& dateTime) const{
    // The only real-world date in the game (Doc R10 section 3.7). Culture-aware by the engine.
    return FText::AsDateTime(dateTime, EDateTimeStyle::Medium, EDateTimeStyle::Short);
}