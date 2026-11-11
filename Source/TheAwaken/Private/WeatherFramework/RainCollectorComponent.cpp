#include "WeatherFramework/RainCollectorComponent.h"
#include "GameFramework/Actor.h"
#include "WeatherFramework/WeatherSubsystem.h"
#include "WeatherFramework/WeatherLog.h"
#include "WeatherFramework/ShelterComponent.h"
#include "WeatherFramework/DA_WeatherTuning.h"
#include "WeatherFramework/02Tags/T_WeatherFramework.h"
#include "WeatherFramework/03Interfaces/BPI_WaterContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"

URainCollectorComponent::URainCollectorComponent(){
    PrimaryComponentTick.bCanEverTick = false;
}

void URainCollectorComponent::BeginPlay(){
    Super::BeginPlay();
    UWeatherSubsystem* subsystem = GetWorld()->GetSubsystem<UWeatherSubsystem>();
    if(subsystem != nullptr && GetOwner()->HasAuthority()){
        subsystem->registerRainCollector(this);
    }
}

void URainCollectorComponent::EndPlay(const EEndPlayReason::Type endPlayReason){
    UWeatherSubsystem* subsystem = GetWorld()->GetSubsystem<UWeatherSubsystem>();
    if(subsystem != nullptr){
        subsystem->unregisterRainCollector(this);
    }
    Super::EndPlay(endPlayReason);
}

void URainCollectorComponent::collect(float units){
    AActor* owner = GetOwner();
    UWeatherSubsystem* subsystem = GetWorld()->GetSubsystem<UWeatherSubsystem>();
    if(owner == nullptr || subsystem == nullptr || !owner->Implements<UBPI_WaterContainer>()){
        return;
    }
    const UDA_WeatherTuning* tuning = subsystem->getTuning();
    if(tuning == nullptr){
        return;
    }

    const FS_ContainerWaterState water = IBPI_WaterContainer::Execute_getWaterState(owner);
    if(water.holdsUncleanWater || water.isBoiling){
        pendingUnits = 0.0f;
        return;
    }
    // Same upward trace as the shelter test (B14 3.4).
    if(UShelterComponent::getRoofFraction(GetWorld(), owner->GetActorLocation(), *tuning, owner) >= tuning->shelteredThreshold){
        return;
    }
    if(water.freeCapacity <= 0){
        pendingUnits = 0.0f;
        return;
    }

    pendingUnits += units;
    const int32 whole = FMath::FloorToInt(pendingUnits);
    if(whole <= 0){
        return;
    }
    const int32 added = IBPI_WaterContainer::Execute_addCleanWater(owner, FMath::Min(whole, water.freeCapacity));
    pendingUnits -= static_cast<float>(added);
    if(added > 0){
        FS_WeatherRainFilledMessage payload;
        payload.container = owner;
        payload.amount = added;
        UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Weather_RainFilledContainer.GetTag(), payload);
        UE_LOG(LogAwakenWeather, Log, TEXT("Rain filled container %s: %d units"), *owner->GetName(), added);
    }
}
