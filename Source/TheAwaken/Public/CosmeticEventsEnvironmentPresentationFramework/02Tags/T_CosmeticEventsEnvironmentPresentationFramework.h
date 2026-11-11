#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

// The message the channel raises on every machine (B21 A.3).
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Received);

// Cosmetic event leaves. Only the leaves that the Tag Registry names are declared. A new effect is a leaf and a data row, not code.
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Combat_Hit);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Combat_ArmorBroken);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Craft_Started);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Craft_Finished);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Interaction_Door);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Interaction_Grab);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Interaction_ToggleChanged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Interaction_Harvested);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Interaction_ItemDropped);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Interaction_NPCVoice);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Building_PiecePlaced);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Building_LinkPlaced);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Building_PacketSent);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_AI_ThreatWarned);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_AI_ZoneCleared);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Carry_Picked);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Carry_Dropped);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_World_Splash);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_World_Break);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Survival_PlayerBodySound);

// Part C messages (server only).
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Water_Entered);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Water_Exited);

// Part B message (local on every machine).
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Environment_LightningFlash);

// Ground type tags of the map B17 reads (PROPOSED, add Environment.Ground.* to the Tag Registry).
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Environment_Ground_Grass);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Environment_Ground_Mountain);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Environment_Ground_Lake);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Environment_Ground_River);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Environment_Ground_Sea);

namespace CosmeticEnvironmentTags{
    // Every Cosmetic.Event leaf declared above. The data checks compare DT_CosmeticEvents against this list.
    const TArray<FGameplayTag>& getCosmeticEventTags();

    // Every Environment.Ground leaf declared above.
    const TArray<FGameplayTag>& getGroundTags();
}
