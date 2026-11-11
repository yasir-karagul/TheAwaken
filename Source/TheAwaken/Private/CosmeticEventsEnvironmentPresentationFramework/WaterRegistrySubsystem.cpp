#include "CosmeticEventsEnvironmentPresentationFramework/WaterRegistrySubsystem.h"
#include "CosmeticEventsEnvironmentPresentationFramework/AwakenWaterBody.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentLog.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentMath.h"
#include "CosmeticEventsEnvironmentPresentationFramework/DA_EnvironmentSettings.h"
#include "CosmeticEventsEnvironmentPresentationFramework/DA_GroundTypeMap.h"
#include "CosmeticEventsEnvironmentPresentationFramework/02Tags/T_CosmeticEventsEnvironmentPresentationFramework.h"
#include "WeatherFramework/02Tags/T_WeatherFramework.h"
#include "WeatherFramework/WeatherSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

UWaterRegistrySubsystem* UWaterRegistrySubsystem::get(const UObject* worldContext){
    const UWorld* world = GEngine ? GEngine->GetWorldFromContextObject(worldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    return world ? world->GetSubsystem<UWaterRegistrySubsystem>() : nullptr;
}

// Every machine, a dedicated server too: the host decides swimming consequences and R07 reads the queries there.
bool UWaterRegistrySubsystem::ShouldCreateSubsystem(UObject* outer) const{
    const UWorld* world = Cast<UWorld>(outer);
    return world && world->IsGameWorld() && Super::ShouldCreateSubsystem(outer);
}

void UWaterRegistrySubsystem::Initialize(FSubsystemCollectionBase& collection){
    Super::Initialize(collection);
    collection.InitializeDependency<UWeatherSubsystem>();
    settings = UDA_EnvironmentSettings::loadDefault();
    if(settings == nullptr){
        UE_LOG(LogAwakenEnvironment, Error, TEXT("DA_EnvironmentSettings_Default is missing. Water bodies fall back to built-in defaults."));
    }
    // No ground map is not an error: the map only answers for water until it is baked.
    groundMap = UDA_GroundTypeMap::loadDefault();
}

void UWaterRegistrySubsystem::OnWorldBeginPlay(UWorld& world){
    Super::OnWorldBeginPlay(world);
    // Weather.WaterFroze and Weather.WaterThawed are raised on every machine from the replicated state. One listener here serves every body.
    UGameplayMessageSubsystem& messages = UGameplayMessageSubsystem::Get(this);
    frozeListener = messages.RegisterListener(T_Weather_WaterFroze.GetTag(), this, &UWaterRegistrySubsystem::onWaterFroze);
    thawedListener = messages.RegisterListener(T_Weather_WaterThawed.GetTag(), this, &UWaterRegistrySubsystem::onWaterThawed);
#if !UE_BUILD_SHIPPING
    world.GetTimerManager().SetTimer(debugTimerHandle, FTimerDelegate::CreateLambda([this](){
        drawDebug();
    }), 1.0f, true);
#endif
}

void UWaterRegistrySubsystem::Deinitialize(){
    frozeListener.Unregister();
    thawedListener.Unregister();
#if !UE_BUILD_SHIPPING
    if(UWorld* world = GetWorld()){
        world->GetTimerManager().ClearTimer(debugTimerHandle);
    }
#endif
    bodies.Reset();
    Super::Deinitialize();
}

void UWaterRegistrySubsystem::onWaterFroze(FGameplayTag channel, const FS_WeatherWaterMessage& message){
    if(AAwakenWaterBody* body = findBodyById(message.bodyId)){
        body->syncIce();
    }
}

void UWaterRegistrySubsystem::onWaterThawed(FGameplayTag channel, const FS_WeatherWaterMessage& message){
    if(AAwakenWaterBody* body = findBodyById(message.bodyId)){
        body->syncIce();
    }
}

void UWaterRegistrySubsystem::registerBody(AAwakenWaterBody* body){
    if(body != nullptr){
        bodies.AddUnique(body);
    }
}

void UWaterRegistrySubsystem::unregisterBody(AAwakenWaterBody* body){
    bodies.Remove(body);
}

AAwakenWaterBody* UWaterRegistrySubsystem::findBodyById(const FGuid& bodyId) const{
    for(const TWeakObjectPtr<AAwakenWaterBody>& body : bodies){
        if(body.IsValid() && body->bodyId == bodyId){
            return body.Get();
        }
    }
    return nullptr;
}

AAwakenWaterBody* UWaterRegistrySubsystem::findBodyAt(const FVector& location) const{
    for(const TWeakObjectPtr<AAwakenWaterBody>& body : bodies){
        if(body.IsValid() && body->containsPoint(location)){
            return body.Get();
        }
    }
    return nullptr;
}

bool UWaterRegistrySubsystem::isLocationInWater(const FVector& location) const{
    for(const TWeakObjectPtr<AAwakenWaterBody>& body : bodies){
        if(body.IsValid() && body->containsPoint(location)){
            return true;
        }
    }
    return false;
}

bool UWaterRegistrySubsystem::getWaterKindAt(const FVector& location, E_WaterKind& kind) const{
    const AAwakenWaterBody* body = findBodyAt(location);
    if(body == nullptr){
        return false;
    }
    kind = body->kind;
    return true;
}

FVector UWaterRegistrySubsystem::getFlowAt(const FVector& location) const{
    const AAwakenWaterBody* body = findBodyAt(location);
    if(body == nullptr || !body->hasCurrent()){
        return FVector::ZeroVector;
    }
    const UWeatherSubsystem* weather = UWeatherSubsystem::get(this);
    if(weather == nullptr){
        return FVector::ZeroVector;
    }
    // Replicated values only: the river strength comes from Weather's carrier, so the host and the owning client agree.
    const float scale = settings ? settings->flowSpeedPerStrength : 2.0f;
    return UCosmeticEnvironmentMathLibrary::computeFlow(body->flowDirection, body->baseFlowSpeed, weather->getRiverStrength(body->bodyId), scale);
}

bool UWaterRegistrySubsystem::isFreezingSeason() const{
    const UWeatherSubsystem* weather = UWeatherSubsystem::get(this);
    if(weather == nullptr){
        return false;
    }
    const E_WeatherSeasonKey key = weather->getCurrentSeasonKey();
    return key == E_WeatherSeasonKey::Winter || key == E_WeatherSeasonKey::Snowy;
}

bool UWaterRegistrySubsystem::isFreezing(const FVector& location) const{
    const AAwakenWaterBody* body = findBodyAt(location);
    return body != nullptr && body->freezes() && isFreezingSeason();
}

bool UWaterRegistrySubsystem::isFrozenWalkable(const FVector& location) const{
    const AAwakenWaterBody* body = findBodyAt(location);
    return body != nullptr && body->isIceSolidNow();
}

FGameplayTag UWaterRegistrySubsystem::getGroundTypeAt(const FVector& location) const{
    // The water registry first: a point in water returns the tag for its kind.
    if(const AAwakenWaterBody* body = findBodyAt(location)){
        switch(body->kind){
            case E_WaterKind::River:
            case E_WaterKind::Stream:
                return T_Environment_Ground_River.GetTag();
            case E_WaterKind::Lake:
                return T_Environment_Ground_Lake.GetTag();
            case E_WaterKind::Sea:
                return T_Environment_Ground_Sea.GetTag();
        }
    }
    return groundMap ? groundMap->sampleAt(location) : FGameplayTag();
}

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> cvarDebugWater(TEXT("Awaken.Debug.Water"), 0, TEXT("Draw the water bodies, swim volumes, currents and ice (the Water view of B22)."), ECVF_Default);

void UWaterRegistrySubsystem::drawDebug() const{
    if(cvarDebugWater.GetValueOnGameThread() == 0 || GetWorld() == nullptr){
        return;
    }
    for(const TWeakObjectPtr<AAwakenWaterBody>& body : bodies){
        if(body.IsValid()){
            body->drawDebug(1.25f);
        }
    }
}
#endif
