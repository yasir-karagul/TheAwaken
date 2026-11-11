#include "SaveFramework/SavePoint.h"
#include "SaveFramework/SaveSubsystem.h"
#include "SaveFramework/AwakenSaveSettings.h"
#include "SaveFramework/02Tags/T_SaveFramework.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

ASavePoint::ASavePoint(){
    bReplicates = true;
}

USaveSubsystem* ASavePoint::getSave() const{
    const UGameInstance* gameInstance = GetGameInstance();
    return gameInstance ? gameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
}

APlayerState* ASavePoint::resolvePlayerState(const AActor* instigator){
    const APawn* pawn = Cast<APawn>(instigator);
    return pawn ? pawn->GetPlayerState() : nullptr;
}

FGameplayTag ASavePoint::getInteractionTag() const{
    return T_Interaction_Save;
}

float ASavePoint::getHoldDuration() const{
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

bool ASavePoint::canInteract(AActor* instigator, FGameplayTag& failReason) const{
    return evaluateGate(instigator, failReason);
}

// Checked when the hold starts and AGAIN at completion, so an enemy becoming aware mid-hold still refuses (Doc B18 3.6).
bool ASavePoint::canActivate(AActor* instigator, FGameplayTag& failReason) const{
    return evaluateGate(instigator, failReason);
}

void ASavePoint::onActivationComplete(AActor* instigator){
    // only the server completes a hold (Doc B18 3.6)
    if(!HasAuthority()){
        return;
    }
    if(USaveSubsystem* save = getSave()){
        // The result and every later event reach the player through USaveFeedbackComponent (Step 12).
        save->requestSaveSession(resolvePlayerState(instigator));
    }
}