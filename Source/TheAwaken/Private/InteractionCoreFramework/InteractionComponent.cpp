// Private/InteractionCoreFramework/InteractionComponent.cpp
#include "InteractionCoreFramework/InteractionComponent.h"
#include "InteractionCoreFramework/DA_InteractionSettings.h"
#include "InteractionCoreFramework/InteractionLockComponent.h"
#include "InteractionCoreFramework/InteractionRules.h"
#include "InteractionCoreFramework/02Tags/T_InteractionCoreFramework.h"
#include "InteractionCoreFramework/03Interfaces/BPI_Interactable.h"
#include "InteractionFrameworkExtension/02Tags/T_InteractionFrameworkExtension.h"
#include "InteractionFrameworkExtension/03Interfaces/BPI_InteractableHoldToActivate.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"
#include "CarryEncumbranceFramework/02Tags/T_CarryEncumbranceFramework.h"
#include "StatsFramework/AS_CoreVitals.h"
#include "LocalizationFramework/LocalizationService.h"
#include "GameFrameworkClassTable/AwakenPlayerController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY(LogAwakenInteraction);

namespace{
    // A line check ends this far short of the target point so a solid surface under the point does not block its own target.
    // Geometry tolerance in centimetres, not a design value.
    constexpr float lineEndInset = 1.0f;

    template<typename TPayload>
    void raiseMessage(const UActorComponent* component, const FGameplayTag& channel, const TPayload& payload){
        const UWorld* world = component->GetWorld();
        UGameInstance* gameInstance = world != nullptr ? world->GetGameInstance() : nullptr;
        UGameplayMessageSubsystem* messages = gameInstance != nullptr ? gameInstance->GetSubsystem<UGameplayMessageSubsystem>() : nullptr;
        if(messages == nullptr){
            return;
        }
        messages->BroadcastMessage(channel, payload);
    }

    bool promptsEqual(const FS_InteractionPrompt& first, const FS_InteractionPrompt& second){
        return first.verbText == second.verbText
            && first.objectName == second.objectName
            && first.key == second.key
            && first.isHold == second.isHold
            && FMath::IsNearlyEqual(first.holdProgress, second.holdProgress)
            && first.blockedReason == second.blockedReason;
    }
}

UInteractionComponent::UInteractionComponent(){
    PrimaryComponentTick.bCanEverTick = false;
    // A component's RPCs only route when the component replicates (guide Step 6). It has no replicated properties of its own.
    SetIsReplicatedByDefault(true);
}

void UInteractionComponent::BeginPlay(){
    Super::BeginPlay();
    ensureSettings();
}

void UInteractionComponent::EndPlay(const EEndPlayReason::Type endPlayReason){
    sendReleaseIfNeeded();
    stopFocus();
    AActor* owner = GetOwner();
    if(owner != nullptr && owner->HasAuthority()){
        // A destroyed pawn never leaves a hold or a lock behind (guide Step 9).
        cancelHold(E_InteractionHoldCancelReason::Disconnected);
        releaseLocksFor(owner);
    }
    unbindAbilitySystem();
    if(UWorld* world = GetWorld()){
        FTimerManager& timers = world->GetTimerManager();
        timers.ClearTimer(holdTimer);
        timers.ClearTimer(finishTimer);
        timers.ClearTimer(watchTimer);
    }
    Super::EndPlay(endPlayReason);
}

// ---------------------------------------------------------------------------------------------------------------------------
// Settings and wiring
// ---------------------------------------------------------------------------------------------------------------------------

bool UInteractionComponent::ensureSettings(){
    if(settings != nullptr){
        return true;
    }
    if(!UAssetManager::IsInitialized()){
        return false;
    }
    // Loaded the way ULocalizationService loads UDA_LocalizationSettings. The primary asset type "InteractionSettings" must be
    // registered under Project Settings > Asset Manager, or the path below is invalid.
    UAssetManager& assetManager = UAssetManager::Get();
    const FSoftObjectPath settingsPath = assetManager.GetPrimaryAssetPath(UDA_InteractionSettings::makeAssetId());
    if(!settingsPath.IsValid()){
        UE_LOG(LogAwakenInteraction, Warning, TEXT("DA_InteractionSettings was not found. Register the primary asset type InteractionSettings in Project Settings > Asset Manager and create the asset."));
        return false;
    }
    settings = Cast<UDA_InteractionSettings>(assetManager.GetStreamableManager().LoadSynchronous(settingsPath));
    if(settings == nullptr){
        UE_LOG(LogAwakenInteraction, Warning, TEXT("DA_InteractionSettings could not be loaded from %s."), *settingsPath.ToString());
    }
    return settings != nullptr;
}

APlayerController* UInteractionComponent::getOwnerController() const{
    const APawn* pawn = Cast<APawn>(GetOwner());
    return pawn != nullptr ? Cast<APlayerController>(pawn->GetController()) : nullptr;
}

bool UInteractionComponent::isLocalOwner() const{
    const APawn* pawn = Cast<APawn>(GetOwner());
    return pawn != nullptr && pawn->IsLocallyControlled();
}

void UInteractionComponent::bindInput(UInputComponent* playerInputComponent){
    UEnhancedInputComponent* input = Cast<UEnhancedInputComponent>(playerInputComponent);
    if(input == nullptr || !ensureSettings() || settings->interactAction == nullptr){
        UE_LOG(LogAwakenInteraction, Warning, TEXT("bindInput: no Enhanced Input component or no interaction settings, interaction input is not bound"));
        return;
    }
    // Started and Completed/Canceled, never Triggered: Triggered fires every frame and would flood the server (B18 3.4).
    input->BindAction(settings->interactAction, ETriggerEvent::Started, this, &UInteractionComponent::onInteractPressed);
    input->BindAction(settings->interactAction, ETriggerEvent::Completed, this, &UInteractionComponent::onInteractReleased);
    input->BindAction(settings->interactAction, ETriggerEvent::Canceled, this, &UInteractionComponent::onInteractReleased);
    startFocusTimer();
}

