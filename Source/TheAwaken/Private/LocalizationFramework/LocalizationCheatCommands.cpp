#include "LocalizationFramework/LocalizationService.h"

#if !UE_BUILD_SHIPPING

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

namespace{
    ULocalizationService* serviceOf(UWorld* world){
        UGameInstance* gameInstance = world != nullptr ? world->GetGameInstance() : nullptr;
        return gameInstance != nullptr ? gameInstance->GetSubsystem<ULocalizationService>() : nullptr;
    }

    void reportResult(const ULocalizationService* localization, const TCHAR* command, const FS_LocalizationResult& result){
        if(result.success){
            UE_LOG(LogAwakenLocalization, Log, TEXT("[%s] %s done: the language is '%s'."), *localization->roleName(), command, *localization->getActiveCulture().ToString());
        }else{
            UE_LOG(LogAwakenLocalization, Warning, TEXT("[%s] %s refused: %s"), *localization->roleName(), command, *result.failReason.ToString());
        }
    }

    FName findPseudoCulture(const ULocalizationService* localization){
        for(const FS_SupportedCultureEntry& entry : localization->getAvailableCultures()){
            if(entry.isDevelopmentOnly){
                return entry.cultureName;
            }
        }
        return NAME_None;
    }

    FAutoConsoleCommandWithWorldAndArgs cmdLanguageSet(
        TEXT("Awaken.Language.Set"),
        TEXT("Awaken.Language.Set code. Changes this machine's language only, for example Awaken.Language.Set tr."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            ULocalizationService* localization = serviceOf(world);
            if(localization == nullptr || args.Num() < 1){
                UE_LOG(LogAwakenLocalization, Warning, TEXT("Usage: Awaken.Language.Set code (and a game instance must exist)."));
                return;
            }
            reportResult(localization, TEXT("Awaken.Language.Set"), localization->setCulture(FName(*args[0])));
        }));

    FAutoConsoleCommandWithWorldAndArgs cmdLanguagePseudo(
        TEXT("Awaken.Language.Pseudo"),
        TEXT("Awaken.Language.Pseudo. Switches this machine to the pseudo-localization culture, and back on the second call."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            static FName cultureBeforePseudo;
            ULocalizationService* localization = serviceOf(world);
            const FName pseudoCulture = localization != nullptr ? findPseudoCulture(localization) : NAME_None;
            if(localization == nullptr || pseudoCulture.IsNone()){
                UE_LOG(LogAwakenLocalization, Warning, TEXT("Awaken.Language.Pseudo: no culture is marked isDevelopmentOnly in DA_LocalizationSettings (Step 12.5)."));
                return;
            }
            if(localization->getActiveCulture() == pseudoCulture){
                const FName target = cultureBeforePseudo.IsNone() ? localization->getDefaultCulture() : cultureBeforePseudo;
                reportResult(localization, TEXT("Awaken.Language.Pseudo"), localization->setCulture(target));
            }else{
                cultureBeforePseudo = localization->getActiveCulture();
                reportResult(localization, TEXT("Awaken.Language.Pseudo"), localization->setCulture(pseudoCulture));
            }
        }));

    FAutoConsoleCommandWithWorldAndArgs cmdCheckKey(
        TEXT("Awaken.Localization.CheckKey"),
        TEXT("Awaken.Localization.CheckKey key. Resolves a key on this machine and logs how it ended."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& args, UWorld* world){
            const ULocalizationService* localization = serviceOf(world);
            if(localization == nullptr || args.Num() < 1){
                UE_LOG(LogAwakenLocalization, Warning, TEXT("Usage: Awaken.Localization.CheckKey key"));
                return;
            }
            const FS_LocalizationLookupResult lookup = localization->lookupText(FName(*args[0]));
            UE_LOG(LogAwakenLocalization, Log, TEXT("[%s] CheckKey '%s' in '%s': %s. Text: %s"), *localization->roleName(), *args[0], *localization->getActiveCulture().ToString(), *UEnum::GetValueAsString(lookup.outcome), *lookup.text.ToString());
        }));
}

#endif
