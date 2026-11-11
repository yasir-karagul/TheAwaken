#include "AbilityActionFramework/GE_Cooldown_Dodge.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"

UGE_Cooldown_Dodge::UGE_Cooldown_Dodge(){
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    FSetByCallerFloat setByCaller;
    setByCaller.DataName = FName("cooldownSeconds");
    DurationMagnitude = FGameplayEffectModifierMagnitude(setByCaller);
    // Constructors must use CreateDefaultSubobject with a name. AddComponent/FindOrAddComponent call NewObject with no name and crash here.
    UTargetTagsGameplayEffectComponent* tagsComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
    FInheritedTagContainer tagChanges;
    tagChanges.Added.AddTag(T_Action_Cooldown_Dodge);
    tagsComponent->SetAndApplyTargetTagChanges(tagChanges);
    GEComponents.Add(tagsComponent);
}