void UInteractionComponent::startFocusTimer(){
    UWorld* world = GetWorld();
    if(world == nullptr || settings == nullptr){
        return;
    }
    // Starting a timer that already runs just restarts it, so a second possession is harmless. A zero rate would clear it.
    world->GetTimerManager().SetTimer(focusTimer, this, &UInteractionComponent::updateFocus, FMath::Max(settings->focusInterval, UE_KINDA_SMALL_NUMBER), true);
}

void UInteractionComponent::stopFocus(){
    if(UWorld* world = GetWorld()){
        world->GetTimerManager().ClearTimer(focusTimer);
    }
    setFocus(nullptr);
}

void UInteractionComponent::onUnpossessed(){
    sendReleaseIfNeeded();
    stopFocus();
    AActor* owner = GetOwner();
    if(owner != nullptr && owner->HasAuthority()){
        cancelHold(E_InteractionHoldCancelReason::Disconnected);
        releaseLocksFor(owner);
    }
}

void UInteractionComponent::bindAbilitySystem(UAbilitySystemComponent* abilitySystem){
    unbindAbilitySystem();
    if(abilitySystem == nullptr){
        return;
    }
    boundAbilitySystem = abilitySystem;
    hpChangedHandle = abilitySystem->GetGameplayAttributeValueChangeDelegate(UAS_CoreVitals::currentHPAttribute()).AddUObject(this, &UInteractionComponent::onHPChanged);
    incapacitatedHandle = abilitySystem->RegisterGameplayTagEvent(T_Player_State_Incapacitated, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UInteractionComponent::onIncapacitatedChanged);
}

void UInteractionComponent::unbindAbilitySystem(){
    if(boundAbilitySystem.IsValid()){
        boundAbilitySystem->GetGameplayAttributeValueChangeDelegate(UAS_CoreVitals::currentHPAttribute()).Remove(hpChangedHandle);
        boundAbilitySystem->RegisterGameplayTagEvent(T_Player_State_Incapacitated, EGameplayTagEventType::NewOrRemoved).Remove(incapacitatedHandle);
    }
    boundAbilitySystem.Reset();
    hpChangedHandle.Reset();
    incapacitatedHandle.Reset();
}

void UInteractionComponent::setPressOnceMode(bool isPressOnce){
    pressOnceMode = isPressOnce;
}

void UInteractionComponent::setLockedIntoScene(bool isLocked){
    lockedIntoScene = isLocked;
    AActor* owner = GetOwner();
    if(isLocked && owner != nullptr && owner->HasAuthority()){
        // A scene start cancels holds and releases every lock (B18 3.8, 3.7).
        cancelHold(E_InteractionHoldCancelReason::Scene);
        releaseLocksFor(owner);
    }
}

void UInteractionComponent::onPlayerLeft(){
    cancelHold(E_InteractionHoldCancelReason::Disconnected);
    releaseLocksFor(GetOwner());
}

// ---------------------------------------------------------------------------------------------------------------------------
// Aim (client) and eye (host). TEMPORARY until Doc B04 exists.
// ---------------------------------------------------------------------------------------------------------------------------

FVector UInteractionComponent::getAimTraceOrigin() const{
    // TEMPORARY (Doc B04 3.1 getAimTraceOrigin): the camera location of the owning player.
    FVector location = FVector::ZeroVector;
    FRotator rotation = FRotator::ZeroRotator;
    if(APlayerController* controller = getOwnerController()){
        controller->GetPlayerViewPoint(location, rotation);
    }else if(const AActor* owner = GetOwner()){
        location = owner->GetActorLocation();
    }
    return location;
}

FVector UInteractionComponent::getAimTraceDirection() const{
    // TEMPORARY (Doc B04 3.1 getAimTraceDirection): the camera's forward vector.
    FVector location = FVector::ZeroVector;
    FRotator rotation = FRotator::ZeroRotator;
    if(APlayerController* controller = getOwnerController()){
        controller->GetPlayerViewPoint(location, rotation);
    }else if(const AActor* owner = GetOwner()){
        rotation = owner->GetActorRotation();
    }
    return rotation.Vector();
}

