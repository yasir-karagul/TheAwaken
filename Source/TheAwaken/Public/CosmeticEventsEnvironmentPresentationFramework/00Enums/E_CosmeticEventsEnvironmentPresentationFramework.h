#pragma once

#include "CoreMinimal.h"
#include "E_CosmeticEventsEnvironmentPresentationFramework.generated.h"

// Part C. Decides what rain and cold do to a body and how it washes blood (B21 C.1, B03).
UENUM(BlueprintType)
enum class E_WaterKind : uint8{
    River,
    Stream,
    Lake,
    Sea
};
