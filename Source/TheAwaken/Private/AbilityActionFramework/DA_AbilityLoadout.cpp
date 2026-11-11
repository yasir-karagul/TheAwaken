#include "AbilityActionFramework/DA_AbilityLoadout.h"
#include "AbilityActionFramework/DA_ActionDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

// A2 step 5: fail on a missing actionTag, a duplicate actionTag or a missing definition.
EDataValidationResult UDA_AbilityLoadout::IsDataValid(FDataValidationContext& context) const{
    EDataValidationResult result = Super::IsDataValid(context);
    TSet<FGameplayTag> seen;
    for(const FS_AbilityLoadoutEntry& entry : entries){
        if(!entry.actionTag.IsValid()){
            context.AddError(FText::FromString(TEXT("A loadout entry has an empty actionTag")));
            result = EDataValidationResult::Invalid;
            continue;
        }
        if(seen.Contains(entry.actionTag)){
            context.AddError(FText::FromString(FString::Printf(TEXT("Duplicate actionTag %s"), *entry.actionTag.ToString())));
            result = EDataValidationResult::Invalid;
        }
        seen.Add(entry.actionTag);
        if(entry.definition == nullptr){
            context.AddError(FText::FromString(FString::Printf(TEXT("Entry %s has no definition"), *entry.actionTag.ToString())));
            result = EDataValidationResult::Invalid;
        }
    }
    return result;
}
#endif
