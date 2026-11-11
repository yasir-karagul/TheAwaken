#include "CameraFramework/CameraFramework.h"

#include "GameplayTagsManager.h"

DEFINE_LOG_CATEGORY(LogAwakenCamera);

FGameplayTag CameraFramework::lookupExternalTag(FName tagName){
    const FGameplayTag tag = UGameplayTagsManager::Get().RequestGameplayTag(tagName, false);
    if(!tag.IsValid()){
        UE_LOG(LogAwakenCamera, Warning, TEXT("Local: tag %s is not defined yet (its framework is not built); the behaviour that listens to it is off"), *tagName.ToString());
    }
    return tag;
}

FString CameraFramework::viewModeToString(E_CameraViewMode mode){
    const UEnum* enumObject = StaticEnum<E_CameraViewMode>();
    return enumObject != nullptr ? enumObject->GetNameStringByValue(static_cast<int64>(mode)) : FString(TEXT("Unknown"));
}
