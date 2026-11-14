#include "CarryEncumbranceFramework/UCarryComponent.h"
#include "CarryEncumbranceFramework/CarryLog.h"
#include "CarryEncumbranceFramework/CarryServices.h"
#include "CarryEncumbranceFramework/UBFL_Carry.h"
#include "CarryEncumbranceFramework/UCarrySettings.h"
#include "CarryEncumbranceFramework/02Tags/T_CarryEncumbranceFramework.h"
#include "CarryEncumbranceFramework/03Interfaces/BPI_CarryableActor.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UInventoryRequestComponent.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

#if !UE_BUILD_SHIPPING
// Tests only: -1 = use the project setting. 0 = hands that can take nothing (every heavy pickup is refused).
static TAutoConsoleVariable<int32> cvarCarrySlotCount(TEXT("Awaken.Carry.SlotCount"), -1, TEXT("Override carrySlotCount (tests only). -1 = use the project setting."), ECVF_Cheat);
#endif

// ---------- construction and replication ----------

UCarryComponent::UCarryComponent(){
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UCarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCarryComponent, carrySlots);   // COND_None: everyone sees who carries what (B07 §10)
}

UCarryComponent* UCarryComponent::findOn(const AActor* carrier){
    if(carrier == nullptr){
        return nullptr;
    }
    const APlayerState* player = Cast<APlayerState>(carrier);
    if(player != nullptr){
        return findFor(player);
    }
    return carrier->FindComponentByClass<UCarryComponent>();
}

UCarryComponent* UCarryComponent::findFor(const APlayerState* player){
    const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
    return pawn != nullptr ? pawn->FindComponentByClass<UCarryComponent>() : nullptr;
}

bool UCarryComponent::isServerSide() const{
    return GetOwner() != nullptr && GetOwner()->HasAuthority();
}

APlayerState* UCarryComponent::getCarrierPlayerState() const{
    const APawn* pawn = Cast<APawn>(GetOwner());
    return pawn != nullptr ? pawn->GetPlayerState() : nullptr;
}

// ---------- queries ----------

bool UCarryComponent::isCarryActive() const{
    for(const FS_CarrySlotState& slot : carrySlots){
        if(slot.isOccupied()){
            return true;
        }
    }
    return false;
}

int32 UCarryComponent::getCarriedItemsForCost(FName itemId) const{
    int32 count = 0;
    for(int32 index = 0; index < carrySlots.Num(); index++){
        const FS_CarrySlotState& slot = carrySlots[index];
        // One carried thing counts once, however many slots it fills. A carried actor is never a building material.
        if(slot.isOccupied() && !slot.holdsActor && slot.itemId == itemId && isPrimarySlot(carrySlots, index)){
            count++;
        }
    }
    return count;
}

const TArray<FS_CarrySlotState>& UCarryComponent::getCarrySlots() const{
    return carrySlots;
}

TArray<FS_CarrySlotState> UCarryComponent::getCarrySlotsCopy() const{
    return carrySlots;
}

int32 UCarryComponent::getSlotCount() const{
#if !UE_BUILD_SHIPPING
    const int32 overrideCount = cvarCarrySlotCount.GetValueOnGameThread();
    if(overrideCount >= 0){
        return overrideCount;
    }
#endif
    return UCarrySettings::get()->carrySlotCount;
}

int32 UCarryComponent::getFreeSlotCount() const{
    int32 occupied = 0;
    for(const FS_CarrySlotState& slot : carrySlots){
        if(slot.isOccupied()){
            occupied++;
        }
    }
    return FMath::Max(0, getSlotCount() - occupied);
}

int32 UCarryComponent::getActiveSlotIndex() const{
    int32 activeIndex = INDEX_NONE;
    int32 highestSequence = -1;
    for(int32 index = 0; index < carrySlots.Num(); index++){
        // Strictly greater, so of two slots that share a carried thing the lower one (the primary) wins.
        if(carrySlots[index].isOccupied() && carrySlots[index].sequence > highestSequence){
            highestSequence = carrySlots[index].sequence;
            activeIndex = index;
        }
    }
    return activeIndex;
}

bool UCarryComponent::isActorCarry(int32 slotIndex) const{
    return carrySlots.IsValidIndex(slotIndex) && carrySlots[slotIndex].isOccupied() && carrySlots[slotIndex].holdsActor;
}

