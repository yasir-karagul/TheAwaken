#include "ItemInventoryFramework/UDA_StackLayout.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

int32 UDA_StackLayout::getVisibleUnitCount(int32 quantity) const{
    return FMath::Clamp(quantity, 0, units.Num());
}

#if WITH_EDITOR
EDataValidationResult UDA_StackLayout::IsDataValid(FDataValidationContext& Context) const{
    EDataValidationResult result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
    if(units.Num() == 0){
        Context.AddError(FText::FromString(FString::Printf(TEXT("%s: a stack layout needs at least one unit."), *GetName())));
        result = EDataValidationResult::Invalid;
    }
    return result;
}
#endif
