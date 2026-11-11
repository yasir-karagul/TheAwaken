#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Misc/DataValidation.h"
#include "LocalizationFramework/01Structs/S_LocalizationFramework.h"
#include "DA_LocalizationSettings.generated.h"

UCLASS(BlueprintType)
class THEAWAKEN_API UDA_LocalizationSettings : public UPrimaryDataAsset{
    GENERATED_BODY()

public:
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
    static FPrimaryAssetId makeAssetId();

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& context) const override;
#endif

    // The game's default language. Must equal the Dashboard native culture: the engine falls back to that language's source text.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization")
    FName defaultCulture = TEXT("en");

    // Every language the game offers, in the order the language menu shows them.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization")
    TArray<FS_SupportedCultureEntry> supportedCultures;

    // Key root to String Table. One row per root.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization")
    TArray<FS_LocalizationTableRoute> tableRoutes;

    // DT_ReasonText: refusal reason tag to text key.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Localization")
    TSoftObjectPtr<UDataTable> reasonTextTable;
};
