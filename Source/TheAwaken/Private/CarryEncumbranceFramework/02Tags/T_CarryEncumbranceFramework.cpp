#include "CarryEncumbranceFramework/02Tags/T_CarryEncumbranceFramework.h"

UE_DEFINE_GAMEPLAY_TAG(T_Carry_Began, "Carry.Began");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_Ended, "Carry.Ended");

UE_DEFINE_GAMEPLAY_TAG(T_Carry_State_Active, "Carry.State.Active");

UE_DEFINE_GAMEPLAY_TAG(T_Carry_WeightClass_Light, "Carry.WeightClass.Light");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_WeightClass_Heavy, "Carry.WeightClass.Heavy");

UE_DEFINE_GAMEPLAY_TAG(T_Carry_FailReason_NotCarryable, "Carry.FailReason.NotCarryable");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_FailReason_SlotEmpty, "Carry.FailReason.SlotEmpty");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_FailReason_CannotThrow, "Carry.FailReason.CannotThrow");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_FailReason_InvalidThrow, "Carry.FailReason.InvalidThrow");

UE_DEFINE_GAMEPLAY_TAG(T_Carry_InterruptCause_Downed, "Carry.InterruptCause.Downed");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_InterruptCause_Death, "Carry.InterruptCause.Death");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_InterruptCause_Restrained, "Carry.InterruptCause.Restrained");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_InterruptCause_PlayerLeft, "Carry.InterruptCause.PlayerLeft");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_InterruptCause_Swim, "Carry.InterruptCause.Swim");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_InterruptCause_Sleep, "Carry.InterruptCause.Sleep");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_InterruptCause_PawnRemoved, "Carry.InterruptCause.PawnRemoved");

TArray<FGameplayTag> CarryTags::getActionsEndedByCarry(){
    return {T_Action_Dodge, T_Action_Sprint, T_Action_Block, T_Action_Reload, T_Action_WeaponCharge};
}
