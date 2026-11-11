#include "AbilityActionFramework/DA_ActionDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

// A2 step 5: fail on a missing actionTag or an abilityClass that does not exist.
EDataValidationResult UDA_ActionDefinition::IsDataValid(FDataValidationContext& context) const{
    EDataValidationResult result = Super::IsDataValid(context);
    if(!actionTag.IsValid()){
        context.AddError(FText::FromString(TEXT("actionTag is empty")));
        result = EDataValidationResult::Invalid;
    }
    if(abilityClass.IsNull()){
        context.AddError(FText::FromString(TEXT("abilityClass is empty")));
        result = EDataValidationResult::Invalid;
    }
    return result;
}
#endif
