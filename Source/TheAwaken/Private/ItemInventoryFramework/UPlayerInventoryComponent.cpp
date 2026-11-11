#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "ItemInventoryFramework/UBFL_InventorySave.h"
#include "SaveFramework/SaveSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "StructUtils/InstancedStruct.h"
#include "Net/UnrealNetwork.h"

UPlayerInventoryComponent::UPlayerInventoryComponent(){
    SetIsReplicatedByDefault(true);
}

void UPlayerInventoryComponent::BeginPlay(){
    Super::BeginPlay();
    if(isServerSide()){
        setHotbarBindings(TArray<FS_HotbarBinding>());   // slots 0 to 9, all unbound
        holdSlots.SetNum(UInventorySettings::get()->baglessSlotCount);
        USaveSubsystem* save = GetWorld() != nullptr && GetWorld()->GetGameInstance() != nullptr ? GetWorld()->GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
        if(save != nullptr){
            save->registerParticipant(this);   // Doc R13 (S11.5)
        }
    }
    if(!isServerSide()){
        clientBagView = NewObject<UInventoryData>(this);
        clientBagView->setup(bagInventoryId, E_InventoryType::Personal, FS_ContainerRules());
        replicatedBag.onReceived.BindUObject(this, &UPlayerInventoryComponent::handleMirrorReceived);
        handleMirrorReceived();   // the first data may have arrived before BeginPlay
    }
}

void UPlayerInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason){
    USaveSubsystem* save = GetWorld() != nullptr && GetWorld()->GetGameInstance() != nullptr ? GetWorld()->GetGameInstance()->GetSubsystem<USaveSubsystem>() : nullptr;
    if(save != nullptr){
        save->unregisterParticipant(this);
    }
    if(isServerSide() && personalInventory != nullptr){
        UBFL_Inventory::handlePlayerLeft(getOwnerPlayerState());
        // The player is gone for good. Doc B20's 15-minute hold must take the bag FIRST with releaseInventory();
        // whatever is still attached here goes with the PlayerState.
        const FGuid inventoryId = personalInventory->getInventoryId();
        releaseInventory();
        UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
        if(registry != nullptr){
            registry->destroyInventory(inventoryId);
        }
    }
    Super::EndPlay(EndPlayReason);
}

void UPlayerInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UPlayerInventoryComponent, bagAttached, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UPlayerInventoryComponent, bagInventoryId, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UPlayerInventoryComponent, replicatedBag, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UPlayerInventoryComponent, characterId, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UPlayerInventoryComponent, hotbar, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UPlayerInventoryComponent, recentInstances, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UPlayerInventoryComponent, holdSlots, COND_OwnerOnly);
}

UPlayerInventoryComponent* UPlayerInventoryComponent::findFor(const APlayerState* player){
    return player != nullptr ? player->FindComponentByClass<UPlayerInventoryComponent>() : nullptr;
}

bool UPlayerInventoryComponent::isServerSide() const{
    return GetOwner() != nullptr && GetOwner()->HasAuthority();
}

APlayerState* UPlayerInventoryComponent::getOwnerPlayerState() const{
    return Cast<APlayerState>(GetOwner());
}

// ---------- queries ----------

bool UPlayerInventoryComponent::hasBag() const{
    return bagAttached;
}

UInventoryData* UPlayerInventoryComponent::getPersonalInventory() const{
    if(!bagAttached){
        return nullptr;
    }
    return isServerSide() ? personalInventory.Get() : clientBagView.Get();
}

TArray<FS_InventoryEntry> UPlayerInventoryComponent::getBagEntries() const{
    const UInventoryData* bag = getPersonalInventory();
    return bag != nullptr ? bag->getEntries() : TArray<FS_InventoryEntry>();
}

int32 UPlayerInventoryComponent::getAvailableQuantity(FName itemId) const{
    const UInventoryData* bag = getPersonalInventory();
    return bag != nullptr ? bag->getAvailableQuantity(itemId) : 0;
}

FGuid UPlayerInventoryComponent::getCharacterId() const{
    return characterId;
}

// ---------- commands ----------