FVector UInteractionComponent::getHostEyeLocation() const{
    // TEMPORARY (Doc B04 3.1): the host eye is the pawn position plus a stance eye height, NEVER a camera (the host does not
    // follow a remote client's view). GetPawnViewLocation is pawn position plus BaseEyeHeight, a test value until B04 exists.
    // Swap this body for the B04 stance eye height; GetPawnViewLocation must not survive in gameplay code (B04 step 13 check).
    const APawn* pawn = Cast<APawn>(GetOwner());
    if(pawn != nullptr){
        return pawn->GetPawnViewLocation();
    }
    const AActor* owner = GetOwner();
    return owner != nullptr ? owner->GetActorLocation() : FVector::ZeroVector;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Geometry shared by focus and the server checks
// ---------------------------------------------------------------------------------------------------------------------------

float UInteractionComponent::measureDistance(const AActor* target, const FVector& point, FVector& closestPoint) const{
    // PROPOSED (B18 does not say what is measured): from the point to the closest point of the target's collision, so a big
    // object such as a door counts from its edge, not its pivot.
    float best = -1.0f;
    closestPoint = target->GetActorLocation();
    TInlineComponentArray<UPrimitiveComponent*> primitives;
    target->GetComponents(primitives);
    const ECollisionChannel channel = settings != nullptr ? settings->interactionChannel.GetValue() : ECC_Visibility;
    for(UPrimitiveComponent* primitive : primitives){
        if(primitive == nullptr || !primitive->IsCollisionEnabled()){
            continue;
        }
        if(primitive->GetCollisionResponseToChannel(channel) == ECR_Ignore){
            continue;
        }
        FVector onBody = FVector::ZeroVector;
        const float distance = primitive->GetClosestPointOnCollision(point, onBody);
        if(distance < 0.0f){
            continue;   // this shape cannot answer (for example complex-only collision)
        }
        if(best < 0.0f || distance < best){
            best = distance;
            closestPoint = onBody;
        }
    }
    if(best < 0.0f){
        best = FVector::Dist(point, closestPoint);
    }
    return best;
}

bool UInteractionComponent::hasClearLine(const FVector& origin, const FVector& targetPoint, const AActor* target) const{
    UWorld* world = GetWorld();
    if(world == nullptr || settings == nullptr){
        return false;
    }
    const FVector toTarget = targetPoint - origin;
    const float length = toTarget.Size();
    if(length <= lineEndInset){
        return true;
    }
    // Interactables Overlap the channel and solids Block it, so only a solid can stop this line.
    const FVector end = origin + toTarget * ((length - lineEndInset) / length);
    FCollisionQueryParams params(SCENE_QUERY_STAT(InteractionLine), false, GetOwner());
    FHitResult hit;
    const bool isBlocked = world->LineTraceSingleByChannel(hit, origin, end, settings->interactionChannel, params);
    if(!isBlocked){
        return true;
    }
    return hit.GetActor() == target;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Focus (client only, never replicated)
// ---------------------------------------------------------------------------------------------------------------------------

void UInteractionComponent::updateFocus(){
    UWorld* world = GetWorld();
    if(world == nullptr || GetOwner() == nullptr || !ensureSettings()){
        return;
    }
    const FVector origin = getAimTraceOrigin();
    const FVector direction = getAimTraceDirection();
    const FVector end = origin + direction * settings->interactionRange;
    FCollisionQueryParams params(SCENE_QUERY_STAT(InteractionFocus), false, GetOwner());
    const FCollisionShape shape = FCollisionShape::MakeSphere(settings->sweepRadius);
    TArray<FHitResult> hits;
    world->SweepMultiByChannel(hits, origin, end, FQuat::Identity, settings->interactionChannel, shape, params);
    setFocus(pickBest(hits, origin, direction));
}

AActor* UInteractionComponent::pickBest(const TArray<FHitResult>& hits, const FVector& origin, const FVector& direction) const{
    AActor* owner = GetOwner();
    TSet<const AActor*> seen;
    TArray<AActor*> actors;
    TArray<FInteractionFocusCandidate> candidates;
    for(const FHitResult& hit : hits){
        AActor* actor = hit.GetActor();
        if(actor == nullptr || seen.Contains(actor)){
            continue;
        }
        seen.Add(actor);
        // B18 3.1 rule 1: implements the interface, isInteractableBy, within range (inclusive, measured to the closest point).
        if(!implementsInteractable(actor) || !IBPI_Interactable::Execute_isInteractableBy(actor, owner)){
            continue;
        }
        FVector closest = FVector::ZeroVector;
        const float distance = measureDistance(actor, origin, closest);
        if(!FInteractionRules::isWithinRange(distance, settings->interactionRange, 0.0f)){
            continue;
        }
        // Rule 2: a clear line from the origin.
        if(!hasClearLine(origin, closest, actor)){
            continue;
        }
        // Rule 3: the angle between the view direction and the direction to the object.
        const FVector toObject = closest - origin;
        float angle = 0.0f;
        if(!toObject.IsNearlyZero()){
            angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(direction, toObject.GetSafeNormal()), -1.0f, 1.0f));
        }
        FInteractionFocusCandidate candidate;
        candidate.angleRadians = angle;
        candidate.priority = IBPI_Interactable::Execute_getInteractionPriority(actor);
        candidate.distance = distance;
        candidates.Add(candidate);
        actors.Add(actor);
    }
    // Rule 3 and 4: smallest angle, then priority, then distance.
    const int32 bestIndex = FInteractionRules::pickBestIndex(candidates);
    return bestIndex != INDEX_NONE ? actors[bestIndex] : nullptr;
}

void UInteractionComponent::setFocus(AActor* newTarget){
    // Runs on every focus tick. The prompt can change while the player keeps looking at the same object (an enemy turns aware,
    // another player takes the object, the key is remapped), so it is rebuilt every time. One broadcast covers both changes.
    const bool targetChanged = focusedTarget.Get() != newTarget;
    focusedTarget = newTarget;
    const FS_InteractionPrompt newPrompt = buildPrompt(newTarget);
    const bool promptChanged = !promptsEqual(newPrompt, focusedPrompt);
    focusedPrompt = newPrompt;
    if(!targetChanged && !promptChanged){
        return;
    }
    if(targetChanged){
        UE_LOG(LogAwakenInteraction, Log, TEXT("focus on %s -> %s"), *GetNameSafe(GetOwner()), *GetNameSafe(newTarget));
    }
    FS_InteractionFocusMessage message;
    message.player = GetOwner();
    message.target = newTarget;
    raiseMessage(this, T_Interaction_FocusChanged, message);
}

