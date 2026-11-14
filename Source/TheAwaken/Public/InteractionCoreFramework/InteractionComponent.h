// Public/InteractionCoreFramework/InteractionComponent.h
// Doc B18 (Interaction Core). Lives on the Character (decision D5, Doc B19); AAwakenCharacter creates it and holds it as `interaction`.
// Client side: focus, prompt, input, requests. Host side: the six checks, holds, locks, interruption, noise.
// Clients send intent, the host decides (Doc02 19.6). Nothing is applied partially: the first failed check wins.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "InteractionCoreFramework/00Enums/E_InteractionCoreFramework.h"
#include "InteractionCoreFramework/01Structs/S_InteractionCoreFramework.h"
#include "InteractionComponent.generated.h"

class UInputComponent;
class UAbilitySystemComponent;
class UDA_InteractionSettings;
class UInteractionLockComponent;
class APlayerController;
struct FOnAttributeChangeData;

// Doc B22 3.9 assigns this category to B11 and B18.
DECLARE_LOG_CATEGORY_EXTERN(LogAwakenInteraction, Log, All);

UCLASS(ClassGroup=(Awaken), meta=(BlueprintSpawnableComponent))
class THEAWAKEN_API UInteractionComponent : public UActorComponent{
    GENERATED_BODY()
public:
    UInteractionComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

    // ---- Wiring, called by AAwakenCharacter ----

    // Local pawn only (Unreal calls SetupPlayerInputComponent on the locally controlled pawn). Binds IA_Interact and starts the focus timer.
    void bindInput(UInputComponent* playerInputComponent);

    // Server only, from AAwakenCharacter::bindAbilitySystem. Runs again after a respawn, so it unbinds first.
    void bindAbilitySystem(UAbilitySystemComponent* abilitySystem);
    void unbindAbilitySystem();

    // From AAwakenCharacter::UnPossessed. Stops focus, sends a lost key release, and on the server ends the hold and frees the locks.
    void onUnpossessed();

    // ---- Queries (never mutate) ----

    UFUNCTION(BlueprintPure, Category = "Interaction")
    AActor* getFocusedTarget() const;

    // The UI re-reads this when Interaction.FocusChanged arrives (it also arrives when only the prompt changed).
    UFUNCTION(BlueprintPure, Category = "Interaction")
    FS_InteractionPrompt getFocusedPrompt() const;

    // ---- Commands ----

    // Timer. Works out the focused target on the owning machine (B18 3.1). Public so a test or a debug command can call it.
    void updateFocus();

    // Server. Ends the player's hold: keeps no progress, frees the lock, raises Interaction.HoldCancelled. No-op when nothing runs.
    void cancelHold(E_InteractionHoldCancelReason reason);

    // Server. A station keeps its lock until its own framework calls this (for example when the bag screen closes).
    void releaseLock(AActor* target);

    // Server. Frees every lock this component's player holds (B18 section 4: ReleaseLocksFor(Player)).
    void releaseLocksFor(AActor* player);

    // Server. The lock component calls this when a locked piece is destroyed or collapses (Doc B01 3.6).
    void notifyTargetDestroyed(AActor* target);

    // Server. Cleanup for a leaving player: ends the hold and frees the locks (B18 amendment A89). Session.PlayerLeft is not declared
    // yet (Doc B20 3.6): call this by hand from the session cleanup, and from a test.
    void onPlayerLeft();

    // Server. Doc R17 is not built: a scene start calls this with true (cancels the hold, frees the locks), the scene end with false.
    void setLockedIntoScene(bool isLocked);

    // TEMPORARY stand-in for the Doc B17 3.13 accessibility setting: true = "press once instead of hold". Local machine only.
    void setPressOnceMode(bool isPressOnce);

    // ---- RPCs (Unreal-forced _Implementation names, Doc02 20.5). The component replicates; it has no replicated properties. ----

    UFUNCTION(Server, Reliable)
    void serverRequestInteract(AActor* target);

    // Hold-style mode only: the key went up.
    UFUNCTION(Server, Reliable)
    void serverReleaseInteract();

    // The refusal line for the owning player. No message exists for it in B18 yet (guide: Open points).
    UFUNCTION(Client, Reliable)
    void clientInteractRefused(AActor* target, FGameplayTag reason);

    // The owning client raises Interaction.HoldStarted locally and starts the clock that drives holdProgress.
    UFUNCTION(Client, Reliable)
    void clientHoldStarted(AActor* target, float durationSeconds);

    UFUNCTION(Client, Reliable)
    void clientHoldCancelled(AActor* target, E_InteractionHoldCancelReason reason);