float UCarryComponent::getSpeedMultiplier() const{
    return isCarryActive() ? UCarrySettings::get()->carrySpeedMultiplier : 1.0f;
}

FGameplayTag UCarryComponent::getLastInterruptCause() const{
    return lastInterruptCause;
}

// ---------- slot helpers ----------

void UCarryComponent::ensureSlots(){
    if(carrySlots.Num() < getSlotCount()){
        carrySlots.SetNum(getSlotCount());
    }
}

int32 UCarryComponent::fillSlots(const FGuid& carryId, FName itemId, int32 cost, AActor* actor){
    ensureSlots();
    nextSequence++;
    int32 primaryIndex = INDEX_NONE;
    int32 filled = 0;
    for(int32 index = 0; index < carrySlots.Num() && filled < cost; index++){
        FS_CarrySlotState& slot = carrySlots[index];
        if(slot.isOccupied()){
            continue;
        }
        slot.itemId = itemId;
        slot.carryId = carryId;
        slot.sequence = nextSequence;
        slot.holdsActor = actor != nullptr;
        slot.carriedActor = actor;
        if(primaryIndex == INDEX_NONE){
            primaryIndex = index;
        }
        filled++;
    }
    return primaryIndex;
}

bool UCarryComponent::isPrimarySlot(const TArray<FS_CarrySlotState>& slots, int32 slotIndex){
    if(!slots.IsValidIndex(slotIndex) || !slots[slotIndex].isOccupied()){
        return false;
    }
    for(int32 index = 0; index < slotIndex; index++){
        if(slots[index].carryId == slots[slotIndex].carryId){
            return false;
        }
    }
    return true;
}

// ---------- the state tag (B07 3.2, R04-R08-IG23) ----------

void UCarryComponent::applyCarryStateTag(bool isActive) const{
    // A player's Ability System Component is on the PlayerState (decision D5); an NPC's is on the pawn.
    // AAwakenCharacter answers for its PlayerState through IAbilitySystemInterface. No component: nothing happens here,
    // and readers use isCarryActive() directly.
    UAbilitySystemComponent* abilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
    if(abilitySystem == nullptr){
        abilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(getCarrierPlayerState());
    }
    if(abilitySystem == nullptr){
        return;
    }
    // 5.8 has no AddReplicatedLooseGameplayTag: one call sets the host's count AND replicates the tag (TagOnly).
    // A count (not add/remove) so a repeated call can never stack or go negative.
    abilitySystem->SetLooseGameplayTagCount(T_Carry_State_Active, isActive ? 1 : 0, EGameplayTagReplicationState::TagOnly);
}

// ---------- messages (B07 §9, Doc02 §19.2: local on every machine) ----------

void UCarryComponent::raiseBegan(int32 slotIndex, FName itemId) const{
    if(!UGameplayMessageSubsystem::HasInstance(this)){
        return;
    }
    FS_CarryBeganMessage message;
    message.carrier = GetOwner();
    message.itemId = itemId;
    message.slotIndex = slotIndex;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Carry_Began, message);
}

void UCarryComponent::raiseEnded(int32 slotIndex, FName itemId, E_EndCarryReason reason, const FGameplayTag& interruptCause){
    if(reason == E_EndCarryReason::Interrupted){
        lastInterruptCause = interruptCause;
    }
    if(!UGameplayMessageSubsystem::HasInstance(this)){
        return;
    }
    FS_CarryEndedMessage message;
    message.carrier = GetOwner();
    message.slotIndex = slotIndex;
    message.reason = reason;
    message.itemId = itemId;
    message.interruptCause = interruptCause;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Carry_Ended, message);
}

// A client learns of a change only here. The previous value lets it tell what began and what ended.
void UCarryComponent::OnRep_carrySlots(const TArray<FS_CarrySlotState>& previousSlots){
    refreshVisuals();
    for(int32 index = 0; index < carrySlots.Num(); index++){
        const FS_CarrySlotState before = previousSlots.IsValidIndex(index) ? previousSlots[index] : FS_CarrySlotState();
        const FS_CarrySlotState& now = carrySlots[index];
        if(before.carryId == now.carryId){
            continue;
        }
        if(isPrimarySlot(previousSlots, index)){
            // The freed slot kept why it was freed, so the client can raise the same message as the host.
            raiseEnded(index, before.itemId, now.lastEndReason, now.lastInterruptCause);
        }
        if(isPrimarySlot(carrySlots, index)){
            raiseBegan(index, now.itemId);
        }
    }
    onCarryChanged.Broadcast(GetOwner());
}

