#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

// Roots. A grant must be a tag below one of these five (not the root itself).
// Discovery holds the NPC and location discoveries (Progression.Discovery.NPC.*, .Location.*). It is not part of the Peaceful bulk grant.
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Progression_Blueprint);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Progression_Recipe);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Progression_Ability);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Progression_Discovery);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Progression_Story);

// The story-complete flag and the message channel.
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Progression_Story_Complete);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Progression_UnlockGranted);

// Refusal reasons (PROPOSED, Doc02 section 6).
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Progression_FailReason_NotAuthority);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Progression_FailReason_NotReady);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Progression_FailReason_InvalidTag);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Progression_FailReason_AlreadyUnlocked);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Progression_FailReason_UnknownStoryPart);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Progression_FailReason_PrerequisiteMissing);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Progression_FailReason_TooEarly);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Progression_FailReason_QuestsDisabled);