void UPlayerInventoryComponent::createFreshBag(){
    UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    if(!isServerSide() || registry == nullptr || personalInventory != nullptr){
        return;
    }
    UInventoryData* inventory = registry->createInventory(E_InventoryType::Personal, FGuid::NewGuid(), FS_ContainerRules());
    attachInventory(inventory);
    INVENTORY_LOG(Log, this, "Player %s received a new empty bag %s", *GetNameSafe(GetOwner()), *bagInventoryId.ToString());
}

void UPlayerInventoryComponent::attachInventory(UInventoryData* inventory){
    if(!isServerSide() || inventory == nullptr || personalInventory != nullptr){
        return;
    }
    personalInventory = inventory;
    bagInventoryId = inventory->getInventoryId();
    bagAttached = true;
    bagChangedHandle = inventory->onChanged.AddUObject(this, &UPlayerInventoryComponent::handleBagChanged);
    handleBagChanged(inventory);
}

UInventoryData* UPlayerInventoryComponent::releaseInventory(){
    if(!isServerSide() || personalInventory == nullptr){
        return nullptr;
    }
    UInventoryData* released = personalInventory;
    released->onChanged.Remove(bagChangedHandle);
    bagChangedHandle.Reset();
    personalInventory = nullptr;
    bagInventoryId.Invalidate();
    bagAttached = false;
    replicatedBag.syncFrom(TArray<FS_InventoryEntry>());
    GetOwner()->ForceNetUpdate();
    notifyChanged();
    UHeldItemComponent* heldItem = UHeldItemComponent::findFor(getOwnerPlayerState());
    if(heldItem != nullptr){
        heldItem->refreshFromInventory();
    }
    return released;
}

void UPlayerInventoryComponent::setCharacterId(const FGuid& newCharacterId){
    if(isServerSide()){
        characterId = newCharacterId;
    }
}

// ---------- change handling ----------

void UPlayerInventoryComponent::notifyChanged(){
    onInventoryChanged.Broadcast(getOwnerPlayerState());
}

void UPlayerInventoryComponent::handleBagChanged(UInventoryData* inventory){
    replicatedBag.syncFrom(inventory->getEntries());
    GetOwner()->ForceNetUpdate();   // a PlayerState replicates slowly by default; the bag should not lag a second behind
    notifyChanged();
    UHeldItemComponent* heldItem = UHeldItemComponent::findFor(getOwnerPlayerState());
    if(heldItem != nullptr){
        heldItem->refreshFromInventory();
    }
}

void UPlayerInventoryComponent::handleMirrorReceived(){
    if(clientBagView == nullptr){
        return;
    }
    clientBagView->setup(bagInventoryId, E_InventoryType::Personal, FS_ContainerRules());
    clientBagView->replaceEntries(replicatedBag.toEntries());   // raises Inventory.Changed on this machine
    notifyChanged();
}

void UPlayerInventoryComponent::OnRep_bagAttached(){
    notifyChanged();
}

void UPlayerInventoryComponent::clientGrantDropped_Implementation(FName itemId, int32 quantity){
    if(isServerSide()){
        return;   // the host raised the message already; this copy is for a remote owner
    }
    FS_GrantDroppedMessage message;
    message.player = getOwnerPlayerState();
    message.itemId = itemId;
    message.quantity = quantity;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Inventory_GrantDropped, message);
}

// ---------- Section 6: hotbar ----------

bool UPlayerInventoryComponent::isHotbarSlotUsable(int32 slotIndex) const{
    if(bagAttached){
        return hotbar.IsValidIndex(slotIndex);
    }
    return slotIndex >= 1 && slotIndex <= UInventorySettings::get()->baglessSlotCount;   // keys 4 to 9 and 0 are locked
}

int32 UPlayerInventoryComponent::getUsableHotbarSlotCount() const{
    return bagAttached ? hotbar.Num() : UInventorySettings::get()->baglessSlotCount;
}

FName UPlayerInventoryComponent::getBoundItemId(int32 slotIndex) const{
    return hotbar.IsValidIndex(slotIndex) ? hotbar[slotIndex].boundItemId : FName();
}

const TArray<FS_HotbarBinding>& UPlayerInventoryComponent::getHotbarBindings() const{
    return hotbar;
}

