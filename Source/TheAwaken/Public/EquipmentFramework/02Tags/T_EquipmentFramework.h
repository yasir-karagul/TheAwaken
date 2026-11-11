#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

// ---- Messages (R05 §7; all five are in the Tag Registry) ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorEquipped);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorUnequipped);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorBroken);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ClothesChanged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_StatsRecalculated);

// ---- Stack positions (R05 §6: Equipment.ArmorStack.*, visual only). One leaf per position up to the cap of 14 ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position01);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position02);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position03);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position04);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position05);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position06);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position07);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position08);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position09);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position10);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position11);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position12);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position13);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position14);

// ---- Refusal reasons (Doc02 §6). All five are NEW and PROPOSED: add them to the Tag Registry ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_FailReason_StackFull);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_FailReason_NotArmor);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_FailReason_NotClothes);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_FailReason_AlreadyWorn);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_FailReason_InvalidStackIndex);

// ---- Tag OWNED BY ANOTHER FRAMEWORK: Doc B21's header exists, so it is included instead of duplicated (E10.8) ----
#include "CosmeticEventsEnvironmentPresentationFramework/02Tags/T_CosmeticEventsEnvironmentPresentationFramework.h"   // T_Cosmetic_Event_Combat_ArmorBroken

namespace EquipmentTags{
    // Query. The position tag of a stack index, or an empty tag past the last defined leaf.
    THEAWAKEN_API FGameplayTag getStackPositionTag(int32 stackIndex);
}
