// Public/BuildingFramework/BuildToolComponent.h (TEMPORARY)
#pragma once
#include "Components/ActorComponent.h"
#include "BuildToolComponent.generated.h"

UCLASS(ClassGroup=(Awaken), meta=(BlueprintSpawnableComponent))
class THEAWAKEN_API UBuildToolComponent : public UActorComponent{
    GENERATED_BODY()
public:
    UBuildToolComponent();
};