// ---------- visuals (C2): each machine draws the carried items from the replicated slots ----------

void UCarryComponent::refreshVisuals(){
    const UCarrySettings* settings = UCarrySettings::get();
    AActor* owner = GetOwner();
    const ACharacter* character = Cast<ACharacter>(owner);
    USkeletalMeshComponent* body = character != nullptr ? character->GetMesh() : nullptr;
    // A dedicated server draws nothing; a body that is going away gets no new components.
    if(!settings->showCarriedMeshes || body == nullptr || GetNetMode() == NM_DedicatedServer || owner->IsActorBeingDestroyed() || !HasBegunPlay()){
        return;
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);

    // One mesh component per slot, made the first time the slot is seen. It is hidden while the slot shows nothing.
    while(slotVisuals.Num() < carrySlots.Num()){
        const int32 newIndex = slotVisuals.Num();
        const FName socketName = settings->carrySocketNames.IsValidIndex(newIndex) ? settings->carrySocketNames[newIndex] : NAME_None;
        UStaticMeshComponent* visual = NewObject<UStaticMeshComponent>(owner);
        visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        visual->SetIsReplicated(false);
        visual->SetupAttachment(body, socketName);
        visual->RegisterComponent();
        visual->SetVisibility(false);
        slotVisuals.Add(visual);
    }

    for(int32 index = 0; index < carrySlots.Num(); index++){
        UStaticMeshComponent* visual = slotVisuals[index];
        const FS_CarrySlotState& slot = carrySlots[index];
        // A carried actor shows itself (it is attached to the body). A two-slot item is drawn once, at its primary slot.
        const UDA_ItemDefinition* definition = (catalog != nullptr && !slot.holdsActor && isPrimarySlot(carrySlots, index)) ? catalog->getDefinition(slot.itemId) : nullptr;
        if(definition == nullptr || definition->worldMesh.IsNull()){
            visual->SetVisibility(false);
            continue;
        }
        // The world mesh is a soft reference (Doc02 19.8): load it only now that something shows it.
        const TSoftObjectPtr<UStaticMesh> wanted = definition->worldMesh;
        const FGuid shownCarryId = slot.carryId;
        UAssetManager::GetStreamableManager().RequestAsyncLoad(wanted.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this, [this, index, shownCarryId, wanted](){
            // The slot may have changed while the mesh was loading.
            if(carrySlots.IsValidIndex(index) && carrySlots[index].carryId == shownCarryId && slotVisuals.IsValidIndex(index) && slotVisuals[index] != nullptr){
                slotVisuals[index]->SetStaticMesh(wanted.Get());
                slotVisuals[index]->SetVisibility(true);
            }
        }));
    }
}

void UCarryComponent::finishChange(){
    if(isServerSide()){
        GetOwner()->ForceNetUpdate();   // the pose of other players should not wait for the next scheduled update
    }
    refreshVisuals();
    onCarryChanged.Broadcast(GetOwner());
}

// ---------- small helpers ----------

FS_CarryBeginResult UCarryComponent::refusedBegin(const FGameplayTag& reason){
    FS_CarryBeginResult result;
    result.failReason = reason;
    return result;
}

FS_CarryEndResult UCarryComponent::refusedEnd(const FGameplayTag& reason){
    FS_CarryEndResult result;
    result.failReason = reason;
    return result;
}

// ---------- Section 4: beginning a carry ----------

FGameplayTag UCarryComponent::checkCanBegin() const{
    const UInventoryServices* services = UInventoryServices::get(this);
    const APlayerState* player = getCarrierPlayerState();
    // Downed, Restrained, Arriving, dead or in a scene: the body cannot take anything (Doc02 19.6).
    if(services != nullptr && player != nullptr && services->isIncapacitated(player)){
        return T_Interaction_FailReason_NotAllowedInState;
    }
    // Swimming drops a carried item (B07 3.6), so a swimmer cannot begin a carry either.
    const ACharacter* character = Cast<ACharacter>(GetOwner());
    if(character != nullptr && character->GetCharacterMovement() != nullptr && character->GetCharacterMovement()->IsSwimming()){
        return T_Interaction_FailReason_NotAllowedInState;
    }
    return FGameplayTag();
}

