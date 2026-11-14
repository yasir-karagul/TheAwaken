#include "CarryEncumbranceFramework/AThrownItemActor.h"
#include "CarryEncumbranceFramework/CarryLog.h"
#include "CarryEncumbranceFramework/CarryServices.h"
#include "CarryEncumbranceFramework/UCarrySettings.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AThrownItemActor::AThrownItemActor(){
    bReplicates = true;
    SetReplicatingMovement(true);

    collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    collision->InitSphereRadius(UCarrySettings::get()->throwCollisionRadius);
    collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    SetRootComponent(collision);

    // The mesh is only the look. The sphere does the colliding, so the flight never waits for a mesh to load.
    mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    mesh->SetupAttachment(collision);
    mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    movement->bShouldBounce = false;           // it stops at the first thing it hits
    movement->ProjectileGravityScale = 1.0f;
    movement->bRotationFollowsVelocity = true;
}

void AThrownItemActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AThrownItemActor, itemId);
}

// A client receives the server's velocity here and hands it to its own movement component, so the flight looks smooth
// between two position updates.
void AThrownItemActor::PostNetReceiveVelocity(const FVector& NewVelocity){
    Super::PostNetReceiveVelocity(NewVelocity);
    if(movement != nullptr){
        movement->Velocity = NewVelocity;
    }
}

void AThrownItemActor::launch(AActor* newThrower, const FS_InventoryEntry& newEntry, const FVector& velocity, float newImpactDamage){
    entry = newEntry;
    entry.reservedQuantity = 0;
    itemId = entry.itemId;
    thrower = newThrower;
    const APawn* throwerPawn = Cast<APawn>(newThrower);
    throwerPlayer = throwerPawn != nullptr ? throwerPawn->GetPlayerState() : nullptr;
    impactDamage = newImpactDamage;

    if(newThrower != nullptr){
        collision->IgnoreActorWhenMoving(newThrower, true);   // it must not hit the hands it left
    }
    movement->Velocity = velocity;
    movement->OnProjectileStop.AddDynamic(this, &AThrownItemActor::onProjectileStopped);
    // The safety net of rule 5: a short timer while the item flies (Doc02 §5).
    GetWorldTimerManager().SetTimer(flightTimer, this, &AThrownItemActor::forceLand, UCarrySettings::get()->maxThrowFlightSeconds, false);
    refreshMesh();
}

void AThrownItemActor::onProjectileStopped(const FHitResult& impactResult){
    land(impactResult.GetActor(), impactResult.ImpactPoint + impactResult.ImpactNormal * collision->GetScaledSphereRadius());
}

void AThrownItemActor::forceLand(){
    land(nullptr, GetActorLocation());
}

void AThrownItemActor::land(AActor* hitActor, const FVector& location){
    if(hasLanded || !HasAuthority()){
        return;
    }
    hasLanded = true;
    GetWorldTimerManager().ClearTimer(flightTimer);

    bool shouldLand = true;
    const UCarryServices* carryServices = UCarryServices::get(this);
    if(IsValid(hitActor) && hitActor != thrower.Get() && carryServices != nullptr){
        // Doc R03 decides what the hit does. It may change the entry (a weapon loses one use) and may say "do not land".
        shouldLand = carryServices->applyThrowImpact(hitActor, thrower.Get(), entry, impactDamage, location);
    }
    if(shouldLand){
        // From here it is an ordinary world pickup with its loose lifetime (R01 3.11, Doc02 19.7).
        const UInventoryServices* services = UInventoryServices::get(this);
        if(services != nullptr){
            services->spawnLooseItem(throwerPlayer.Get(), entry, location);
        }
    }
    CARRY_LOG(Log, this, "Thrown %s landed at %s%s%s%s", *entry.itemId.ToString(), *location.ToCompactString(), IsValid(hitActor) ? TEXT(", it hit ") : TEXT(""), IsValid(hitActor) ? *hitActor->GetName() : TEXT(""), shouldLand ? TEXT("") : TEXT(" (the impact sink kept it from landing)"));
    Destroy();
}

FS_InventoryEntry AThrownItemActor::getEntry() const{
    return entry;
}

void AThrownItemActor::OnRep_itemId(){
    refreshMesh();
}

void AThrownItemActor::refreshMesh(){
    if(GetNetMode() == NM_DedicatedServer){
        return;
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(itemId) : nullptr;
    if(definition == nullptr || definition->worldMesh.IsNull()){
        return;
    }
    const TSoftObjectPtr<UStaticMesh> wanted = definition->worldMesh;
    UAssetManager::GetStreamableManager().RequestAsyncLoad(wanted.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this, [this, wanted](){
        if(mesh != nullptr){
            mesh->SetStaticMesh(wanted.Get());
        }
    }));
}
