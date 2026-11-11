// Private/GameFrameworkClassTable/WeatherCarrierComponent.cpp
#include "GameFrameworkClassTable/WeatherCarrierComponent.h"
#include "WeatherFramework/WeatherSubsystem.h"
#include "Net/UnrealNetwork.h"

UWeatherCarrierComponent::UWeatherCarrierComponent(){
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UWeatherCarrierComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UWeatherCarrierComponent, weatherState, COND_None);
    DOREPLIFETIME_CONDITION(UWeatherCarrierComponent, waterBodies, COND_None);
}

void UWeatherCarrierComponent::BeginPlay(){
    Super::BeginPlay();
    if(UWeatherSubsystem* subsystem = UWeatherSubsystem::get(this)){
        subsystem->registerCarrier(this);
    }
}

void UWeatherCarrierComponent::EndPlay(const EEndPlayReason::Type reason){
    if(UWeatherSubsystem* subsystem = UWeatherSubsystem::get(this)){
        subsystem->registerCarrier(nullptr);
    }
    Super::EndPlay(reason);
}

void UWeatherCarrierComponent::OnRep_weatherState(const FS_WeatherState& previous){
    if(UWeatherSubsystem* subsystem = UWeatherSubsystem::get(this)){
        subsystem->handleWeatherStateReplicated(previous);
    }
}

void UWeatherCarrierComponent::OnRep_waterBodies(const TArray<FS_WaterBodyState>& previous){
    if(UWeatherSubsystem* subsystem = UWeatherSubsystem::get(this)){
        subsystem->handleWaterBodiesReplicated(previous);
    }
}