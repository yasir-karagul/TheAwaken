#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "CameraFramework/01Structs/S_CameraFramework.h"
#include "CameraFramework/DA_ScreenEffectMapping.h"
#include "ScreenEffectComponent.generated.h"

class APawn;
class UCameraComponent;
class UDA_FirstPersonCameraConfig;
class UMaterialInstanceDynamic;
class UMaterialInterface;

// A post-process parameter layer. It lives on the PlayerController so effect state survives a respawn (spec section 2, Doc B19 3.4),
// and it runs only for the locally controlled controller. Nothing here replicates.
// It is also where the controller's camera config and comfort settings are kept, so the rig on the Character can reach them.
UCLASS(ClassGroup=(Camera), Blueprintable, meta=(BlueprintSpawnableComponent))
class UScreenEffectComponent : public UActorComponent{
    GENERATED_BODY()
public:
    UScreenEffectComponent();

    // Used by the mapping and by the dev tools only. No other framework calls them, so both
    // functions are C++ only and deliberately not BlueprintCallable (spec 4.2, S7-F12).
    void setEffectParameter(FGameplayTag effectTag, float targetValue, float blendTime);
    void clearEffectParameter(FGameplayTag effectTag, float blendTime);
    // For Awaken.Camera.ClearEffect without a tag.
    void clearAllEffectParameters(float blendTime);

    // While held, the component keeps tracking the target of every row but stops changing the pushed values (step 12).
    // On release every parameter blends to its current target with its normal blend time.
    void setHold(bool hold);

    // The controller feeds these in. The copy here is what the camera component on the Character reads (step 5).
    void setComfortSettings(const FS_CameraComfortSettings& newSettings);
    const FS_CameraComfortSettings& getComfortSettings() const{ return comfortSettings; }
    const UDA_FirstPersonCameraConfig* getCameraConfig() const;

    // Pushes the dynamic material into the current Character's camera. The controller calls it from AcknowledgePossession (step 9 item 3).
    void refreshCameraAttachment();

    // The whole of the mapping rule (step 10). The message adapters turn their payload into a source value and an optional filter tag
    // and call this. eventLocation is given only for events that have a place (Cosmetic.Received), which switches on the maxPawnDistance test.
    // snap applies the result with no blend, which is what the start-up snapshot uses.
    void handleSourceValue(const FGameplayTag& sourceTag, const FGameplayTag& filterTag, float sourceValue, const FVector* eventLocation = nullptr, bool snap = false);

    // Runs the start-up snapshot again (Awaken.Camera.Snapshot).
    void runStartupSnapshot();

    // max(screenEffectIntensity, screenEffectIntensityFloor). The floor never applies to a channel at rest.
    float getEffectiveIntensity() const;

    // Debug view and tests.
    const TMap<FGameplayTag, FS_ScreenEffectParameterState>& getParameterStates() const{ return parameters; }
    bool isBlendTimerActive() const;
    bool isHeld() const{ return held; }
    bool isActive() const{ return active; }
    bool areRowsReady() const{ return rowsReady; }

    // Assigned in the editor, on the component of the controller Blueprint.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<UDA_FirstPersonCameraConfig> cameraConfig;

    // M_ScreenEffects, built by art (spec 9). PROPOSED: the guide names no place for this reference, so it sits here.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<UMaterialInterface> screenEffectMaterial;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type reason) override;

private:
    void resolveEffect(const FGameplayTag& effectTag, float releaseBlendTime, bool snapNow);

    void armBlendTimer();
    void stepBlend();
    void pushParameter(const FS_ScreenEffectParameterState& state);
    void repushAll();
    void updateBlendableWeight();

    void loadMappingRows();
    void onRowsLoaded();

    void schedulePulseEnd(const UDA_ScreenEffectMapping* row);
    void onPulseEnded(const UDA_ScreenEffectMapping* row);

    void startSnapshot();
    bool trySnapshot();
    void onSnapshotTimer();

    APawn* getLocalPawn() const;
    UCameraComponent* getActiveCamera() const;

    bool active = false;
    bool held = false;
    bool rowsReady = false;
    float blendableWeight = -1.0f;
    int32 snapshotAttempts = 0;

    FS_CameraComfortSettings comfortSettings;

    TMap<FGameplayTag, FS_ScreenEffectParameterState> parameters;
    FTimerHandle blendTimer;
    FTimerHandle snapshotTimer;
    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> effectMaterial;

    // The loaded mapping rows. This array is what keeps them alive: a pointer that is stored
    // only in a plain TMap is not seen by the garbage collector. Keep the Asset Manager handle too.
    UPROPERTY()
    TArray<TObjectPtr<UDA_ScreenEffectMapping>> loadedRows;
    TSharedPtr<FStreamableHandle> rowsHandle;
    TMap<const UDA_ScreenEffectMapping*, float> rowTargets;
    TMap<FGameplayTag, TArray<const UDA_ScreenEffectMapping*>> rowsBySource;
    TMap<const UDA_ScreenEffectMapping*, FTimerHandle> pulseTimers;

    TArray<FGameplayMessageListenerHandle> listenerHandles;
};
