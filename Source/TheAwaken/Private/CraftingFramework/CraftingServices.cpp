#include "CraftingFramework/CraftingServices.h"
#include "CraftingFramework/CraftingLog.h"
#include "CraftingFramework/CraftingProgressionBridge.h"
#include "CraftingFramework/03Interfaces/BPI_ProgressionQuery.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEventSubsystem.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

void UCraftingServices::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);
    Collection.InitializeDependency<UInventoryServices>();   // the registry must exist first

    UInventoryServices* registry = getRegistry();
    if(registry == nullptr){
        CRAFTING_LOG(Error, this, "Crafting framework: UInventoryServices is missing");
        return;
    }
    // Default the outgoing seam to the real Doc R06 set, unless a framework registered something first.
    if(registry->getSeam(UBPI_ProgressionQuery::StaticClass()) == nullptr){
        registry->registerSeam(UBPI_ProgressionQuery::StaticClass(), NewObject<UCraftingProgressionBridge>(this));
    }
    CRAFTING_LOG(Log, this, "Crafting framework started");
}

UCraftingServices* UCraftingServices::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    if(gameInstance == nullptr){
        return nullptr;
    }
    return gameInstance->GetSubsystem<UCraftingServices>();
}

UInventoryServices* UCraftingServices::getRegistry() const{
    const UGameInstance* gameInstance = GetGameInstance();
    return gameInstance != nullptr ? gameInstance->GetSubsystem<UInventoryServices>() : nullptr;
}

bool UCraftingServices::isUnlocked(const FGameplayTag& unlockTag) const{
    if(!unlockTag.IsValid()){
        return true;
    }
    const UInventoryServices* registry = getRegistry();
    UObject* progression = registry != nullptr ? registry->getSeam(UBPI_ProgressionQuery::StaticClass()) : nullptr;
    // Nobody to ask means locked: a gated recipe must never slip through because a framework is missing.
    return progression != nullptr && IBPI_ProgressionQuery::Execute_isUnlocked(progression, unlockTag);
}

void UCraftingServices::raiseCosmeticEvent(const FGameplayTag& eventTag, const FVector& location, AActor* instigator) const{
    // Doc B21's own raise function exists, so it is called directly (as R05 and B07 do). The subsystem raises only on the
    // host and sends the event to the clients in range.
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