FS_InteractionPrompt UInteractionComponent::buildPrompt(AActor* target) const{
    FS_InteractionPrompt prompt;
    if(target == nullptr || settings == nullptr){
        return prompt;
    }
    AActor* owner = GetOwner();
    prompt = IBPI_Interactable::Execute_getPrompt(target, owner);

    // Stamp what the object cannot know: the key, whether it is a hold, and the progress of THIS player's own hold.
    prompt.key = getInteractKey();
    prompt.isHold = IBPI_Interactable::Execute_getInteractionKind(target) == E_InteractionKind::Hold;
    const bool isOwnHold = localHoldTarget.Get() == target;
    prompt.holdProgress = 0.0f;
    if(isOwnHold && localHoldDuration > 0.0f && GetWorld() != nullptr){
        const double elapsed = GetWorld()->GetTimeSeconds() - localHoldStartTime;
        prompt.holdProgress = FMath::Clamp(static_cast<float>(elapsed / localHoldDuration), 0.0f, 1.0f);
    }

    // Prompt rule (guide Step 5): blockedReason comes from canInteract; an Exclusive lock held by someone else is InUse, except
    // while this player's own hold on that target runs (isInUse is a bare bool and does not say who holds it).
    FGameplayTag blocked;
    if(!IBPI_Interactable::Execute_canInteract(target, owner, blocked)){
        prompt.blockedReason = blocked.IsValid() ? blocked : T_Interaction_FailReason_NotAllowedInState.GetTag();
    }else{
        prompt.blockedReason = FGameplayTag();
    }
    if(!prompt.blockedReason.IsValid() && !isOwnHold){
        const UInteractionLockComponent* lock = findLock(target);
        if(lock != nullptr && lock->isInUse && IBPI_Interactable::Execute_getLockPolicy(target) == E_InteractionLockPolicy::Exclusive){
            prompt.blockedReason = T_Interaction_FailReason_InUse;
        }
    }
    return prompt;
}

FKey UInteractionComponent::getInteractKey() const{
    if(settings == nullptr || settings->interactAction == nullptr){
        return FKey();
    }
    const APlayerController* controller = getOwnerController();
    const ULocalPlayer* localPlayer = controller != nullptr ? controller->GetLocalPlayer() : nullptr;
    UEnhancedInputLocalPlayerSubsystem* subsystem = localPlayer != nullptr ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(localPlayer) : nullptr;
    if(subsystem == nullptr){
        return FKey();
    }
    const TArray<FKey> keys = subsystem->QueryKeysMappedToAction(settings->interactAction);
    return keys.IsEmpty() ? FKey() : keys[0];
}

AActor* UInteractionComponent::getFocusedTarget() const{
    return focusedTarget.Get();
}

