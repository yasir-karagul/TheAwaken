// Public/GameFrameworkClassTable/AwakenPlayerController.h
#pragma once
#include "GameFramework/PlayerController.h"
#include "AwakenPlayerController.generated.h"

class UInputMappingContext;
class UDialogueComponent;
class UScreenEffectComponent;
class UAudioListenerComponent;
class UActionInputComponent;
class UCosmeticEventRelayComponent;

UCLASS()
class THEAWAKEN_API AAwakenPlayerController : public APlayerController{
    GENERATED_BODY()
public:
    AAwakenPlayerController();

    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    /** Doc R12 11.1 (R12-IG13). The input layer (R04, B17) calls this from its Enhanced Input action callbacks on the owning machine.
     *  At most one unreliable call per heartbeatIntervalSeconds reaches the host, which keeps the time of the last one. */
    void noteInputActivity();

    /** Host only. Seconds since the host last heard from this player's input. The PlayerState sleep adapter reads it. */
    double getSecondsSinceLastInput() const;

    UFUNCTION(Server, Unreliable)
    void serverInputHeartbeat();

    // TEMPORARY (test 6), remove afterwards: destroys this player's pawn and respawns it.
    UFUNCTION(Exec)
    void debugRespawn();

    UFUNCTION(Server, Reliable)
    void serverDebugRespawn();

private:
    void addInputMappings();
    void stampInputOnHost();

    static constexpr double heartbeatIntervalSeconds = 5.0;
    double lastInputHostTime = 0.0;   // host world time of the last heartbeat
    double lastHeartbeatSentTime = -1000.0;   // owning machine, to rate-limit the call

    UPROPERTY(EditDefaultsOnly, Category="Awaken|Input")
    TArray<TObjectPtr<UInputMappingContext>> defaultMappings;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UDialogueComponent> dialogue;

    // Doc R04 A5: binds DA_ActionInputMap to the ability system. Set inputMap on this component in the controller Blueprint.
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UActionInputComponent> actionInput;

    // Doc B21 A.3 (gap G2): the client call of the cosmetic event channel.
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCosmeticEventRelayComponent> cosmeticRelay;

    UPROPERTY()
    TObjectPtr<UScreenEffectComponent> screenEffects;

    UPROPERTY()
    TObjectPtr<UAudioListenerComponent> audioListener;
};
