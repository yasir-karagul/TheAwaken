#include "CosmeticEventsEnvironmentPresentationFramework/AwakenWaterBody.h"
#include "CosmeticEventsEnvironmentPresentationFramework/WaterRegistrySubsystem.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEventSubsystem.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEnvironmentLog.h"
#include "CosmeticEventsEnvironmentPresentationFramework/DA_EnvironmentSettings.h"
#include "CosmeticEventsEnvironmentPresentationFramework/01Structs/S_CosmeticEventsEnvironmentPresentationFramework.h"
#include "CosmeticEventsEnvironmentPresentationFramework/02Tags/T_CosmeticEventsEnvironmentPresentationFramework.h"
#include "WeatherFramework/WeatherSubsystem.h"
#include "WeatherFramework/01Structs/S_WeatherFramework.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/DataValidation.h"

namespace{
    const FName swimVolumeTag = TEXT("SwimVolume");
    const FName iceCollisionTag = TEXT("IceCollision");
    const FName iceLevelParameter = TEXT("iceLevel");
    // A default engine collision profile (Project Settings > Collision). UCollisionProfile has no constant for it in UE 5.8.
    const FName overlapAllDynamicProfile = TEXT("OverlapAllDynamic");
}

AAwakenWaterBody::AAwakenWaterBody(){
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;   // loaded the same way on every machine (B21 section 7)
    SetCanBeDamaged(false);

    root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(root);

    swimVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SwimVolume"));
    swimVolume->SetupAttachment(root);
    swimVolume->SetBoxExtent(FVector(500.0f, 500.0f, 100.0f));
    // Overlaps only, never blocks. QueryOnly keeps GetClosestPointOnCollision working for the point test.
    swimVolume->SetCollisionProfileName(overlapAllDynamicProfile);
    swimVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    swimVolume->SetGenerateOverlapEvents(true);
    swimVolume->ComponentTags.Add(swimVolumeTag);

    iceCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("IceCollision"));
    iceCollision->SetupAttachment(root);
    iceCollision->SetBoxExtent(FVector(500.0f, 500.0f, 5.0f));
    // Off by default. The profile is the blocking one, and the ice logic switches the collision on at the right level.
    iceCollision->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    iceCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    iceCollision->SetGenerateOverlapEvents(false);
    iceCollision->ComponentTags.Add(iceCollisionTag);

    iceMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IceMesh"));
    iceMesh->SetupAttachment(root);
    iceMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    iceMesh->SetVisibility(false);
}

void AAwakenWaterBody::PostActorCreated(){
    Super::PostActorCreated();
    if(!bodyId.IsValid()){
        bodyId = FGuid::NewGuid();
    }
}

float AAwakenWaterBody::getSurfaceHeight() const{
    if(!useVolumeTopAsSurface){
        return surfaceHeight;
    }
    return worldBounds.IsValid ? static_cast<float>(worldBounds.Max.Z) : GetActorLocation().Z;
}

bool AAwakenWaterBody::containsPoint(const FVector& point) const{
    if(!worldBounds.IsValid || !worldBounds.IsInside(point)){
        return false;
    }
    for(const TWeakObjectPtr<UPrimitiveComponent>& volume : swimVolumes){
        const UPrimitiveComponent* component = volume.Get();
        if(component == nullptr){
            continue;
        }
        // A distance of zero means the point is inside the collision. -1 means the component has no collision at all.
        FVector closest;
        const float distance = component->GetClosestPointOnCollision(point, closest);
        if(distance >= 0.0f && FMath::IsNearlyZero(distance, 0.01f)){
            return true;
        }
    }
    return false;
}

// ---------------- Lifecycle ----------------

void AAwakenWaterBody::BeginPlay(){
    Super::BeginPlay();

    if(!bodyId.IsValid()){
        UE_LOG(LogAwakenEnvironment, Error, TEXT("%s has an empty bodyId (Doc02 19.3). It will not register with Weather."), *GetName());
    }

    registry = UWaterRegistrySubsystem::get(this);
    settings = registry ? registry->getSettings() : nullptr;

    collectVolumes();

    // Register with the local list (the queries search it) and with the weather subsystem (B14 keeps the state by bodyId).
    if(registry != nullptr){
        registry->registerBody(this);
    }
    registerWithWeather();

    if(HasAuthority()){
        bindOverlaps();
    }

    if(GetNetMode() != NM_DedicatedServer && iceMesh->GetStaticMesh() != nullptr && iceMesh->GetNumMaterials() > 0){
        iceMaterial = iceMesh->CreateAndSetMaterialInstanceDynamic(0);
    }

    syncIce();

    // The ice look follows the replicated level, and the same timer re-checks the collision as a safety net. Only bodies that can freeze need it.
    if(freezes()){
        const float interval = settings ? settings->iceReconcileSeconds : 1.0f;
        GetWorldTimerManager().SetTimer(iceTimer, this, &AAwakenWaterBody::onIceTimer, FMath::Max(interval, 0.1f), true);
    }
}

