#include "SaveFramework/SavePoint.h"
#include "SaveFramework/SaveSubsystem.h"
#include "SaveFramework/AwakenSaveSettings.h"
#include "SaveFramework/02Tags/T_SaveFramework.h"
#include "InteractionCoreFramework/InteractionLockComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

ASavePoint::ASavePoint(){
    bReplicates = true;
    // Doc B18 3.7: an Exclusive lock needs the component, and the actor must replicate so isInUse reaches every prompt.
    lock = CreateDefaultSubobject<UInteractionLockComponent>(TEXT("InteractionLock"));
}

USaveSubsystem* ASavePoint::getSave() const{
    const UGameInstance* gameInstance = GetGameInstance();
    return gameInstance ? gameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
}

APlayerState* ASavePoint::resolvePlayerState(const AActor* instigator){
    const APawn* pawn = Cast<APawn>(instigator);
    return pawn ? pawn->GetPlayerState() : nullptr;
}

FGameplayTag ASavePoint::getInteractionTag_Implementation() const{
    return T_Interaction_Save;
}

float ASavePoint::getHoldDuration_Implementation() const{
    return UAwakenSaveSettings::get()->saveHoldSeconds;
}

bool ASavePoint::evaluateGate(AActor* instigator, FGameplayTag& failReason) const{
    // Clients show the prompt and let the server decide: the aware-hostile query exists only on the server (Doc B12 3.22).
    if(!HasAuthority()){
        return true;
    }

    USaveSubsystem* save = getSave();
    if(!save){
        failReason = T_Save_FailReason_NoActiveSlot;
        return false;
    }

    failReason = save->evaluateSaveGate(resolvePlayerState(instigator));
    return !failReason.IsValid();
}

bool ASavePoint::canInteract_Implementation(AActor* instigator, FGameplayTag& failReason) const{
    return evaluateGate(instigator, failReason);
}

// Checked when the hold starts and AGAIN at completion, so an enemy becoming aware mid-hold still refuses (Doc B18 3.6).
bool ASavePoint::canActivate_Implementation(AActor* instigator, FGameplayTag& failReason) const{
    return evaluateGate(instigator, failReason);
}

void ASavePoint::onActivationComplete_Implementation(AActor* instigator){
    // only the server completes a hold (Doc B18 3.6)
    if(!HasAuthority()){
        return;
    }
    if(USaveSubsystem* save = getSave()){
        // The result and every later event reach the player through USaveFeedbackComponent (Step 12).
        save->requestSaveSession(resolvePlayerState(instigator));
    }
}

E_InteractionKind ASavePoint::getInteractionKind_Implementation() const{
    return E_InteractionKind::Hold;
}

E_InteractionLockPolicy ASavePoint::getLockPolicy_Implementation() const{
    return E_InteractionLockPolicy::Exclusive;
}

FS_InteractionPrompt ASavePoint::getPrompt_Implementation(AActor* instigator) const{
    // PLACEHOLDER keys: replace with the real Doc R10 keys for "Save" and "Save Point" when they exist in the String Tables.
    // The Interaction framework stamps key, isHold, holdProgress and blockedReason.
    FS_InteractionPrompt prompt;
    prompt.verbText = TEXT("Interaction.Verb.Save");
    prompt.objectName = TEXT("Interaction.Object.SavePoint");
    return prompt;
}

void ASavePoint::interact_Implementation(AActor* instigator){
    // A Hold object is completed through onActivationComplete; the framework never calls interact for it.
}

bool ASavePoint::getNoise_Implementation(FS_InteractionNoise& noise) const{
    return false;   // quiet (Doc B18 3.9); loudness would be data on this object
}

int32 ASavePoint::getInteractionPriority_Implementation() const{
    return 0;
}

bool ASavePoint::isInteractableBy_Implementation(AActor* instigator) const{
    return true;   // every player sees a Save Point
}
