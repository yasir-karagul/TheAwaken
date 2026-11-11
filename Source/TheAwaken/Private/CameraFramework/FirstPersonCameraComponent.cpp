#include "CameraFramework/FirstPersonCameraComponent.h"

#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "CameraFramework/02Tags/T_CameraFramework.h"
#include "CameraFramework/CameraFramework.h"
#include "CameraFramework/CameraMessageBindings.h"
#include "CameraFramework/DA_FirstPersonCameraConfig.h"
#include "CameraFramework/ScreenEffectComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/AssetManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

UFirstPersonCameraComponent::UFirstPersonCameraComponent(){
    bUsePawnControlRotation = true;
    // The tick stays off. applyRecoil turns it on while a recoil is recovering (step 8).
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

// ---------------------------------------------------------------------------------------------
// Step 4: aim origin and direction, view
// ---------------------------------------------------------------------------------------------

FVector UFirstPersonCameraComponent::getAimTraceOrigin() const{
    // Deliberately no eye socket and no second reference point (spec 3.1).
    return GetComponentLocation();
}

FVector UFirstPersonCameraComponent::getAimTraceDirection() const{
    return (GetComponentQuat() * recoilOffset.Quaternion()).GetForwardVector();
}

void UFirstPersonCameraComponent::GetCameraView(float deltaTime, FMinimalViewInfo& desiredView){
    Super::GetCameraView(deltaTime, desiredView);
    // One rotation for the aim and for the view: a shake is added later by the camera manager and never moves the aim (spec 3.2).
    desiredView.Rotation = (desiredView.Rotation.Quaternion() * recoilOffset.Quaternion()).Rotator();
    if(nearClip > 0.0f){
        // CONFIRM in 5.8: the per-view near clip member and whether the renderer honours it (step 12 item 4).
        desiredView.PerspectiveNearClipPlane = nearClip;
    }
}

void UFirstPersonCameraComponent::attachToHeadSocket(USkeletalMeshComponent* bodyMesh){
    if(bodyMesh == nullptr || headSocketName.IsNone()){
        UE_LOG(LogAwakenCamera, Warning, TEXT("Local: camera attach refused, no body mesh or no head socket name"));
        return;
    }
    // Without scale, so a head bone that is hidden by scaling does not scale the camera (confirm in 5.8).
    AttachToComponent(bodyMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, headSocketName);
}

// ---------------------------------------------------------------------------------------------
// Step 4: hiding the owner's head
// ---------------------------------------------------------------------------------------------

void UFirstPersonCameraComponent::applyOwnerHeadHiding(USkeletalMeshComponent* bodyMesh, FName headBoneName){
    if(bodyMesh == nullptr || headBoneName.IsNone()){
        UE_LOG(LogAwakenCamera, Warning, TEXT("Local: head hiding refused, no body mesh or no head bone"));
        return;
    }
    clearOwnerHeadHiding();
    bodyMesh->HideBoneByName(headBoneName, PBO_None);
    hiddenHeadMesh = bodyMesh;
    hiddenHeadBone = headBoneName;
    UE_LOG(LogAwakenCamera, Log, TEXT("Local: owner head hidden"));
}

void UFirstPersonCameraComponent::applyOwnerHeadHidingFromConfig(USkeletalMeshComponent* bodyMesh){
    const UDA_FirstPersonCameraConfig* config = getConfig();
    if(config == nullptr){
        UE_LOG(LogAwakenCamera, Warning, TEXT("Local: head hiding refused, no camera config reachable from the controller"));
        return;
    }
    applyOwnerHeadHiding(bodyMesh, config->headBoneName);
}

void UFirstPersonCameraComponent::clearOwnerHeadHiding(){
    if(hiddenHeadMesh.IsValid()){
        hiddenHeadMesh->UnHideBoneByName(hiddenHeadBone);
        UE_LOG(LogAwakenCamera, Log, TEXT("Local: owner head shown"));
    }
    hiddenHeadMesh.Reset();
    hiddenHeadBone = NAME_None;
}

// ---------------------------------------------------------------------------------------------
// Step 5: settings from the controller
// ---------------------------------------------------------------------------------------------

UScreenEffectComponent* UFirstPersonCameraComponent::getControllerEffects() const{
    const APawn* pawn = Cast<APawn>(GetOwner());
    AController* controller = pawn != nullptr ? pawn->GetController() : nullptr;
    return controller != nullptr ? controller->FindComponentByClass<UScreenEffectComponent>() : nullptr;
}

const UDA_FirstPersonCameraConfig* UFirstPersonCameraComponent::getConfig() const{
    const UScreenEffectComponent* effects = getControllerEffects();
    return effects != nullptr ? effects->getCameraConfig() : nullptr;
}

APlayerController* UFirstPersonCameraComponent::getLocalController() const{
    const APawn* pawn = Cast<APawn>(GetOwner());
    APlayerController* controller = pawn != nullptr ? Cast<APlayerController>(pawn->GetController()) : nullptr;
    return (controller != nullptr && controller->IsLocalController()) ? controller : nullptr;
}

void UFirstPersonCameraComponent::applyControllerSettings(){
    UScreenEffectComponent* effects = getControllerEffects();
    if(effects == nullptr){
        UE_LOG(LogAwakenCamera, Warning, TEXT("Local: camera settings not applied, no screen-effect component on the controller"));
        return;
    }
    const UDA_FirstPersonCameraConfig* config = effects->getCameraConfig();
    comfortSettings = effects->getComfortSettings();
    nearClip = config != nullptr ? config->firstPersonNearClip : -1.0f;
    baseFov = comfortSettings.fieldOfView;
    if(viewMode != E_CameraViewMode::BagView){
        SetFieldOfView(baseFov);
    }
    // The new Character has a fresh component after a respawn, so the effect material is attached again here (step 9 item 3).
    effects->refreshCameraAttachment();
    preloadShakes();
}

// ---------------------------------------------------------------------------------------------
// Step 6: host-side eye check
// ---------------------------------------------------------------------------------------------

float UFirstPersonCameraComponent::getEyeHeight(const FGameplayTag& stance, const UDA_FirstPersonCameraConfig* config){
    if(config == nullptr){
        UE_LOG(LogAwakenCamera, Error, TEXT("Host: no camera config, eye height is 0"));
        return 0.0f;
    }
    const float* height = config->eyeHeightByStance.Find(stance);
    if(height == nullptr){
        UE_LOG(LogAwakenCamera, Error, TEXT("Host: no eye height for the stance, using Standing"));
        return config->eyeHeightByStance.FindRef(T_Camera_Stance_Standing.GetTag());
    }
    return *height;
}

FGameplayTag UFirstPersonCameraComponent::validateAimOrigin(const ACharacter* character, const FGameplayTag& stance, const FVector& clientOrigin, const UDA_FirstPersonCameraConfig* config){
    // Fails closed: with no character or no config the host cannot know where the eye is, so it must not accept the origin.
    if(character == nullptr || config == nullptr){
        UE_LOG(LogAwakenCamera, Warning, TEXT("Host: aim origin refused, no character or no camera config to check against (%s)"), *T_Camera_FailReason_OriginOutsideTolerance.GetTag().ToString());
        return T_Camera_FailReason_OriginOutsideTolerance.GetTag();
    }
    const FVector hostEye = character->GetActorLocation() + FVector(0.0f, 0.0f, getEyeHeight(stance, config));
    // Inclusive at the limit (Doc02 19, session 11 rule): a distance equal to the tolerance passes.
    if(FVector::Dist(hostEye, clientOrigin) > config->eyeTolerance){
        UE_LOG(LogAwakenCamera, Warning, TEXT("Host: aim origin refused, outside eye tolerance (%s)"), *T_Camera_FailReason_OriginOutsideTolerance.GetTag().ToString());
        return T_Camera_FailReason_OriginOutsideTolerance.GetTag();
    }
    return FGameplayTag();
}

// ---------------------------------------------------------------------------------------------
// Step 7: camera shake
// ---------------------------------------------------------------------------------------------

void UFirstPersonCameraComponent::setLocalListening(bool listening){
    if(listening == localListening){
        return;
    }
    if(listening){
        const APawn* pawn = Cast<APawn>(GetOwner());
        if(pawn == nullptr || !pawn->IsLocallyControlled()){
            UE_LOG(LogAwakenCamera, Warning, TEXT("Local: listening refused, the Character is not controlled by the local player"));
            return;
        }
        localListening = true;
        // Remote Characters never get here, so they register nothing.
        CameraMessageBindings::bindFirstPersonCamera(this, listenerHandles);
        preloadShakes();
        UE_LOG(LogAwakenCamera, Log, TEXT("Local: camera listening started"));
    }else{
        for(FGameplayMessageListenerHandle& handle : listenerHandles){
            handle.Unregister();
        }
        listenerHandles.Reset();
        localListening = false;
        UE_LOG(LogAwakenCamera, Log, TEXT("Local: camera listening stopped"));
    }
}

void UFirstPersonCameraComponent::preloadShakes(){
    const UDA_FirstPersonCameraConfig* config = getConfig();
    if(config == nullptr){
        return;
    }
    TArray<FSoftObjectPath> paths;
    for(const TPair<FGameplayTag, FS_CameraShakeEntry>& entry : config->shakeByTag){
        if(!entry.Value.shakeClass.IsNull()){
            paths.Add(entry.Value.shakeClass.ToSoftObjectPath());
        }
    }
    if(paths.Num() > 0){
        // The handle keeps the classes loaded, so the first shake does not hitch.
        shakeLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(paths);
    }
}

TSubclassOf<UCameraShakeBase> UFirstPersonCameraComponent::resolveShakeClass(const FS_CameraShakeEntry& entry){
    if(entry.shakeClass.IsNull()){
        return nullptr;
    }
    UClass* loaded = entry.shakeClass.Get();
    if(loaded == nullptr){
        UE_LOG(LogAwakenCamera, Verbose, TEXT("Local: shake class not preloaded yet, loading it now"));
        loaded = entry.shakeClass.LoadSynchronous();
    }
    return loaded;
}

void UFirstPersonCameraComponent::playCameraShake(TSubclassOf<UCameraShakeBase> shakeAsset, float scale){
    if(shakeAsset == nullptr || scale <= 0.0f){
        return;
    }
    // The scene camera is in charge while a scene plays, so no shakes (step 12).
    if(viewMode == E_CameraViewMode::SceneYielded){
        return;
    }
    APlayerController* controller = getLocalController();
    if(controller != nullptr && controller->PlayerCameraManager != nullptr){
        controller->PlayerCameraManager->StartCameraShake(shakeAsset, scale);
        UE_LOG(LogAwakenCamera, Verbose, TEXT("Local: camera shake %s at scale %.2f"), *shakeAsset->GetName(), scale);
    }
}

void UFirstPersonCameraComponent::onCosmeticEvent(const FGameplayTag& eventTag, const FVector& eventLocation, float magnitude){
    if(!localListening){
        return;
    }
    const UDA_FirstPersonCameraConfig* config = getConfig();
    if(config == nullptr){
        return;
    }
    // Ignore any event whose tag is not in the shake table. The row with the highest matching minimum magnitude wins.
    const FS_CameraShakeRow* bestRow = nullptr;
    for(const FS_CameraShakeRow& row : config->shakeTable){
        if(!eventTag.MatchesTag(row.eventTag) || magnitude < row.minMagnitude){
            continue;
        }
        if(bestRow == nullptr || row.minMagnitude > bestRow->minMagnitude){
            bestRow = &row;
        }
    }
    if(bestRow == nullptr){
        return;
    }
    const FS_CameraShakeEntry* entry = config->shakeByTag.Find(bestRow->shakeTag);
    if(entry == nullptr){
        UE_LOG(LogAwakenCamera, Warning, TEXT("Local: shake row uses %s, which is not in shakeByTag"), *bestRow->shakeTag.ToString());
        return;
    }
    // The event has no victim field (B21 A.1), so a distance test against the local pawn decides whether it is aimed at this player.
    const AActor* owner = GetOwner();
    const float distance = owner != nullptr ? FVector::Dist(owner->GetActorLocation(), eventLocation) : 0.0f;
    float scale = entry->baseScale * comfortSettings.shakeScale;
    if(bestRow->scaleByDistance){
        const float alpha = distance / bestRow->hitRadius;
        if(alpha >= 1.0f){
            return;
        }
        scale *= config->explosionFalloffCurve != nullptr ? config->explosionFalloffCurve->GetFloatValue(alpha) : (1.0f - alpha);
    }else if(distance > bestRow->hitRadius){
        return;
    }
    playCameraShake(resolveShakeClass(*entry), scale);
}

void UFirstPersonCameraComponent::playFootstepShake(){
    if(!localListening){
        return;
    }
    const UDA_FirstPersonCameraConfig* config = getConfig();
    if(config == nullptr){
        return;
    }
    const FS_CameraShakeEntry* entry = config->shakeByTag.Find(T_Camera_Shake_Footstep.GetTag());
    if(entry == nullptr){
        return;
    }
    // Head bob scale, not shake scale (PROPOSED reading), and reduced while the bag is open so the cursor and items stay readable (step 12 item 3).
    float scale = entry->baseScale * comfortSettings.headBobScale;
    if(viewMode == E_CameraViewMode::BagView){
        scale *= config->bagSwayScale;
    }
    playCameraShake(resolveShakeClass(*entry), scale);
}

// ---------------------------------------------------------------------------------------------
// Step 8: recoil and look limits
// ---------------------------------------------------------------------------------------------

void UFirstPersonCameraComponent::applyRecoil(FRotator rotationDelta, float recoverySpeed){
    if(recoverySpeed <= 0.0f){
        UE_LOG(LogAwakenCamera, Warning, TEXT("Local: recoil refused, recovery speed must be above zero"));
        return;
    }
    recoilOffset += rotationDelta;
    recoveryRate = recoverySpeed;
    SetComponentTickEnabled(true);
}

void UFirstPersonCameraComponent::TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction){
    Super::TickComponent(deltaTime, tickType, thisTickFunction);
    recoilOffset = FMath::RInterpConstantTo(recoilOffset, FRotator::ZeroRotator, deltaTime, recoveryRate);
    if(recoilOffset.IsNearlyZero()){
        recoilOffset = FRotator::ZeroRotator;
        // Nothing ticks while the player is not firing (Doc02 5).
        SetComponentTickEnabled(false);
    }
}

