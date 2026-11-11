#pragma once

#include "CoreMinimal.h"
#include "Misc/DataValidation.h"
#include "LocalizationFramework/00Enums/E_LocalizationFramework.h"

class UDA_LocalizationSettings;

// Editor and build checks for text keys (Doc R10 section 3.10).
struct THEAWAKEN_API FLocalizationKeyValidator{
#if WITH_EDITOR
    // How the key would resolve, with no game instance running.
    static E_LocalizationLookupOutcome findOutcome(FName key);

    // Adds an error to the context when the key is empty or has no default text.
    static EDataValidationResult validateKey(FName key, const FText& ownerLabel, FDataValidationContext& context);

    // Checks the settings asset, every routed String Table and DT_ReasonText.
    static EDataValidationResult validateSettings(const UDA_LocalizationSettings* settings, FDataValidationContext& context);

    // Logs every table key that no validated asset referenced. Lists, never deletes.
    static void reportUnusedKeys();
#endif
};
