#include "CameraFramework/ScreenEffectComponent.h"

#include "Camera/CameraComponent.h"
#include "CameraFramework/CameraFramework.h"
#include "CameraFramework/CameraMessageBindings.h"
#include "CameraFramework/DA_FirstPersonCameraConfig.h"
#include "CameraFramework/FirstPersonCameraComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

namespace{
    // The start-up snapshot waits for state that arrives late on a client. These bound the wait so the timer can never run for ever.
    constexpr float SnapshotRetrySeconds = 0.25f;
    constexpr int32 SnapshotMaxAttempts = 40;
}

UScreenEffectComponent::UScreenEffectComponent(){
    // No tick at all: blending runs on a short-lived timer that exists only while something is blending (step 9 item 4).
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(false);
}

// ---------------------------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------------------------

void UScreenEffectComponent::BeginPlay(){
    Super::BeginPlay();

    // On a listen server the other players' controllers exist too, so the component switches itself off unless the controller is local.
    const APlayerController* controller = Cast<APlayerController>(GetOwner());
    if(controller == nullptr || !controller->IsLocalController()){
        UE_LOG(LogAwakenCamera, Verbose, TEXT("Local: screen-effect component off, the controller is not local"));
        return;
    }
    if(cameraConfig == nullptr || screenEffectMaterial == nullptr){
        UE_LOG(LogAwakenCamera, Error, TEXT("Local: screen-effect component off, cameraConfig or screenEffectMaterial is not set on the controller Blueprint"));
        return;
    }
    active = true;

    // One post-process material, one dynamic instance (step 9 item 1). Every channel starts at rest.
    effectMaterial = UMaterialInstanceDynamic::Create(screenEffectMaterial, this);
    for(const TPair<FGameplayTag, FName>& entry : cameraConfig->materialParameterByEffect){
        effectMaterial->SetScalarParameterValue(entry.Value, 0.0f);
    }
    refreshCameraAttachment();
    loadMappingRows();
}

void UScreenEffectComponent::EndPlay(const EEndPlayReason::Type reason){
    // Unregister every listener and clear every timer (step 10 item 7).
    for(FGameplayMessageListenerHandle& handle : listenerHandles){
        handle.Unregister();
    }
    listenerHandles.Reset();
    UWorld* world = GetWorld();
    if(world != nullptr){
        FTimerManager& timers = world->GetTimerManager();
        timers.ClearTimer(blendTimer);
        timers.ClearTimer(snapshotTimer);
        for(TPair<const UDA_ScreenEffectMapping*, FTimerHandle>& pulse : pulseTimers){
            timers.ClearTimer(pulse.Value);
        }
    }
    pulseTimers.Reset();
    rowsHandle.Reset();
    Super::EndPlay(reason);
}

APawn* UScreenEffectComponent::getLocalPawn() const{
    const APlayerController* controller = Cast<APlayerController>(GetOwner());
    return controller != nullptr ? controller->GetPawn() : nullptr;
}

UCameraComponent* UScreenEffectComponent::getActiveCamera() const{
    const APawn* pawn = getLocalPawn();
    return pawn != nullptr ? pawn->FindComponentByClass<UFirstPersonCameraComponent>() : nullptr;
}

const UDA_FirstPersonCameraConfig* UScreenEffectComponent::getCameraConfig() const{
    return cameraConfig;
}

// ---------------------------------------------------------------------------------------------
// Settings and material attachment
// ---------------------------------------------------------------------------------------------

float UScreenEffectComponent::getEffectiveIntensity() const{
    const float floorValue = cameraConfig != nullptr ? cameraConfig->screenEffectIntensityFloor : 0.0f;
    return FMath::Max(comfortSettings.screenEffectIntensity, floorValue);
}

void UScreenEffectComponent::setComfortSettings(const FS_CameraComfortSettings& newSettings){
    comfortSettings = newSettings;
    // The unscaled values are kept in the state, so a changed setting only re-pushes (step 9 item 5).
    repushAll();
    UCameraComponent* camera = getActiveCamera();
    if(UFirstPersonCameraComponent* rig = Cast<UFirstPersonCameraComponent>(camera)){
        rig->applyControllerSettings();
    }
}

