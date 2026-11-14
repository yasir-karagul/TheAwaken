// Private/InteractionCoreFramework/02Tags/T_InteractionCoreFramework.cpp
#include "InteractionCoreFramework/02Tags/T_InteractionCoreFramework.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_FailReason_OutOfRange,        "Interaction.FailReason.OutOfRange",        "The target is farther than the interaction range");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_FailReason_NoLineOfSight,     "Interaction.FailReason.NoLineOfSight",     "Something solid is between the player's eye and the target");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_FailReason_InUse,             "Interaction.FailReason.InUse",             "Another player holds an Exclusive lock on the target");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_FailReason_NotAllowedInState, "Interaction.FailReason.NotAllowedInState", "The player's state does not allow interaction (Downed, Restrained, in a scene)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_FailReason_HandsFull,         "Interaction.FailReason.HandsFull",         "A carry slot is occupied and this interaction needs free hands");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_FailReason_BagFull,           "Interaction.FailReason.BagFull",           "A pickup was refused because the bag is at its limit for that item type");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_FailReason_EnemyAware,        "Interaction.FailReason.EnemyAware",        "An enemy is aware of the player (Talk, Sleep, Save gates)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_FailReason_Cooldown,          "Interaction.FailReason.Cooldown",          "A short time must pass after the last conversation (Talk)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_FailReason_HostOnly,          "Interaction.FailReason.HostOnly",          "Only the host can talk to this story NPC");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_FailReason_Busy,              "Interaction.FailReason.Busy",              "Another player is talking to this NPC");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_FailReason_Bagless,           "Interaction.FailReason.Bagless",           "A Bagless player cannot use this (Save Point, Doc R13)");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_FocusChanged,  "Interaction.FocusChanged",  "The focused target or its prompt changed. Delivery: local to the owning player (Doc02 19.2)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_HoldStarted,   "Interaction.HoldStarted",   "A hold began. Delivery: server, then local to the owning player");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_HoldCancelled, "Interaction.HoldCancelled", "A hold ended early; see the cancel reason. Delivery: server, then local to the owning player");
