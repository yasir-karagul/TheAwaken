#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAwakenInventory, Log, All);

namespace InventoryLog{
    // "Host", "Client" or "Offline" (Doc B22 §3.6: every log line names the role of the machine).
    THEAWAKEN_API FString roleName(const UObject* worldContext);
}

// Use: INVENTORY_LOG(Log, this, "text %d", value);
#define INVENTORY_LOG(verbosity, worldContext, format, ...) \
    UE_LOG(LogAwakenInventory, verbosity, TEXT("[%s] " format), *InventoryLog::roleName(worldContext), ##__VA_ARGS__)
