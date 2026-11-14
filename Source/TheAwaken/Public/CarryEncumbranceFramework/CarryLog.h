#pragma once

#include "CoreMinimal.h"
#include "ItemInventoryFramework/InventoryLog.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAwakenCarry, Log, All);

// Every line names the machine role (Host, Client, Offline). Use: CARRY_LOG(Log, this, "text %d", value);
#define CARRY_LOG(verbosity, worldContext, format, ...) \
    UE_LOG(LogAwakenCarry, verbosity, TEXT("[%s] " format), *InventoryLog::roleName(worldContext), ##__VA_ARGS__)
