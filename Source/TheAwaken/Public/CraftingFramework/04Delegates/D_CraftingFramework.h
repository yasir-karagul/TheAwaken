#pragma once

#include "CoreMinimal.h"
// FD_CraftingChanged (the one dynamic delegate of this framework) is declared at the top of S_CraftingFramework.h:
// UHT does not register a dynamic delegate that sits alone in a header with its own .generated.h (R01 guide S1.9 fallback).
#include "CraftingFramework/01Structs/S_CraftingFramework.h"