FS_InteractionPrompt UInteractionComponent::getFocusedPrompt() const{
    return focusedPrompt;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Input (client)
// ---------------------------------------------------------------------------------------------------------------------------

void UInteractionComponent::onInteractPressed(){
    // Doc R12 (R12-IG13): every input callback calls this, so a player who only interacts is not marked Away.
    if(AAwakenPlayerController* controller = Cast<AAwakenPlayerController>(getOwnerController())){
        controller->noteInputActivity();
    }
    if(!ensureSettings()){
        return;
    }
    // Press-once mode: a second press while the hold runs is a cancel. It is sent for the target being held, and the server treats a
    // request for the target already being held as a cancel.
    if(pressOnceMode && localHoldTarget.IsValid()){
        serverRequestInteract(localHoldTarget.Get());
        return;
    }
    AActor* target = getFocusedTarget();
    if(target == nullptr){
        return;
    }
    // Sent even when the prompt is blocked: the host's check decides, and a refusal shows the reason line (guide Step 5).
    releaseExpected = !pressOnceMode && IBPI_Interactable::Execute_getInteractionKind(target) == E_InteractionKind::Hold;
    serverRequestInteract(target);
}

void UInteractionComponent::onInteractReleased(){
    // Press-once mode never sends a release. The server does not need to know which mode the client uses.
    if(pressOnceMode){
        return;
    }
    if(releaseExpected || localHoldTarget.IsValid()){
        releaseExpected = false;
        serverReleaseInteract();
    }
}

void UInteractionComponent::sendReleaseIfNeeded(){
    // A key release must never be lost and leave the host completing a hold nobody is holding (guide Step 4).
    if(!pressOnceMode && isLocalOwner() && (releaseExpected || localHoldTarget.IsValid())){
        serverReleaseInteract();
    }
    releaseExpected = false;
    localHoldTarget.Reset();
}

// ---------------------------------------------------------------------------------------------------------------------------
// Requests and the server checks (B18 3.5)
// ---------------------------------------------------------------------------------------------------------------------------

bool UInteractionComponent::implementsInteractable(const AActor* actor){
    return actor != nullptr && actor->GetClass()->ImplementsInterface(UBPI_Interactable::StaticClass());
}

UInteractionLockComponent* UInteractionComponent::findLock(const AActor* target) const{
    return target != nullptr ? target->FindComponentByClass<UInteractionLockComponent>() : nullptr;
}

bool UInteractionComponent::hasPlayerTag(const FGameplayTag& tag) const{
    const UAbilitySystemComponent* abilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
    return abilitySystem != nullptr && abilitySystem->HasMatchingGameplayTag(tag);
}

void UInteractionComponent::serverRequestInteract_Implementation(AActor* target){
    if(!ensureSettings()){
        return;
    }
    // A request while a hold runs is handled BEFORE the checks, because a cancel must never be refused for state reasons.
    // The same target cancels (press-once mode); another target is ignored and the hold continues (PROPOSED).
    if(hasHold()){
        if(target != nullptr && target == activeHold.target.Get()){
            cancelHold(E_InteractionHoldCancelReason::Released);
        }
        return;
    }
    FGameplayTag failReason;
    const E_InteractionRequestVerdict verdict = validateRequest(target, failReason);
    if(verdict == E_InteractionRequestVerdict::Ignore){
        return;
    }
    if(verdict == E_InteractionRequestVerdict::Refuse){
        UE_LOG(LogAwakenInteraction, Log, TEXT("request refused: %s -> %s (%s)"), *GetNameSafe(GetOwner()), *GetNameSafe(target), *failReason.ToString());
        clientInteractRefused(target, failReason);
        return;
    }
    runInteraction(target);
}

E_InteractionRequestVerdict UInteractionComponent::validateRequest(AActor* target, FGameplayTag& failReason) const{
    // Checks 1 to 6 only READ state. Interact and lock acquisition happen after check 6 passes (Doc02 6).
    AActor* owner = GetOwner();
    const UDA_InteractionSettings* config = settings;

    // The host's eye, never the client's camera (B04 3.1).
    const FVector eye = getHostEyeLocation();
    FVector closest = FVector::ZeroVector;
    float distance = 0.0f;
    bool isMeasured = false;
    auto measure = [&](){
        if(!isMeasured){
            distance = measureDistance(target, eye, closest);
            isMeasured = true;
        }
    };
    FGameplayTag interactionTag;
    bool hasTag = false;
    auto getTag = [&]() -> const FGameplayTag&{
        if(!hasTag){
            interactionTag = IBPI_Interactable::Execute_getInteractionTag(target);
            hasTag = true;
        }
        return interactionTag;
    };

    FInteractionRequestProbe probe;
    probe.targetUsable = [&]() -> bool{
        return implementsInteractable(target) && IBPI_Interactable::Execute_isInteractableBy(target, owner);
    };
    probe.stateAllows = [&]() -> bool{
        return FInteractionRules::stateAllows(hasPlayerTag(T_Player_State_Incapacitated), lockedIntoScene, getTag() == T_Interaction_Escape);
    };
    probe.inRange = [&]() -> bool{
        measure();
        return FInteractionRules::isWithinRange(distance, config->interactionRange, config->rangeSlack);
    };
    probe.hasLineOfSight = [&]() -> bool{
        measure();
        return hasClearLine(eye, closest, target);
    };
    probe.lockAllows = [&]() -> bool{
        const E_InteractionLockPolicy policy = IBPI_Interactable::Execute_getLockPolicy(target);
        if(policy == E_InteractionLockPolicy::None){
            return true;
        }
        const UInteractionLockComponent* lock = findLock(target);
        if(lock == nullptr){
            UE_LOG(LogAwakenInteraction, Warning, TEXT("%s declares a lock policy but has no UInteractionLockComponent: no lock is taken."), *GetNameSafe(target));
            return true;
        }
        return const_cast<UInteractionLockComponent*>(lock)->canAcquire(owner, policy);
    };
    probe.handsAllow = [&]() -> bool{
        return FInteractionRules::handsAllow(hasPlayerTag(T_Carry_State_Active), config->allowedWhileCarrying.HasTagExact(getTag()));
    };
    probe.objectAllows = [&](FGameplayTag& reason) -> bool{
        return IBPI_Interactable::Execute_canInteract(target, owner, reason);
    };
    return FInteractionRules::validate(probe, failReason);
}

void UInteractionComponent::runInteraction(AActor* target){
    AActor* owner = GetOwner();
    switch(IBPI_Interactable::Execute_getInteractionKind(target)){
        case E_InteractionKind::Press:{
            TWeakObjectPtr<AActor> weakTarget = target;
            // Read before interact: the object may destroy itself (a pickup). Noise is raised after, so a refusal is silent.
            const FPendingNoise pendingNoise = capturePendingNoise(target);
            acquireLock(target);
            IBPI_Interactable::Execute_interact(target, owner);
            UE_LOG(LogAwakenInteraction, Log, TEXT("press: %s used %s"), *GetNameSafe(owner), *GetNameSafe(weakTarget.Get()));
            reportNoise(pendingNoise);
            releaseIfPressOnly(weakTarget);
            break;
        }
        case E_InteractionKind::Hold:
            beginHold(target);
            break;
        case E_InteractionKind::Grab:
            // Doc B11 3.3 owns the grab: acquire the Exclusive lock and call its beginGrab once that interface is built.
            UE_LOG(LogAwakenInteraction, Warning, TEXT("%s is a Grab object, but IBPI_InteractableGrabbable is not built yet (Doc B11 pass)."), *GetNameSafe(target));
            break;
    }
}

void UInteractionComponent::clientInteractRefused_Implementation(AActor* target, FGameplayTag reason){
    // The server refused, so no hold will come: the key release is not owed any more.
    releaseExpected = false;
    showReasonLine(reason);
}

void UInteractionComponent::showReasonLine(const FGameplayTag& reason) const{
    // TEMPORARY stub for the Doc B17 notice line. Text comes from the tag through DT_ReasonText (Doc R10 3.9).
    FText text = FText::FromString(reason.ToString());
    const UWorld* world = GetWorld();
    const UGameInstance* gameInstance = world != nullptr ? world->GetGameInstance() : nullptr;
    if(gameInstance != nullptr){
        if(const ULocalizationService* localization = gameInstance->GetSubsystem<ULocalizationService>()){
            text = localization->getReasonText(reason);
        }
    }
    UE_LOG(LogAwakenInteraction, Log, TEXT("reason line: %s (%s)"), *text.ToString(), *reason.ToString());
#if !UE_BUILD_SHIPPING
    if(GEngine != nullptr && settings != nullptr){
        // One key per component, so a new line replaces the old one.
        GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), settings->reasonLineSeconds, FColor::Yellow, text.ToString());
    }
#endif
}

// ---------------------------------------------------------------------------------------------------------------------------
// Locks (B18 3.7)
// ---------------------------------------------------------------------------------------------------------------------------

void UInteractionComponent::acquireLock(AActor* target){
    const E_InteractionLockPolicy policy = IBPI_Interactable::Execute_getLockPolicy(target);
    if(policy == E_InteractionLockPolicy::None){
        return;
    }
    UInteractionLockComponent* lock = findLock(target);
    if(lock == nullptr){
        return;   // validateRequest already warned
    }
    lock->acquire(GetOwner(), policy);
    lockedTargets.AddUnique(target);
    updateWatch();
}

