#pragma once

#include "CoreMinimal.h"
#include "ItemInventoryFramework/InventoryLog.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAwakenCrafting, Log, All);

// Every line names the machine role (Host, Client, Offline). Use: CRAFTING_LOG(Log, this, "text %d", value);
#define CRAFTING_LOG(verbosity, worldContext, format, ...) \
    UE_LOG(LogAwakenCrafting, verbosity, TEXT("[%s] " format), *InventoryLog::roleName(worldContext), ##__VA_ARGS__)
