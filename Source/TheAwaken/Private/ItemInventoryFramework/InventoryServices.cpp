#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/Stubs/InventoryStubs.h"
#include "ItemInventoryFramework/03Interfaces/BPI_EquipmentGear.h"
#include "ItemInventoryFramework/ULooseItemSpawner.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "CosmeticEventsEnvironmentPresentationFramework/CosmeticEventSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

#if !UE_BUILD_SHIPPING
// Tests only. -1 = a different sequence every run.
static TAutoConsoleVariable<int32> cvarRandomSeed(TEXT("Awaken.Inventory.RandomSeed"), -1, TEXT("Fixed seed for the illness-chance roll (tests only). -1 = not fixed. Read when the game instance starts."), ECVF_Cheat);
#endif

void UInventoryServices::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);

    // One stub per seam. A real framework replaces its stub with registerSeam.
    registerSeam(UBPI_GameClock::StaticClass(), NewObject<UStubGameClock>(this));                  // Doc R12
    registerSeam(UBPI_PlayerStateQuery::StaticClass(), NewObject<UStubPlayerStateQuery>(this));    // Docs B02, R03, R11, R17
    registerSeam(UBPI_CarryService::StaticClass(), NewObject<UStubCarryService>(this));            // Doc B07
    registerSeam(UBPI_SurvivalService::StaticClass(), NewObject<UStubSurvivalService>(this));      // Doc B03
    registerSeam(UBPI_HealthService::StaticClass(), NewObject<UStubHealthService>(this));          // Docs R08, R03
    registerSeam(UBPI_ActionService::StaticClass(), NewObject<UStubActionService>(this));          // Doc R04
    // IBPI_EquipmentGear: registered by Doc R05's UEquipmentServices (its stub was removed by the R05 guide, E7.2).
    registerSeam(UBPI_BagActorService::StaticClass(), NewObject<UStubBagActorService>(this));      // Docs B02, R13
    registerSeam(UBPI_LooseItemSpawner::StaticClass(), NewObject<ULooseItemSpawner>(this));        // this framework (I7)

    int32 seed = FMath::Rand();
#if !UE_BUILD_SHIPPING
    if(cvarRandomSeed.GetValueOnGameThread() >= 0){
        seed = cvarRandomSeed.GetValueOnGameThread();
    }
#endif
    randomStream.Initialize(seed);

    INVENTORY_LOG(Log, this, "Item & Inventory framework started, %d seams registered", seams.Num());
}

UInventoryServices* UInventoryServices::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    if(gameInstance == nullptr){
        return nullptr;
    }
    return gameInstance->GetSubsystem<UInventoryServices>();
}

void UInventoryServices::registerSeam(UClass* interfaceClass, UObject* implementation){
    if(interfaceClass == nullptr || implementation == nullptr || !implementation->GetClass()->ImplementsInterface(interfaceClass)){
        INVENTORY_LOG(Error, this, "registerSeam refused: %s does not implement %s", *GetNameSafe(implementation), *GetNameSafe(interfaceClass));
        return;
    }
    seams.Add(interfaceClass, implementation);
}

UObject* UInventoryServices::getSeam(const UClass* interfaceClass) const{
    const TObjectPtr<UObject>* found = seams.Find(const_cast<UClass*>(interfaceClass));
    return found != nullptr ? found->Get() : nullptr;
}

int64 UInventoryServices::getGameTime() const{
    UObject* seam = find<UBPI_GameClock>();
    return seam != nullptr ? IBPI_GameClock::Execute_getGameTime(seam) : 0;
}

bool UInventoryServices::isIncapacitated(const APlayerState* player) const{
    UObject* seam = find<UBPI_PlayerStateQuery>();
    return seam != nullptr && IBPI_PlayerStateQuery::Execute_isIncapacitated(seam, const_cast<APlayerState*>(player));
}

bool UInventoryServices::isCarryActive(const APlayerState* player) const{
    UObject* seam = find<UBPI_CarryService>();
    return seam != nullptr && IBPI_CarryService::Execute_isCarryActive(seam, const_cast<APlayerState*>(player));
}