void UInteractionComponent::releaseIfPressOnly(TWeakObjectPtr<AActor> target){
    // A Press object frees its lock right after the use, unless it keeps it (a station): then its own framework calls releaseLock.
    AActor* actor = target.Get();
    if(actor == nullptr){
        return;
    }
    const UInteractionLockComponent* lock = findLock(actor);
    if(lock != nullptr && lock->keepsLockUntilReleased){
        return;
    }
    releaseLock(actor);
}

void UInteractionComponent::releaseLock(AActor* target){
    if(target == nullptr){
        return;
    }
    if(UInteractionLockComponent* lock = findLock(target)){
        lock->release(GetOwner());
    }
    lockedTargets.RemoveAll([target](const TWeakObjectPtr<AActor>& entry){
        return !entry.IsValid() || entry.Get() == target;
    });
    updateWatch();
}

void UInteractionComponent::releaseLocksFor(AActor* player){
    AActor* holder = player != nullptr ? player : GetOwner();
    const TArray<TWeakObjectPtr<AActor>> snapshot = lockedTargets;
    lockedTargets.Reset();
    for(const TWeakObjectPtr<AActor>& entry : snapshot){
        if(AActor* target = entry.Get()){
            if(UInteractionLockComponent* lock = findLock(target)){
                lock->release(holder);
            }
        }
    }
    updateWatch();
}

void UInteractionComponent::releaseLocksExceptEscapeTarget(){
    // Downed or dead frees every lock, except the one an Escape hold stands on: a Restrained player is Incapacitated by design.
    AActor* keep = (hasHold() && isEscapeHold()) ? activeHold.target.Get() : nullptr;
    const TArray<TWeakObjectPtr<AActor>> snapshot = lockedTargets;
    for(const TWeakObjectPtr<AActor>& entry : snapshot){
        AActor* target = entry.Get();
        if(target != nullptr && target != keep){
            releaseLock(target);
        }
    }
}

void UInteractionComponent::notifyTargetDestroyed(AActor* target){
    if(hasHold() && activeHold.target.Get() == target){
        cancelHold(E_InteractionHoldCancelReason::InUse);
    }
    lockedTargets.RemoveAll([target](const TWeakObjectPtr<AActor>& entry){
        return !entry.IsValid() || entry.Get() == target;
    });
    updateWatch();
}

void UInteractionComponent::updateWatch(){
    // One watch timer at lockWatchInterval, ONLY while a hold runs or a lock is held (Doc02 5). A hold on a None object has no
    // lock, so the hold itself keeps the watch running.
    UWorld* world = GetWorld();
    const AActor* owner = GetOwner();
    if(world == nullptr || owner == nullptr || settings == nullptr){
        return;
    }
    FTimerManager& timers = world->GetTimerManager();
    const bool isNeeded = owner->HasAuthority() && (hasHold() || !lockedTargets.IsEmpty());
    if(isNeeded){
        if(!timers.IsTimerActive(watchTimer)){
            timers.SetTimer(watchTimer, this, &UInteractionComponent::watchTick, FMath::Max(settings->lockWatchInterval, UE_KINDA_SMALL_NUMBER), true);
        }
    }else{
        timers.ClearTimer(watchTimer);
    }
}

void UInteractionComponent::watchTick(){
    if(GetOwner() == nullptr || !ensureSettings()){
        return;
    }
    const FVector eye = getHostEyeLocation();

    // The running hold: range and line (B18 3.8), re-running check 3 of the server checks.
    if(hasHold()){
        AActor* holdTarget = activeHold.target.Get();
        if(holdTarget == nullptr){
            cancelHold(E_InteractionHoldCancelReason::InUse);   // the target became invalid
        }else{
            FVector closest = FVector::ZeroVector;
            const float distance = measureDistance(holdTarget, eye, closest);
            if(!FInteractionRules::isWithinRange(distance, settings->interactionRange, settings->rangeSlack)){
                cancelHold(E_InteractionHoldCancelReason::OutOfRange);
            }else if(!hasClearLine(eye, closest, holdTarget)){
                cancelHold(E_InteractionHoldCancelReason::NoLineOfSight);
            }
        }
    }

    // Every locked target: the range part only; the lock is freed when the player has walked away.
    lockedTargets.RemoveAll([](const TWeakObjectPtr<AActor>& entry){
        return !entry.IsValid();
    });
    const TArray<TWeakObjectPtr<AActor>> snapshot = lockedTargets;
    for(const TWeakObjectPtr<AActor>& entry : snapshot){
        AActor* target = entry.Get();
        if(target == nullptr){
            continue;
        }
        FVector closest = FVector::ZeroVector;
        const float distance = measureDistance(target, eye, closest);
        if(!FInteractionRules::isWithinRange(distance, settings->interactionRange, settings->rangeSlack)){
            UE_LOG(LogAwakenInteraction, Log, TEXT("lock freed: %s walked out of range of %s"), *GetNameSafe(GetOwner()), *GetNameSafe(target));
            releaseLock(target);
        }
    }
    updateWatch();
}

// ---------------------------------------------------------------------------------------------------------------------------
// Holds (B18 3.6, 3.8). The server starts the hold and the server completes it; the client's progress ring is a display.
// ---------------------------------------------------------------------------------------------------------------------------

bool UInteractionComponent::hasHold() const{
    // IsExplicitlyNull is false for a target that was set and has since been destroyed, so a stale hold is still found and ended.
    return !activeHold.target.IsExplicitlyNull();
}

bool UInteractionComponent::isEscapeHold() const{
    return activeHold.interactionTag == T_Interaction_Escape;
}