void UScreenEffectComponent::refreshCameraAttachment(){
    if(!active || effectMaterial == nullptr){
        return;
    }
    UCameraComponent* camera = getActiveCamera();
    if(camera != nullptr){
        camera->AddOrUpdateBlendable(effectMaterial.Get(), FMath::Max(blendableWeight, 0.0f));
    }
}

void UScreenEffectComponent::updateBlendableWeight(){
    if(!active || effectMaterial == nullptr || held){
        return;
    }
    // The material's full-screen pass runs every frame while it is in the stack, even with every channel at 0.
    // So the weight is 0 when every parameter is at rest, and 1 as soon as one leaves rest (step 9 item 4).
    bool anyActive = false;
    for(const TPair<FGameplayTag, FS_ScreenEffectParameterState>& entry : parameters){
        if(!FMath::IsNearlyZero(entry.Value.current) || !FMath::IsNearlyZero(entry.Value.target)){
            anyActive = true;
            break;
        }
    }
    const float weight = anyActive ? 1.0f : 0.0f;
    if(FMath::IsNearlyEqual(weight, blendableWeight)){
        return;
    }
    blendableWeight = weight;
    UCameraComponent* camera = getActiveCamera();
    if(camera != nullptr){
        camera->AddOrUpdateBlendable(effectMaterial.Get(), weight);
    }
}

void UScreenEffectComponent::pushParameter(const FS_ScreenEffectParameterState& state){
    if(effectMaterial == nullptr || cameraConfig == nullptr){
        return;
    }
    const FName* parameterName = cameraConfig->materialParameterByEffect.Find(state.effectTag);
    if(parameterName == nullptr){
        return;
    }
    // The intensity is applied here, on output. A channel at rest stays at 0, and an active channel is never scaled down to nothing.
    effectMaterial->SetScalarParameterValue(*parameterName, state.current * getEffectiveIntensity());
}

void UScreenEffectComponent::repushAll(){
    if(!active || held){
        return;
    }
    for(const TPair<FGameplayTag, FS_ScreenEffectParameterState>& entry : parameters){
        pushParameter(entry.Value);
    }
}

// ---------------------------------------------------------------------------------------------
// Step 9: parameters and blending
// ---------------------------------------------------------------------------------------------

void UScreenEffectComponent::setEffectParameter(FGameplayTag effectTag, float targetValue, float blendTime){
    if(!active){
        return;
    }
    if(cameraConfig == nullptr || !cameraConfig->materialParameterByEffect.Contains(effectTag)){
        UE_LOG(LogAwakenCamera, Warning, TEXT("Local: effect %s refused, the config has no material parameter for it"), *effectTag.ToString());
        return;
    }
    FS_ScreenEffectParameterState& state = parameters.FindOrAdd(effectTag);
    state.effectTag = effectTag;
    state.startValue = state.current;
    state.target = targetValue;
    state.blendTime = FMath::Max(blendTime, 0.0f);
    state.elapsed = 0.0f;
    if(held){
        // The target is recorded; the pushed value does not change until the hold is released.
        return;
    }
    if(state.blendTime <= 0.0f){
        state.current = state.target;
        state.startValue = state.current;
        pushParameter(state);
    }else{
        armBlendTimer();
    }
    updateBlendableWeight();
}

void UScreenEffectComponent::clearEffectParameter(FGameplayTag effectTag, float blendTime){
    // Rest is 0 for every channel here (step 9 item 6).
    setEffectParameter(effectTag, 0.0f, blendTime);
}

void UScreenEffectComponent::clearAllEffectParameters(float blendTime){
    TArray<FGameplayTag> tags;
    parameters.GetKeys(tags);
    for(const FGameplayTag& tag : tags){
        clearEffectParameter(tag, blendTime);
    }
}

