#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

// Messages (Doc B14 section 4)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Weather_Changed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Weather_WaterFroze);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Weather_WaterThawed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Weather_SnowLevelChanged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Weather_RainFilledContainer);

// Refusal reasons (Doc02 6) [W-IG20]
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Weather_FailReason_NotAuthority);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Weather_FailReason_NotReady);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Weather_FailReason_InvalidArgument);
