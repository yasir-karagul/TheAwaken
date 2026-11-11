#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_Dodge);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_Sprint);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_WeaponCharge);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_Crouch);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_Block);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_Reload);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_Started);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_Ended);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_State);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_State_DodgeInvulnerable);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_State_SprintExhausted);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_Cooldown);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_Cooldown_Dodge);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Player_State_Incapacitated);
// A11: marks the one non-illness long effect kind that is saved (PROPOSED).
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Effect_Saved);
// A12 cue tags (PROPOSED). Sprint and Crouch have none.
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_GameplayCue_Action_Dodge);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_GameplayCue_Action_Block);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_GameplayCue_Action_WeaponCharge);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_GameplayCue_Action_Reload);
