#pragma once

#include "CoreMinimal.h"
#include "ItemInventoryFramework/InventoryLog.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAwakenEquipment, Log, All);

// Every line names the machine role (Host, Client, Offline). Use: EQUIPMENT_LOG(Log, this, "text %d", value);
#define EQUIPMENT_LOG(verbosity, worldContext, format, ...) \
    UE_LOG(LogAwakenEquipment, verbosity, TEXT("[%s] " format), *InventoryLog::roleName(worldContext), ##__VA_ARGS__)
