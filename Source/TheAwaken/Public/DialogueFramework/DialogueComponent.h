// Public/DialogueFramework/DialogueComponent.h (TEMPORARY)
#pragma once
#include "Components/ActorComponent.h"
#include "DialogueComponent.generated.h"

UCLASS(ClassGroup=(Awaken), meta=(BlueprintSpawnableComponent))
class THEAWAKEN_API UDialogueComponent : public UActorComponent{
    GENERATED_BODY()
public:
    UDialogueComponent();
};
