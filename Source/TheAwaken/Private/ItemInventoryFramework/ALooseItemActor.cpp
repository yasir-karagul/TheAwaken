#include "ItemInventoryFramework/ALooseItemActor.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"

ALooseItemActor::ALooseItemActor(){
    bReplicates = true;
    SetReplicatingMovement(true);
    mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(mesh);
    mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
}

void ALooseItemActor::BeginPlay(){
    Super::BeginPlay();
    refreshMesh();
}

void ALooseItemActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ALooseItemActor, entry);
}

void ALooseItemActor::setEntry(const FS_InventoryEntry& newEntry){
    entry = newEntry;
    entry.reservedQuantity = 0;
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(entry.itemId) : nullptr;
    // Each kind has its own lifetime in data; 0 on the item means the project default (Doc02 19.7).
    const float ownLifetime = definition != nullptr ? definition->looseLifetimeSeconds : 0.0f;
    const float lifetime = ownLifetime > 0.0f ? ownLifetime : UInventorySettings::get()->defaultLooseLifetimeSeconds;
    if(lifetime > 0.0f){
        SetLifeSpan(lifetime);
    }
    refreshMesh();
}

void ALooseItemActor::reduceQuantity(int32 takenQuantity){
    entry.totalQuantity -= takenQuantity;
    if(entry.totalQuantity <= 0){
        Destroy();
    }
}

FS_InventoryEntry ALooseItemActor::getEntry() const{
    return entry;
}

void ALooseItemActor::OnRep_entry(){
    refreshMesh();
}

void ALooseItemActor::refreshMesh(){
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(entry.itemId) : nullptr;
    if(definition == nullptr || definition->worldMesh.IsNull()){
        return;
    }
    // The world mesh is a soft reference (Doc02 19.8): load it only now that something shows it.
    const TSoftObjectPtr<UStaticMesh> wanted = definition->worldMesh;
    UAssetManager::GetStreamableManager().RequestAsyncLoad(wanted.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this, [this, wanted](){
        mesh->SetStaticMesh(wanted.Get());
        if(HasAuthority()){
            mesh->SetSimulatePhysics(true);   // the server simulates; clients receive the position; a body at rest goes to sleep
        }
    }));
}
