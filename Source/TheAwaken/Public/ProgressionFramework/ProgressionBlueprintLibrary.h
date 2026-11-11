#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "ProgressionFramework/01Structs/S_ProgressionFramework.h"
#include "ProgressionBlueprintLibrary.generated.h"

UCLASS()
class THEAWAKEN_API UProgressionBlueprintLibrary : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // Pure queries: no execution pin, so a widget can call them in a binding or in Construct.
    UFUNCTION(BlueprintPure, Category = "Progression", meta = (WorldContext = "worldContextObject"))
    static bool isUnlocked(const UObject* worldContextObject, FGameplayTag tag);

    UFUNCTION(BlueprintPure, Category = "Progression", meta = (WorldContext = "worldContextObject"))
    static FGameplayTagContainer getAllUnlocked(const UObject* worldContextObject);

    // Callable, host only. On a guest it returns NotAuthority and changes nothing.
    UFUNCTION(BlueprintCallable, Category = "Progression", meta = (WorldContext = "worldContextObject"))
    static FS_ProgressionResult grantUnlock(const UObject* worldContextObject, FGameplayTag tag);
};
