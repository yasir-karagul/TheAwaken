// Private/GameFrameworkClassTable/AwakenServerProbeSubsystem.cpp (TEMPORARY)
#include "GameFrameworkClassTable/AwakenServerProbeSubsystem.h"
#include "Engine/World.h"

void UAwakenServerProbeSubsystem::Initialize(FSubsystemCollectionBase& collection){
    Super::Initialize(collection);
    const UWorld* world = GetWorld();
    UE_LOG(LogTemp, Warning, TEXT("ServerProbe created: world=%s netMode=%d"), *world->GetName(), static_cast<int32>(world->GetNetMode()));
}
