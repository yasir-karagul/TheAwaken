#include "EquipmentFramework/UEquipmentComponent.h"
#include "EquipmentFramework/EquipmentServices.h"
#include "EquipmentFramework/UEquipmentSettings.h"
#include "EquipmentFramework/EquipmentLog.h"
#include "EquipmentFramework/02Tags/T_EquipmentFramework.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UContainerViewComponent.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "GameFramework/Pawn.h"
#include "ItemInventoryFramework/UInventoryRequestComponent.h"
#include "SaveFramework/SaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "StructUtils/InstancedStruct.h"

#if !UE_BUILD_SHIPPING
// Tests only: -1 = use the project setting. Lets a test lower the cap while pieces are worn (R05 3.2, S2-F22).
static TAutoConsoleVariable<int32> cvarStackCap(TEXT("Awaken.Equipment.StackCap"), -1, TEXT("Override armorStackCap (tests only). -1 = use the project setting."), ECVF_Cheat);
#endif

// ---------- construction and replication ----------

UEquipmentComponent::UEquipmentComponent(){
    SetIsReplicatedByDefault(true);
}

void UEquipmentComponent::BeginPlay(){
    Super::BeginPlay();
    recalculate();
    if(isServerSide()){
        pushStats();   // a fresh player reports an aggregate of 0
        USaveSubsystem* save = (GetWorld() != nullptr && GetWorld()->GetGameInstance() != nullptr) ? GetWorld()->GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
        if(save != nullptr){
            save->registerParticipant(this);   // Doc R13 (E8.1)
        }
    }
}

void UEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UEquipmentComponent, armorStack);     // COND_None: everyone sees worn armor (R05 §8)
    DOREPLIFETIME(UEquipmentComponent, wornClothes);
}

UEquipmentComponent* UEquipmentComponent::findFor(const APlayerState* player){
    return player != nullptr ? player->FindComponentByClass<UEquipmentComponent>() : nullptr;
}

bool UEquipmentComponent::isServerSide() const{
    return GetOwner() != nullptr && GetOwner()->HasAuthority();
}

APlayerState* UEquipmentComponent::getOwnerPlayerState() const{
    return Cast<APlayerState>(GetOwner());
}

// ---------- queries ----------

const TArray<FS_EquippedArmorState>& UEquipmentComponent::getArmorStack() const{
    return armorStack;
}

TArray<FS_EquippedArmorState> UEquipmentComponent::getArmorStackCopy() const{
    return armorStack;
}

FS_WornClothesState UEquipmentComponent::getWornClothes() const{
    return wornClothes;
}

FS_AggregatedArmorStats UEquipmentComponent::getAggregatedArmorStats() const{
    return cachedArmorStats;
}

FS_ClothesProtection UEquipmentComponent::getClothesProtection() const{
    return cachedClothes;
}

float UEquipmentComponent::getTotalWarmth() const{
    return cachedArmorStats.warmth + cachedClothes.warmth;
}

float UEquipmentComponent::getTotalRemainingDurability() const{
    float total = 0.0f;
    for(const FS_EquippedArmorState& piece : armorStack){
        total += piece.remainingDurability;
    }
    return total;
}

int32 UEquipmentComponent::getArmorStackCap() const{
#if !UE_BUILD_SHIPPING
    const int32 overrideCap = cvarStackCap.GetValueOnGameThread();
    if(overrideCap >= 0){
        return overrideCap;
    }
#endif
    return UEquipmentSettings::get()->armorStackCap;
}

// ---------- the cache ----------

void UEquipmentComponent::recalculate(){
    cachedArmorStats = FS_AggregatedArmorStats();
    cachedClothes = FS_ClothesProtection();
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    if(catalog == nullptr){
        return;
    }
    // The stack protects the player as a whole: the values simply add up (R05 3.2). Position plays no part.
    for(const FS_EquippedArmorState& piece : armorStack){
        const UDA_ItemDefinition* definition = catalog->getDefinition(piece.itemId);
        if(definition != nullptr){
            cachedArmorStats.armorValue += definition->armorValue;
            cachedArmorStats.warmth += definition->warmth;
        }
        cachedArmorStats.pieceCount++;
    }
    if(!wornClothes.isDefault()){
        const UDA_ItemDefinition* clothes = catalog->getDefinition(wornClothes.itemId);
        if(clothes != nullptr){
            cachedClothes.protection = clothes->clothesProtection;
            cachedClothes.warmth = clothes->warmth;
        }
    }
}