FS_CarryBeginResult UCarryComponent::beginCarry(const FS_InventoryEntry& entry){
    if(!isServerSide()){
        return refusedBegin(T_Inventory_FailReason_NotServer);
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(entry.itemId) : nullptr;
    if(definition == nullptr || entry.totalQuantity < 1){
        return refusedBegin(T_Inventory_FailReason_InvalidRequest);
    }
    if(!definition->isHeavy()){
        return refusedBegin(T_Carry_FailReason_NotCarryable);   // an ordinary item goes to the bag, never to the hands (Index A93, UD9)
    }
    const FGameplayTag blocked = checkCanBegin();
    if(blocked.IsValid()){
        return refusedBegin(blocked);
    }
    // All or nothing. An item whose cost is larger than the number of slots can never pass this line (B07 3.1).
    const int32 neededSlots = definition->carryCapacityCost * entry.totalQuantity;
    if(neededSlots > getFreeSlotCount()){
        return refusedBegin(T_Interaction_FailReason_HandsFull);
    }

    // Everything checked. One carried thing per unit: a slot never holds a quantity (Index A103, S4-F18).
    FS_CarryBeginResult result;
    for(int32 unit = 0; unit < entry.totalQuantity; unit++){
        const bool wasActive = isCarryActive();
        FS_InventoryEntry unitEntry = entry;
        unitEntry.totalQuantity = 1;
        unitEntry.reservedQuantity = 0;
        const FGuid carryId = FGuid::NewGuid();
        carriedEntries.Add(carryId, unitEntry);
        result.slotIndex = fillSlots(carryId, entry.itemId, definition->carryCapacityCost, nullptr);
        afterBegin(result.slotIndex, entry.itemId, wasActive);
    }
    return result;
}

void UCarryComponent::afterBegin(int32 slotIndex, FName itemId, bool wasActive){
    if(!wasActive){
        // The first slot was filled: the lockout starts (B07 3.2).
        applyCarryStateTag(true);
        const UInventoryServices* services = UInventoryServices::get(this);
        APlayerState* player = getCarrierPlayerState();
        if(services != nullptr && player != nullptr){
            // A carry that begins ends a running Dodge, Sprint, Block, Reload or Weapon Charge (R04-R08-IG25).
            for(const FGameplayTag& actionTag : CarryTags::getActionsEndedByCarry()){
                services->cancelAction(player, actionTag);
            }
        }
    }
    finishChange();
    raiseBegan(slotIndex, itemId);
    const UCarryServices* carryServices = UCarryServices::get(this);
    if(carryServices != nullptr){
        carryServices->raiseCosmeticEvent(T_Cosmetic_Event_Carry_Picked, GetOwner()->GetActorLocation(), GetOwner());   // Doc B21 A.4
    }
    CARRY_LOG(Log, this, "%s began carrying %s in slot %d (%d of %d slots free)", *GetNameSafe(GetOwner()), *itemId.ToString(), slotIndex, getFreeSlotCount(), getSlotCount());
}

// ---------- Section 5: ending a carry ----------

FVector UCarryComponent::getDropLocation(bool atFeet) const{
    const AActor* owner = GetOwner();
    if(atFeet){
        return owner->GetActorLocation();
    }
    return owner->GetActorLocation() + owner->GetActorForwardVector() * UInventorySettings::get()->dropForwardDistance;
}

FS_CarryEndResult UCarryComponent::endCarry(int32 slotIndex, E_EndCarryReason reason){
    return endCarryInternal(slotIndex, reason, FGameplayTag());
}

FS_CarryEndResult UCarryComponent::endCarryInternal(int32 slotIndex, E_EndCarryReason reason, const FGameplayTag& interruptCause){
    if(!isServerSide()){
        return refusedEnd(T_Inventory_FailReason_NotServer);
    }
    if(!carrySlots.IsValidIndex(slotIndex) || !carrySlots[slotIndex].isOccupied()){
        return refusedEnd(T_Carry_FailReason_SlotEmpty);
    }
    const bool isPutDown = reason == E_EndCarryReason::Dropped || reason == E_EndCarryReason::Interrupted;
    const FS_CarrySlotState released = carrySlots[slotIndex];
    if(released.holdsActor && !isPutDown){
        // A carried corpse is not an item: it cannot be thrown, built with or sent on a line (C10, Doc B09 3.11).
        return refusedEnd(reason == E_EndCarryReason::Thrown ? T_Carry_FailReason_CannotThrow.GetTag() : T_Interaction_FailReason_NotAllowedInState.GetTag());
    }

    // Everything checked. Free every slot of this carried thing; the freed slots remember why (the clients read it).
    FS_CarryEndResult result;
    int32 primaryIndex = INDEX_NONE;
    for(int32 index = 0; index < carrySlots.Num(); index++){
        if(carrySlots[index].carryId != released.carryId){
            continue;
        }
        if(primaryIndex == INDEX_NONE){
            primaryIndex = index;
        }
        carrySlots[index] = FS_CarrySlotState();
        carrySlots[index].lastEndReason = reason;
        carrySlots[index].lastInterruptCause = interruptCause;
    }
    carriedEntries.RemoveAndCopyValue(released.carryId, result.entry);

    const FVector dropLocation = getDropLocation(reason == E_EndCarryReason::Interrupted);
    if(released.holdsActor){
        AActor* actor = released.carriedActor;
        result.actor = actor;
        if(IsValid(actor)){
            actor->OnDestroyed.RemoveDynamic(this, &UCarryComponent::onCarriedActorDestroyed);   // put down: it no longer reports to these hands
            actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
            actor->SetActorLocation(dropLocation);
            IBPI_CarryableActor::Execute_onCarryEnded(actor, GetOwner(), reason);   // it is an ordinary corpse where it lies (Index A93)
        }
    }else if(isPutDown){
        // A dropped item is a loose item: never saved, removed only by its own lifetime (Doc02 19.7). It is never destroyed here.
        const UInventoryServices* services = UInventoryServices::get(this);
        if(services != nullptr){
            services->spawnLooseItem(getCarrierPlayerState(), result.entry, dropLocation);
        }
    }

    if(!isCarryActive()){
        applyCarryStateTag(false);   // the last slot was freed: the lockout ends
    }
    finishChange();
    raiseEnded(primaryIndex, released.itemId, reason, interruptCause);
    if(reason != E_EndCarryReason::Placed && reason != E_EndCarryReason::Sent){
        const UCarryServices* carryServices = UCarryServices::get(this);
        if(carryServices != nullptr){
            carryServices->raiseCosmeticEvent(T_Cosmetic_Event_Carry_Dropped, dropLocation, GetOwner());   // Doc B21 A.4
        }
    }
    CARRY_LOG(Log, this, "%s stopped carrying %s from slot %d: %s%s%s", *GetNameSafe(GetOwner()), *released.itemId.ToString(), primaryIndex, *UEnum::GetValueAsString(reason), interruptCause.IsValid() ? TEXT(", cause ") : TEXT(""), interruptCause.IsValid() ? *interruptCause.ToString() : TEXT(""));
    return result;
}

int32 UCarryComponent::consumeCarriedForCost(FName itemId, int32 quantity){
    int32 consumed = 0;
    if(!isServerSide()){
        return consumed;
    }
    while(consumed < quantity){
        int32 foundIndex = INDEX_NONE;
        for(int32 index = 0; index < carrySlots.Num(); index++){
            if(carrySlots[index].isOccupied() && !carrySlots[index].holdsActor && carrySlots[index].itemId == itemId){
                foundIndex = index;
                break;
            }
        }
        if(foundIndex == INDEX_NONE || !endCarryInternal(foundIndex, E_EndCarryReason::Placed, FGameplayTag()).succeeded()){
            break;
        }
        consumed++;
    }
    return consumed;
}

// ---------- Section 6: throwing ----------

FS_CarryEndResult UCarryComponent::throwCarried(int32 slotIndex, const FVector& direction){
    if(!isServerSide()){
        return refusedEnd(T_Inventory_FailReason_NotServer);
    }
    const FVector throwDirection = direction.GetSafeNormal();
    if(throwDirection.IsNearlyZero()){
        return refusedEnd(T_Carry_FailReason_InvalidThrow);
    }
    // Everything else (an empty slot, a carried actor) is checked by endCarry before it changes anything.
    FS_CarryEndResult result = endCarryInternal(slotIndex, E_EndCarryReason::Thrown, FGameplayTag());
    if(!result.succeeded()){
        return result;
    }
    const UCarrySettings* settings = UCarrySettings::get();
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(result.entry.itemId) : nullptr;
    const APawn* pawn = Cast<APawn>(GetOwner());
    // The origin is the HOST's idea of the eye. The client never sends one (C9).
    const FVector eye = pawn != nullptr ? pawn->GetPawnViewLocation() : GetOwner()->GetActorLocation();
    const FVector origin = eye + throwDirection * settings->throwStartDistance;
    const float damage = definition != nullptr ? definition->throwImpactDamage : 0.0f;
    if(UBFL_Carry::throwEntry(GetOwner(), result.entry, origin, throwDirection, settings->throwSpeed, damage) == nullptr){
        // The flight could not start. The item is already out of the hands, so it is put down instead: never lost.
        const UInventoryServices* services = UInventoryServices::get(this);
        if(services != nullptr){
            services->spawnLooseItem(getCarrierPlayerState(), result.entry, getDropLocation(false));
        }
    }
    return result;
}

// ---------- Section 7: interrupts and the two safety nets (C13) ----------

int32 UCarryComponent::interruptCarry(const FGameplayTag& causeTag){
    int32 droppedCount = 0;
    if(!isServerSide()){
        return droppedCount;
    }
    for(int32 index = 0; index < carrySlots.Num(); index++){
        // endCarry frees every slot of the carried thing, so a two-slot item is counted once.
        if(carrySlots[index].isOccupied() && endCarryInternal(index, E_EndCarryReason::Interrupted, causeTag).succeeded()){
            droppedCount++;
        }
    }
    return droppedCount;
}

void UCarryComponent::BeginPlay(){
    Super::BeginPlay();
    if(isServerSide()){
        // Safety net 1 (swimming). Event-driven, no tick. Doc R07 may also call interruptCarry from BeginSwim; the second
        // call finds nothing to drop.
        ACharacter* character = Cast<ACharacter>(GetOwner());
        if(character != nullptr){
            character->MovementModeChangedDelegate.AddUniqueDynamic(this, &UCarryComponent::onMovementModeChanged);
        }
    }
}

void UCarryComponent::onMovementModeChanged(ACharacter* character, EMovementMode previousMovementMode, uint8 previousCustomMode){
    if(character != nullptr && character->GetCharacterMovement() != nullptr && character->GetCharacterMovement()->IsSwimming()){
        interruptCarry(T_Carry_InterruptCause_Swim);   // dropped when swimming begins (B07 3.6, Index A87)
    }
}

void UCarryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason){
    if(isServerSide() && isCarryActive()){
        if(EndPlayReason == EEndPlayReason::Destroyed){
            // Safety net 2. The pawn is being removed (a death, a player who left) and nobody interrupted the carry first.
            // The item drops where the pawn was; it is never deleted with the pawn.
            interruptCarry(T_Carry_InterruptCause_PawnRemoved);
        }else{
            // The world itself is closing (a level change, quitting). Nothing is dropped and nothing is saved (B07 3.5).
            // Only the tag is cleared, because a PlayerState can outlive this pawn.
            applyCarryStateTag(false);
        }
    }
    Super::EndPlay(EndPlayReason);
}