void UFirstPersonCameraComponent::setLookLimit(float referenceYaw, float limitDegrees){
    if(limitDegrees <= 0.0f){
        UE_LOG(LogAwakenCamera, Warning, TEXT("Local: look limit refused, the limit must be above zero degrees"));
        return;
    }
    lookLimitActive = true;
    lookLimitReferenceYaw = referenceYaw;
    lookLimitDegrees = limitDegrees;
}

void UFirstPersonCameraComponent::clearLookLimit(){
    lookLimitActive = false;
}

float UFirstPersonCameraComponent::clampYaw(float newYaw) const{
    if(!lookLimitActive){
        return newYaw;
    }
    // The delta is taken across the wrap, so a reference near 180 degrees works. Pitch is never touched here.
    const float delta = FMath::FindDeltaAngleDegrees(lookLimitReferenceYaw, newYaw);
    const float limited = FMath::Clamp(delta, -lookLimitDegrees, lookLimitDegrees);
    return FRotator::NormalizeAxis(lookLimitReferenceYaw + limited);
}

// ---------------------------------------------------------------------------------------------
// Step 12: view states
// ---------------------------------------------------------------------------------------------

void UFirstPersonCameraComponent::onSceneStarted(){
    if(!localListening){
        return;
    }
    sceneHoldActive = true;
    refreshViewMode();
}

