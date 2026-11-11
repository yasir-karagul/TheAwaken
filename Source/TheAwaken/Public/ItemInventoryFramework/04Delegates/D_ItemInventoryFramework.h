#pragma once

#include "CoreMinimal.h"
// The four dynamic (Blueprint) delegates of this framework are declared at the top of S_ItemInventoryFramework.h:
// UHT does not register a dynamic delegate that sits alone in a header with its own .generated.h (the fallback of guide S1.9).
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"

class UInventoryData;

// ---- C++ only ----
DECLARE_MULTICAST_DELEGATE_OneParam(FD_InventoryDataChanged, UInventoryData*);
// A staged item was lost because its stack spoiled (R01 3.2a). Doc R02 listens and refuses the craft.
DECLARE_MULTICAST_DELEGATE_TwoParams(FD_StagedItemLost, const FGuid& /*stationId*/, FName /*itemId*/);
