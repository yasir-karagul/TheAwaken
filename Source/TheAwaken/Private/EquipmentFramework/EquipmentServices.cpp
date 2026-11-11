#include "EquipmentFramework/EquipmentServices.h"
#include "EquipmentFramework/EquipmentLog.h"
#include "EquipmentFramework/UEquipmentComponent.h"
#include "EquipmentFramework/03Interfaces/BPI_ArmorStatsSink.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEventSubsystem.h"
#include "EquipmentFramework/Stubs/EquipmentStubs.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

void UEquipmentServices::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);
    // R01's services must exist first: its Initialize registers its own stubs, and Section 7 replaces one of them.
    Collection.InitializeDependency<UInventoryServices>();

    UInventoryServices* registry = getRegistry();
    if(registry == nullptr){
        EQUIPMENT_LOG(Error, this, "Equipment framework: UInventoryServices is missing");
        return;
    }
    // Default each outgoing seam to its stub, unless a real framework registered first.
    if(registry->getSeam(UBPI_ArmorStatsSink::StaticClass()) == nullptr){
        registry->registerSeam(UBPI_ArmorStatsSink::StaticClass(), NewObject<UStubArmorStatsSink>(this));
    }
    // Replace R01's UStubEquipmentGear with the real implementation (R01 guide, stub register).
    registry->registerSeam(UBPI_EquipmentGear::StaticClass(), this);
    EQUIPMENT_LOG(Log, this, "Equipment framework started");
}

UEquipmentServices* UEquipmentServices::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    if(gameInstance == nullptr){
        return nullptr;
    }
    return gameInstance->GetSubsystem<UEquipmentServices>();
}

UInventoryServices* UEquipmentServices::getRegistry() const{
    const UGameInstance* gameInstance = GetGameInstance();
    return gameInstance != nullptr ? gameInstance->GetSubsystem<UInventoryServices>() : nullptr;
}

void UEquipmentServices::applyArmorAggregate(APlayerState* player, float totalArmorValue) const{
    const UInventoryServices* registry = getRegistry();
    UObject* sink = registry != nullptr ? registry->getSeam(UBPI_ArmorStatsSink::StaticClass()) : nullptr;
    if(sink != nullptr){
        IBPI_ArmorStatsSink::Execute_applyArmorAggregate(sink, player, totalArmorValue);
    }
}

void UEquipmentServices::raiseCosmeticEvent(const FGameplayTag& eventTag, const FVector& location, AActor* instigator, float magnitude) const{
    // Doc B21's own raise function exists, so it is called directly instead of through a seam (E1.4, E10.8).
    // The subsystem raises only on the host and sends the event to the clients in range.
    UCosmeticEventSubsystem* cosmetics = UCosmeticEventSubsystem::get(this);
    if(cosmetics == nullptr){
        return;
    }
    FS_CosmeticEvent event;
    event.eventTag = eventTag;
    event.location = location;
    event.instigator = UCosmeticEventSubsystem::makeInstigatorId(instigator);
    event.magnitude = magnitude;
    cosmetics->raiseCosmeticEvent(event);
    EQUIPMENT_LOG(Verbose, this, "Cosmetic event %s raised at %s", *eventTag.ToString(), *location.ToCompactString());
}

FS_GearReleaseResult UEquipmentServices::releaseEquippedGear_Implementation(APlayerState* player){
    UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
    if(equipment == nullptr){
        return FS_GearReleaseResult();   // no component: nothing worn, and that is a success
    }
    return equipment->releaseEquippedGear();
}

bool UEquipmentServices::reEquipGear_Implementation(APlayerState* player, const TArray<FS_GearSlotRecord>& gear){
    UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
    return equipment != nullptr && equipment->reEquipGear(gear);
}
