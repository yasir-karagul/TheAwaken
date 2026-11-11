#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "LocalizationFramework/01Structs/S_LocalizationFramework.h"
#include "LocalizationBlueprintLibrary.generated.h"

UCLASS()
class THEAWAKEN_API ULocalizationBlueprintLibrary : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category = "Localization", meta = (WorldContext = "worldContextObject"))
    static FText getLocalizedText(const UObject* worldContextObject, FName key);

    UFUNCTION(BlueprintPure, Category = "Localization", meta = (WorldContext = "worldContextObject"))
    static FText formatLocalizedText(const UObject* worldContextObject, FName key, const TArray<FS_LocalizationArgument>& arguments);

    UFUNCTION(BlueprintPure, Category = "Localization", meta = (WorldContext = "worldContextObject"))
    static FText getLocalizedReason(const UObject* worldContextObject, FGameplayTag reasonTag);

    UFUNCTION(BlueprintPure, Category = "Localization", meta = (WorldContext = "worldContextObject"))
    static FName getCurrentCulture(const UObject* worldContextObject);

    UFUNCTION(BlueprintCallable, Category = "Localization", meta = (WorldContext = "worldContextObject"))
    static FS_LocalizationResult setGameCulture(const UObject* worldContextObject, FName cultureName);

    // Argument builders so a Blueprint does not need a Make Struct node per argument.
    UFUNCTION(BlueprintPure, Category = "Localization")
    static FS_LocalizationArgument makeTextArgument(FName name, FText value);

    UFUNCTION(BlueprintPure, Category = "Localization")
    static FS_LocalizationArgument makeIntegerArgument(FName name, int64 value);

    UFUNCTION(BlueprintPure, Category = "Localization")
    static FS_LocalizationArgument makeFloatArgument(FName name, double value);

    UFUNCTION(BlueprintPure, Category = "Localization")
    static FS_LocalizationArgument makeGenderArgument(FName name, E_LocalizationGender value);

    // A player or host name inside a sentence. Use this, not Make Text Argument, so the name keeps its own direction (R10 3.6).
    UFUNCTION(BlueprintPure, Category = "Localization")
    static FS_LocalizationArgument makeNameArgument(FName name, FText value);
};
