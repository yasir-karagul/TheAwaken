#include "EquipmentFramework/02Tags/T_EquipmentFramework.h"
#include "GameplayTagContainer.h"

UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorEquipped, "Equipment.ArmorEquipped");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorUnequipped, "Equipment.ArmorUnequipped");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorBroken, "Equipment.ArmorBroken");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ClothesChanged, "Equipment.ClothesChanged");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_StatsRecalculated, "Equipment.StatsRecalculated");

UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position01, "Equipment.ArmorStack.Position01");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position02, "Equipment.ArmorStack.Position02");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position03, "Equipment.ArmorStack.Position03");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position04, "Equipment.ArmorStack.Position04");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position05, "Equipment.ArmorStack.Position05");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position06, "Equipment.ArmorStack.Position06");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position07, "Equipment.ArmorStack.Position07");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position08, "Equipment.ArmorStack.Position08");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position09, "Equipment.ArmorStack.Position09");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position10, "Equipment.ArmorStack.Position10");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position11, "Equipment.ArmorStack.Position11");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position12, "Equipment.ArmorStack.Position12");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position13, "Equipment.ArmorStack.Position13");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position14, "Equipment.ArmorStack.Position14");

UE_DEFINE_GAMEPLAY_TAG(T_Equipment_FailReason_StackFull, "Equipment.FailReason.StackFull");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_FailReason_NotArmor, "Equipment.FailReason.NotArmor");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_FailReason_NotClothes, "Equipment.FailReason.NotClothes");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_FailReason_AlreadyWorn, "Equipment.FailReason.AlreadyWorn");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_FailReason_InvalidStackIndex, "Equipment.FailReason.InvalidStackIndex");


FGameplayTag EquipmentTags::getStackPositionTag(int32 stackIndex){
    if(stackIndex < 0){
        return FGameplayTag();
    }
    const FName tagName(*FString::Printf(TEXT("Equipment.ArmorStack.Position%02d"), stackIndex + 1));
    return FGameplayTag::RequestGameplayTag(tagName, false);   // false = no error when the leaf does not exist
}