void UInteractionComponent::beginHold(AActor* target){
    UWorld* world = GetWorld();
    AActor* owner = GetOwner();
    if(world == nullptr || owner == nullptr){
        return;
    }
    if(!target->GetClass()->ImplementsInterface(UBPI_InteractableHoldToActivate::StaticClass())){
        UE_LOG(LogAwakenInteraction, Error, TEXT("%s is a Hold object but does not implement IBPI_InteractableHoldToActivate."), *GetNameSafe(target));
        clientInteractRefused(target, T_Interaction_FailReason_NotAllowedInState);
        return;
    }
    // 1: canActivate once now (B11 3.4).
    FGameplayTag failReason;
    if(!IBPI_InteractableHoldToActivate::Execute_canActivate(target, owner, failReason)){
        if(!failReason.IsValid()){
            failReason = T_Interaction_FailReason_NotAllowedInState;
        }
        clientInteractRefused(target, failReason);
        return;
    }
    // 2: the lock. A None object has no lock, and the hold still runs.
    acquireLock(target);
    // 3: the record. World time on the host: a short gameplay timer that does not touch GameTime (Doc02 19.4).
    const float duration = FMath::Max(IBPI_InteractableHoldToActivate::Execute_getHoldDuration(target), 0.0f);
    activeHold.target = target;
    activeHold.interactionTag = IBPI_Interactable::Execute_getInteractionTag(target);
    activeHold.endTime = world->GetTimeSeconds() + duration;
    activeHold.isDue = false;
    // 4: one timer for the duration, no tick, and make sure the watch runs. A zero rate would clear the timer.
    world->GetTimerManager().SetTimer(holdTimer, this, &UInteractionComponent::dueHold, FMath::Max(duration, UE_KINDA_SMALL_NUMBER), false);
    updateWatch();
    // 5: server broadcast, and the owning client raises it locally and starts its progress clock.
    UE_LOG(LogAwakenInteraction, Log, TEXT("hold started: %s on %s (%.2f s, %s)"), *GetNameSafe(owner), *GetNameSafe(target), duration, *activeHold.interactionTag.ToString());
    raiseHoldMessageOnServer(T_Interaction_HoldStarted, target, E_InteractionHoldCancelReason::Released);
    clientHoldStarted(target, duration);
}

void UInteractionComponent::dueHold(){
    if(!hasHold()){
        return;
    }
    // Does NOT complete the hold. Unreal's timer manager runs before the gameplay tick groups, so a hit applied later in this same
    // frame would arrive after a hold completed inside the timer callback and a lethal hit could never cancel it. Completion runs
    // one tick later, so that frame's damage and state changes arrive first (guide Step 7).
    activeHold.isDue = true;
    if(UWorld* world = GetWorld()){
        finishTimer = world->GetTimerManager().SetTimerForNextTick(this, &UInteractionComponent::finishHold);
    }
}

void UInteractionComponent::finishHold(){
    if(!activeHold.isDue){
        return;   // a stale callback after a cancel does nothing
    }
    AActor* owner = GetOwner();
    AActor* target = activeHold.target.Get();

    // Same-frame order (Step 9): damage, then Downed or death, then disconnect or scene start (each has already cancelled the
    // hold through its own handler), then completion.
    E_InteractionHoldCancelReason reason = E_InteractionHoldCancelReason::Downed;
    if(shouldCancelForState(reason)){
        cancelHold(reason);
        return;
    }
    // The target must still be valid and, for a locking object, the lock must still be this player's.
    if(target == nullptr){
        cancelHold(E_InteractionHoldCancelReason::InUse);
        return;
    }
    const UInteractionLockComponent* lock = findLock(target);
    if(lock != nullptr && IBPI_Interactable::Execute_getLockPolicy(target) == E_InteractionLockPolicy::Exclusive && !lock->isHeldBy(owner)){
        cancelHold(E_InteractionHoldCancelReason::InUse);
        return;
    }
    // canActivate AGAIN: a change during the hold, such as an enemy becoming aware during Sleep, still refuses it (B18 3.6).
    FGameplayTag failReason;
    if(!IBPI_InteractableHoldToActivate::Execute_canActivate(target, owner, failReason)){
        if(!failReason.IsValid()){
            failReason = T_Interaction_FailReason_NotAllowedInState;
        }
        clientInteractRefused(target, failReason);
        cancelHold(E_InteractionHoldCancelReason::Refused);
        return;
    }

    // Complete. Read the noise and the tag first: the object may destroy itself (harvest consumes it).
    const FPendingNoise pendingNoise = capturePendingNoise(target);
    const FGameplayTag completedTag = activeHold.interactionTag;
    IBPI_InteractableHoldToActivate::Execute_onActivationComplete(target, owner);
    UE_LOG(LogAwakenInteraction, Log, TEXT("hold completed: %s on %s (%s)"), *GetNameSafe(owner), *GetNameSafe(target), *completedTag.ToString());

    // Server only. Doc B03 (Sleep) and Doc B08 (Harvest) listen on the host.
    FS_InteractionCompletedMessage message;
    message.interactionTag = completedTag;
    message.instigator = owner;
    raiseMessage(this, T_Interaction_HoldToActivateCompleted, message);

    reportNoise(pendingNoise);
    clearHoldState();
    clientHoldCompleted(target);
}

void UInteractionComponent::clearHoldState(){
    TWeakObjectPtr<AActor> holdTarget = activeHold.target;
    if(UWorld* world = GetWorld()){
        FTimerManager& timers = world->GetTimerManager();
        timers.ClearTimer(holdTimer);
        timers.ClearTimer(finishTimer);
    }
    activeHold = FS_ActiveHold();
    if(AActor* target = holdTarget.Get()){
        releaseLock(target);
    }else{
        lockedTargets.RemoveAll([](const TWeakObjectPtr<AActor>& entry){
            return !entry.IsValid();
        });
        updateWatch();
    }
}

