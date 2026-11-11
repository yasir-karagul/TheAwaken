// Public/AudioFramework/AudioListenerComponent.h (TEMPORARY)
#pragma once
#include "Components/ActorComponent.h"
#include "AudioListenerComponent.generated.h"

UCLASS(ClassGroup=(Awaken), meta=(BlueprintSpawnableComponent))
class THEAWAKEN_API UAudioListenerComponent : public UActorComponent{
    GENERATED_BODY()
public:
    UAudioListenerComponent();
};
