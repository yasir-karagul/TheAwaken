// Public/InteractionFrameworkExtension/02Tags/T_InteractionFrameworkExtension.h
// Doc B11 3.1 and B18 section 5. Interaction.Save lives in T_SaveFramework. Interaction.Revive, Escape and Rope are declared HERE:
// tell the B02, B09 and B11 owners to use these and not declare their own (a second definition with the same C++ name will not link).
#pragma once

#include "NativeGameplayTags.h"

// Type tags
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_Door);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_Window);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_Fence);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_Furniture);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_Container);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FireSource);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_Machine);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_Generator);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_WorldObject);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_Throwable);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_Placeable);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_Escape);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_Revive);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_Climb);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_Rope);

// Messages (B11 4, B18 section 6)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_HoldToActivateCompleted);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_ToggleStateChanged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_GrabBegun);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_GrabEnded);