void AAwakenWaterBody::EndPlay(const EEndPlayReason::Type reason){
    GetWorldTimerManager().ClearTimer(iceTimer);
    GetWorldTimerManager().ClearTimer(exitTimer);

    // Streaming can unload a body. Raise Water.Exited for every actor still counted or waiting to be announced, and clear the map.
    if(HasAuthority()){
        TSet<TWeakObjectPtr<AActor>> announced;
        for(const TPair<TWeakObjectPtr<AActor>, int32>& pair : overlapCounts){
            announced.Add(pair.Key);
        }
        for(const TPair<TWeakObjectPtr<AActor>, double>& pair : pendingExits){
            announced.Add(pair.Key);
        }
        for(const TWeakObjectPtr<AActor>& actor : announced){
            raiseExited(actor);
        }
    }
    overlapCounts.Reset();
    pendingExits.Reset();

    if(registry != nullptr){
        registry->unregisterBody(this);
    }
    if(UWeatherSubsystem* weather = UWeatherSubsystem::get(this)){
        weather->unregisterWaterBody(bodyId);
    }
    Super::EndPlay(reason);
}

void AAwakenWaterBody::collectVolumes(){
    swimVolumes.Reset();
    iceCollisions.Reset();
    worldBounds = FBox(ForceInit);

    TArray<UPrimitiveComponent*> components;
    GetComponents<UPrimitiveComponent>(components);
    for(UPrimitiveComponent* component : components){
        if(component->ComponentHasTag(swimVolumeTag)){
            swimVolumes.Add(component);
            worldBounds += component->Bounds.GetBox();
        }else if(component->ComponentHasTag(iceCollisionTag)){
            iceCollisions.Add(component);
        }
    }
    if(swimVolumes.Num() == 0){
        UE_LOG(LogAwakenEnvironment, Warning, TEXT("%s has no component tagged SwimVolume. Nothing can swim in it."), *GetName());
    }
}

void AAwakenWaterBody::registerWithWeather(){
    UWeatherSubsystem* weather = UWeatherSubsystem::get(this);
    if(weather == nullptr || !bodyId.IsValid()){
        return;
    }
    FS_WaterBodyRegistration registration;
    registration.bodyId = bodyId;
    registration.hasCurrent = hasCurrent();
    registration.canFreeze = freezes();   // the Sea is never registered as freezable
    weather->registerWaterBody(registration);   // the host stores it; on a client the call returns at once, the state arrives replicated
}

// ---------------- Enter and leave (C.4) ----------------

void AAwakenWaterBody::bindOverlaps(){
    for(const TWeakObjectPtr<UPrimitiveComponent>& volume : swimVolumes){
        UPrimitiveComponent* component = volume.Get();
        if(component == nullptr){
            continue;
        }
        component->OnComponentBeginOverlap.AddDynamic(this, &AAwakenWaterBody::onSwimVolumeBeginOverlap);
        component->OnComponentEndOverlap.AddDynamic(this, &AAwakenWaterBody::onSwimVolumeEndOverlap);

        // A body that streams in while someone is already in it gets no begin event for that pair.
        TSet<UPrimitiveComponent*> overlapping;
        component->GetOverlappingComponents(overlapping);
        for(UPrimitiveComponent* other : overlapping){
            if(other != nullptr){
                noteActorEntered(other->GetOwner());
            }
        }
    }
}

bool AAwakenWaterBody::isTrackedActor(const AActor* actor) const{
    // A character with a stable id: a player pawn or an NPC. Dropped items, projectiles and bodies raise no message and no splash:
    // a loose item in deep water simply sinks and stays (B21 C.5).
    return actor != nullptr && actor->IsA<ACharacter>();
}

bool AAwakenWaterBody::isFreezingNow() const{
    // Cold water that makes people ill: a body that can freeze, in Winter or the Snowy block. The Sea is never cold in this sense.
    return freezes() && registry != nullptr && registry->isFreezingSeason();
}

