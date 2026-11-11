#pragma once

#include "CoreMinimal.h"

class UDataTable;
class UDA_WeatherTuning;

namespace WeatherValidation{
    // Returns true when there are no errors. Warnings do not fail the check.
    bool validateWeatherData(const UDataTable* seasonTable, const UDA_WeatherTuning* tuning, TArray<FString>& errors, TArray<FString>& warnings);
}
