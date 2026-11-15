#include "CraftingFramework/UCraftingComponent.h"
#include "CraftingFramework/CraftingLog.h"
#include "CraftingFramework/CraftingServices.h"
#include "CraftingFramework/UBFL_Crafting.h"
#include "CraftingFramework/UCraftingRecipeSubsystem.h"
#include "CraftingFramework/UCraftingSettings.h"
#include "CraftingFramework/UCraftingStationComponent.h"
#include "CraftingFramework/UDA_CraftingRecipe.h"
#include "CraftingFramework/02Tags/T_CraftingFramework.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRequestComponent.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

// ---------- construction and replication ----------

UCraftingComponent::UCraftingComponent(){
    PrimaryComponentTick.bCanEverTick = false;   // no tick: one timer exists, only while a craft runs
    SetIsReplicatedByDefault(true);
}

void UCraftingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UCraftingComponent, sessionState, COND_OwnerOnly);   // only the crafting player needs it
}

UCraftingComponent* UCraftingComponent::findOn(const AActor* crafter){
    if(crafter == nullptr){
        return nullptr;
    }
    const APlayerState* player = Cast<APlayerState>(crafter);
    if(player != nullptr){
        return findFor(player);
    }
    return crafter->FindComponentByClass<UCraftingComponent>();
}

UCraftingComponent* UCraftingComponent::findFor(const APlayerState* player){
    const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
    return pawn != nullptr ? pawn->FindComponentByClass<UCraftingComponent>() : nullptr;
}

bool UCraftingComponent::isServerSide() const{
    return GetOwner() != nullptr && GetOwner()->HasAuthority();
}

APlayerState* UCraftingComponent::getCrafterPlayerState() const{
    const APawn* pawn = Cast<APawn>(GetOwner());
    return pawn != nullptr ? pawn->GetPlayerState() : nullptr;
}

bool UCraftingComponent::isLocalCrafter() const{
    const APawn* pawn = Cast<APawn>(GetOwner());
    return pawn != nullptr && pawn->IsLocallyControlled();
}

FS_InventoryResult UCraftingComponent::refusal(const FGameplayTag& reason){
    FS_InventoryResult result;
    result.failReason = reason;
    return result;
}

// ---------- queries ----------

FS_CraftingSessionState UCraftingComponent::getSessionState() const{
    return sessionState;
}

bool UCraftingComponent::isSessionOpen() const{
    return sessionState.isOpen;
}

bool UCraftingComponent::isCraftInProgress() const{
    return !sessionState.activeRecipeId.IsNone();
}

TMap<FName, int32> UCraftingComponent::getStagedQuantities() const{
    TMap<FName, int32> staged;
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getCrafterPlayerState());
    if(inventory == nullptr){
        return staged;
    }
    // The same code on the server (the real bag) and on the owning client (its replicated copy of the bag).
    // A staged weapon is an entry with a reserved quantity of 1; several staged copies of one type add up.
    for(const FS_InventoryEntry& entry : inventory->getBagEntries()){
        if(entry.reservedQuantity > 0){
            staged.FindOrAdd(entry.itemId) += entry.reservedQuantity;
        }
    }
    return staged;
}

int32 UCraftingComponent::getStagedQuantity(FName itemId) const{
    return getStagedQuantities().FindRef(itemId);
}

TArray<FS_AvailableRecipeOption> UCraftingComponent::getAvailableRecipes() const{
    return UBFL_Crafting::buildRecipeList(this, getStagedQuantities(), sessionState.stationTags);
}

// ---------- shared checks and helpers ----------

FGameplayTag UCraftingComponent::checkPlayerCanCraft() const{
    const APlayerState* player = getCrafterPlayerState();
    const UInventoryServices* services = UInventoryServices::get(this);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(player == nullptr || services == nullptr || inventory == nullptr){
        return T_Inventory_FailReason_NotFound;
    }
    if(!inventory->hasBag()){
        return T_Crafting_FailReason_BagRequired;   // a Bagless player cannot craft at all (R02 3.11, R01 3.8)
    }
    if(services->isIncapacitated(player)){
        return T_Interaction_FailReason_NotAllowedInState;
    }
    if(services->isCarryActive(player)){
        return T_Interaction_FailReason_HandsFull;   // refused while carrying; refusing never loses an item (Index A93, A115 S12-P5)
    }
    return FGameplayTag();
}