void UFirstPersonCameraComponent::onSceneEnded(){
    if(!localListening){
        return;
    }
    sceneHoldActive = false;
    refreshViewMode();
}

void UFirstPersonCameraComponent::onBagScreenOpened(){
    if(!localListening){
        return;
    }
    // A scene locks input, so a bag-opened signal during a scene is ignored.
    if(sceneHoldActive){
        UE_LOG(LogAwakenCamera, Verbose, TEXT("Local: bag-opened ignored while a scene plays"));
        return;
    }
    bagOpen = true;
    refreshViewMode();
}

void UFirstPersonCameraComponent::onBagScreenClosed(){
    if(!localListening){
        return;
    }
    bagOpen = false;
    refreshViewMode();
}

void UFirstPersonCameraComponent::refreshViewMode(){
    E_CameraViewMode newMode = E_CameraViewMode::FirstPerson;
    if(sceneHoldActive){
        newMode = E_CameraViewMode::SceneYielded;
    }else if(bagOpen){
        newMode = E_CameraViewMode::BagView;
    }
    if(newMode == viewMode){
        return;
    }
    const E_CameraViewMode oldMode = viewMode;
    viewMode = newMode;
    exitViewMode(oldMode, newMode);
    enterViewMode(newMode, oldMode);
    UE_LOG(LogAwakenCamera, Log, TEXT("Local: view mode %s -> %s"), *CameraFramework::viewModeToString(oldMode), *CameraFramework::viewModeToString(newMode));
    onViewModeChanged.Broadcast(oldMode, newMode);
}