bool UInventoryServices::beginCarry(APlayerState* player, const FS_InventoryEntry& entry) const{
    UObject* seam = find<UBPI_CarryService>();
    return seam != nullptr && IBPI_CarryService::Execute_beginCarry(seam, player, entry);
}

void UInventoryServices::modifyMeter(APlayerState* player, const FGameplayTag& meterTag, float delta, const FGameplayTag& sourceTag) const{
    UObject* seam = find<UBPI_SurvivalService>();
    if(seam != nullptr){
        IBPI_SurvivalService::Execute_modifyMeter(seam, player, meterTag, delta, sourceTag);
    }
}

void UInventoryServices::registerIllnessSource(APlayerState* player, const FGameplayTag& sourceTag, float durationHours) const{
    UObject* seam = find<UBPI_SurvivalService>();
    if(seam != nullptr){
        IBPI_SurvivalService::Execute_registerIllnessSource(seam, player, sourceTag, durationHours);
    }
}

bool UInventoryServices::isIllnessActive(const APlayerState* player) const{
    UObject* seam = find<UBPI_SurvivalService>();
    return seam != nullptr && IBPI_SurvivalService::Execute_isIllnessActive(seam, const_cast<APlayerState*>(player));
}

void UInventoryServices::healPlayer(APlayerState* player, float amount, const FGameplayTag& sourceTag) const{
    UObject* seam = find<UBPI_HealthService>();
    if(seam != nullptr){
        IBPI_HealthService::Execute_healPlayer(seam, player, amount, sourceTag);
    }
}

void UInventoryServices::damagePlayer(APlayerState* player, float amount, const FGameplayTag& damageTypeTag) const{
    UObject* seam = find<UBPI_HealthService>();
    if(seam != nullptr){
        IBPI_HealthService::Execute_damagePlayer(seam, player, amount, damageTypeTag);
    }
}

void UInventoryServices::cancelAction(APlayerState* player, const FGameplayTag& actionTag) const{
    UObject* seam = find<UBPI_ActionService>();
    if(seam != nullptr){
        IBPI_ActionService::Execute_cancelAction(seam, player, actionTag);
    }
}

AActor* UInventoryServices::spawnLooseItem(APlayerState* instigatorPlayer, const FS_InventoryEntry& entry, const FVector& location) const{
    UObject* seam = find<UBPI_LooseItemSpawner>();
    return seam != nullptr ? IBPI_LooseItemSpawner::Execute_spawnLooseItem(seam, instigatorPlayer, entry, location) : nullptr;
}

FS_GearReleaseResult UInventoryServices::releaseEquippedGear(APlayerState* player) const{
    UObject* seam = find<UBPI_EquipmentGear>();
    return seam != nullptr ? IBPI_EquipmentGear::Execute_releaseEquippedGear(seam, player) : FS_GearReleaseResult();
}

bool UInventoryServices::reEquipGear(APlayerState* player, const TArray<FS_GearSlotRecord>& gear) const{
    UObject* seam = find<UBPI_EquipmentGear>();
    return seam != nullptr && IBPI_EquipmentGear::Execute_reEquipGear(seam, player, gear);
}

UObject* UInventoryServices::getBagService() const{
    return find<UBPI_BagActorService>();
}

void UInventoryServices::raiseItemDroppedCosmetic(APlayerState* player, const FVector& location) const{
    // S5.7 (I20): Doc B21's cosmetic channel exists, so the event is raised through it. Server only: the subsystem
    // sends it to the clients in range and every machine re-raises it locally.
    UCosmeticEventSubsystem* cosmetics = UCosmeticEventSubsystem::get(this);
    if(cosmetics == nullptr){
        return;
    }
    const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
    FS_CosmeticEvent event;
    event.eventTag = T_Cosmetic_Event_Interaction_ItemDropped;
    event.location = location;
    event.instigator = UCosmeticEventSubsystem::makeInstigatorId(pawn);
    event.magnitude = 1.0f;
    cosmetics->raiseCosmeticEvent(event);
}

bool UInventoryServices::rollChance(float chance) const{
    return randomStream.FRand() < chance;
}
