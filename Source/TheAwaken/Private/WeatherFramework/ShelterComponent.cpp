#include "WeatherFramework/ShelterComponent.h"
#include "WeatherFramework/WeatherSubsystem.h"
#include "WeatherFramework/DA_WeatherTuning.h"
#include "WeatherFramework/03Interfaces/BPI_WarmthSource.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Net/UnrealNetwork.h"

UShelterComponent::UShelterComponent(){
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UShelterComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UShelterComponent, shelterState, COND_OwnerOnly);
}

void UShelterComponent::BeginPlay(){
    Super::BeginPlay();
    UWeatherSubsystem* subsystem = GetWorld()->GetSubsystem<UWeatherSubsystem>();
    if(subsystem != nullptr && GetOwner()->HasAuthority()){
        subsystem->registerShelterComponent(this);
    }
}

void UShelterComponent::EndPlay(const EEndPlayReason::Type endPlayReason){
    UWeatherSubsystem* subsystem = GetWorld()->GetSubsystem<UWeatherSubsystem>();
    if(subsystem != nullptr){
        subsystem->unregisterShelterComponent(this);
    }
    Super::EndPlay(endPlayReason);
}

const FS_ShelterState& UShelterComponent::getShelterState() const{
    return shelterState;
}

float UShelterComponent::getRoofFraction(const UWorld* world, const FVector& location, const UDA_WeatherTuning& tuning, const AActor* ignoredActor){
    if(world == nullptr || tuning.shelterUpTraceCount <= 0){
        return 0.0f;
    }
    FCollisionQueryParams params(SCENE_QUERY_STAT(WeatherRoofTrace), false, ignoredActor);
    int32 blocked = 0;
    for(int32 index = 0; index < tuning.shelterUpTraceCount; index++){
        FVector offset = FVector::ZeroVector;
        if(index > 0 && tuning.shelterUpTraceCount > 1){
            const float angle = (2.0f * PI * static_cast<float>(index - 1)) / static_cast<float>(tuning.shelterUpTraceCount - 1);
            offset = FVector(FMath::Cos(angle), FMath::Sin(angle), 0.0f) * tuning.shelterUpTraceSpread;
        }
        const FVector start = location + offset;
        const FVector end = start + FVector::UpVector * tuning.shelterUpTraceDistance;
        if(world->LineTraceTestByChannel(start, end, tuning.shelterTraceChannel, params)){
            blocked++;
        }
    }
    return static_cast<float>(blocked) / static_cast<float>(tuning.shelterUpTraceCount);
}

void UShelterComponent::evaluate(){
    AActor* owner = GetOwner();
    UWeatherSubsystem* subsystem = GetWorld()->GetSubsystem<UWeatherSubsystem>();
    const UDA_WeatherTuning* tuning = (subsystem != nullptr) ? subsystem->getTuning() : nullptr;
    if(owner == nullptr || tuning == nullptr){
        return;
    }
    const FVector location = owner->GetActorLocation();

    FS_ShelterState result;
    result.shelterValue = getRoofFraction(GetWorld(), location, *tuning, owner);

    int32 wallHits = 0;
    FCollisionQueryParams params(SCENE_QUERY_STAT(WeatherWallTrace), false, owner);
    for(int32 index = 0; index < tuning->shelterSideTraceCount; index++){
        const float angle = (2.0f * PI * static_cast<float>(index)) / static_cast<float>(tuning->shelterSideTraceCount);
        const FVector direction(FMath::Cos(angle), FMath::Sin(angle), 0.0f);
        const FVector end = location + direction * tuning->shelterSideTraceDistance;
        if(GetWorld()->LineTraceTestByChannel(location, end, tuning->shelterTraceChannel, params)){
            wallHits++;
        }
    }
    result.isIndoors = wallHits >= tuning->indoorsWallHitCount || subsystem->isInsideIndoorZone(location);
    result.nearFire = isNearWarmth(location, *tuning);

    if(!(shelterState == result)){
        shelterState = result;
    }
}

// Optional (Step 19, B01): a lit fire piece implements IBPI_WarmthSource. With fireSearchRadius at 0 this returns false.
bool UShelterComponent::isNearWarmth(const FVector& location, const UDA_WeatherTuning& tuning) const{
    if(tuning.fireSearchRadius <= 0.0f){
        return false;
    }
    TArray<FOverlapResult> overlaps;
    FCollisionObjectQueryParams objectParams;
    objectParams.AddObjectTypesToQuery(ECC_WorldStatic);
    objectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams params(SCENE_QUERY_STAT(WeatherWarmthOverlap), false, GetOwner());
    GetWorld()->OverlapMultiByObjectType(overlaps, location, FQuat::Identity, objectParams, FCollisionShape::MakeSphere(tuning.fireSearchRadius), params);
    for(const FOverlapResult& overlap : overlaps){
        AActor* actor = overlap.GetActor();
        if(actor == nullptr || !actor->Implements<UBPI_WarmthSource>()){
            continue;
        }
        if(!IBPI_WarmthSource::Execute_isProvidingWarmth(actor)){
            continue;
        }
        const float radius = IBPI_WarmthSource::Execute_getWarmthRadius(actor);
        if(FVector::Dist(actor->GetActorLocation(), location) <= radius){
            return true;
        }
    }
    return false;
}