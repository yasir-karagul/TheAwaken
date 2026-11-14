#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
// Tags OWNED BY OTHER FRAMEWORKS that Carry uses, from their owners' headers (never redefined here):
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"                                         // T_Action_Dodge/Sprint/Block/Reload/WeaponCharge (Doc R04)
#include "CosmeticEventsEnvironmentPresentationFramework/02Tags/T_CosmeticEventsEnvironmentPresentationFramework.h"   // T_Cosmetic_Event_Carry_Picked/Dropped (Doc B21 holds the definitions)

// ---- Messages (B07 §9; both are in the Tag Registry) ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_Began);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_Ended);

// ---- State (B07 §6): present while any carry slot is occupied. Read by Abilities, Interaction, Stats and the PlayerState ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_State_Active);

// ---- Weight classes (B07 §6). Heavy = a carryCapacityCost above 0; Light = a cost of 0 ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_WeightClass_Light);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_WeightClass_Heavy);

// ---- Refusal reasons (Doc02 §6). All four are NEW and PROPOSED: add them to the Tag Registry (C21) ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_FailReason_NotCarryable);   // the item has no carry cost, or the actor is not carryable
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_FailReason_SlotEmpty);      // nothing is carried in that slot
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_FailReason_CannotThrow);    // a carried actor can only be put down (C10)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_FailReason_InvalidThrow);   // the requested direction failed the host's check (C9)

// ---- Interrupt causes (C14). All NEW and PROPOSED: add them to the Tag Registry ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_InterruptCause_Downed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_InterruptCause_Death);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_InterruptCause_Restrained);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_InterruptCause_PlayerLeft);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_InterruptCause_Swim);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_InterruptCause_Sleep);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_InterruptCause_PawnRemoved);   // the safety net of C13

namespace CarryTags{
    // Query. The actions a beginning carry ends (B07 3.2, R04-R08-IG25): Dodge, Sprint, Block, Reload, Weapon Charge.
    THEAWAKEN_API TArray<FGameplayTag> getActionsEndedByCarry();
}
