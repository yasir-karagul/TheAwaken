#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEventRelayComponent.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEventSubsystem.h"

UCosmeticEventRelayComponent::UCosmeticEventRelayComponent(){
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UCosmeticEventRelayComponent::clientReceiveCosmeticEvent_Implementation(const FS_CosmeticEvent& event){
    if(UCosmeticEventSubsystem* subsystem = UCosmeticEventSubsystem::get(this)){
        subsystem->deliverLocally(event);
    }
}
