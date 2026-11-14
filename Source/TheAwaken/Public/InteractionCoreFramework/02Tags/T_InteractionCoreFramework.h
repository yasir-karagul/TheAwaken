// Public/InteractionCoreFramework/02Tags/T_InteractionCoreFramework.h
// Declared natively, like every built framework. The project has no DefaultGameplayTags.ini: do not add one (guide Step 2).
// Already defined elsewhere, do not redefine: T_Interaction_Save (T_SaveFramework), T_Player_State_Incapacitated
// (T_AbilityActionFramework), T_Carry_State_Active (T_CarryEncumbranceFramework).
#pragma once

#include "NativeGameplayTags.h"

// Refusal reasons (B18 section 5, Doc R11 3.3a, Doc R13). Add one DT_ReasonText row per tag (Doc R10 3.9).
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FailReason_OutOfRange);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FailReason_NoLineOfSight);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FailReason_InUse);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FailReason_NotAllowedInState);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FailReason_HandsFull);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FailReason_BagFull);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FailReason_EnemyAware);   // shared with Doc R11 (Talk)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FailReason_Cooldown);     // shared with Doc R11 (Talk)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FailReason_HostOnly);     // shared with Doc R11 (Talk)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FailReason_Busy);         // shared with Doc R11 (Talk)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FailReason_Bagless);      // belongs to Doc R13, nothing declared it yet

// Messages (B18 section 6)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FocusChanged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_HoldStarted);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_HoldCancelled);