void UInteractionComponent::cancelHold(E_InteractionHoldCancelReason reason){
    if(!hasHold()){
        return;
    }
    AActor* target = activeHold.target.Get();
    UE_LOG(LogAwakenInteraction, Log, TEXT("hold cancelled: %s on %s (reason %d)"), *GetNameSafe(GetOwner()), *GetNameSafe(target), static_cast<int32>(reason));
    // Stops the timer, releases the lock, clears the record. A cancelled hold keeps no progress: the next try starts from zero.
    clearHoldState();
    raiseHoldMessageOnServer(T_Interaction_HoldCancelled, target, reason);
    clientHoldCancelled(target, reason);
}

bool UInteractionComponent::shouldCancelForState(E_InteractionHoldCancelReason& reason) const{
    if(isEscapeHold()){
        return false;
    }
    const UAbilitySystemComponent* abilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
    if(abilitySystem == nullptr){
        return false;
    }
    // HP at 0 stands in for the death message until Doc B02 provides one. GetNumericAttribute reads 0 when the set is missing,
    // which would look like death, so the attribute set is checked first.
    const FGameplayAttribute hpAttribute = UAS_CoreVitals::currentHPAttribute();
    const bool isDead = abilitySystem->HasAttributeSetForAttribute(hpAttribute) && abilitySystem->GetNumericAttribute(hpAttribute) <= 0.0f;
    if(isDead || abilitySystem->HasMatchingGameplayTag(T_Player_State_Incapacitated)){
        reason = E_InteractionHoldCancelReason::Downed;
        return true;
    }
    return false;
}

void UInteractionComponent::onDamaged(){
    if(!hasHold() || isEscapeHold()){
        return;
    }
    // A non-lethal hit in the completion frame never cancels the hold, even one that lands before the duration timer has fired:
    // the end-time check covers that (Doc02 19.9). A hit in an earlier frame cancels normally.
    const double now = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0;
    if(FInteractionRules::shouldCancelOnDamage(isEscapeHold(), activeHold.isDue, now, activeHold.endTime)){
        cancelHold(E_InteractionHoldCancelReason::Damaged);
    }
}

void UInteractionComponent::onHPChanged(const FOnAttributeChangeData& data){
    if(data.NewValue < data.OldValue){
        onDamaged();
    }
}

void UInteractionComponent::onIncapacitatedChanged(const FGameplayTag tag, int32 count){
    if(count <= 0){
        return;
    }
    // A Restrained player is Incapacitated by design, so Escape is excluded inside shouldCancelForState.
    E_InteractionHoldCancelReason reason = E_InteractionHoldCancelReason::Downed;
    if(hasHold() && shouldCancelForState(reason)){
        cancelHold(reason);
    }
    releaseLocksExceptEscapeTarget();
}

void UInteractionComponent::serverReleaseInteract_Implementation(){
    cancelHold(E_InteractionHoldCancelReason::Released);
}

// ---------------------------------------------------------------------------------------------------------------------------
// Client reactions to the hold
// ---------------------------------------------------------------------------------------------------------------------------

void UInteractionComponent::clientHoldStarted_Implementation(AActor* target, float durationSeconds){
    localHoldTarget = target;
    localHoldDuration = durationSeconds;
    localHoldStartTime = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0;
    raiseHoldMessage(T_Interaction_HoldStarted, target, E_InteractionHoldCancelReason::Released);
}

void UInteractionComponent::clientHoldCancelled_Implementation(AActor* target, E_InteractionHoldCancelReason reason){
    localHoldTarget.Reset();
    localHoldDuration = 0.0f;
    releaseExpected = false;
    raiseHoldMessage(T_Interaction_HoldCancelled, target, reason);
}

void UInteractionComponent::clientHoldCompleted_Implementation(AActor* target){
    localHoldTarget.Reset();
    localHoldDuration = 0.0f;
    releaseExpected = false;
}

// ---------------------------------------------------------------------------------------------------------------------------
// Noise (B18 3.9) and messages
// ---------------------------------------------------------------------------------------------------------------------------

UInteractionComponent::FPendingNoise UInteractionComponent::capturePendingNoise(AActor* target) const{
    FPendingNoise pending;
    FS_InteractionNoise noise;
    if(target != nullptr && IBPI_Interactable::Execute_getNoise(target, noise)){
        pending.hasNoise = true;
        pending.noise = noise;
        pending.location = target->GetActorLocation();
    }
    return pending;
}

void UInteractionComponent::reportNoise(const FPendingNoise& pending) const{
    // Server only, after all state changes, so a refused or cancelled use never makes noise.
    const AActor* owner = GetOwner();
    if(!pending.hasNoise || owner == nullptr || !owner->HasAuthority()){
        return;
    }
    reportHearingStimulus(pending.location, pending.noise.loudness, pending.noise.noiseTag, GetOwner());
}

void UInteractionComponent::reportHearingStimulus(const FVector& location, float loudness, const FGameplayTag& noiseTag, AActor* instigator){
    // PLACEHOLDER: Doc B12's hearing entry point does not exist yet. Confirm the real name in Doc B12 before compiling against it.
    UE_LOG(LogAwakenInteraction, Log, TEXT("hearing stimulus (PLACEHOLDER for Doc B12): %s at %s, loudness %.2f, tag %s"), *GetNameSafe(instigator), *location.ToCompactString(), loudness, *noiseTag.ToString());
}

void UInteractionComponent::raiseHoldMessage(const FGameplayTag& channel, AActor* target, E_InteractionHoldCancelReason reason) const{
    FS_InteractionHoldMessage message;
    message.player = GetOwner();
    message.target = target;
    message.cancelReason = reason;
    raiseMessage(this, channel, message);
}

void UInteractionComponent::raiseHoldMessageOnServer(const FGameplayTag& channel, AActor* target, E_InteractionHoldCancelReason reason) const{
    // "Server, then local to the owning player" (B18 section 6). On a machine where the owner is local (offline, or the host's own
    // pawn) the client RPC already raises it, so raising it here too would deliver it twice.
    if(isLocalOwner()){
        return;
    }
    raiseHoldMessage(channel, target, reason);
}
