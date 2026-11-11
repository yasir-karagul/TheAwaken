#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CameraFramework/00Enums/E_CameraFramework.h"

// Log category from Doc B22 3.6.
// Every line names the machine role (Host, Client, Offline, or Local when it can come from any machine).
// Anything that repeats is Verbose. A refusal logs the same reason tag the UI would use.
DECLARE_LOG_CATEGORY_EXTERN(LogAwakenCamera, Log, All);

namespace CameraFramework{
    // Looks up a tag that another framework owns, by name, without the not-found error.
    // Logs a Warning when the tag is missing, so a framework that is not built yet does not stop this one (Doc00 4.2).
    FGameplayTag lookupExternalTag(FName tagName);

    // Text for log lines.
    FString viewModeToString(E_CameraViewMode mode);
}
