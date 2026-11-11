#include "GameFrameworkClassTable/TimeCarrierComponent.h"
#include "TimeFramework/TimeSubsystem.h"
#include "Net/UnrealNetwork.h"

UTimeCarrierComponent::UTimeCarrierComponent(){
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UTimeCarrierComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UTimeCarrierComponent, syncState, COND_None);
    DOREPLIFETIME_CONDITION(UTimeCarrierComponent, calendar, COND_None);
    DOREPLIFETIME_CONDITION(UTimeCarrierComponent, sleepState, COND_None);
    DOREPLIFETIME_CONDITION(UTimeCarrierComponent, skipState, COND_None);
}

void UTimeCarrierComponent::BeginPlay(){
    Super::BeginPlay();
    if(UTimeSubsystem* subsystem = UTimeSubsystem::get(this)){
        subsystem->registerCarrier(this);
    }
}

void UTimeCarrierComponent::EndPlay(const EEndPlayReason::Type reason){
    if(UTimeSubsystem* subsystem = UTimeSubsystem::get(this)){
        subsystem->registerCarrier(nullptr);
    }
    Super::EndPlay(reason);
}

void UTimeCarrierComponent::OnRep_syncState(const FS_TimeSyncState& previous){
    if(UTimeSubsystem* subsystem = UTimeSubsystem::get(this)){
        subsystem->handleSyncReplicated(previous);
    }
}

void UTimeCarrierComponent::OnRep_calendar(const FS_CalendarState& previous){
    if(UTimeSubsystem* subsystem = UTimeSubsystem::get(this)){
        subsystem->handleCalendarReplicated(previous);
    }
}

void UTimeCarrierComponent::OnRep_sleepState(const FS_SleepWaitState& previous){
    if(UTimeSubsystem* subsystem = UTimeSubsystem::get(this)){
        subsystem->handleSleepStateReplicated();
    }
}

void UTimeCarrierComponent::OnRep_skipState(const FS_SkipState& previous){
    if(UTimeSubsystem* subsystem = UTimeSubsystem::get(this)){
        subsystem->handleSkipStateReplicated(previous);
    }
}