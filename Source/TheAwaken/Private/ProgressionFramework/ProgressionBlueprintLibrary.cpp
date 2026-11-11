#include "ProgressionFramework/ProgressionBlueprintLibrary.h"
#include "ProgressionFramework/ProgressionComponent.h"
#include "ProgressionFramework/02Tags/T_ProgressionFramework.h"

bool UProgressionBlueprintLibrary::isUnlocked(const UObject* worldContextObject, FGameplayTag tag){
    const UProgressionComponent* progression = UProgressionComponent::findProgression(worldContextObject);
    return progression != nullptr && progression->isUnlocked(tag);
}

FGameplayTagContainer UProgressionBlueprintLibrary::getAllUnlocked(const UObject* worldContextObject){
    const UProgressionComponent* progression = UProgressionComponent::findProgression(worldContextObject);
    if(progression == nullptr){
        return FGameplayTagContainer();
    }
    return progression->getAllUnlocked();
}

FS_ProgressionResult UProgressionBlueprintLibrary::grantUnlock(const UObject* worldContextObject, FGameplayTag tag){
    UProgressionComponent* progression = UProgressionComponent::findProgression(worldContextObject);
    if(progression == nullptr){
        FS_ProgressionResult result;
        result.failReason = T_Progression_FailReason_NotReady.GetTag();   // no GameState yet: nothing was attempted
        return result;
    }
    return progression->grantUnlock(tag);
}