void AAwakenWaterBody::onSwimVolumeBeginOverlap(UPrimitiveComponent* component, AActor* other, UPrimitiveComponent* otherComponent, int32 bodyIndex, bool fromSweep, const FHitResult& hit){
    if(HasAuthority() == false){
        return;
    }
    noteActorEntered(other);
}

void AAwakenWaterBody::onSwimVolumeEndOverlap(UPrimitiveComponent* component, AActor* other, UPrimitiveComponent* otherComponent, int32 bodyIndex){
    if(HasAuthority() == false){
        return;
    }
    noteActorLeft(other);
}

// Count overlaps per actor. A body can have several swim volumes (an irregular lake), and an actor can overlap two of them at once.
// Water.Entered goes out only when the count goes from 0 to 1, so crossing the join of two volumes of one body raises nothing.
void AAwakenWaterBody::noteActorEntered(AActor* actor){
    if(isTrackedActor(actor) == false){
        return;
    }
    const TWeakObjectPtr<AActor> key(actor);
    int32& count = overlapCounts.FindOrAdd(key);
    count++;
    if(count > 1){
        return;
    }
    // Back inside before the exit was announced: the actor never left as far as anyone else knows.
    if(pendingExits.Remove(key) > 0){
        return;
    }

    FS_WaterEnteredMessage message;
    message.actor = actor;
    message.kind = kind;
    message.isFreezing = isFreezingNow();
    message.isFrozenSurface = isIceSolid;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Water_Entered.GetTag(), message);
    raiseSplash(actor);
}

void AAwakenWaterBody::noteActorLeft(AActor* actor){
    const TWeakObjectPtr<AActor> key(actor);
    int32* count = overlapCounts.Find(key);
    if(count == nullptr){
        return;
    }
    (*count)--;
    if(*count > 0){
        return;
    }
    overlapCounts.Remove(key);

    const float delay = settings ? settings->waterReentryDelaySeconds : 0.0f;
    if(delay <= 0.0f){
        raiseExited(key);
        return;
    }
    pendingExits.Add(key, GetWorld()->GetRealTimeSeconds() + delay);
    if(!GetWorldTimerManager().IsTimerActive(exitTimer)){
        GetWorldTimerManager().SetTimer(exitTimer, this, &AAwakenWaterBody::processPendingExits, FMath::Max(delay * 0.5f, 0.05f), true);
    }
}

void AAwakenWaterBody::processPendingExits(){
    const double now = GetWorld()->GetRealTimeSeconds();
    TArray<TWeakObjectPtr<AActor>> due;
    for(const TPair<TWeakObjectPtr<AActor>, double>& pair : pendingExits){
        if(now >= pair.Value){
            due.Add(pair.Key);
        }
    }
    for(const TWeakObjectPtr<AActor>& actor : due){
        pendingExits.Remove(actor);
        raiseExited(actor);
    }
    // Doc02 5: the timer runs only while something waits.
    if(pendingExits.Num() == 0){
        GetWorldTimerManager().ClearTimer(exitTimer);
    }
}

void AAwakenWaterBody::raiseExited(const TWeakObjectPtr<AActor>& actor){
    // An actor that was destroyed in the water has nobody to tell: consumers drop their state with the actor.
    if(!actor.IsValid()){
        return;
    }
    FS_WaterExitedMessage message;
    message.actor = actor;
    message.kind = kind;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Water_Exited.GetTag(), message);
}

// Raised through Part A on entry only, at the surface under the actor, with the actor's stable id as the instigator. The player who splashed plays
// their own splash locally (R07), so that player's controller is the one excluded. Magnitude carries the vertical speed in cm/s for R15 and R09.
void AAwakenWaterBody::raiseSplash(AActor* actor){
    UCosmeticEventSubsystem* cosmetic = UCosmeticEventSubsystem::get(this);
    if(cosmetic == nullptr || actor == nullptr){
        return;
    }
    const FVector actorLocation = actor->GetActorLocation();
    FS_CosmeticEvent event;
    event.eventTag = T_Cosmetic_Event_World_Splash.GetTag();
    event.location = FVector(actorLocation.X, actorLocation.Y, getSurfaceHeight());
    event.instigator = UCosmeticEventSubsystem::makeInstigatorId(actor);
    event.magnitude = FMath::Abs(static_cast<float>(actor->GetVelocity().Z));
    const APawn* pawn = Cast<APawn>(actor);
    cosmetic->raiseCosmeticEvent(event, nullptr, pawn ? pawn->GetController() : nullptr);
}

// ---------------- Ice (C.3) ----------------

