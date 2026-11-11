#include "ItemInventoryFramework/UDA_InventoryPageLayout.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const FS_InventorySlotDef* UDA_InventoryPageLayout::findSlot(FName slotId) const{
    return slots.FindByPredicate([slotId](const FS_InventorySlotDef& slot){
        return slot.slotId == slotId;
    });
}

#if WITH_EDITOR
EDataValidationResult UDA_InventoryPageLayout::IsDataValid(FDataValidationContext& Context) const{
    EDataValidationResult result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
    if(!pageTag.IsValid()){
        Context.AddError(FText::FromString(FString::Printf(TEXT("%s: the layout has no page tag."), *GetName())));
        result = EDataValidationResult::Invalid;
    }
    TSet<FName> seenIds;
    for(const FS_InventorySlotDef& slot : slots){
        if(slot.slotId.IsNone() || seenIds.Contains(slot.slotId)){
            Context.AddError(FText::FromString(FString::Printf(TEXT("%s: slot id '%s' is empty or used twice."), *GetName(), *slot.slotId.ToString())));
            result = EDataValidationResult::Invalid;
        }
        seenIds.Add(slot.slotId);
        if(slot.size.X <= 0.0 || slot.size.Y <= 0.0){
            Context.AddError(FText::FromString(FString::Printf(TEXT("%s: slot '%s' has no size."), *GetName(), *slot.slotId.ToString())));
            result = EDataValidationResult::Invalid;
        }
    }
    return result;
}
#endif
