#include "SaveFramework/SaveParticipantComponent.h"
#include "SaveFramework/SaveSubsystem.h"
#include "Engine/GameInstance.h"

APlayerState* USaveParticipantComponent::getSaveOwner_Implementation() const{
	return scope == E_SaveScope::Character ? Cast<APlayerState>(GetOwner()) : nullptr;
}

void USaveParticipantComponent::BeginPlay(){
	Super::BeginPlay();
	ensureMsgf(scope != E_SaveScope::Character || Cast<APlayerState>(GetOwner()),
		TEXT("Character-scope save participant %s must be on a PlayerState."), *GetName());

	if(GetOwner() && GetOwner()->HasAuthority()){
		if(UGameInstance* gameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr){
			if(USaveSubsystem* save = gameInstance->GetSubsystem<USaveSubsystem>()){
				save->registerParticipant(this);
			}
		}
	}
}

void USaveParticipantComponent::EndPlay(const EEndPlayReason::Type endPlayReason){
	if(UGameInstance* gameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr){
		if(USaveSubsystem* save = gameInstance->GetSubsystem<USaveSubsystem>()){
			save->unregisterParticipant(this);
		}
	}
	Super::EndPlay(endPlayReason);
}