void UCraftingComponent::pushState(){
    if(isServerSide()){
        GetOwner()->ForceNetUpdate();
    }
    onCraftingChanged.Broadcast(GetOwner());
}

void UCraftingComponent::releaseStation(){
    if(station.IsValid()){
        station->release(this);
    }
    station = nullptr;
    sessionState.hasStation = false;
    sessionState.stationId = FGuid();
    sessionState.stationTags.Reset();
}

// ---------- messages (R02 §7: local to the crafting player) ----------

void UCraftingComponent::raiseSessionOpened() const{
    if(!UGameplayMessageSubsystem::HasInstance(this)){
        return;
    }
    FS_CraftingSessionOpenedMessage message;
    message.crafter = GetOwner();
    message.stationId = sessionState.stationId;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Crafting_SessionOpened, message);
}

void UCraftingComponent::raiseItemCrafted(FName recipeId, FName outputItemId, int32 outputQuantity) const{
    if(!UGameplayMessageSubsystem::HasInstance(this)){
        return;
    }
    FS_CraftingItemCraftedMessage message;
    message.crafter = GetOwner();
    message.recipeId = recipeId;
    message.outputItemId = outputItemId;
    message.outputQuantity = outputQuantity;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Crafting_ItemCrafted, message);
}

// The owning client learns of a change only here.
void UCraftingComponent::OnRep_sessionState(const FS_CraftingSessionState& previousState){
    if(!previousState.isOpen && sessionState.isOpen){
        raiseSessionOpened();
    }
    onCraftingChanged.Broadcast(GetOwner());
}

void UCraftingComponent::clientSessionClosed_Implementation(const TArray<FS_ItemStack>& returnedItems, E_CraftingCloseReason reason){
    if(!UGameplayMessageSubsystem::HasInstance(this)){
        return;
    }
    FS_CraftingSessionClosedMessage message;
    message.crafter = GetOwner();
    message.returnedItems = returnedItems;
    message.reason = reason;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Crafting_SessionClosed, message);
}

void UCraftingComponent::clientItemCrafted_Implementation(FName recipeId, FName outputItemId, int32 outputQuantity){
    if(!isServerSide()){
        raiseItemCrafted(recipeId, outputItemId, outputQuantity);   // the host has already raised it for its own player
    }
}

// ---------- the station context (K8) ----------