// ---------- Section 8: carrying an actor ----------

FS_CarryBeginResult UCarryComponent::beginCarryActor(AActor* actor){
    if(!isServerSide()){
        return refusedBegin(T_Inventory_FailReason_NotServer);
    }
    if(!IsValid(actor) || !actor->GetClass()->ImplementsInterface(UBPI_CarryableActor::StaticClass())){
        return refusedBegin(T_Carry_FailReason_NotCarryable);
    }
    const int32 cost = IBPI_CarryableActor::Execute_getCarrySlotCost(actor);
    // A cost below 1 means "not now" (mid-harvest). An actor that is already attached is in somebody's hands.
    if(cost < 1 || actor->GetAttachParentActor() != nullptr){
        return refusedBegin(T_Carry_FailReason_NotCarryable);
    }
    const FGameplayTag blocked = checkCanBegin();
    if(blocked.IsValid()){
        return refusedBegin(blocked);
    }
    if(cost > getFreeSlotCount()){
        return refusedBegin(T_Interaction_FailReason_HandsFull);
    }

    // Everything checked.
    const bool wasActive = isCarryActive();
    const FName label = IBPI_CarryableActor::Execute_getCarryLabel(actor);
    FS_CarryBeginResult result;
    result.slotIndex = fillSlots(FGuid::NewGuid(), label, cost, actor);

    // The actor first makes itself carryable (physics and collision off, timer paused), then it is attached.
    IBPI_CarryableActor::Execute_onCarryBegan(actor, GetOwner());
    const ACharacter* character = Cast<ACharacter>(GetOwner());
    USceneComponent* attachParent = character != nullptr ? static_cast<USceneComponent*>(character->GetMesh()) : GetOwner()->GetRootComponent();
    const TArray<FName>& socketNames = UCarrySettings::get()->carrySocketNames;
    const FName socketName = socketNames.IsValidIndex(result.slotIndex) ? socketNames[result.slotIndex] : NAME_None;
    actor->AttachToComponent(attachParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, socketName);
    actor->OnDestroyed.AddUniqueDynamic(this, &UCarryComponent::onCarriedActorDestroyed);

    afterBegin(result.slotIndex, label, wasActive);
    return result;
}

