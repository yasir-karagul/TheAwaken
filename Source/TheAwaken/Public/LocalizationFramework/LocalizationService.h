#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "Internationalization/Text.h"
#include "LocalizationFramework/00Enums/E_LocalizationFramework.h"
#include "LocalizationFramework/01Structs/S_LocalizationFramework.h"
#include "LocalizationService.generated.h"

class UDA_LocalizationSettings;
class UStringTable;
class UDataTable;

DECLARE_LOG_CATEGORY_EXTERN(LogAwakenLocalization, Log, All);

UCLASS()
class THEAWAKEN_API ULocalizationService : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;
    virtual bool ShouldCreateSubsystem(UObject* outer) const override;

    // ---- Commands ----
    UFUNCTION(BlueprintCallable, Category = "Localization")
    FS_LocalizationResult setCulture(FName cultureName);

    // ---- Queries (never mutate) ----
    UFUNCTION(BlueprintPure, Category = "Localization")
    FText getText(FName key) const;

    UFUNCTION(BlueprintPure, Category = "Localization")
    FS_LocalizationLookupResult lookupText(FName key) const;

    UFUNCTION(BlueprintPure, Category = "Localization")
    bool hasKey(FName key) const;

    // Development only: explains a missing key with Localization.FailReason.MissingKey.
    UFUNCTION(BlueprintPure, Category = "Localization")
    FS_LocalizationResult checkKey(FName key) const;

    UFUNCTION(BlueprintPure, Category = "Localization")
    FText formatText(FName key, const TArray<FS_LocalizationArgument>& arguments) const;

    // C++ only. FFormatNamedArguments is not a Blueprint type.
    FText formatTextNamed(FName key, const FFormatNamedArguments& namedArguments) const;

    UFUNCTION(BlueprintPure, Category = "Localization")
    FText formatList(const TArray<FText>& items) const;

    // For real-world times only: the save slot's last write time (Doc R10 section 3.7).
    UFUNCTION(BlueprintPure, Category = "Localization")
    FText formatRealDateTime(const FDateTime& dateTime) const;

    // DT_ReasonText: refusal reason tag to text (Doc R10 section 3.9).
    UFUNCTION(BlueprintPure, Category = "Localization")
    FText getReasonText(FGameplayTag reasonTag) const;

    UFUNCTION(BlueprintPure, Category = "Localization")
    FName getActiveCulture() const;

    // Raw defaultCulture from the settings asset. Applies no fallback logic.
    UFUNCTION(BlueprintPure, Category = "Localization")
    FName getDefaultCulture() const;

    // The language menu list, in settings order. Development-only entries are removed in shipping.
    UFUNCTION(BlueprintPure, Category = "Localization")
    TArray<FS_SupportedCultureEntry> getAvailableCultures() const;

    UFUNCTION(BlueprintPure, Category = "Localization")
    bool isCultureSupported(FName cultureName) const;

    // Fallback chain: the culture itself, then its regional parent, then the default.
    UFUNCTION(BlueprintPure, Category = "Localization")
    FName resolveEffectiveCulture(FName requestedCulture) const;

    // Audio asks this to choose between a voice and the default voice with subtitles.
    UFUNCTION(BlueprintPure, Category = "Localization")
    bool hasVoice(FName cultureName) const;

    // Host, Client or Offline, for log lines (Doc B22 section 3.6). Not a Blueprint node.
    FString roleName() const;

    // A player or host name inside a sentence, wrapped so that it keeps its own direction (Doc R10 section 3.6, Step 8).
    static FText wrapNameForSentence(const FText& name);

private:
    UPROPERTY()
    TObjectPtr<UDA_LocalizationSettings> settings;

    // Key root (for example "UI") to its loaded String Table.
    UPROPERTY()
    TMap<FName, TObjectPtr<UStringTable>> routedTables;

    UPROPERTY()
    TObjectPtr<UDataTable> reasonTable;

    // Built from reasonTable once at startup.
    UPROPERTY()
    TMap<FGameplayTag, FName> reasonKeys;

    UPROPERTY()
    FName activeCulture;

    // Log de-duplication only. It is bookkeeping, not state a caller can observe.
    mutable TSet<FName> loggedMissingKeys;

    bool loadSettings();
    void buildRouteTable();
    void buildReasonTable();
    FName readStartupCulture() const;
    const FS_SupportedCultureEntry* findEntry(FName cultureName) const;
    const UStringTable* findTableForKey(FName key) const;
    E_LocalizationLookupOutcome classifyKey(FName key, const UStringTable*& foundTable) const;
    FText joinListPair(FName key, const FText& first, const FText& second) const;
    FText makeMissingText(FName key) const;
    void logMissingOnce(FName key, E_LocalizationLookupOutcome outcome) const;
};
