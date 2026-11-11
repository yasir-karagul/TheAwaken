#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "CameraFramework/00Enums/E_CameraFramework.h"
#include "CameraFramework/01Structs/S_CameraFramework.h"
#include "CameraFramework/04Delegates/D_CameraFramework.h"
#include "FirstPersonCameraComponent.generated.h"

class ACharacter;
class APlayerController;
class UCameraShakeBase;
class UDA_FirstPersonCameraConfig;
class UScreenEffectComponent;
class USkeletalMeshComponent;

// The first-person rig on the Character (Doc B04). Local only: nothing here replicates.
// It owns the one aim origin and direction every other framework reads, recoil, camera shake, the look limit,
// the view mode and the owner's head hiding. The Blueprint child BPC_FirstPersonCameraComponent is used for tuning.
UCLASS(ClassGroup=(Camera), Blueprintable, meta=(BlueprintSpawnableComponent))
class UFirstPersonCameraComponent : public UCameraComponent{
    GENERATED_BODY()
public:
    UFirstPersonCameraComponent();

    // Step 4: the one aim origin and direction. Both use the same rotation as the rendered view (recoil included, shake not).
    UFUNCTION(BlueprintPure)
    FVector getAimTraceOrigin() const;

    UFUNCTION(BlueprintPure)
    FVector getAimTraceDirection() const;

    UFUNCTION(BlueprintCallable)
    void playCameraShake(TSubclassOf<UCameraShakeBase> shakeAsset, float scale);

    UFUNCTION(BlueprintCallable)
    void applyRecoil(FRotator rotationDelta, float recoverySpeed);

    // PROPOSED, spec 3.5: hide the owner's head from the owner's own camera. Local only.
    UFUNCTION(BlueprintCallable)
    void applyOwnerHeadHiding(USkeletalMeshComponent* bodyMesh, FName headBoneName);

    // Convenience: the same, with the bone name taken from the config.
    UFUNCTION(BlueprintCallable)
    void applyOwnerHeadHidingFromConfig(USkeletalMeshComponent* bodyMesh);

    UFUNCTION(BlueprintCallable)
    void clearOwnerHeadHiding();

    // Convenience for the Character: attaches the rig to the head socket without scale (step 4 item 2).
    UFUNCTION(BlueprintCallable)
    void attachToHeadSocket(USkeletalMeshComponent* bodyMesh);

    // The Character calls it with true when it is possessed by the local controller, false when released or destroyed (step 7).
    UFUNCTION(BlueprintCallable)
    void setLocalListening(bool listening);

    UFUNCTION(BlueprintPure)
    bool isLocalListening() const{ return localListening; }

    // The animation notify of Doc R09 3.6 calls this for each footstep (step 7 item 5).
    UFUNCTION(BlueprintCallable)
    void playFootstepShake();

    // Look limits for RopeRide and Climb (step 8). Doc R07 calls these; the controller's UpdateRotation uses clampYaw.
    UFUNCTION(BlueprintCallable)
    void setLookLimit(float referenceYaw, float limitDegrees);

    UFUNCTION(BlueprintCallable)
    void clearLookLimit();

    UFUNCTION(BlueprintPure)
    float clampYaw(float newYaw) const;

    // Pulls the config and comfort settings from the controller's UScreenEffectComponent and applies them (step 5 item 4).
    // The controller calls it from AcknowledgePossession; the component calls it again when a setting changes.
    UFUNCTION(BlueprintCallable)
    void applyControllerSettings();

    UFUNCTION(BlueprintPure)
    E_CameraViewMode getViewMode() const{ return viewMode; }

    // Step 6. Static queries, so the shared validator of Doc02 19.6 can call them without a component instance.
    // validateAimOrigin returns an empty tag when the origin is accepted, otherwise the reason tag.
    static FGameplayTag validateAimOrigin(const ACharacter* character, const FGameplayTag& stance, const FVector& clientOrigin, const UDA_FirstPersonCameraConfig* config);
    static float getEyeHeight(const FGameplayTag& stance, const UDA_FirstPersonCameraConfig* config);

    // Message handlers. C++ only: the adapters in CameraMessageBindings.cpp call them, nothing else does.
    void onCosmeticEvent(const FGameplayTag& eventTag, const FVector& eventLocation, float magnitude);
    void onSceneStarted();
    void onSceneEnded();
    void onBagScreenOpened();
    void onBagScreenClosed();

    // Debug view (Awaken.Debug.Camera) and tests.
    const FRotator& getRecoilOffset() const{ return recoilOffset; }
    bool isOwnerHeadHidden() const{ return hiddenHeadMesh.IsValid(); }

    UPROPERTY(BlueprintAssignable)
    FD_CameraViewModeChanged onViewModeChanged;

    UPROPERTY(EditDefaultsOnly)
    FName headSocketName;           // data, set on the Blueprint child

    virtual void TickComponent(float deltaTime, ELevelTick tickType, FActorComponentTickFunction* thisTickFunction) override;

protected:
    virtual void GetCameraView(float deltaTime, FMinimalViewInfo& desiredView) override;
    virtual void EndPlay(const EEndPlayReason::Type reason) override;

private:
    const UDA_FirstPersonCameraConfig* getConfig() const;
    UScreenEffectComponent* getControllerEffects() const;
    APlayerController* getLocalController() const;

    // Step 12: the mode is derived from two flags, so nothing depends on the order the signals arrive in.
    void refreshViewMode();
    void exitViewMode(E_CameraViewMode oldMode, E_CameraViewMode newMode);
    void enterViewMode(E_CameraViewMode newMode, E_CameraViewMode oldMode);

    void startFovBlend(float targetFov, float blendTime);
    void stepFovBlend();

    void preloadShakes();
    TSubclassOf<UCameraShakeBase> resolveShakeClass(const FS_CameraShakeEntry& entry);

    FRotator recoilOffset;
    float recoveryRate = 0.0f;

    TWeakObjectPtr<USkeletalMeshComponent> hiddenHeadMesh;
    FName hiddenHeadBone;

    bool lookLimitActive = false;
    float lookLimitReferenceYaw = 0.0f;
    float lookLimitDegrees = 0.0f;

    bool localListening = false;
    TArray<FGameplayMessageListenerHandle> listenerHandles;

    E_CameraViewMode viewMode = E_CameraViewMode::FirstPerson;
    bool sceneHoldActive = false;
    bool bagOpen = false;

    // Copy of the controller's settings, refreshed by applyControllerSettings.
    FS_CameraComfortSettings comfortSettings;
    float baseFov = 90.0f;
    float nearClip = -1.0f;

    FTimerHandle fovTimer;
    float fovStart = 0.0f;
    float fovTarget = 0.0f;
    float fovElapsed = 0.0f;
    float fovBlendTime = 0.0f;

    TSharedPtr<FStreamableHandle> shakeLoadHandle;
};
