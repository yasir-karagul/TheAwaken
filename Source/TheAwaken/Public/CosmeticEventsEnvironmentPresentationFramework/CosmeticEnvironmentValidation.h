#pragma once

#include "CoreMinimal.h"

class UDataTable;
class UDA_EnvironmentSettings;

// The automatic data checks of B21 (Doc02 19.8). Same shape as WeatherValidation: returns true when there are no errors, warnings do not fail.
// The editor data validation (IsDataValid of each asset and placed actor) calls these, which is what fails the build.
namespace CosmeticEnvironmentValidation{
    // A declared cosmetic leaf tag with no row, a row name that is not a declared full tag string (other than Default), a row with a radius of 0
    // that does not read the hearing distance, and a row that allows fewer than one event a second.
    bool validateCosmeticEventTable(const UDataTable* table, TArray<FString>& errors, TArray<FString>& warnings);

    // blendSeconds of 0 or less, a rate of 0, a lightning range that runs backwards, a missing table.
    bool validateSettings(const UDA_EnvironmentSettings* settings, TArray<FString>& errors, TArray<FString>& warnings);

    // A weather type with no DA_WeatherPresentation, or two for the same type.
    bool validateWeatherPresentations(TArray<FString>& errors, TArray<FString>& warnings);

    // The row name for a tag: the full tag string. The underscore form is accepted when the table import rejects dots.
    FName makeRowName(const FString& tagString);
    FName makeUnderscoreRowName(const FString& tagString);
}
