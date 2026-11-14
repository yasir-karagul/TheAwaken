#include "CarryEncumbranceFramework/CarryServices.h"
#include "CarryEncumbranceFramework/CarryLog.h"
#include "CarryEncumbranceFramework/UCarryComponent.h"
#include "CarryEncumbranceFramework/03Interfaces/BPI_ThrowImpactSink.h"
#include "CarryEncumbranceFramework/Stubs/CarryStubs.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEventSubsystem.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

void UCarryServices::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);
    // R01's services must exist first: its Initialize registers its own stubs, and this replaces one of them.
    Collection.InitializeDependency<UInventoryServices>();

    UInventoryServices* registry = getRegistry();
    if(registry == nullptr){
        CARRY_LOG(Error, this, "Carry framework: UInventoryServices is missing");
        return;
    }
    // Default the outgoing seam to its stub, unless a real framework registered first.
    if(registry->getSeam(UBPI_ThrowImpactSink::StaticClass()) == nullptr){
        registry->registerSeam(UBPI_ThrowImpactSink::StaticClass(), NewObject<UStubThrowImpactSink>(this));   // Doc R03
    }
    // This object is the real IBPI_CarryService R01 has been calling (C4.2). R01 registers no stub for it any more.
    registry->registerSeam(UBPI_CarryService::StaticClass(), this);
    CARRY_LOG(Log, this, "Carry framework started");
}

UCarryServices* UCarryServices::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    if(gameInstance == nullptr){
        return nullptr;
    }
    return gameInstance->GetSubsystem<UCarryServices>();
}

UInventoryServices* UCarryServices::getRegistry() const{
    const UGameInstance* gameInstance = GetGameInstance();
    return gameInstance != nullptr ? gameInstance->GetSubsystem<UInventoryServices>() : nullptr;
}

// ---------- IBPI_CarryService ----------

bool UCarryServices::isCarryActive_Implementation(APlayerState* player){
    const UCarryComponent* carry = UCarryComponent::findFor(player);
    return carry != nullptr && carry->isCarryActive();
}

bool UCarryServices::beginCarry_Implementation(APlayerState* player, const FS_InventoryEntry& entry){
    UCarryComponent* carry = UCarryComponent::findFor(player);
    // No pawn or no component: the hands cannot take it, so R01 lets the item land loose. Nothing is lost.
    return carry != nullptr && carry->beginCarry(entry).succeeded();
}

// ---------- facade ----------

bool UCarryServices::applyThrowImpact(AActor* target, AActor* thrower, FS_InventoryEntry& entry, float damage, const FVector& location) const{
    const UInventoryServices* registry = getRegistry();
    UObject* sink = registry != nullptr ? registry->getSeam(UBPI_ThrowImpactSink::StaticClass()) : nullptr;
    if(sink == nullptr){
        return true;   // nobody to tell: the item simply lands
    }
    return IBPI_ThrowImpactSink::Execute_applyThrowImpact(sink, target, thrower, entry, damage, location);
}

void UCarryServices::raiseCosmeticEvent(const FGameplayTag& eventTag, const FVector& location, AActor* instigator) const{
    // Doc B21's own raise function exists, so it is called directly (the R05 guide did the same, E1.4).
    // The subsystem raises only on the host and sends the event to the clients in range.
    UCosmeticEventSubsystem* cosmetics = UCosmeticEventSubsystem::get(this);
    if(cosmetics == nullptr){
        return;
    }
    FS_CosmeticEvent event;
    event.eventTag = eventTag;
    event.location = location;
    event.instigator = UCosmeticEventSubsystem::makeInstigatorId(instigator);
    event.magnitude = 1.0f;
    cosmetics->raiseCosmeticEvent(event);
}
