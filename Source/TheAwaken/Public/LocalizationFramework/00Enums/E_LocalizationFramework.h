#pragma once

#include "CoreMinimal.h"
#include "E_LocalizationFramework.generated.h"

UENUM(BlueprintType)
enum class E_LocalizationLookupOutcome : uint8{
    Found UMETA(DisplayName = "Found"),
    EmptyKey UMETA(DisplayName = "Empty Key"),
    UnknownRoot UMETA(DisplayName = "Unknown Root"),
    MissingKey UMETA(DisplayName = "Missing Key")
};

UENUM(BlueprintType)
enum class E_LocalizationArgumentKind : uint8{
    Text UMETA(DisplayName = "Text"),
    Integer UMETA(DisplayName = "Integer"),
    Float UMETA(DisplayName = "Float"),
    Gender UMETA(DisplayName = "Gender"),
    Name UMETA(DisplayName = "Name")
};

UENUM(BlueprintType)
enum class E_LocalizationGender : uint8{
    Masculine UMETA(DisplayName = "Masculine"),
    Feminine UMETA(DisplayName = "Feminine"),
    Neuter UMETA(DisplayName = "Neuter")
};