// Weather owns the ice: it integrates the level, applies the margin (collision on at level 1, off only below 1 minus iceThawMargin, so it
// cannot flicker) and replicates the result as iceSolid. The body follows that one flag, on every machine, so host and client flip together.
// "Walkable" means the collision is on: a player standing on thawing ice that is still solid takes no immersion illness.
void AAwakenWaterBody::syncIce(){
    UWeatherSubsystem* weather = UWeatherSubsystem::get(this);
    if(weather == nullptr || !bodyId.IsValid()){
        return;
    }
    setIceSolid(weather->isWaterFrozen(bodyId));
    if(GetNetMode() != NM_DedicatedServer){
        setIceMaterialParameter(weather->getIceLevel(bodyId));
    }
}

void AAwakenWaterBody::onIceTimer(){
    syncIce();
}

void AAwakenWaterBody::setIceSolid(bool solid){
    if(solid == isIceSolid){
        return;
    }
    isIceSolid = solid;
    for(const TWeakObjectPtr<UPrimitiveComponent>& ice : iceCollisions){
        if(UPrimitiveComponent* component = ice.Get()){
            component->SetCollisionEnabled(solid ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
        }
    }
    UE_LOG(LogAwakenEnvironment, Log, TEXT("Water body %s ice collision %s"), *bodyId.ToString(), solid ? TEXT("on") : TEXT("off"));
    // When the collision turns on with someone in the water, lift them onto the ice (A98). Only the host does it.
    if(solid && HasAuthority()){
        liftSwimmersOntoIce();
    }
}

void AAwakenWaterBody::setIceMaterialParameter(float level){
    if(FMath::IsNearlyEqual(level, lastIceLevel)){
        return;
    }
    lastIceLevel = level;
    if(iceMaterial != nullptr){
        iceMaterial->SetScalarParameterValue(iceLevelParameter, level);
    }
    iceMesh->SetVisibility(level > 0.001f && iceMaterial != nullptr);
}

void AAwakenWaterBody::liftSwimmersOntoIce(){
    TArray<TWeakObjectPtr<AActor>> swimmers;
    for(const TPair<TWeakObjectPtr<AActor>, int32>& pair : overlapCounts){
        swimmers.Add(pair.Key);
    }
    for(const TWeakObjectPtr<AActor>& swimmer : swimmers){
        ACharacter* character = Cast<ACharacter>(swimmer.Get());
        FVector target;
        if(character == nullptr || !computeIceLiftTarget(character, target)){
            continue;
        }
        // Gap G8: R07's movement component should own this call and its spec does not describe it yet. Until it does, the body moves the
        // character itself, and takes it out of Swim so the movement mode agrees with the new position.
        character->TeleportTo(target, character->GetActorRotation(), false, false);
        if(UCharacterMovementComponent* movement = character->GetCharacterMovement()){
            if(movement->IsSwimming()){
                movement->SetMovementMode(MOVE_Falling);
            }
        }
        UE_LOG(LogAwakenEnvironment, Log, TEXT("Lifted %s onto the ice of %s"), *GetNameSafe(character), *bodyId.ToString());
    }
}

// Where a swimmer ends up when the ice turns on (A98, A108, Q-08): straight up onto the ice surface; if the space above is blocked, as far up
// as there is free room; and if there is none at all, the nearest free point on the ice. Returns false when nothing needs to move.
bool AAwakenWaterBody::computeIceLiftTarget(ACharacter* character, FVector& outTarget) const{
    const UCapsuleComponent* capsule = character->GetCapsuleComponent();
    const float radius = capsule->GetScaledCapsuleRadius();
    const float halfHeight = capsule->GetScaledCapsuleHalfHeight();
    const float clearance = settings ? settings->iceLiftClearanceCm : 5.0f;
    const float searchRadius = settings ? settings->iceLiftSearchRadiusCm : 400.0f;

    const FVector current = character->GetActorLocation();
    const float surface = getSurfaceHeight();
    const float desiredZ = surface + halfHeight + clearance;
    if(current.Z >= desiredZ){
        return false;   // already above the ice
    }

    const FVector wanted(current.X, current.Y, desiredZ);
    const FCollisionShape shape = FCollisionShape::MakeCapsule(radius, halfHeight);
    // The ice collision is on by now and would block the sweep, so the body itself and the character are ignored.
    FCollisionQueryParams params(SCENE_QUERY_STAT(IceLift), false, character);
    params.AddIgnoredActor(this);

    FHitResult hit;
    if(!GetWorld()->SweepSingleByChannel(hit, current, wanted, FQuat::Identity, ECC_Pawn, shape, params)){
        outTarget = wanted;
        return true;
    }
    if(!hit.bStartPenetrating && hit.Time > KINDA_SMALL_NUMBER){
        outTarget = current + (wanted - current) * hit.Time;   // lift as far as there is free room
        return true;
    }

    // No room straight above: the nearest free point on the ice, in rings around the swimmer.
    const float step = FMath::Max(radius, 50.0f);
    for(float ring = step; ring <= searchRadius; ring += step){
        for(int32 direction = 0; direction < 8; direction++){
            const float angle = FMath::DegreesToRadians(45.0f * direction);
            const FVector candidate(current.X + FMath::Cos(angle) * ring, current.Y + FMath::Sin(angle) * ring, desiredZ);
            if(!containsPoint(FVector(candidate.X, candidate.Y, surface - 1.0f))){
                continue;   // not over this body
            }
            if(!GetWorld()->OverlapBlockingTestByChannel(candidate, FQuat::Identity, ECC_Pawn, shape, params)){
                outTarget = candidate;
                return true;
            }
        }
    }
    outTarget = wanted;   // nothing free found: TeleportTo with collision adjustment looks for the nearest spot
    return true;
}

// ---------------- Debug and data checks ----------------

#if !UE_BUILD_SHIPPING
void AAwakenWaterBody::drawDebug(float lifetime) const{
    UWorld* world = GetWorld();
    if(world == nullptr){
        return;
    }
    const FColor color = isIceSolid ? FColor::Cyan : FColor::Blue;
    for(const TWeakObjectPtr<UPrimitiveComponent>& volume : swimVolumes){
        if(const UPrimitiveComponent* component = volume.Get()){
            DrawDebugBox(world, component->Bounds.Origin, component->Bounds.BoxExtent, color, false, lifetime);
        }
    }
    if(!worldBounds.IsValid){
        return;
    }
    const UWeatherSubsystem* weather = UWeatherSubsystem::get(this);
    const FVector centre = worldBounds.GetCenter();
    const FVector top(centre.X, centre.Y, getSurfaceHeight());
    const float strength = weather ? weather->getRiverStrength(bodyId) : 0.0f;
    const float ice = weather ? weather->getIceLevel(bodyId) : 0.0f;
    if(hasCurrent()){
        const float scale = settings ? settings->flowSpeedPerStrength : 2.0f;
        const FVector flow = flowDirection.GetSafeNormal() * (baseFlowSpeed * strength * scale);
        DrawDebugDirectionalArrow(world, top, top + flow, 80.0f, FColor::Green, false, lifetime, 0, 4.0f);
    }
    DrawDebugString(world, top + FVector(0.0f, 0.0f, 50.0f), FString::Printf(TEXT("%s %s river=%.0f ice=%.2f%s"), *bodyId.ToString().Left(8),
        *StaticEnum<E_WaterKind>()->GetNameStringByValue(static_cast<int64>(kind)), strength, ice, isIceSolid ? TEXT(" SOLID") : TEXT("")), nullptr, FColor::White, lifetime);
}
#endif

#if WITH_EDITOR
EDataValidationResult AAwakenWaterBody::IsDataValid(FDataValidationContext& context) const{
    EDataValidationResult result = Super::IsDataValid(context);
    bool isInvalid = false;

    // Doc02 19.3 kind 1: an empty or duplicate id fails the build.
    if(!bodyId.IsValid()){
        context.AddError(FText::FromString(TEXT("AAwakenWaterBody has an empty bodyId")));
        isInvalid = true;
    }else if(const UWorld* world = GetWorld()){
        for(TActorIterator<AAwakenWaterBody> it(const_cast<UWorld*>(world)); it; ++it){
            if(*it != this && it->bodyId == bodyId){
                context.AddError(FText::FromString(FString::Printf(TEXT("AAwakenWaterBody %s has the same bodyId as %s"), *GetName(), *it->GetName())));
                isInvalid = true;
                break;
            }
        }
    }
    if(hasCurrent() && flowDirection.IsNearlyZero()){
        context.AddWarning(FText::FromString(TEXT("A river or stream has no flowDirection, so it has no current")));
    }
    if(kind == E_WaterKind::Sea && canFreeze){
        context.AddWarning(FText::FromString(TEXT("The Sea never freezes: canFreeze is ignored on it. Untick it to keep the data clean")));
    }
    if(isInvalid){
        return EDataValidationResult::Invalid;
    }
    return result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : result;
}
#endif