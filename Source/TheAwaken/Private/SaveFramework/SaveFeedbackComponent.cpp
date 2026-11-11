#include "SaveFramework/SaveFeedbackComponent.h"
#include "SaveFramework/SaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

USaveFeedbackComponent::USaveFeedbackComponent(){
    SetIsReplicatedByDefault(true);   // a component must replicate to receive an RPC
}

void USaveFeedbackComponent::clientSaveEvent_Implementation(FGameplayTag eventTag, FGameplayTag failReasonTag){
    // The controller is the component's own owner: it does not depend on the PlayerState having replicated yet.
    const APlayerController* playerController = Cast<APlayerController>(GetOwner());
    UGameInstance* gameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    if(USaveSubsystem* save = gameInstance ? gameInstance->GetSubsystem<USaveSubsystem>() : nullptr){
        save->notifyLocalFeedback(eventTag, playerController ? playerController->GetPlayerState<APlayerState>() : nullptr, failReasonTag, playerController);
    }
}