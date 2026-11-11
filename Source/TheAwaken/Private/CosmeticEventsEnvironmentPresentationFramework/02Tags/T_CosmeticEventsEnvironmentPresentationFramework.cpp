#include "CosmeticEventsEnvironmentPresentationFramework/02Tags/T_CosmeticEventsEnvironmentPresentationFramework.h"

UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Received, "Cosmetic.Received");

UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Combat_Hit, "Cosmetic.Event.Combat.Hit");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Combat_ArmorBroken, "Cosmetic.Event.Combat.ArmorBroken");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Craft_Started, "Cosmetic.Event.Craft.Started");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Craft_Finished, "Cosmetic.Event.Craft.Finished");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Interaction_Door, "Cosmetic.Event.Interaction.Door");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Interaction_Grab, "Cosmetic.Event.Interaction.Grab");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Interaction_ToggleChanged, "Cosmetic.Event.Interaction.ToggleChanged");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Interaction_Harvested, "Cosmetic.Event.Interaction.Harvested");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Interaction_ItemDropped, "Cosmetic.Event.Interaction.ItemDropped");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Interaction_NPCVoice, "Cosmetic.Event.Interaction.NPCVoice");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Building_PiecePlaced, "Cosmetic.Event.Building.PiecePlaced");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Building_LinkPlaced, "Cosmetic.Event.Building.LinkPlaced");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Building_PacketSent, "Cosmetic.Event.Building.PacketSent");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_AI_ThreatWarned, "Cosmetic.Event.AI.ThreatWarned");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_AI_ZoneCleared, "Cosmetic.Event.AI.ZoneCleared");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Carry_Picked, "Cosmetic.Event.Carry.Picked");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Carry_Dropped, "Cosmetic.Event.Carry.Dropped");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_World_Splash, "Cosmetic.Event.World.Splash");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_World_Break, "Cosmetic.Event.World.Break");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Survival_PlayerBodySound, "Cosmetic.Event.Survival.PlayerBodySound");

UE_DEFINE_GAMEPLAY_TAG(T_Water_Entered, "Water.Entered");
UE_DEFINE_GAMEPLAY_TAG(T_Water_Exited, "Water.Exited");

UE_DEFINE_GAMEPLAY_TAG(T_Environment_LightningFlash, "Environment.LightningFlash");

UE_DEFINE_GAMEPLAY_TAG(T_Environment_Ground_Grass, "Environment.Ground.Grass");
UE_DEFINE_GAMEPLAY_TAG(T_Environment_Ground_Mountain, "Environment.Ground.Mountain");
UE_DEFINE_GAMEPLAY_TAG(T_Environment_Ground_Lake, "Environment.Ground.Lake");
UE_DEFINE_GAMEPLAY_TAG(T_Environment_Ground_River, "Environment.Ground.River");
UE_DEFINE_GAMEPLAY_TAG(T_Environment_Ground_Sea, "Environment.Ground.Sea");

namespace CosmeticEnvironmentTags{

    // Built on first use, because a native tag has no value before the tag manager has finished adding native tags.
    const TArray<FGameplayTag>& getCosmeticEventTags(){
        static TArray<FGameplayTag> tags;
        if(tags.Num() == 0){
            tags.Add(T_Cosmetic_Event_Combat_Hit.GetTag());
            tags.Add(T_Cosmetic_Event_Combat_ArmorBroken.GetTag());
            tags.Add(T_Cosmetic_Event_Craft_Started.GetTag());
            tags.Add(T_Cosmetic_Event_Craft_Finished.GetTag());
            tags.Add(T_Cosmetic_Event_Interaction_Door.GetTag());
            tags.Add(T_Cosmetic_Event_Interaction_Grab.GetTag());
            tags.Add(T_Cosmetic_Event_Interaction_ToggleChanged.GetTag());
            tags.Add(T_Cosmetic_Event_Interaction_Harvested.GetTag());
            tags.Add(T_Cosmetic_Event_Interaction_ItemDropped.GetTag());
            tags.Add(T_Cosmetic_Event_Interaction_NPCVoice.GetTag());
            tags.Add(T_Cosmetic_Event_Building_PiecePlaced.GetTag());
            tags.Add(T_Cosmetic_Event_Building_LinkPlaced.GetTag());
            tags.Add(T_Cosmetic_Event_Building_PacketSent.GetTag());
            tags.Add(T_Cosmetic_Event_AI_ThreatWarned.GetTag());
            tags.Add(T_Cosmetic_Event_AI_ZoneCleared.GetTag());
            tags.Add(T_Cosmetic_Event_Carry_Picked.GetTag());
            tags.Add(T_Cosmetic_Event_Carry_Dropped.GetTag());
            tags.Add(T_Cosmetic_Event_World_Splash.GetTag());
            tags.Add(T_Cosmetic_Event_World_Break.GetTag());
            tags.Add(T_Cosmetic_Event_Survival_PlayerBodySound.GetTag());
        }
        return tags;
    }

    const TArray<FGameplayTag>& getGroundTags(){
        static TArray<FGameplayTag> tags;
        if(tags.Num() == 0){
            tags.Add(T_Environment_Ground_Grass.GetTag());
            tags.Add(T_Environment_Ground_Mountain.GetTag());
            tags.Add(T_Environment_Ground_Lake.GetTag());
            tags.Add(T_Environment_Ground_River.GetTag());
            tags.Add(T_Environment_Ground_Sea.GetTag());
        }
        return tags;
    }
}
