#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "DA_StartingUnlocks.generated.h"

UCLASS(BlueprintType)
class THEAWAKEN_API UDA_StartingUnlocks : public UDataAsset{
    GENERATED_BODY()

public:
    // Unlocked at the start of every new world, silently (Doc R06 7). Story tags are ignored here:
    // a story part only ever comes through the story chain.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression", meta = (Categories = "Progression"))
    FGameplayTagContainer startingTags;
};