    // Small addition to the guide: the client must stop its progress clock when a hold completes (nothing is raised).
    UFUNCTION(Client, Reliable)
    void clientHoldCompleted(AActor* target);

private:
    // ---- Settings and wiring ----
    bool ensureSettings();
    void startFocusTimer();
    void stopFocus();
    APlayerController* getOwnerController() const;
    bool isLocalOwner() const;

    // ---- Aim (client) and eye (host). TEMPORARY until Doc B04 exists (B04 files are empty). Swap the bodies, never read the camera elsewhere. ----
    FVector getAimTraceOrigin() const;
    FVector getAimTraceDirection() const;
    FVector getHostEyeLocation() const;

    // ---- Geometry shared by focus (client) and the server checks ----
    float measureDistance(const AActor* target, const FVector& point, FVector& closestPoint) const;
    bool hasClearLine(const FVector& origin, const FVector& targetPoint, const AActor* target) const;

    // ---- Focus (client) ----
    AActor* pickBest(const TArray<FHitResult>& hits, const FVector& origin, const FVector& direction) const;
    void setFocus(AActor* newTarget);
    FS_InteractionPrompt buildPrompt(AActor* target) const;
    FKey getInteractKey() const;

    // ---- Input (client) ----
    void onInteractPressed();
    void onInteractReleased();
    void sendReleaseIfNeeded();

    // ---- Requests (server) ----
    E_InteractionRequestVerdict validateRequest(AActor* target, FGameplayTag& failReason) const;
    void runInteraction(AActor* target);
    bool hasPlayerTag(const FGameplayTag& tag) const;
    static bool implementsInteractable(const AActor* actor);
    UInteractionLockComponent* findLock(const AActor* target) const;
    void acquireLock(AActor* target);
    void releaseIfPressOnly(TWeakObjectPtr<AActor> target);

    // ---- Holds (server) ----
    void beginHold(AActor* target);
    void dueHold();
    void finishHold();
    void clearHoldState();
    bool hasHold() const;
    bool isEscapeHold() const;
    bool shouldCancelForState(E_InteractionHoldCancelReason& reason) const;
    void onDamaged();
    void watchTick();
    void updateWatch();
    void releaseLocksExceptEscapeTarget();

    // ---- Ability System signals (server) ----
    void onHPChanged(const FOnAttributeChangeData& data);
    void onIncapacitatedChanged(const FGameplayTag tag, int32 count);

    // ---- Noise (server, B18 3.9) ----
    struct FPendingNoise{
        bool hasNoise = false;
        FS_InteractionNoise noise;
        FVector location = FVector::ZeroVector;
    };
    FPendingNoise capturePendingNoise(AActor* target) const;
    void reportNoise(const FPendingNoise& pending) const;
    // PLACEHOLDER for Doc B12's hearing entry point (B12 files are empty): logs the call. B12 decides what an enemy does about it.
    static void reportHearingStimulus(const FVector& location, float loudness, const FGameplayTag& noiseTag, AActor* instigator);

    // ---- Messages (Doc02 11) ----
    void raiseHoldMessage(const FGameplayTag& channel, AActor* target, E_InteractionHoldCancelReason reason) const;
    void raiseHoldMessageOnServer(const FGameplayTag& channel, AActor* target, E_InteractionHoldCancelReason reason) const;

    // TEMPORARY stub for the Doc B17 notice line: logs and prints the refusal text for reasonLineSeconds.
    void showReasonLine(const FGameplayTag& reason) const;

    // ---- State ----
    UPROPERTY()
    TObjectPtr<UDA_InteractionSettings> settings;

    // Focus is local, never replicated (B18 3.1, 3.10).
    TWeakObjectPtr<AActor> focusedTarget;
    FS_InteractionPrompt focusedPrompt;

    // Host: one hold per player and the targets this player has locked.
    FS_ActiveHold activeHold;
    TArray<TWeakObjectPtr<AActor>> lockedTargets;
    bool lockedIntoScene = false;

    // Client: the hold the server told us about, and what the key was doing.
    TWeakObjectPtr<AActor> localHoldTarget;
    double localHoldStartTime = 0.0;
    float localHoldDuration = 0.0f;
    bool releaseExpected = false;
    bool pressOnceMode = false;

    TWeakObjectPtr<UAbilitySystemComponent> boundAbilitySystem;
    FDelegateHandle hpChangedHandle;
    FDelegateHandle incapacitatedHandle;

    FTimerHandle focusTimer;
    FTimerHandle holdTimer;
    FTimerHandle finishTimer;
    FTimerHandle watchTimer;
};