void UPlayerInventoryComponent::setHotbarBindings(const TArray<FS_HotbarBinding>& newBindings){
    if(!isServerSide()){
        return;
    }
    const int32 slotCount = UInventorySettings::get()->hotbarSlotCount;
    TArray<FS_HotbarBinding> rebuilt;
    rebuilt.SetNum(slotCount);
    for(int32 index = 0; index < slotCount; index++){
        rebuilt[index].slotIndex = index;
    }
    for(const FS_HotbarBinding& binding : newBindings){
        if(rebuilt.IsValidIndex(binding.slotIndex)){
            rebuilt[binding.slotIndex].boundItemId = binding.boundItemId;
        }
    }
    const TArray<FS_HotbarBinding> previous = hotbar;
    hotbar = rebuilt;
    OnRep_hotbar(previous);   // the host raises Hotbar.Changed for each slot that differs
    GetOwner()->ForceNetUpdate();
}

FS_InventoryResult UPlayerInventoryComponent::bindHotbarSlot(int32 slotIndex, FName itemId){
    if(!isServerSide()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotServer);
    }
    if(!bagAttached){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NoBag);   // a Bagless slot holds one physical item, not a binding (R01 3.8)
    }
    if(!hotbar.IsValidIndex(slotIndex)){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_InvalidSlot);
    }
    if(!itemId.IsNone()){
        const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
        const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(itemId) : nullptr;
        if(definition == nullptr){
            return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
        }
        if(definition->isHeavy() || itemId == UInventorySettings::get()->unarmedItemId){
            return UBFL_Inventory::refusal(T_Inventory_FailReason_InvalidRequest);   // neither can ever be in the bag
        }
    }
    // The player does not need to hold the item: the slot points at a type and works as soon as they have one (Index A102).
    if(hotbar[slotIndex].boundItemId != itemId){
        hotbar[slotIndex].boundItemId = itemId;
        raiseHotbarChanged(slotIndex);
        GetOwner()->ForceNetUpdate();
        notifyChanged();
        INVENTORY_LOG(Log, this, "%s bound hotbar slot %d to %s", *GetNameSafe(GetOwner()), slotIndex, itemId.IsNone() ? TEXT("nothing") : *itemId.ToString());
    }
    return FS_InventoryResult();
}

FGuid UPlayerInventoryComponent::findUsableInstance(FName itemId) const{
    const UInventoryData* bag = getPersonalInventory();
    if(bag == nullptr){
        return FGuid();
    }
    // The last used copy first (decision D4); if it is gone, the next most recently used one.
    for(int32 index = recentInstances.Num() - 1; index >= 0; index--){
        const FS_InventoryEntry* entry = bag->findInstance(recentInstances[index]);
        if(entry != nullptr && entry->itemId == itemId && entry->reservedQuantity == 0){
            return entry->instanceId;
        }
    }
    // No history (for example right after a load): the first copy in the bag (I9).
    for(const FS_InventoryEntry& entry : bag->getEntries()){
        if(entry.isInstanced() && entry.itemId == itemId && entry.reservedQuantity == 0){
            return entry.instanceId;
        }
    }
    return FGuid();
}

void UPlayerInventoryComponent::noteInstanceUsed(const FGuid& instanceId){
    if(!isServerSide() || !instanceId.IsValid()){
        return;
    }
    recentInstances.Remove(instanceId);
    recentInstances.Add(instanceId);
    if(recentInstances.Num() > 32){
        recentInstances.RemoveAt(0);
    }
}

FS_ResolvedHotbarSlot UPlayerInventoryComponent::resolveHotbarSlot(int32 slotIndex) const{
    FS_ResolvedHotbarSlot resolved;
    resolved.slotIndex = slotIndex;
    if(!isHotbarSlotUsable(slotIndex)){
        return resolved;
    }
    const UInventoryData* bag = getPersonalInventory();
    if(bag == nullptr){
        // Bagless: key 1, 2 or 3 holds one physical item directly (R01 3.8, the Bagless holding mode).
        const int32 holdIndex = slotIndex - 1;
        if(holdSlots.IsValidIndex(holdIndex) && holdSlots[holdIndex].isOccupied){
            resolved.itemId = holdSlots[holdIndex].entry.itemId;
            resolved.instanceId = holdSlots[holdIndex].entry.instanceId;
            resolved.availableQuantity = 1;
            resolved.isUsable = true;
        }
        return resolved;
    }
    resolved.itemId = hotbar[slotIndex].boundItemId;
    if(resolved.itemId.IsNone()){
        return resolved;
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(resolved.itemId) : nullptr;
    if(definition == nullptr){
        return resolved;
    }
    // Every reader sees the available quantity, never the total (R01 3.6).
    resolved.availableQuantity = bag->getAvailableQuantity(resolved.itemId);
    if(definition->isInstanced()){
        resolved.instanceId = findUsableInstance(resolved.itemId);
    }
    resolved.isUsable = resolved.availableQuantity > 0;
    return resolved;
}

void UPlayerInventoryComponent::raiseHotbarChanged(int32 slotIndex) const{
    FS_HotbarChangedMessage message;
    message.player = getOwnerPlayerState();
    message.slotIndex = slotIndex;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Hotbar_Changed, message);
}