void UScreenEffectComponent::armBlendTimer(){
    UWorld* world = GetWorld();
    if(world == nullptr){
        return;
    }
    FTimerManager& timers = world->GetTimerManager();
    if(!timers.IsTimerActive(blendTimer)){
        blendTimer = timers.SetTimerForNextTick(this, &UScreenEffectComponent::stepBlend);
    }
}

bool UScreenEffectComponent::isBlendTimerActive() const{
    const UWorld* world = GetWorld();
    return world != nullptr && world->GetTimerManager().IsTimerActive(blendTimer);
}

void UScreenEffectComponent::stepBlend(){
    UWorld* world = GetWorld();
    if(world == nullptr || held){
        return;
    }
    const float deltaTime = world->GetDeltaSeconds();
    bool stillBlending = false;
    for(TPair<FGameplayTag, FS_ScreenEffectParameterState>& entry : parameters){
        FS_ScreenEffectParameterState& state = entry.Value;
        if(FMath::IsNearlyEqual(state.current, state.target)){
            state.current = state.target;
            continue;
        }
        state.elapsed += deltaTime;
        const float alpha = state.blendTime > 0.0f ? FMath::Clamp(state.elapsed / state.blendTime, 0.0f, 1.0f) : 1.0f;
        state.current = FMath::Lerp(state.startValue, state.target, alpha);
        if(alpha >= 1.0f){
            state.current = state.target;
        }else{
            stillBlending = true;
        }
        pushParameter(state);
        UE_LOG(LogAwakenCamera, Verbose, TEXT("Local: blend step %s %.3f -> %.3f"), *state.effectTag.ToString(), state.current, state.target);
    }
    updateBlendableWeight();
    if(stillBlending){
        // Set the handle again without asking IsTimerActive: inside its own callback the timer still counts as active.
        // When everything has arrived the timer is not re-armed, so an idle screen costs nothing on the CPU (Doc02 5).
        blendTimer = world->GetTimerManager().SetTimerForNextTick(this, &UScreenEffectComponent::stepBlend);
    }
}

void UScreenEffectComponent::setHold(bool hold){
    if(!active || hold == held){
        return;
    }
    held = hold;
    UWorld* world = GetWorld();
    if(hold){
        // Freeze: the timer stops and the material keeps the last pushed values.
        if(world != nullptr){
            world->GetTimerManager().ClearTimer(blendTimer);
        }
        UE_LOG(LogAwakenCamera, Log, TEXT("Local: screen-effect hold started"));
        return;
    }
    // Release: every parameter that is not at its recorded target blends there with its own blend time.
    bool needsBlend = false;
    for(TPair<FGameplayTag, FS_ScreenEffectParameterState>& entry : parameters){
        FS_ScreenEffectParameterState& state = entry.Value;
        if(!FMath::IsNearlyEqual(state.current, state.target)){
            state.startValue = state.current;
            state.elapsed = 0.0f;
            needsBlend = true;
        }
    }
    repushAll();
    updateBlendableWeight();
    if(needsBlend){
        armBlendTimer();
    }
    UE_LOG(LogAwakenCamera, Log, TEXT("Local: screen-effect hold released"));
}

// ---------------------------------------------------------------------------------------------
// Step 10: drive the parameters from the mapping
// ---------------------------------------------------------------------------------------------

void UScreenEffectComponent::loadMappingRows(){
    UAssetManager& manager = UAssetManager::Get();
    TArray<FPrimaryAssetId> ids;
    manager.GetPrimaryAssetIdList(UDA_ScreenEffectMapping::getAssetType(), ids);
    if(ids.Num() == 0){
        UE_LOG(LogAwakenCamera, Warning, TEXT("Local: no ScreenEffectMapping rows found; check that the Asset Manager scans the primary asset type ScreenEffectMapping"));
    }
    rowsHandle = manager.LoadPrimaryAssets(ids, TArray<FName>(), FStreamableDelegate::CreateUObject(this, &UScreenEffectComponent::onRowsLoaded));
    // If everything was already loaded there may be nothing to wait for. onRowsLoaded runs once, whichever path gets there first.
    if(!rowsHandle.IsValid() || rowsHandle->HasLoadCompleted()){
        onRowsLoaded();
    }
}

