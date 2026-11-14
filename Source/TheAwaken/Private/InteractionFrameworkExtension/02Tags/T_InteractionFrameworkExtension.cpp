// Private/InteractionFrameworkExtension/02Tags/T_InteractionFrameworkExtension.cpp
#include "InteractionFrameworkExtension/02Tags/T_InteractionFrameworkExtension.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_Door,        "Interaction.Door",        "Toggle: open or closed (Doc B11 3.1)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_Window,      "Interaction.Window",      "One-way toggle: break is a single irreversible change");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_Fence,       "Interaction.Fence",       "One-way: cut consumes the object");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_Furniture,   "Interaction.Furniture",   "Grabbable: continuous reposition");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_Container,   "Interaction.Container",   "Opens the inventory screen; Inventory owns everything after (Doc R01)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_FireSource,  "Interaction.FireSource",  "Toggle: lit or unlit, gates Processing (Doc B10 3.3)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_Machine,     "Interaction.Machine",     "Toggle: on or off");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_Generator,   "Interaction.Generator",   "Toggle: on or off, consumes fuel over time");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_WorldObject, "Interaction.WorldObject", "Generic pickup");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_Throwable,   "Interaction.Throwable",   "Reuses the throw pipeline (Doc B07 3.4)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_Placeable,   "Interaction.Placeable",   "Small carried objects placed directly; not a Building piece");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_Escape,      "Interaction.Escape",      "Hold to escape while Restrained (Doc B02 4.5). The one hold that damage does not cancel");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_Revive,      "Interaction.Revive",      "Hold to revive a Downed teammate (Doc B02 3.5)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_Climb,       "Interaction.Climb",       "Press: the Climb prompt only. The climbing is Doc R07 3.7");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_Rope,        "Interaction.Rope",        "Press: ride a line or send a carried item (Doc B09 3.11)");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_HoldToActivateCompleted, "Interaction.HoldToActivateCompleted", "A hold finished. Delivery: server only. Listeners: Doc B03 (Sleep), Doc B08 (Harvest)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_ToggleStateChanged,      "Interaction.ToggleStateChanged",      "Doc B11: a toggle changed state. Raised from replicated state on every machine");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_GrabBegun,               "Interaction.GrabBegun",               "Doc B11: a grab began. Server only");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_GrabEnded,               "Interaction.GrabEnded",               "Doc B11: a grab ended. Server only");
