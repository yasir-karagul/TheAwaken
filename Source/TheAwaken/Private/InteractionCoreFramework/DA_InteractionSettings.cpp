// Private/InteractionCoreFramework/DA_InteractionSettings.cpp
#include "InteractionCoreFramework/DA_InteractionSettings.h"
#include "InputAction.h"

FPrimaryAssetId UDA_InteractionSettings::makeAssetId(){
    return FPrimaryAssetId(FPrimaryAssetType(TEXT("InteractionSettings")), FName(TEXT("DA_InteractionSettings")));
}

FPrimaryAssetId UDA_InteractionSettings::GetPrimaryAssetId() const{
    return FPrimaryAssetId(FPrimaryAssetType(TEXT("InteractionSettings")), GetFName());
}

#if WITH_EDITOR
EDataValidationResult UDA_InteractionSettings::IsDataValid(FDataValidationContext& context) const{
    EDataValidationResult result = Super::IsDataValid(context);
    if(interactionRange <= 0.0f){
        context.AddError(FText::FromString(TEXT("interactionRange must be greater than zero.")));
        result = EDataValidationResult::Invalid;
    }
    if(sweepRadius <= 0.0f){
        context.AddError(FText::FromString(TEXT("sweepRadius must be greater than zero.")));
        result = EDataValidationResult::Invalid;
    }
    if(focusInterval <= 0.0f || lockWatchInterval <= 0.0f){
        context.AddError(FText::FromString(TEXT("focusInterval and lockWatchInterval must be greater than zero (a zero timer rate clears the timer).")));
        result = EDataValidationResult::Invalid;
    }
    if(rangeSlack < 0.0f){
        context.AddError(FText::FromString(TEXT("rangeSlack must not be negative.")));
        result = EDataValidationResult::Invalid;
    }
    if(interactAction == nullptr){
        context.AddError(FText::FromString(TEXT("interactAction (IA_Interact) is not set: interaction input cannot be bound.")));
        result = EDataValidationResult::Invalid;
    }
    return result;
}
#endif
