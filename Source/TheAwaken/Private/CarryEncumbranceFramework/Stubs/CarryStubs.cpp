// TEMPORARY STUBS — see CarryStubs.h for the owner of each.
#include "CarryEncumbranceFramework/Stubs/CarryStubs.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

bool UStubThrowImpactSink::applyThrowImpact_Implementation(AActor* target, AActor* thrower, FS_InventoryEntry& entry, float damage, FVector location){
    hitTargets.Add(target);
    totalDamage += damage;
    return letItemLand;
}

AStubCarryableActor::AStubCarryableActor(){
    bReplicates = true;
    SetReplicatingMovement(true);
    // A visible cube, so the dev command shows something in the hands. No collision: it must not push its carrier.
    UStaticMeshComponent* body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if(cube.Succeeded()){
        body->SetStaticMesh(cube.Object);
    }
    body->SetWorldScale3D(FVector(0.4f));
    body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetRootComponent(body);
}

int32 AStubCarryableActor::getCarrySlotCost_Implementation(){
    return slotCost;
}

FName AStubCarryableActor::getCarryLabel_Implementation(){
    return TEXT("Dummy");
}

void AStubCarryableActor::onCarryBegan_Implementation(AActor* carrier){
    beganCount++;
}

void AStubCarryableActor::onCarryEnded_Implementation(AActor* carrier, E_EndCarryReason reason){
    endedCount++;
    lastReason = reason;
}
