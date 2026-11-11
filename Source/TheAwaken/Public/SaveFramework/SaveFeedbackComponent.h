#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "SaveFeedbackComponent.generated.h"

/** Add to your PlayerController. Server -> owning client; raises the Save.* events locally there (Doc02 19.2). */
UCLASS(ClassGroup = (Save), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API USaveFeedbackComponent : public UActorComponent{
	GENERATED_BODY()
public:
	USaveFeedbackComponent();

	UFUNCTION(Client, Reliable)
	void clientSaveEvent(FGameplayTag eventTag, FGameplayTag failReasonTag);
};