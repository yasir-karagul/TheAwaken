#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
// Owners' tag headers that already exist, included instead of redefining their tags (CR1.3 rule):
#include "InteractionCoreFramework/02Tags/T_InteractionCoreFramework.h"   // T_Interaction_FailReason_InUse, HandsFull, NotAllowedInState (Doc B18)
#include "CosmeticEventsEnvironmentPresentationFramework/02Tags/T_CosmeticEventsEnvironmentPresentationFramework.h"   // T_Cosmetic_Event_Craft_Started / _Finished (Doc B21 declares the R02 leaves)

// ---- Messages (R02 §7) ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_SessionOpened);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_ItemCrafted);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_SessionClosed);

// ---- Refusal reasons (R02 §5). The last three are PROPOSED names in the spec ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_FailReason_StationRequired);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_FailReason_BlueprintLocked);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_FailReason_InsufficientStaged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_FailReason_BagRequired);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_FailReason_CraftInProgress);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_FailReason_ExtraIngredients);

// ---- Tags OWNED BY ANOTHER FRAMEWORK, referenced here (see the stub register, CR9.8) ----
// TEMPORARY duplicates: remove the definitions here when the owner's tag header exists and include that header instead.
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Station_CraftingTable);   // Docs B01, B10 (the shared Station.* namespace)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Station_Forge);           // Docs B01, B10