void UFirstPersonCameraComponent::exitViewMode(E_CameraViewMode oldMode, E_CameraViewMode newMode){
    const UDA_FirstPersonCameraConfig* config = getConfig();
    if(oldMode == E_CameraViewMode::BagView){
        UE_LOG(LogAwakenCamera, Log, TEXT("Local: leaving the bag view"));
        if(newMode == E_CameraViewMode::SceneYielded || config == nullptr){
            // The scene camera is in charge, so no blend. The field of view is put back at once so it is right when the scene ends.
            startFovBlend(baseFov, 0.0f);
        }else{
            startFovBlend(baseFov, config->bagViewBlendTime);
        }
        // Showing the held item again is the Character's job: it binds onViewModeChanged.
    }
    if(oldMode == E_CameraViewMode::SceneYielded){
        // Make the Character the view target again if the scene left it elsewhere.
        APlayerController* controller = getLocalController();
        if(controller != nullptr && GetOwner() != nullptr && controller->GetViewTarget() != GetOwner()){
            controller->SetViewTarget(GetOwner());
        }
        UScreenEffectComponent* effects = getControllerEffects();
        if(effects != nullptr){
            effects->setHold(false);
        }
        UE_LOG(LogAwakenCamera, Log, TEXT("Local: screen-effect hold released"));
    }
}

