// Public/GameFrameworkClassTable/BuildingRegistryCarrierComponent.h
#pragma once
#include "Components/ActorComponent.h"
#include "BuildingRegistryCarrierComponent.generated.h"

UCLASS(ClassGroup=(Awaken), meta=(BlueprintSpawnableComponent))
class THEAWAKEN_API UBuildingRegistryCarrierComponent : public UActorComponent{
    GENERATED_BODY()
public:
    UBuildingRegistryCarrierComponent();
};
