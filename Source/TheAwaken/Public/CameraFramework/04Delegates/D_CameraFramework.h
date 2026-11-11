#pragma once

#include "CoreMinimal.h"
#include "CameraFramework/00Enums/E_CameraFramework.h"

// FD_CameraViewModeChanged (old mode, new mode) is declared in 00Enums/E_CameraFramework.h, next to E_CameraViewMode.
// UHT does not register a dynamic delegate that sits alone in a header with its own .generated.h, so the declaration moved.
// This header stays so the Doc02 20.3 layout is complete and existing includes keep working.