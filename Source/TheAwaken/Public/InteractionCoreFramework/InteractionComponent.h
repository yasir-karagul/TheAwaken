// Public/InteractionCoreFramework/InteractionComponent.h (TEMPORARY)
#pragma once
#include "Components/ActorComponent.h"
#include "InteractionComponent.generated.h"

UCLASS(ClassGroup=(Awaken), meta=(BlueprintSpawnableComponent))
class THEAWAKEN_API UInteractionComponent : public UActorComponent{
    GENERATED_BODY()
public:
    UInteractionComponent();
};
