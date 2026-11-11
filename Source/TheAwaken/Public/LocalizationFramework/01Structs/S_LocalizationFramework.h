#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "Internationalization/StringTable.h"
#include "LocalizationFramework/00Enums/E_LocalizationFramework.h"
#include "S_LocalizationFramework.generated.h"

// Result of a command (Doc02 section 6: explained rejection, never a bare false).
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_LocalizationResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Localization")
    bool success = false;

    UPROPERTY(BlueprintReadOnly, Category = "Localization")
    FGameplayTag failReason;

    UPROPERTY(BlueprintReadOnly, Category = "Localization")
    FName cultureName;
};

// Result of one key lookup. Text is always filled, never blank (Doc R10 section 3.1).
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_LocalizationLookupResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Localization")
    FName key;

    UPROPERTY(BlueprintReadOnly, Category = "Localization")
    FText text;

    UPROPERTY(BlueprintReadOnly, Category = "Localization")
    E_LocalizationLookupOutcome outcome = E_LocalizationLookupOutcome::Found;
};

// One named argument for a format key, for example {ItemName} or {Count}.
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_LocalizationArgument{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localization")
    FName name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localization")
    E_LocalizationArgumentKind kind = E_LocalizationArgumentKind::Text;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localization")
    FText textValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localization")
    int64 integerValue = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localization")
    double floatValue = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Localization")
    E_LocalizationGender genderValue = E_LocalizationGender::Masculine;
};

// Routes the first part of a key (the root, for example "UI") to its String Table.
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_LocalizationTableRoute{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization")
    FName keyRoot;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization")
    TSoftObjectPtr<UStringTable> table;
};

// One language the game offers. Entries live in DA_LocalizationSettings.
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_SupportedCultureEntry{
    GENERATED_BODY()

    // Engine culture code, for example "en", "tr", "pt-BR".
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization")
    FName cultureName;

    // The language's own name, typed in the data asset (for example the Turkish name). Never translated, so it is a plain string.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization")
    FString nativeName;

    // Voice availability. R10 item 87: no language has voice yet.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization")
    bool hasVoice = false;

    // True for the pseudo-localization test culture. Hidden in shipping builds.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization")
    bool isDevelopmentOnly = false;
};

// One row of DT_ReasonText: a refusal reason tag to a text key (Doc R10 section 3.9).
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_ReasonTextRow : public FTableRowBase{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization")
    FGameplayTag reasonTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization")
    FName textKey;
};

// Payload of the Localization.CultureChanged message.
USTRUCT(BlueprintType)
struct THEAWAKEN_API FS_LocalizationCultureChanged{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Localization")
    FName cultureName;

    UPROPERTY(BlueprintReadOnly, Category = "Localization")
    FName previousCultureName;
};