void UFirstPersonCameraComponent::enterViewMode(E_CameraViewMode newMode, E_CameraViewMode oldMode){
    const UDA_FirstPersonCameraConfig* config = getConfig();
    if(newMode == E_CameraViewMode::BagView){
        UE_LOG(LogAwakenCamera, Log, TEXT("Local: entering the bag view"));
        if(config != nullptr){
            startFovBlend(config->bagViewFov, config->bagViewBlendTime);
        }
        // Hiding the held item is the Character's job: it binds onViewModeChanged. The backdrop effect follows the same signal through the mapping.
    }
    if(newMode == E_CameraViewMode::SceneYielded){
        // Zero the recoil, stop running shakes, and hold the screen effects.
        recoilOffset = FRotator::ZeroRotator;
        SetComponentTickEnabled(false);
        APlayerController* controller = getLocalController();
        if(controller != nullptr && controller->PlayerCameraManager != nullptr){
            controller->PlayerCameraManager->StopAllCameraShakes();
        }
        UScreenEffectComponent* effects = getControllerEffects();
        if(effects != nullptr){
            effects->setHold(true);
        }
        UE_LOG(LogAwakenCamera, Log, TEXT("Local: screen-effect hold started"));
    }
}

void UFirstPersonCameraComponent::startFovBlend(float targetFov, float blendTime){
    UWorld* world = GetWorld();
    fovStart = FieldOfView;
    fovTarget = targetFov;
    fovElapsed = 0.0f;
    fovBlendTime = blendTime;
    if(world == nullptr || blendTime <= 0.0f){
        if(world != nullptr){
            world->GetTimerManager().ClearTimer(fovTimer);
        }
        SetFieldOfView(targetFov);
        return;
    }
    // The same short-lived timer as the screen effects: it is armed only while a blend is running.
    if(!world->GetTimerManager().IsTimerActive(fovTimer)){
        fovTimer = world->GetTimerManager().SetTimerForNextTick(this, &UFirstPersonCameraComponent::stepFovBlend);
    }
}

void UFirstPersonCameraComponent::stepFovBlend(){
    UWorld* world = GetWorld();
    if(world == nullptr){
        return;
    }
    fovElapsed += world->GetDeltaSeconds();
    const float alpha = fovBlendTime > 0.0f ? FMath::Clamp(fovElapsed / fovBlendTime, 0.0f, 1.0f) : 1.0f;
    SetFieldOfView(FMath::Lerp(fovStart, fovTarget, alpha));
    if(alpha < 1.0f){
        // Set the handle again without asking IsTimerActive: inside its own callback the timer still counts as active.
        fovTimer = world->GetTimerManager().SetTimerForNextTick(this, &UFirstPersonCameraComponent::stepFovBlend);
    }
}

void UFirstPersonCameraComponent::EndPlay(const EEndPlayReason::Type reason){
    UWorld* world = GetWorld();
    if(world != nullptr){
        world->GetTimerManager().ClearTimer(fovTimer);
    }
    // A scene that is still playing when the Character goes away must not leave the effects held.
    if(viewMode == E_CameraViewMode::SceneYielded){
        UScreenEffectComponent* effects = getControllerEffects();
        if(effects != nullptr){
            effects->setHold(false);
        }
    }
    setLocalListening(false);
    clearOwnerHeadHiding();
    shakeLoadHandle.Reset();
    Super::EndPlay(reason);
}