void UScreenEffectComponent::onRowsLoaded(){
    if(rowsReady){
        return;
    }
    UAssetManager& manager = UAssetManager::Get();
    TArray<FPrimaryAssetId> ids;
    manager.GetPrimaryAssetIdList(UDA_ScreenEffectMapping::getAssetType(), ids);
    for(const FPrimaryAssetId& id : ids){
        UDA_ScreenEffectMapping* row = manager.GetPrimaryAssetObject<UDA_ScreenEffectMapping>(id);
        if(row == nullptr){
            UE_LOG(LogAwakenCamera, Warning, TEXT("Local: mapping row %s did not load"), *id.ToString());
            continue;
        }
        loadedRows.Add(row);
        rowsBySource.FindOrAdd(row->sourceMessageTag).Add(row);
        UE_LOG(LogAwakenCamera, Log, TEXT("Local: mapping row loaded %s"), *row->GetName());
    }
    rowsReady = true;

    // One listener per distinct source tag (step 10 item 2).
    TArray<FGameplayTag> sourceTags;
    rowsBySource.GetKeys(sourceTags);
    CameraMessageBindings::bindScreenEffects(this, sourceTags, listenerHandles);

    startSnapshot();
}

void UScreenEffectComponent::handleSourceValue(const FGameplayTag& sourceTag, const FGameplayTag& filterTag, float sourceValue, const FVector* eventLocation, bool snap){
    if(!active || !rowsReady){
        return;
    }
    const TArray<const UDA_ScreenEffectMapping*>* rows = rowsBySource.Find(sourceTag);
    if(rows == nullptr){
        return;
    }
    for(const UDA_ScreenEffectMapping* row : *rows){
        // The filter, for example the Illness meter or the hit event tag. A child tag matches its parent.
        if(row->sourceFilterTag.IsValid() && (!filterTag.IsValid() || !filterTag.MatchesTag(row->sourceFilterTag))){
            continue;
        }
        // A cosmetic event has no victim field (B21 A.1), so an event farther than the row's radius from the local pawn is ignored.
        if(eventLocation != nullptr && row->maxPawnDistance > 0.0f){
            const APawn* pawn = getLocalPawn();
            if(pawn == nullptr || FVector::Dist(pawn->GetActorLocation(), *eventLocation) > row->maxPawnDistance){
                continue;
            }
        }
        if(row->valueCurve == nullptr){
            UE_LOG(LogAwakenCamera, Warning, TEXT("Local: mapping row %s has no value curve and is skipped"), *row->GetName());
            continue;
        }
        // Source value -> curve -> clamp to 0..1 -> the row's current target (step 10 item 3).
        const float target = FMath::Clamp(row->valueCurve->GetFloatValue(sourceValue), 0.0f, 1.0f);
        rowTargets.Add(row, target);
        resolveEffect(row->effectTag, row->defaultBlendTime, snap);
        if(row->pulseSeconds > 0.0f && target > 0.0f && !snap){
            schedulePulseEnd(row);
        }
    }
}