void UCarryComponent::onCarriedActorDestroyed(AActor* destroyedActor){
    for(int32 index = 0; index < carrySlots.Num(); index++){
        if(carrySlots[index].isOccupied() && carrySlots[index].holdsActor && carrySlots[index].carriedActor == destroyedActor){
            carrySlots[index].carriedActor = nullptr;   // it is going away: endCarry must not detach, move or call it
            endCarryInternal(index, E_EndCarryReason::Interrupted, FGameplayTag());
            return;
        }
    }
}

// ---------- Section 9: requests ----------

bool UCarryComponent::passRequestGate(const TCHAR* operation) const{
    UInventoryRequestComponent* requests = UInventoryRequestComponent::findFor(getCarrierPlayerState());
    if(requests == nullptr){
        CARRY_LOG(Warning, this, "%s refused: the player has no request component", operation);
        return false;
    }
    const FGameplayTag blocked = requests->gate(false, false);   // hands not needed; not while Downed or Restrained
    if(blocked.IsValid()){
        requests->refuse(blocked, operation);
        return false;
    }
    return true;
}

void UCarryComponent::replyIfRefused(const FGameplayTag& failReason, const TCHAR* operation) const{
    if(!failReason.IsValid()){
        return;
    }
    UInventoryRequestComponent* requests = UInventoryRequestComponent::findFor(getCarrierPlayerState());
    if(requests != nullptr){
        requests->refuse(failReason, operation);   // logs the reason tag and tells only the requester (Doc02 §6)
    }
}