// Hotbar.Changed is local on every machine that holds the bindings, raised from the replicated state (R01 §6).
void UPlayerInventoryComponent::OnRep_hotbar(const TArray<FS_HotbarBinding>& previousBindings){
    for(int32 index = 0; index < hotbar.Num(); index++){
        const FName before = previousBindings.IsValidIndex(index) ? previousBindings[index].boundItemId : FName();
        if(before != hotbar[index].boundItemId){
            raiseHotbarChanged(index);
        }
    }
    notifyChanged();
}

// ---------- Section 9: Bagless hold slots ----------

TArray<FS_BaglessHoldSlot> UPlayerInventoryComponent::getHoldSlots() const{
    return holdSlots;
}

int32 UPlayerInventoryComponent::findFreeHoldSlot() const{
    return holdSlots.IndexOfByPredicate([](const FS_BaglessHoldSlot& slot){
        return !slot.isOccupied;
    });
}

bool UPlayerInventoryComponent::putInHoldSlot(int32 holdIndex, const FS_InventoryEntry& unit){
    if(!isServerSide() || !holdSlots.IsValidIndex(holdIndex) || holdSlots[holdIndex].isOccupied){
        return false;
    }
    const UInventoryServices* services = UInventoryServices::get(this);
    FS_BaglessHoldSlot& slot = holdSlots[holdIndex];
    slot.isOccupied = true;
    slot.entry = unit;
    slot.entry.totalQuantity = 1;      // exactly one item per slot, no stacking
    slot.entry.reservedQuantity = 0;
    // Frozen in the hand: keep the time LEFT, not the time on the clock (R01 amendment).
    slot.hasSpoilClock = unit.spoilAtTime > 0;
    slot.timeToSpoil = slot.hasSpoilClock && services != nullptr ? FMath::Max<int64>(0, unit.spoilAtTime - services->getGameTime()) : 0;
    slot.entry.spoilAtTime = 0;
    raiseHotbarChanged(holdIndex + 1);
    GetOwner()->ForceNetUpdate();
    notifyChanged();
    return true;
}

bool UPlayerInventoryComponent::takeFromHoldSlot(int32 holdIndex, FS_InventoryEntry& outEntry){
    if(!isServerSide() || !holdSlots.IsValidIndex(holdIndex) || !holdSlots[holdIndex].isOccupied){
        return false;
    }
    const UInventoryServices* services = UInventoryServices::get(this);
    outEntry = holdSlots[holdIndex].entry;
    if(holdSlots[holdIndex].hasSpoilClock && services != nullptr){
        outEntry.spoilAtTime = services->getGameTime() + holdSlots[holdIndex].timeToSpoil;   // the clock starts again
    }
    holdSlots[holdIndex] = FS_BaglessHoldSlot();
    raiseHotbarChanged(holdIndex + 1);
    GetOwner()->ForceNetUpdate();
    notifyChanged();
    UHeldItemComponent* heldItem = UHeldItemComponent::findFor(getOwnerPlayerState());
    if(heldItem != nullptr){
        heldItem->refreshFromInventory();   // if it was shown in the hand, the hand empties
    }
    return true;
}

int32 UPlayerInventoryComponent::clearHoldSlots(){
    int32 destroyed = 0;
    for(int32 index = 0; index < holdSlots.Num(); index++){
        FS_InventoryEntry discarded;
        if(takeFromHoldSlot(index, discarded)){
            destroyed++;
        }
    }
    return destroyed;
}

bool UPlayerInventoryComponent::isKitGranted() const{
    return kitGranted;
}

void UPlayerInventoryComponent::setKitGranted(bool newValue){
    if(isServerSide()){
        kitGranted = newValue;
    }
}