void UEquipmentComponent::pushStats() const{
    APlayerState* player = getOwnerPlayerState();
    const UEquipmentServices* services = UEquipmentServices::get(this);
    if(services != nullptr){
        services->applyArmorAggregate(player, cachedArmorStats.armorValue);   // Doc R08 ApplyArmorAggregate
    }
    FS_ArmorStatsMessage message;
    message.player = player;
    message.aggregatedArmorValue = cachedArmorStats.armorValue;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Equipment_StatsRecalculated, message);   // server only (R05 §7)
}

void UEquipmentComponent::finishChange(){
    recalculate();
    if(isServerSide()){
        pushStats();
        GetOwner()->ForceNetUpdate();   // a PlayerState replicates slowly by default; worn gear should not lag a second behind
    }
    onEquipmentChanged.Broadcast(getOwnerPlayerState());
}

void UEquipmentComponent::finishWearChange(){
    if(isServerSide()){
        GetOwner()->ForceNetUpdate();
    }
    onEquipmentChanged.Broadcast(getOwnerPlayerState());
}

void UEquipmentComponent::renumberStack(){
    for(int32 index = 0; index < armorStack.Num(); index++){
        armorStack[index].stackIndex = index;   // E2: the stack closes up
    }
}

// ---------- messages (R05 §7, Doc02 §19.2: local on every machine) ----------

void UEquipmentComponent::raiseArmorMessage(const FGameplayTag& channel, const FS_EquippedArmorState& piece) const{
    FS_ArmorChangedMessage message;
    message.player = getOwnerPlayerState();
    message.stackIndex = piece.stackIndex;
    message.instanceId = piece.instanceId;
    message.itemId = piece.itemId;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(channel, message);
}

void UEquipmentComponent::raiseClothesMessage() const{
    FS_ClothesChangedMessage message;
    message.player = getOwnerPlayerState();
    message.itemId = wornClothes.itemId;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Equipment_ClothesChanged, message);
}

// A client learns of a change only here. The previous value lets it tell what was added and what left.
void UEquipmentComponent::OnRep_armorStack(const TArray<FS_EquippedArmorState>& previousStack){
    recalculate();
    for(const FS_EquippedArmorState& before : previousStack){
        const bool stillWorn = armorStack.ContainsByPredicate([&before](const FS_EquippedArmorState& piece){
            return piece.instanceId == before.instanceId;
        });
        if(!stillWorn){
            raiseArmorMessage(T_Equipment_ArmorUnequipped, before);   // unequipped, released or broken: the mesh goes
        }
    }
    for(const FS_EquippedArmorState& piece : armorStack){
        const bool wasWorn = previousStack.ContainsByPredicate([&piece](const FS_EquippedArmorState& before){
            return before.instanceId == piece.instanceId;
        });
        if(!wasWorn){
            raiseArmorMessage(T_Equipment_ArmorEquipped, piece);
        }
    }
    onEquipmentChanged.Broadcast(getOwnerPlayerState());   // also covers a wear-only change
}

void UEquipmentComponent::OnRep_wornClothes(const FS_WornClothesState& previousClothes){
    recalculate();
    if(previousClothes.itemId != wornClothes.itemId){
        raiseClothesMessage();
    }
    onEquipmentChanged.Broadcast(getOwnerPlayerState());
}

// ---------- small helpers ----------

FS_InventoryEntry UEquipmentComponent::makeEntry(const FS_EquippedArmorState& piece){
    FS_InventoryEntry entry;
    entry.itemId = piece.itemId;
    entry.instanceId = piece.instanceId;
    entry.totalQuantity = 1;
    entry.state.durability = piece.remainingDurability;
    return entry;
}

