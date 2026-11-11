// Private/GameFrameworkClassTable/AwakenServerWorldSubsystem.cpp
#include "GameFrameworkClassTable/AwakenServerWorldSubsystem.h"
#include "Engine/World.h"

bool UAwakenServerWorldSubsystem::ShouldCreateSubsystem(UObject* outer) const{
    if(!Super::ShouldCreateSubsystem(outer)){
        return false;
    }
    const UWorld* world = Cast<UWorld>(outer);
    if(world == nullptr){
        return false;
    }
    return world->GetNetMode() != NM_Client;
}
