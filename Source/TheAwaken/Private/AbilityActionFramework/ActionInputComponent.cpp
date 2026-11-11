#include "AbilityActionFramework/ActionInputComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFrameworkClassTable/AwakenPlayerController.h"
#include "AbilityActionFramework/AbilityLibrary.h"
#include "AbilityActionFramework/DA_ActionInputMap.h"

UAbilitySystemComponent* UActionInputComponent::getAbilitySystem() const{
    const APlayerController* controller = Cast<APlayerController>(GetOwner());
    if(controller == nullptr || controller->PlayerState == nullptr){
        return nullptr;
    }
    return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(controller->PlayerState);
}

void UActionInputComponent::bindToInputComponent(UInputComponent* playerInputComponent){
    UEnhancedInputComponent* enhanced = Cast<UEnhancedInputComponent>(playerInputComponent);
    if(enhanced == nullptr || inputMap == nullptr){
        return;
    }
    for(const FS_ActionInputBinding& binding : inputMap->bindings){
        if(binding.inputAction == nullptr || !binding.actionTag.IsValid()){
            continue;
        }
        enhanced->BindAction(binding.inputAction, ETriggerEvent::Started, this, &UActionInputComponent::onActionPressed, binding.actionTag);
        enhanced->BindAction(binding.inputAction, ETriggerEvent::Completed, this, &UActionInputComponent::onActionReleased, binding.actionTag);
        enhanced->BindAction(binding.inputAction, ETriggerEvent::Canceled, this, &UActionInputComponent::onActionReleased, binding.actionTag);
    }
}

void UActionInputComponent::onActionPressed(FGameplayTag actionTag){
    // Doc R12 11.1: the input layer reports activity so the sleep adapter can tell an Away player.
    AAwakenPlayerController* awakenController = Cast<AAwakenPlayerController>(GetOwner());
    if(awakenController != nullptr){
        awakenController->noteInputActivity();
    }
    UAbilitySystemComponent* asc = getAbilitySystem();
    FGameplayAbilitySpec* spec = UAbilityLibrary::findSpecByActionTag(asc, actionTag);
    if(spec == nullptr){
        return;
    }
    // Set the input state first: held abilities test it at activation (WaitInputRelease).
    // A press on an already active ability is a toggle (Crouch). It must not start a second activation.
    const bool wasActive = spec->IsActive();
    asc->AbilitySpecInputPressed(*spec);
    if(!wasActive){
        asc->TryActivateAbility(spec->Handle);
    }
}

void UActionInputComponent::onActionReleased(FGameplayTag actionTag){
    UAbilitySystemComponent* asc = getAbilitySystem();
    FGameplayAbilitySpec* spec = UAbilityLibrary::findSpecByActionTag(asc, actionTag);
    if(spec != nullptr){
        asc->AbilitySpecInputReleased(*spec);
    }
}