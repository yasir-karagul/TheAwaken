#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EquipmentFramework/01Structs/S_EquipmentFramework.h"
#include "UDA_ArmorStackLayout.generated.h"

// Where each stack position shows on the body. Visual only: it never changes protection (R05 3.2).
UCLASS(BlueprintType)
class THEAWAKEN_API UDA_ArmorStackLayout : public UDataAsset{
    GENERATED_BODY()

public:
    // Entry 0 is where the first piece worn shows, entry 1 the second, and so on.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout") TArray<FS_ArmorStackPosition> positions;

    // Query. Null when the layout has no entry for that index.
    const FS_ArmorStackPosition* findPosition(int32 stackIndex) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
