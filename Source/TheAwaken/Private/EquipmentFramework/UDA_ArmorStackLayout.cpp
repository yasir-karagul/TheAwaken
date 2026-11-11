#include "EquipmentFramework/UDA_ArmorStackLayout.h"
#include "EquipmentFramework/UEquipmentSettings.h"
#include "EquipmentFramework/02Tags/T_EquipmentFramework.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const FS_ArmorStackPosition* UDA_ArmorStackLayout::findPosition(int32 stackIndex) const{
    return positions.IsValidIndex(stackIndex) ? &positions[stackIndex] : nullptr;
}

#if WITH_EDITOR
EDataValidationResult UDA_ArmorStackLayout::IsDataValid(FDataValidationContext& Context) const{
    EDataValidationResult result = Super::IsDataValid(Context);
    const int32 cap = UEquipmentSettings::get()->armorStackCap;
    if(positions.Num() < cap){
        Context.AddWarning(FText::FromString(FString::Printf(TEXT("The layout has %d positions but armorStackCap is %d. Pieces past the last position have no place to show."), positions.Num(), cap)));
    }
    for(int32 index = 0; index < positions.Num(); index++){
        if(positions[index].positionTag != EquipmentTags::getStackPositionTag(index)){
            Context.AddError(FText::FromString(FString::Printf(TEXT("Entry %d must use the tag Equipment.ArmorStack.Position%02d."), index, index + 1)));
            result = EDataValidationResult::Invalid;
        }
        if(positions[index].socketName.IsNone()){
            Context.AddError(FText::FromString(FString::Printf(TEXT("Entry %d has no socket name."), index)));
            result = EDataValidationResult::Invalid;
        }
    }
    return result;
}
#endif
