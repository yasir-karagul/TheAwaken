// TEMPORARY STUBS — see CraftingStubs.h for the owner of each.
#include "CraftingFramework/Stubs/CraftingStubs.h"
#include "CraftingFramework/UCraftingStationComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

bool UStubProgressionQuery::isUnlocked_Implementation(FGameplayTag unlockTag){
    return unlockEverything || unlockedTags.HasTagExact(unlockTag);
}

AStubCraftingStation::AStubCraftingStation(){
    bReplicates = true;
    UStaticMeshComponent* body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if(cube.Succeeded()){
        body->SetStaticMesh(cube.Object);
    }
    body->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.6f));
    body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetRootComponent(body);
    stationComponent = CreateDefaultSubobject<UCraftingStationComponent>(TEXT("Station"));
}
