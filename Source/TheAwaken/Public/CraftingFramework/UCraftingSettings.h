#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UCraftingSettings.generated.h"

// Project Settings -> Game -> Awaken Crafting. Every value is tuning data (Doc00 2.7).
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Awaken Crafting"))
class THEAWAKEN_API UCraftingSettings : public UDeveloperSettings{
    GENERATED_BODY()

public:
    UCraftingSettings();

    static const UCraftingSettings* get(){
        return GetDefault<UCraftingSettings>();
    }

    // R02 3.4: "At most 15 recipe entries are visible. 15 is data (PROPOSED name MaxVisibleRecipes)."
    UPROPERTY(Config, EditAnywhere, Category = "Craft List", meta = (ClampMin = "1")) int32 maxVisibleRecipes = 15;
    // How close the pawn's eye must be to a station to enter it (K20). Inclusive (Doc02 19.9). PROPOSED value;
    // Doc B18 3.7 owns the range at which the station's lock is released later.
    UPROPERTY(Config, EditAnywhere, Category = "Station", meta = (ClampMin = "0")) float stationUseRange = 250.0f;
};
