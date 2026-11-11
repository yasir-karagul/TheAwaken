#include "LocalizationFramework/LocalizationBlueprintLibrary.h"
#include "LocalizationFramework/LocalizationService.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace{
    ULocalizationService* findService(const UObject* worldContextObject){
        if(worldContextObject == nullptr || GEngine == nullptr){
            return nullptr;
        }
        const UWorld* world = GEngine->GetWorldFromContextObject(worldContextObject, EGetWorldErrorMode::ReturnNull);
        if(world == nullptr){
            return nullptr;
        }
        UGameInstance* gameInstance = world->GetGameInstance();
        if(gameInstance == nullptr){
            return nullptr;
        }
        return gameInstance->GetSubsystem<ULocalizationService>();
    }

    FText placeholderText(const FString& name){
        return FText::AsCultureInvariant(FString::Printf(TEXT("[%s]"), *name));
    }
}

FText ULocalizationBlueprintLibrary::getLocalizedText(const UObject* worldContextObject, FName key){
    const ULocalizationService* service = findService(worldContextObject);
    if(service == nullptr){
        return placeholderText(key.ToString());
    }
    return service->getText(key);
}

FText ULocalizationBlueprintLibrary::formatLocalizedText(const UObject* worldContextObject, FName key, const TArray<FS_LocalizationArgument>& arguments){
    const ULocalizationService* service = findService(worldContextObject);
    if(service == nullptr){
        return placeholderText(key.ToString());
    }
    return service->formatText(key, arguments);
}

FText ULocalizationBlueprintLibrary::getLocalizedReason(const UObject* worldContextObject, FGameplayTag reasonTag){
    const ULocalizationService* service = findService(worldContextObject);
    if(service == nullptr){
        return placeholderText(reasonTag.ToString());
    }
    return service->getReasonText(reasonTag);
}

FName ULocalizationBlueprintLibrary::getCurrentCulture(const UObject* worldContextObject){
    const ULocalizationService* service = findService(worldContextObject);
    if(service == nullptr){
        return NAME_None;
    }
    return service->getActiveCulture();
}

FS_LocalizationResult ULocalizationBlueprintLibrary::setGameCulture(const UObject* worldContextObject, FName cultureName){
    ULocalizationService* service = findService(worldContextObject);
    if(service == nullptr){
        FS_LocalizationResult result;
        result.cultureName = cultureName;
        return result;
    }
    return service->setCulture(cultureName);
}

FS_LocalizationArgument ULocalizationBlueprintLibrary::makeTextArgument(FName name, FText value){
    FS_LocalizationArgument argument;
    argument.name = name;
    argument.kind = E_LocalizationArgumentKind::Text;
    argument.textValue = value;
    return argument;
}

FS_LocalizationArgument ULocalizationBlueprintLibrary::makeIntegerArgument(FName name, int64 value){
    FS_LocalizationArgument argument;
    argument.name = name;
    argument.kind = E_LocalizationArgumentKind::Integer;
    argument.integerValue = value;
    return argument;
}

FS_LocalizationArgument ULocalizationBlueprintLibrary::makeFloatArgument(FName name, double value){
    FS_LocalizationArgument argument;
    argument.name = name;
    argument.kind = E_LocalizationArgumentKind::Float;
    argument.floatValue = value;
    return argument;
}

FS_LocalizationArgument ULocalizationBlueprintLibrary::makeGenderArgument(FName name, E_LocalizationGender value){
    FS_LocalizationArgument argument;
    argument.name = name;
    argument.kind = E_LocalizationArgumentKind::Gender;
    argument.genderValue = value;
    return argument;
}

FS_LocalizationArgument ULocalizationBlueprintLibrary::makeNameArgument(FName name, FText value){
    FS_LocalizationArgument argument;
    argument.name = name;
    argument.kind = E_LocalizationArgumentKind::Name;
    argument.textValue = value;
    return argument;
}