FS_InventoryResult UEquipmentComponent::refusal(const FGameplayTag& reason){
    FS_InventoryResult result;
    result.failReason = reason;
    return result;
}

FS_InventoryResult UEquipmentComponent::accepted(){
    FS_InventoryResult result;
    result.quantity = 1;
    return result;
}

// ---------- Section 4: armor commands ----------

FS_InventoryResult UEquipmentComponent::equipArmor(const FGuid& instanceId){
    if(!isServerSide()){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    APlayerState* player = getOwnerPlayerState();
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    if(inventory == nullptr || catalog == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(!inventory->hasBag()){
        return refusal(T_Inventory_FailReason_NoBag);   // a Bagless player cannot equip armor (R05 3.3, R01 3.8)
    }
    UInventoryData* bag = inventory->getPersonalInventory();
    const FS_InventoryEntry* entry = bag->findInstance(instanceId);
    if(entry == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    const UDA_ItemDefinition* definition = catalog->getDefinition(entry->itemId);
    if(definition == nullptr || !definition->isArmor()){
        return refusal(T_Equipment_FailReason_NotArmor);
    }
    if(entry->reservedQuantity > 0){
        return refusal(T_Inventory_FailReason_ReservedQuantity);   // it is staged for a craft (R01 3.6)
    }
    if(armorStack.Num() >= getArmorStackCap()){
        return refusal(T_Equipment_FailReason_StackFull);   // refused with a short reason (R05 3.2)
    }

    // Everything checked. Copy what we need first: removing the entry invalidates the pointer.
    FS_EquippedArmorState piece;
    piece.stackIndex = armorStack.Num();   // the next free position
    piece.instanceId = entry->instanceId;
    piece.itemId = entry->itemId;
    piece.remainingDurability = entry->state.durability;

    const FS_InventoryResult removed = bag->removeInstance(instanceId);
    if(!removed.succeeded()){
        return removed;
    }
    armorStack.Add(piece);
    finishChange();
    raiseArmorMessage(T_Equipment_ArmorEquipped, piece);
    EQUIPMENT_LOG(Log, this, "Equipped %s at position %d (durability %.1f), aggregate armor %.1f", *piece.itemId.ToString(), piece.stackIndex, piece.remainingDurability, cachedArmorStats.armorValue);
    return accepted();
}

FS_InventoryResult UEquipmentComponent::unequipArmor(int32 stackIndex){
    if(!isServerSide()){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    if(!armorStack.IsValidIndex(stackIndex)){
        return refusal(T_Equipment_FailReason_InvalidStackIndex);
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getOwnerPlayerState());
    if(inventory == nullptr || !inventory->hasBag()){
        return refusal(T_Inventory_FailReason_NoBag);
    }
    UInventoryData* bag = inventory->getPersonalInventory();
    const FS_EquippedArmorState piece = armorStack[stackIndex];
    FS_ItemStack single;
    single.itemId = piece.itemId;
    single.quantity = 1;
    if(bag->canFit(single) < 1){
        return refusal(T_Interaction_FailReason_BagFull);   // refused, the piece stays worn: nothing is lost (R05 §5)
    }

    const FS_InventoryResult added = bag->addInstance(makeEntry(piece));
    if(!added.succeeded()){
        return added;
    }
    armorStack.RemoveAt(stackIndex);
    renumberStack();
    finishChange();
    raiseArmorMessage(T_Equipment_ArmorUnequipped, piece);
    EQUIPMENT_LOG(Log, this, "Unequipped %s from position %d, aggregate armor %.1f", *piece.itemId.ToString(), stackIndex, cachedArmorStats.armorValue);
    return accepted();
}

// ---------- Section 5: armor wear ----------

FS_ArmorDamageResult UEquipmentComponent::applyDamageToArmor(float damageAmount){
    FS_ArmorDamageResult result;
    if(!isServerSide() || damageAmount <= 0.0f || armorStack.Num() == 0){
        return result;
    }
    float remaining = damageAmount;
    TArray<FS_EquippedArmorState> brokenPieces;
    // The last piece stacked absorbs the hit first (R05 amendment). What it cannot take passes down.
    for(int32 index = armorStack.Num() - 1; index >= 0 && remaining > 0.0f; index--){
        const float absorbed = FMath::Min(armorStack[index].remainingDurability, remaining);
        armorStack[index].remainingDurability -= absorbed;
        remaining -= absorbed;
        result.absorbedAmount += absorbed;
        if(armorStack[index].remainingDurability <= 0.0f){
            brokenPieces.Add(armorStack[index]);   // copied before it leaves the array
            armorStack.RemoveAt(index);
        }
    }
    result.brokenPieces = brokenPieces.Num();

    if(brokenPieces.Num() == 0){
        finishWearChange();   // wear only: the aggregate has not changed, so it is not rebuilt (R05 §5)
        return result;
    }

    renumberStack();
    finishChange();           // a piece broke: rebuild the cache and tell Stats
    APlayerState* player = getOwnerPlayerState();
    APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
    const UEquipmentServices* services = UEquipmentServices::get(this);
    for(const FS_EquippedArmorState& piece : brokenPieces){
        raiseArmorMessage(T_Equipment_ArmorBroken, piece);        // server only (R05 §7)
        raiseArmorMessage(T_Equipment_ArmorUnequipped, piece);    // local on the host; clients raise it from replication (E11)
        if(services != nullptr && pawn != nullptr){
            // The effect and the sound travel on the cosmetic channel (R05 A89, Doc B21 A.4).
            services->raiseCosmeticEvent(T_Cosmetic_Event_Combat_ArmorBroken, pawn->GetActorLocation(), pawn, 1.0f);
        }
        EQUIPMENT_LOG(Log, this, "Armor %s broke and is destroyed, aggregate armor %.1f", *piece.itemId.ToString(), cachedArmorStats.armorValue);
    }
    return result;
}

// ---------- Section 6: clothes ----------

FGameplayTag UEquipmentComponent::checkClothesChange(FName itemId) const{
    if(!isServerSide()){
        return T_Inventory_FailReason_NotServer;
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getOwnerPlayerState());
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    if(inventory == nullptr || catalog == nullptr){
        return T_Inventory_FailReason_NotFound;
    }
    if(!inventory->hasBag()){
        return T_Inventory_FailReason_NoBag;   // a Bagless player cannot change clothes (R05 amendment)
    }
    const UDA_ItemDefinition* definition = catalog->getDefinition(itemId);
    if(definition == nullptr || !definition->isClothing()){
        return T_Equipment_FailReason_NotClothes;
    }
    if(wornClothes.itemId == itemId){
        return T_Equipment_FailReason_AlreadyWorn;
    }
    return FGameplayTag();
}

void UEquipmentComponent::applyClothesChange(FName itemId){
    wornClothes.itemId = itemId;
    finishChange();          // clothes carry warmth, so the cache is rebuilt; the armor aggregate is simply re-sent
    raiseClothesMessage();
}

FS_InventoryResult UEquipmentComponent::equipClothes(FName itemId){
    const FGameplayTag blocked = checkClothesChange(itemId);
    if(blocked.IsValid()){
        return refusal(blocked);
    }
    APlayerState* player = getOwnerPlayerState();
    UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
    if(bag->getAvailableQuantity(itemId) < 1){
        return refusal(T_Inventory_FailReason_InsufficientQuantity);   // none in the bag, or all of them staged
    }

    const FS_InventoryResult removed = bag->removeStack(itemId, 1);   // one unit leaves the stack (S2-F12)
    if(!removed.succeeded()){
        return removed;
    }
    const FName previous = wornClothes.itemId;
    applyClothesChange(itemId);
    if(!previous.IsNone()){
        // Back to the bag; if that type is at its limit it drops at the player's feet (Doc02 §19.5, E9).
        FS_ItemStack returned;
        returned.itemId = previous;
        returned.quantity = 1;
        UBFL_Inventory::grantOrDrop(player, returned);
    }
    EQUIPMENT_LOG(Log, this, "Clothes changed from %s to %s", previous.IsNone() ? TEXT("default") : *previous.ToString(), *itemId.ToString());
    return accepted();
}

FS_InventoryResult UEquipmentComponent::equipClothesFromContainer(FName itemId){
    const FGameplayTag blocked = checkClothesChange(itemId);
    if(blocked.IsValid()){
        return refusal(blocked);
    }
    APlayerState* player = getOwnerPlayerState();
    const UContainerViewComponent* view = UContainerViewComponent::findFor(player);
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    UInventoryData* source = (view != nullptr && registry != nullptr) ? registry->findInventory(view->getViewedContainerId()) : nullptr;
    if(source == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);   // no container is open
    }
    if(source->isDeathBag()){
        return refusal(T_Inventory_FailReason_DeathBagLocked);
    }
    if(source->getAvailableQuantity(itemId) < 1){
        return refusal(T_Inventory_FailReason_InsufficientQuantity);
    }

    const FS_InventoryResult removed = source->removeStack(itemId, 1);
    if(!removed.succeeded()){
        return removed;
    }
    const FName previous = wornClothes.itemId;
    applyClothesChange(itemId);
    if(!previous.IsNone()){
        // "The old clothes are dropped on the ground" (R05 amendment). Dropped, never destroyed.
        FS_InventoryEntry loose;
        loose.itemId = previous;
        loose.totalQuantity = 1;
        const UInventoryServices* services = UInventoryServices::get(this);
        if(services != nullptr){
            services->spawnLooseItem(player, loose, UBFL_Inventory::getDropLocation(player));
        }
    }
    EQUIPMENT_LOG(Log, this, "Clothes changed from %s to %s (taken from a container, old ones dropped)", previous.IsNone() ? TEXT("default") : *previous.ToString(), *itemId.ToString());
    return accepted();
}

// ---------- Section 7: bag lifecycle ----------

FS_GearReleaseResult UEquipmentComponent::releaseEquippedGear(){
    FS_GearReleaseResult result;
    if(!isServerSide()){
        result.succeeded = false;
        return result;
    }
    const TArray<FS_EquippedArmorState> released = armorStack;
    for(const FS_EquippedArmorState& piece : released){
        result.gearEntries.Add(makeEntry(piece));   // same instance id, current durability
        FS_GearSlotRecord record;
        record.instanceId = piece.instanceId;
        record.stackIndex = piece.stackIndex;
        result.gearSlots.Add(record);
    }
    if(released.Num() == 0){
        return result;   // nothing worn: nothing to announce
    }
    armorStack.Reset();
    finishChange();      // aggregate is now 0 and Stats is told: a Bagless player has no armor protection (S2-F03)
    for(const FS_EquippedArmorState& piece : released){
        raiseArmorMessage(T_Equipment_ArmorUnequipped, piece);
    }
    EQUIPMENT_LOG(Log, this, "Released %d worn armor pieces to the bag record", released.Num());
    return result;
}

bool UEquipmentComponent::reEquipGear(const TArray<FS_GearSlotRecord>& gear){
    if(!isServerSide()){
        return false;
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getOwnerPlayerState());
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    if(bag == nullptr){
        return false;
    }
    // "In the slots it occupied" (Doc B02): re-adding in ascending position rebuilds the same stack.
    TArray<FS_GearSlotRecord> ordered = gear;
    ordered.Sort([](const FS_GearSlotRecord& left, const FS_GearSlotRecord& right){
        return left.stackIndex < right.stackIndex;
    });
    TArray<FS_EquippedArmorState> restored;
    for(const FS_GearSlotRecord& record : ordered){
        const FS_InventoryEntry* entry = bag->findInstance(record.instanceId);
        if(entry == nullptr){
            EQUIPMENT_LOG(Warning, this, "Re-equip skipped: instance %s is not in the recovered bag", *record.instanceId.ToString());
            continue;
        }
        FS_EquippedArmorState piece;
        piece.stackIndex = armorStack.Num();
        piece.instanceId = entry->instanceId;
        piece.itemId = entry->itemId;
        piece.remainingDurability = entry->state.durability;
        if(!bag->removeInstance(record.instanceId).succeeded()){
            continue;   // it stays in the bag; nothing is lost
        }
        armorStack.Add(piece);
        restored.Add(piece);
    }
    if(restored.Num() > 0){
        finishChange();
        for(const FS_EquippedArmorState& piece : restored){
            raiseArmorMessage(T_Equipment_ArmorEquipped, piece);
        }
    }
    EQUIPMENT_LOG(Log, this, "Re-equipped %d of %d recovered armor pieces", restored.Num(), gear.Num());
    return true;
}

void UEquipmentComponent::discardWornArmor(){
    if(!isServerSide() || armorStack.Num() == 0){
        return;
    }
    const TArray<FS_EquippedArmorState> discarded = armorStack;
    armorStack.Reset();
    finishChange();
    for(const FS_EquippedArmorState& piece : discarded){
        raiseArmorMessage(T_Equipment_ArmorUnequipped, piece);
    }
    EQUIPMENT_LOG(Log, this, "Discarded %d worn armor pieces", discarded.Num());
}

void UEquipmentComponent::resetForNewCharacter(){
    if(!isServerSide()){
        return;
    }
    discardWornArmor();
    if(!wornClothes.isDefault()){
        wornClothes = FS_WornClothesState();   // a player at game start wears the default clothes (R05 amendment)
        finishChange();
        raiseClothesMessage();
    }
}

// ---------- Section 9: requests ----------

bool UEquipmentComponent::passRequestGate(const TCHAR* operation) const{
    UInventoryRequestComponent* requests = UInventoryRequestComponent::findFor(getOwnerPlayerState());
    if(requests == nullptr){
        EQUIPMENT_LOG(Warning, this, "%s refused: the player has no request component", operation);
        return false;
    }
    const FGameplayTag blocked = requests->gate(true, false);   // needs hands; not while Downed or Restrained
    if(blocked.IsValid()){
        requests->refuse(blocked, operation);
        return false;
    }
    return true;
}

void UEquipmentComponent::replyIfRefused(const FS_InventoryResult& result, const TCHAR* operation) const{
    if(result.succeeded()){
        return;
    }
    UInventoryRequestComponent* requests = UInventoryRequestComponent::findFor(getOwnerPlayerState());
    if(requests != nullptr){
        requests->refuse(result.failReason, operation);   // logs the reason tag and tells only the requester (Doc02 §6)
    }
}

void UEquipmentComponent::serverEquipArmor_Implementation(FGuid instanceId){
    if(!passRequestGate(TEXT("equipArmor"))){
        return;
    }
    replyIfRefused(equipArmor(instanceId), TEXT("equipArmor"));
}

void UEquipmentComponent::serverUnequipArmor_Implementation(int32 stackIndex){
    if(!passRequestGate(TEXT("unequipArmor"))){
        return;
    }
    replyIfRefused(unequipArmor(stackIndex), TEXT("unequipArmor"));
}

void UEquipmentComponent::serverEquipClothes_Implementation(FName itemId, bool fromOpenContainer){
    if(!passRequestGate(TEXT("equipClothes"))){
        return;
    }
    if(!fromOpenContainer){
        replyIfRefused(equipClothes(itemId), TEXT("equipClothes"));
        return;
    }
    // Taken from a chest: the chest must still be in reach (Doc02 §19.6; range is inclusive, Doc02 §19.9).
    APlayerState* player = getOwnerPlayerState();
    const UContainerViewComponent* view = UContainerViewComponent::findFor(player);
    UInventoryRequestComponent* requests = UInventoryRequestComponent::findFor(player);
    if(view == nullptr || !requests->isContainerReachable(player, view->getViewedContainerId())){
        requests->refuse(T_Inventory_FailReason_OutOfRange, TEXT("equipClothes"));
        return;
    }
    replyIfRefused(equipClothesFromContainer(itemId), TEXT("equipClothes"));
}

// ---------- Section 8: save (Doc R13's IBPI_SaveParticipant) ----------

FName UEquipmentComponent::getSaveKey_Implementation() const{
    return TEXT("Equipment.Character");
}

E_SaveScope UEquipmentComponent::getSaveScope_Implementation() const{
    return E_SaveScope::Character;
}

int32 UEquipmentComponent::getSnapshotVersion_Implementation() const{
    return currentSnapshotVersion;
}

APlayerState* UEquipmentComponent::getSaveOwner_Implementation() const{
    return getOwnerPlayerState();
}

// Query: never mutates.
bool UEquipmentComponent::getSnapshotForSave_Implementation(FInstancedStruct& snapshot) const{
    if(!isServerSide()){
        return false;
    }
    FS_EquipmentSave record;
    record.version = currentSnapshotVersion;
    record.armorStack = armorStack;   // includes each piece's remaining durability (R05 §9)
    record.clothes = wornClothes;
    snapshot = FInstancedStruct::Make(record);
    return true;
}

bool UEquipmentComponent::migrateSnapshotStep_Implementation(int32 fromVersion, const FInstancedStruct& oldSnapshot, FInstancedStruct& newSnapshot) const{
    // One step per version: when version 2 exists, add `if(fromVersion == 1){ ...upgrade into newSnapshot...; return true; }`.
    return false;   // a missing step: the file is refused
}

bool UEquipmentComponent::contributeToSlotHeader_Implementation(const FS_SaveSlotHeader& display, FS_SaveSlotHeader& contribution) const{
    return false;   // Character scope: nothing for the slot list
}

bool UEquipmentComponent::restoreFromSnapshot_Implementation(const FInstancedStruct& snapshot, int32 savedVersion){
    if(!isServerSide()){
        return false;
    }
    const FS_EquipmentSave* record = snapshot.GetPtr<FS_EquipmentSave>();
    if(record == nullptr || savedVersion != currentSnapshotVersion || record->version != currentSnapshotVersion){
        return false;   // refused; R13 shows the reason and a failed load never overwrites a file
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const TArray<FS_EquippedArmorState> previousStack = armorStack;
    const FName previousClothes = wornClothes.itemId;

    armorStack.Reset();
    for(const FS_EquippedArmorState& saved : record->armorStack){
        const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(saved.itemId) : nullptr;
        if(definition == nullptr || !definition->isArmor() || !saved.instanceId.IsValid() || saved.remainingDurability <= 0.0f){
            // R13 3.11: skip the entry, one log line, the load continues. Never crash.
            EQUIPMENT_LOG(Warning, this, "Skipped saved armor %s: the item no longer exists or is not armor", *saved.itemId.ToString());
            continue;
        }
        armorStack.Add(saved);   // the cap is NOT applied: these pieces were already worn (R05 3.2)
    }
    renumberStack();

    wornClothes = FS_WornClothesState();
    if(!record->clothes.isDefault()){
        const UDA_ItemDefinition* clothes = catalog != nullptr ? catalog->getDefinition(record->clothes.itemId) : nullptr;
        if(clothes != nullptr && clothes->isClothing()){
            wornClothes = record->clothes;
        }else{
            EQUIPMENT_LOG(Warning, this, "Saved clothes %s no longer exist: the default clothes are worn", *record->clothes.itemId.ToString());
        }
    }

    finishChange();
    for(const FS_EquippedArmorState& piece : previousStack){
        raiseArmorMessage(T_Equipment_ArmorUnequipped, piece);
    }
    for(const FS_EquippedArmorState& piece : armorStack){
        raiseArmorMessage(T_Equipment_ArmorEquipped, piece);
    }
    if(previousClothes != wornClothes.itemId){
        raiseClothesMessage();
    }
    EQUIPMENT_LOG(Log, this, "Restored %d armor pieces and clothes %s", armorStack.Num(), wornClothes.isDefault() ? TEXT("default") : *wornClothes.itemId.ToString());
    return true;
}

void UEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason){
    USaveSubsystem* save = (GetWorld() != nullptr && GetWorld()->GetGameInstance() != nullptr) ? GetWorld()->GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
    if(save != nullptr){
        save->unregisterParticipant(this);
    }
    Super::EndPlay(EndPlayReason);
}
