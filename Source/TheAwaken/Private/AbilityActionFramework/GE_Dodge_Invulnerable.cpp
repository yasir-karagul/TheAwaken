#include "AbilityActionFramework/GE_Dodge_Invulnerable.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"

UGE_Dodge_Invulnerable::UGE_Dodge_Invulnerable(){
    DurationPolicy = EGameplayEffectDurationType::HasDuration;
    FSetByCallerFloat setByCaller;
    setByCaller.DataName = FName("invulnerabilitySeconds");
    DurationMagnitude = FGameplayEffectModifierMagnitude(setByCaller);
    // Constructors must use CreateDefaultSubobject with a name. AddComponent/FindOrAddComponent call NewObject with no name and crash here.
    UTargetTagsGameplayEffectComponent* tagsComponent = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTagsComponent"));
    FInheritedTagContainer tagChanges;
    tagChanges.Added.AddTag(T_Action_State_DodgeInvulnerable);
    tagsComponent->SetAndApplyTargetTagChanges(tagChanges);
    GEComponents.Add(tagsComponent);
}