#pragma once

#include "CoreMinimal.h"

namespace InventoryTime{
    inline int64 hoursToGameSeconds(float hours){
        return FMath::RoundToInt64(static_cast<double>(hours) * 3600.0);
    }
}