void UPlayerInventoryComponent::OnRep_holdSlots(){
    notifyChanged();
}

// ---------- Section 11: save (Doc R13's IBPI_SaveParticipant) ----------

FName UPlayerInventoryComponent::getSaveKey_Implementation() const{
    return TEXT("Inventory.Character");
}

E_SaveScope UPlayerInventoryComponent::getSaveScope_Implementation() const{
    return E_SaveScope::Character;
}

int32 UPlayerInventoryComponent::getSnapshotVersion_Implementation() const{
    return currentSnapshotVersion;
}

APlayerState* UPlayerInventoryComponent::getSaveOwner_Implementation() const{
    return getOwnerPlayerState();
}

// Query: never mutates.
bool UPlayerInventoryComponent::getSnapshotForSave_Implementation(FInstancedStruct& snapshot) const{
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    const UInventoryServices* services = UInventoryServices::get(this);
    if(!isServerSide() || registry == nullptr || services == nullptr || personalInventory == nullptr){
        // A Bagless player is never captured: their previous Character file is kept (Doc R13 3.3).
        INVENTORY_LOG(Warning, this, "No inventory snapshot for %s: the player is Bagless or this is not the host", *GetNameSafe(GetOwner()));
        return false;
    }
    FS_PlayerInventorySave record;
    record.version = currentSnapshotVersion;
    record.isKitGranted = kitGranted;
    record.hotbar = hotbar;
    // A Character file stores the time LEFT of each stack (R01 3.2a amendment).
    UBFL_InventorySave::capture(registry, personalInventory, true, services->getGameTime(), record.bag, record.containedInventories);
    snapshot = FInstancedStruct::Make(record);
    return true;
}

bool UPlayerInventoryComponent::migrateSnapshotStep_Implementation(int32 fromVersion, const FInstancedStruct& oldSnapshot, FInstancedStruct& newSnapshot) const{
    // One step per version: when version 2 exists, add `if(fromVersion == 1){ ...upgrade 1 -> 2 into newSnapshot...; return true; }`.
    return false;   // a missing step: the file is refused
}

bool UPlayerInventoryComponent::contributeToSlotHeader_Implementation(const FS_SaveSlotHeader& display, FS_SaveSlotHeader& contribution) const{
    return false;   // Character scope: nothing for the slot list
}

bool UPlayerInventoryComponent::restoreFromSnapshot_Implementation(const FInstancedStruct& snapshot, int32 savedVersion){
    UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    const UInventoryServices* services = UInventoryServices::get(this);
    if(!isServerSide() || registry == nullptr || services == nullptr){
        return false;
    }
    const FS_PlayerInventorySave* record = snapshot.GetPtr<FS_PlayerInventorySave>();
    if(record == nullptr || savedVersion != currentSnapshotVersion || record->version != currentSnapshotVersion){
        return false;   // refused; nothing changed, and a failed load never overwrites a file (Doc R13 3.11)
    }

    // What the player holds now is replaced by what the file says.
    UBFL_Inventory::releaseAllStaged(getOwnerPlayerState());
    UInventoryData* current = releaseInventory();
    if(current != nullptr){
        registry->destroyInventory(current->getInventoryId());
    }
    clearHoldSlots();
    kitGranted = record->isKitGranted;

    // The one case where the file's bag does not come back: the world already holds this very bag, lying at a
    // death site (Doc R13 3.6, "the world bag wins"). In every other case Doc B06 P4 removed the world bag first.
    const UInventoryData* worldBag = registry->findInventory(record->bag.inventoryId);
    if(worldBag != nullptr && worldBag->isDeathBag()){
        setHotbarBindings(TArray<FS_HotbarBinding>());
        INVENTORY_LOG(Log, this, "%s is restored Bagless: bag %s lies in the world", *GetNameSafe(GetOwner()), *record->bag.inventoryId.ToString());
        return true;
    }

    UInventoryData* bag = UBFL_InventorySave::restore(registry, record->bag, record->containedInventories, services->getGameTime());
    if(bag == nullptr){
        return false;
    }
    attachInventory(bag);
    setHotbarBindings(record->hotbar);
    INVENTORY_LOG(Log, this, "Restored the bag of %s: %d entries", *GetNameSafe(GetOwner()), bag->getEntries().Num());
    return true;
}