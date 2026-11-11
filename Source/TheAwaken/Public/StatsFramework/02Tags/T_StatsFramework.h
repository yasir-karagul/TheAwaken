#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Stats_HPChanged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Stats_StaminaChanged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Stats_State_LowHealth);
// PROPOSED
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Stats_State_MedicineActive);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Stats_Source_Medicine);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Stats_Source_Regeneration);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Stats_Source_Sleep);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Stats_Source_Respawn);
// PROPOSED dev flag: Doc B22's Awaken.Player.God adds and removes this loose tag on the target's ASC.
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Stats_Dev_God);
