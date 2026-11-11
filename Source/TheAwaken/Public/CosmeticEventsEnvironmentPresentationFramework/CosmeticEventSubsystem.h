#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "CosmeticEventsEnvironmentPresentationFramework/01Structs/S_CosmeticEventsEnvironmentPresentationFramework.h"
#include "CosmeticEventSubsystem.generated.h"

class AController;
class APlayerController;
class UDataTable;
class UDA_EnvironmentSettings;

// Part A of B21: the one shared cosmetic event channel (Doc02 19.2).
// The host sends an event to the clients in range, every machine re-raises it as the local message Cosmetic.Received.
// It is unreliable, never resent, limited by distance and rate limited per area. It has no tick, no history and no saving.
// It carries nothing that changes a gameplay result: if every event were lost the game would play the same.
UCLASS()
class THEAWAKEN_API UCosmeticEventSubsystem : public UWorldSubsystem{
    GENERATED_BODY()

public:
    static UCosmeticEventSubsystem* get(const UObject* worldContext);

    virtual bool ShouldCreateSubsystem(UObject* outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& world) override;

    // Safe on any machine. ownerPlayer and excludePlayer are never sent.
    // ownerPlayer is exempt from the radius (the victim of a hit is never dropped by distance). excludePlayer never receives the event.
    // A client that calls without localOnly returns silently: only the host raises. A client that wants its own feedback passes localOnly.
    // Raise events only after the gameplay result has been applied, and never read one back.
    UFUNCTION(BlueprintCallable, Category="Cosmetic")
    void raiseCosmeticEvent(const FS_CosmeticEvent& event, AController* ownerPlayer = nullptr, AController* excludePlayer = nullptr, bool localOnly = false);

    // Called by the relay component on a client, and by sendToPlayer for the local player. Raises the local message Cosmetic.Received.
    void deliverLocally(const FS_CosmeticEvent& event);

    // Null when the actor is not loaded on this machine (B21 A.5). A listener then skips any attach and plays at event.location.
    AActor* findInstigatorActor(const FS_CosmeticEvent& event) const;

    // The one helper that turns an actor into an instigator id (gap G11), shared by the callers and by findInstigatorActor.
    // A player pawn gives the deterministic GUID of its PlayerIdentity. An NPC gives its NPCId once R16 names the accessor (stub: empty).
    static FGuid makeInstigatorId(const AActor* actor);

#if !UE_BUILD_SHIPPING
    // Test-only counters for the B22 test: events that passed the limiter, and sends to players.
    int32 testEventsSent = 0;
    int32 testPlayerSends = 0;

    // The last 32 events received on this machine, for the Net debug view.
    const TArray<FS_CosmeticEvent>& getRecentReceived() const{
        return recentReceived;
    }

    void drawDebug() const;

    // For the B22 test: the radius and the rate limit that apply to a tag. False when there is no row and no Default row.
    bool devGetRule(const FGameplayTag& eventTag, float& radius, int32& maxPerSecond) const;
#endif

private:
    const FS_CosmeticEventRule* findRule(const FGameplayTag& eventTag) const;
    float resolveRadius(const FS_CosmeticEventRule& rule) const;
    bool isRateLimited(const FS_CosmeticEvent& event, float radius, int32 maxPerSecond);
    bool isInsideRadius(const APlayerController* controller, const FVector& location, float radius) const;
    void sendToPlayer(APlayerController* controller, const FS_CosmeticEvent& event);
    FString roleName() const;

    UPROPERTY()
    TObjectPtr<UDA_EnvironmentSettings> settings;

    UPROPERTY()
    TObjectPtr<UDataTable> ruleTable;

    TMap<FGameplayTag, TArray<FS_RecentSend>> recentByTag;

#if !UE_BUILD_SHIPPING
    TArray<FS_CosmeticEvent> recentReceived;
    FTimerHandle debugTimerHandle;
#endif
};