void UScreenEffectComponent::resolveEffect(const FGameplayTag& effectTag, float releaseBlendTime, bool snapNow){
    const UDA_ScreenEffectMapping* winner = nullptr;
    float winnerTarget = 0.0f;
    for(const TPair<const UDA_ScreenEffectMapping*, float>& entry : rowTargets){
        const UDA_ScreenEffectMapping* row = entry.Key;
        // Only rows holding a target above zero compete.
        if(row->effectTag != effectTag || entry.Value <= 0.0f){
            continue;
        }
        // The higher priority wins. On a tie the larger target wins and its blend time is used (spec 3.4).
        const bool higherPriority = winner == nullptr || row->priority > winner->priority;
        const bool tieWithLarger = winner != nullptr && row->priority == winner->priority && entry.Value > winnerTarget;
        if(higherPriority || tieWithLarger){
            winner = row;
            winnerTarget = entry.Value;
        }
    }

    const FS_ScreenEffectParameterState* before = parameters.Find(effectTag);
    const float previousTarget = before != nullptr ? before->target : 0.0f;

    if(winner != nullptr){
        setEffectParameter(effectTag, winnerTarget, snapNow ? 0.0f : winner->defaultBlendTime);
        if(FS_ScreenEffectParameterState* state = parameters.Find(effectTag)){
            state->winningRowName = winner->GetFName();
            state->winningPriority = winner->priority;
        }
        if(!FMath::IsNearlyEqual(previousTarget, winnerTarget)){
            UE_LOG(LogAwakenCamera, Log, TEXT("Local: effect %s target %.2f, winning row %s"), *effectTag.ToString(), winnerTarget, *winner->GetName());
        }
    }else{
        // No row holds a target above zero any more: back to rest with the blend time of the row that just dropped to zero.
        clearEffectParameter(effectTag, snapNow ? 0.0f : releaseBlendTime);
        if(FS_ScreenEffectParameterState* state = parameters.Find(effectTag)){
            state->winningRowName = NAME_None;
            state->winningPriority = 0;
        }
        if(!FMath::IsNearlyEqual(previousTarget, 0.0f)){
            UE_LOG(LogAwakenCamera, Log, TEXT("Local: effect %s target 0.00, no row holds a target"), *effectTag.ToString());
        }
    }
}

void UScreenEffectComponent::schedulePulseEnd(const UDA_ScreenEffectMapping* row){
    UWorld* world = GetWorld();
    if(world == nullptr){
        return;
    }
    // A new event restarts the pulse: SetTimer on an existing handle replaces its timer.
    FTimerHandle& handle = pulseTimers.FindOrAdd(row);
    world->GetTimerManager().SetTimer(handle, FTimerDelegate::CreateUObject(this, &UScreenEffectComponent::onPulseEnded, row), row->pulseSeconds, false);
}

void UScreenEffectComponent::onPulseEnded(const UDA_ScreenEffectMapping* row){
    rowTargets.Add(row, 0.0f);
    resolveEffect(row->effectTag, row->defaultBlendTime, false);
}

// ---------------------------------------------------------------------------------------------
// Step 10 item 6: start-up snapshot
// ---------------------------------------------------------------------------------------------

void UScreenEffectComponent::runStartupSnapshot(){
    if(active && rowsReady){
        startSnapshot();
    }
}

void UScreenEffectComponent::startSnapshot(){
    UWorld* world = GetWorld();
    if(world == nullptr){
        return;
    }
    world->GetTimerManager().ClearTimer(snapshotTimer);
    snapshotAttempts = 0;
    if(!trySnapshot()){
        // The state arrives late on a client (PlayerState and owner-only meters replicate after the controller starts).
        // Retry from a short-lived timer until the state is valid, run it once, and stop.
        world->GetTimerManager().SetTimer(snapshotTimer, this, &UScreenEffectComponent::onSnapshotTimer, SnapshotRetrySeconds, true);
    }
}

void UScreenEffectComponent::onSnapshotTimer(){
    ++snapshotAttempts;
    const bool done = trySnapshot();
    if(done || snapshotAttempts >= SnapshotMaxAttempts){
        UWorld* world = GetWorld();
        if(world != nullptr){
            world->GetTimerManager().ClearTimer(snapshotTimer);
        }
        if(!done){
            UE_LOG(LogAwakenCamera, Warning, TEXT("Local: start-up snapshot gave up, the state never became valid"));
        }
    }
}

bool UScreenEffectComponent::trySnapshot(){
    // The reads of other frameworks' state live in CameraMessageBindings.cpp, the one file that knows their types.
    const bool ready = CameraMessageBindings::applyStartupSnapshot(this);
    if(ready){
        UE_LOG(LogAwakenCamera, Log, TEXT("Local: start-up snapshot applied"));
    }
    return ready;
}
