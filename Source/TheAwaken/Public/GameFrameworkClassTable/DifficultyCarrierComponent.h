// Public/GameFrameworkClassTable/DifficultyCarrierComponent.h
#pragma once
#include "Components/ActorComponent.h"
#include "DifficultyCarrierComponent.generated.h"

UCLASS(ClassGroup=(Awaken), meta=(BlueprintSpawnableComponent))
class THEAWAKEN_API UDifficultyCarrierComponent : public UActorComponent{
    GENERATED_BODY()
public:
    UDifficultyCarrierComponent();
};