void UCarryComponent::serverDropCarried_Implementation(int32 slotIndex){
    if(!passRequestGate(TEXT("dropCarried"))){
        return;
    }
    const int32 wantedSlot = slotIndex < 0 ? getActiveSlotIndex() : slotIndex;
    replyIfRefused(endCarry(wantedSlot, E_EndCarryReason::Dropped).failReason, TEXT("dropCarried"));
}

void UCarryComponent::serverThrowCarried_Implementation(int32 slotIndex, FVector_NetQuantizeNormal direction){
    if(!passRequestGate(TEXT("throwCarried"))){
        return;
    }
    // Doc02 19.6, the throw vector: the direction must be close to where the host knows this pawn is looking.
    // The check is inclusive: a direction exactly at the limit is allowed (Doc02 §19.9).
    const APawn* pawn = Cast<APawn>(GetOwner());
    const FVector viewDirection = pawn != nullptr ? pawn->GetBaseAimRotation().Vector() : GetOwner()->GetActorForwardVector();
    const FVector requested = FVector(direction).GetSafeNormal();
    const float smallestDot = FMath::Cos(FMath::DegreesToRadians(UCarrySettings::get()->maxThrowAngleDegrees));
    if(requested.IsNearlyZero() || FVector::DotProduct(viewDirection, requested) < smallestDot - UE_KINDA_SMALL_NUMBER){
        replyIfRefused(T_Carry_FailReason_InvalidThrow, TEXT("throwCarried"));
        return;
    }
    const int32 wantedSlot = slotIndex < 0 ? getActiveSlotIndex() : slotIndex;
    replyIfRefused(throwCarried(wantedSlot, requested).failReason, TEXT("throwCarried"));
}
