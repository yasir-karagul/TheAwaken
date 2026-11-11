#include "AbilityActionFramework/GE_State_SprintExhausted.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"

UGE_State_SprintExhausted::UGE_State_SprintExhausted(){
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    // Constructors must use CreateDefaultSubobject with a name. AddComponent/FindOrAddComponent call NewObject with no name and crash here.
    UTargetTagsGameplayEffectComponent* tagsComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
    FInheritedTagContainer tagChanges;
    tagChanges.Added.AddTag(T_Action_State_SprintExhausted);
    tagsComponent->SetAndApplyTargetTagChanges(tagChanges);
    GEComponents.Add(tagsComponent);
}