FS_InventoryResult UCraftingComponent::enterStation(UCraftingStationComponent* newStation){
    if(!isServerSide()){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    if(newStation == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    const FGameplayTag blocked = checkPlayerCanCraft();
    if(blocked.IsValid()){
        return refusal(blocked);
    }
    if(station.Get() == newStation){
        return FS_InventoryResult();   // already there
    }
    if(newStation->isInUse()){
        return refusal(T_Interaction_FailReason_InUse);   // checked before anything of ours is given up
    }
    // Everything checked. Whatever was open belongs to another place and ends first.
    if(sessionState.isOpen || station.IsValid()){
        closeCraftingSession(E_CraftingCloseReason::BagClosed);
    }
    const FGameplayTag lockRefusal = newStation->tryLock(this);
    if(lockRefusal.IsValid()){
        return refusal(lockRefusal);
    }
    station = newStation;
    sessionState.hasStation = true;
    sessionState.stationId = newStation->getStationId();
    sessionState.stationTags = newStation->stationTags;
    pushState();
    CRAFTING_LOG(Log, this, "%s entered station %s (%s)", *GetNameSafe(GetOwner()), *GetNameSafe(newStation->GetOwner()), *newStation->stationTags.ToStringSimple());
    return FS_InventoryResult();
}

// ---------- the session (R02 3.2) ----------

FS_InventoryResult UCraftingComponent::openCraftingSession(){
    if(!isServerSide()){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    if(sessionState.isOpen){
        return FS_InventoryResult();
    }
    const FGameplayTag blocked = checkPlayerCanCraft();
    if(blocked.IsValid()){
        return refusal(blocked);
    }
    sessionState.isOpen = true;
    bindStagedItemLost();
    pushState();
    if(isLocalCrafter()){
        raiseSessionOpened();   // the host's own player; a client raises it from OnRep_sessionState
    }
    CRAFTING_LOG(Log, this, "%s opened a crafting session (%s)", *GetNameSafe(GetOwner()), sessionState.hasStation ? TEXT("at a station") : TEXT("by hand"));
    return FS_InventoryResult();
}

FS_InventoryResult UCraftingComponent::stageIngredient(const FS_ItemRef& ref, int32 quantity){
    if(!isServerSide()){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const FGameplayTag blocked = checkPlayerCanCraft();
    if(blocked.IsValid()){
        return refusal(blocked);
    }
    if(isCraftInProgress()){
        return refusal(T_Crafting_FailReason_CraftInProgress);   // the crafting area is fixed while an item is being made (K13)
    }
    APlayerState* player = getCrafterPlayerState();
    // R01 does the real work and every item check: the Axe, the map, a wielded weapon, a container that holds something,
    // a quantity that is not available (R01 guide S7.2). The station id is the key of this session's reservations.
    const FS_InventoryResult reserved = UBFL_Inventory::reserveForStaging(player, ref, quantity, sessionState.stationId);
    if(!reserved.succeeded()){
        return reserved;
    }
    // The session starts when the first ingredient is staged (R02 3.2, Index A115 S12-P2).
    if(!sessionState.isOpen){
        const FS_InventoryResult opened = openCraftingSession();
        if(!opened.succeeded()){
            UBFL_Inventory::releaseStagedReservation(player, sessionState.stationId);   // cannot happen after the check above; never leave it staged
            return opened;
        }
    }
    CRAFTING_LOG(Verbose, this, "%s staged %s x%d", *GetNameSafe(GetOwner()), *ref.itemId.ToString(), reserved.quantity);
    return reserved;
}

FS_InventoryResult UCraftingComponent::unstageIngredient(const FS_ItemRef& ref, int32 quantity){
    if(!isServerSide()){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    if(isCraftInProgress()){
        return refusal(T_Crafting_FailReason_CraftInProgress);
    }
    // No state check: giving something back is always allowed. The session stays open with an empty crafting area;
    // it ends when the bag closes (R02 3.2).
    return UBFL_Inventory::releaseStagedItem(getCrafterPlayerState(), ref, quantity);
}

void UCraftingComponent::closeCraftingSession(E_CraftingCloseReason reason){
    if(!isServerSide()){
        return;
    }
    cancelCraft();             // a session that ends takes its running craft with it; nothing is consumed (R02 3.7)
    unbindStagedItemLost();
    const bool wasOpen = sessionState.isOpen;
    // What was still staged, for the message. The items never left the bag: releasing only makes them available again.
    TArray<FS_ItemStack> returnedItems;
    if(wasOpen){
        for(const TPair<FName, int32>& staged : getStagedQuantities()){
            FS_ItemStack stack;
            stack.itemId = staged.Key;
            stack.quantity = staged.Value;
            returnedItems.Add(stack);
        }
        APlayerState* player = getCrafterPlayerState();
        if(player != nullptr){
            UBFL_Inventory::releaseStagedReservation(player, sessionState.stationId);
        }
    }
    const bool hadAnything = wasOpen || sessionState.hasStation;
    releaseStation();
    sessionState = FS_CraftingSessionState();
    if(!hadAnything){
        return;   // nothing was open: nothing to tell anyone
    }
    pushState();
    if(wasOpen){
        clientSessionClosed(returnedItems, reason);
        CRAFTING_LOG(Log, this, "%s's crafting session closed (%s), %d staged item type(s) returned", *GetNameSafe(GetOwner()), *UEnum::GetValueAsString(reason), returnedItems.Num());
    }else{
        CRAFTING_LOG(Log, this, "%s left the station (%s)", *GetNameSafe(GetOwner()), *UEnum::GetValueAsString(reason));
    }
}

// ---------- Section 6: crafting ----------

FS_InventoryResult UCraftingComponent::requestCraft(FName recipeId){
    if(!isServerSide()){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const FGameplayTag blocked = checkPlayerCanCraft();
    if(blocked.IsValid()){
        return refusal(blocked);
    }
    if(isCraftInProgress()){
        return refusal(T_Crafting_FailReason_CraftInProgress);   // a press while a craft is running is ignored (R02 3.7, K13)
    }
    const UCraftingRecipeSubsystem* recipes = UCraftingRecipeSubsystem::get(this);
    const UCraftingServices* services = UCraftingServices::get(this);
    const UDA_CraftingRecipe* recipe = recipes != nullptr ? recipes->getRecipe(recipeId) : nullptr;
    if(recipe == nullptr || services == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(!services->isUnlocked(recipe->requiredBlueprintTag)){
        return refusal(T_Crafting_FailReason_BlueprintLocked);
    }
    if(!sessionState.isOpen){
        return refusal(T_Crafting_FailReason_InsufficientStaged);   // nothing is staged at all
    }
    // The same rule the screen used, with the SERVER's data (K3). Only green passes.
    const FS_AvailableRecipeOption option = UBFL_Crafting::evaluateRecipe(*recipe, getStagedQuantities(), sessionState.stationTags);
    if(option.isStationMissing){
        return refusal(T_Crafting_FailReason_StationRequired);   // also when an ingredient is missing as well (K14)
    }
    if(option.state == E_RecipeListState::Missing){
        return refusal(T_Crafting_FailReason_InsufficientStaged);
    }
    if(option.state == E_RecipeListState::ExtraItems){
        return refusal(T_Crafting_FailReason_ExtraIngredients);
    }

    // Everything checked. The craft is running from here; nothing is consumed until it completes.
    sessionState.activeRecipeId = recipeId;
    sessionState.activeCraftSeconds = recipe->craftSeconds;
    pushState();
    services->raiseCosmeticEvent(T_Cosmetic_Event_Craft_Started, GetOwner()->GetActorLocation(), GetOwner());   // other players see the animation (Doc B21 A.4)
    CRAFTING_LOG(Log, this, "%s started crafting %s (%.2f s)", *GetNameSafe(GetOwner()), *recipeId.ToString(), recipe->craftSeconds);

    if(recipe->craftSeconds <= 0.0f){
        completeCraft();
    }else{
        GetWorld()->GetTimerManager().SetTimer(craftTimer, this, &UCraftingComponent::completeCraft, recipe->craftSeconds, false);
    }
    FS_InventoryResult result;
    result.quantity = 1;
    return result;
}

void UCraftingComponent::cancelCraft(){
    if(!isCraftInProgress()){
        return;
    }
    if(UWorld* world = GetWorld()){
        world->GetTimerManager().ClearTimer(craftTimer);
    }
    CRAFTING_LOG(Log, this, "%s's craft of %s was cancelled; nothing was consumed", *GetNameSafe(GetOwner()), *sessionState.activeRecipeId.ToString());
    sessionState.activeRecipeId = NAME_None;
    sessionState.activeCraftSeconds = 0.0f;
    pushState();
}

void UCraftingComponent::completeCraft(){
    const UCraftingRecipeSubsystem* recipes = UCraftingRecipeSubsystem::get(this);
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UCraftingServices* services = UCraftingServices::get(this);
    const UDA_CraftingRecipe* recipe = recipes != nullptr ? recipes->getRecipe(sessionState.activeRecipeId) : nullptr;
    APlayerState* player = getCrafterPlayerState();
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;

    // Damage, then Downed or death, then completion (Doc02 19.9): a player who is Downed or dead by now makes nothing.
    // The same check covers a bag that is gone and a carry that began.
    if(recipe == nullptr || catalog == nullptr || services == nullptr || bag == nullptr || checkPlayerCanCraft().IsValid()){
        cancelCraft();
        return;
    }
    // Still green? A staged stack may have spoiled while the animation played (Section 7).
    const FS_AvailableRecipeOption option = UBFL_Crafting::evaluateRecipe(*recipe, getStagedQuantities(), sessionState.stationTags);
    if(option.state != E_RecipeListState::Craftable){
        cancelCraft();
        return;
    }

    // Work out exactly what will be consumed BEFORE consuming anything (Doc02 §6).
    TArray<FS_ItemRef> refs;
    TArray<int32> amounts;
    for(const FS_CraftingRecipeRequirement& requirement : recipe->requirementList){
        const UDA_ItemDefinition* definition = catalog->getDefinition(requirement.itemId);
        if(definition != nullptr && definition->isInstanced()){
            // A weapon is reserved by its instance id (R02 3.10): take the staged copies one by one (K15).
            int32 found = 0;
            for(const FS_StagedReservation& reservation : bag->getReservations()){
                if(found < requirement.quantity && reservation.instanceId.IsValid() && reservation.itemId == requirement.itemId && reservation.owningStationId == sessionState.stationId){
                    FS_ItemRef ref;
                    ref.itemId = requirement.itemId;
                    ref.instanceId = reservation.instanceId;
                    refs.Add(ref);
                    amounts.Add(1);
                    found++;
                }
            }
            if(found < requirement.quantity){
                cancelCraft();
                return;
            }
        }else{
            FS_ItemRef ref;
            ref.itemId = requirement.itemId;
            refs.Add(ref);
            amounts.Add(requirement.quantity);
        }
    }

    // Everything checked. Consume one recipe's worth of the RESERVED ingredients; the surplus stays staged (R02 3.5).
    for(int32 index = 0; index < refs.Num(); index++){
        const FS_InventoryResult consumed = UBFL_Inventory::consumeStaged(player, sessionState.stationId, refs[index], amounts[index]);
        if(!consumed.succeeded()){
            CRAFTING_LOG(Error, this, "Craft %s: consuming %s failed (%s) after the checks passed", *recipe->recipeId.ToString(), *refs[index].itemId.ToString(), *consumed.failReason.ToString());
        }
    }
    // The output: into the bag, or on the ground next to the player when its type is at the limit. Never destroyed
    // (R02 3.6, Doc02 19.5). An instanced output is a NEW instance at full condition (R02 3.10).
    FS_ItemStack output;
    output.itemId = recipe->outputItemId;
    output.quantity = recipe->outputQuantity;
    const FS_GrantResult granted = UBFL_Inventory::grantOrDrop(player, output);

    const FName craftedRecipeId = recipe->recipeId;
    sessionState.activeRecipeId = NAME_None;
    sessionState.activeCraftSeconds = 0.0f;
    pushState();

    raiseItemCrafted(craftedRecipeId, output.itemId, output.quantity);              // on the server
    clientItemCrafted(craftedRecipeId, output.itemId, output.quantity);             // then local to the crafting player
    services->raiseCosmeticEvent(T_Cosmetic_Event_Craft_Finished, GetOwner()->GetActorLocation(), GetOwner());
    CRAFTING_LOG(Log, this, "%s crafted %s: %s x%d (stored %d, dropped %d, carried %d)", *GetNameSafe(GetOwner()), *craftedRecipeId.ToString(), *output.itemId.ToString(), output.quantity, granted.quantityStored, granted.quantityDropped, granted.quantityCarried);
}

// ---------- Section 7: ending a session ----------

void UCraftingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason){
    if(isServerSide()){
        if(EndPlayReason == EEndPlayReason::EndPlayInEditor || EndPlayReason == EEndPlayReason::Quit){
            // The whole world is going away: nothing is saved and nobody is left to tell. Only stop the timer.
            if(UWorld* world = GetWorld()){
                world->GetTimerManager().ClearTimer(craftTimer);
            }
            unbindStagedItemLost();
        }else{
            // The safety net (K12): the Character is going away and nobody closed the session first.
            // The reservations are released if the bag is still there, and the station is unlocked for the next player.
            closeCraftingSession(E_CraftingCloseReason::PawnRemoved);
        }
    }
    Super::EndPlay(EndPlayReason);
}

void UCraftingComponent::bindStagedItemLost(){
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getCrafterPlayerState());
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    if(bag != nullptr && !stagedItemLostHandle.IsValid()){
        stagedItemLostHandle = bag->onStagedItemLost.AddUObject(this, &UCraftingComponent::onStagedItemLost);
        boundBag = bag;
    }
}

void UCraftingComponent::unbindStagedItemLost(){
    if(boundBag.IsValid() && stagedItemLostHandle.IsValid()){
        boundBag->onStagedItemLost.Remove(stagedItemLostHandle);
    }
    stagedItemLostHandle.Reset();
    boundBag = nullptr;
}

void UCraftingComponent::onStagedItemLost(const FGuid& lostStationId, FName itemId){
    // The fresh stack became its spoiled item, and R01 dropped the reservation (Index A106, S3-F04). The crafting area
    // simply holds less now. A craft that was counting on that item must not complete.
    const UCraftingRecipeSubsystem* recipes = UCraftingRecipeSubsystem::get(this);
    const UDA_CraftingRecipe* recipe = recipes != nullptr ? recipes->getRecipe(sessionState.activeRecipeId) : nullptr;
    if(recipe != nullptr && recipe->usesItem(itemId)){
        cancelCraft();
    }
    CRAFTING_LOG(Log, this, "%s: staged %s spoiled and left the crafting area", *GetNameSafe(GetOwner()), *itemId.ToString());
}

// ---------- Section 8: requests ----------

bool UCraftingComponent::passRequestGate(bool needsHands, bool allowWhenIncapacitated, const TCHAR* operation) const{
    UInventoryRequestComponent* requests = UInventoryRequestComponent::findFor(getCrafterPlayerState());
    if(requests == nullptr){
        CRAFTING_LOG(Warning, this, "%s refused: the player has no request component", operation);
        return false;
    }
    const FGameplayTag blocked = requests->gate(needsHands, allowWhenIncapacitated);
    if(blocked.IsValid()){
        requests->refuse(blocked, operation);
        return false;
    }
    return true;
}

void UCraftingComponent::replyIfRefused(const FGameplayTag& failReason, const TCHAR* operation) const{
    if(!failReason.IsValid()){
        return;
    }
    UInventoryRequestComponent* requests = UInventoryRequestComponent::findFor(getCrafterPlayerState());
    if(requests != nullptr){
        requests->refuse(failReason, operation);   // logs the reason tag and tells only the requester (Doc02 §6)
    }
}

void UCraftingComponent::serverUseStation_Implementation(AActor* stationActor){
    if(!passRequestGate(true, false, TEXT("useStation"))){
        return;
    }
    UCraftingStationComponent* targetStation = UCraftingStationComponent::findOn(stationActor);
    const APawn* pawn = Cast<APawn>(GetOwner());
    if(targetStation == nullptr || pawn == nullptr){
        replyIfRefused(T_Inventory_FailReason_NotFound, TEXT("useStation"));
        return;
    }
    // Range from the pawn's eye, inclusive: exactly at the limit is allowed (Doc02 §19.6, §19.9).
    if(FVector::Dist(pawn->GetPawnViewLocation(), stationActor->GetActorLocation()) > UCraftingSettings::get()->stationUseRange){
        replyIfRefused(T_Inventory_FailReason_OutOfRange, TEXT("useStation"));
        return;
    }
    replyIfRefused(enterStation(targetStation).failReason, TEXT("useStation"));
}

void UCraftingComponent::serverStageIngredient_Implementation(const FS_ItemRef& ref, bool wholeStack){
    if(!passRequestGate(true, false, TEXT("stageIngredient"))){
        return;
    }
    // The server counts "the whole stack" itself (K10). A weapon is always one copy per request (K16).
    int32 quantity = 1;
    if(wholeStack && !ref.isInstance()){
        const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getCrafterPlayerState());
        const UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
        quantity = bag != nullptr ? FMath::Max(1, bag->getAvailableQuantity(ref.itemId)) : 1;
    }
    replyIfRefused(stageIngredient(ref, quantity).failReason, TEXT("stageIngredient"));
}

void UCraftingComponent::serverUnstageIngredient_Implementation(const FS_ItemRef& ref, bool wholeStack){
    if(!passRequestGate(false, true, TEXT("unstageIngredient"))){
        return;
    }
    const int32 quantity = (wholeStack && !ref.isInstance()) ? FMath::Max(1, getStagedQuantity(ref.itemId)) : 1;
    replyIfRefused(unstageIngredient(ref, quantity).failReason, TEXT("unstageIngredient"));
}

void UCraftingComponent::serverRequestCraft_Implementation(FName recipeId){
    if(!passRequestGate(true, false, TEXT("requestCraft"))){
        return;
    }
    replyIfRefused(requestCraft(recipeId).failReason, TEXT("requestCraft"));
}

void UCraftingComponent::serverCloseBag_Implementation(){
    closeCraftingSession(E_CraftingCloseReason::BagClosed);
}
