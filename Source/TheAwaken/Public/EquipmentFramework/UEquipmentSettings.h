#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UEquipmentSettings.generated.h"

class UDA_ArmorStackLayout;

// Project Settings -> Game -> Awaken Equipment. Every value is tuning data (Doc00 2.7).
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Awaken Equipment"))
class THEAWAKEN_API UEquipmentSettings : public UDeveloperSettings{
    GENERATED_BODY()

public:
    UEquipmentSettings();

    static const UEquipmentSettings* get(){
        return GetDefault<UEquipmentSettings>();
    }

    // R05 amendment (Q1 to Q21): 14, "a tuning value to be changed in testing".
    UPROPERTY(Config, EditAnywhere, Category = "Armor", meta = (ClampMin = "0")) int32 armorStackCap = 14;
    // Where each stack position shows on the body (R05 3.2). Visual only; read by Doc R07.
    UPROPERTY(Config, EditAnywhere, Category = "Armor") TSoftObjectPtr<UDA_ArmorStackLayout> armorStackLayout;
};
