#pragma once
#include "Components/ActorComponent.h"
#include "TimeFramework/01Structs/S_TimeFramework.h"
#include "TimeCarrierComponent.generated.h"

UCLASS(ClassGroup=(Awaken), meta=(BlueprintSpawnableComponent))
class THEAWAKEN_API UTimeCarrierComponent : public UActorComponent{
    GENERATED_BODY()
public:
    UTimeCarrierComponent();
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(ReplicatedUsing=OnRep_syncState)
    FS_TimeSyncState syncState;

    UPROPERTY(ReplicatedUsing=OnRep_calendar)
    FS_CalendarState calendar;

    UPROPERTY(ReplicatedUsing=OnRep_sleepState)
    FS_SleepWaitState sleepState;

    // Declared last on purpose (R12 7).
    UPROPERTY(ReplicatedUsing=OnRep_skipState)
    FS_SkipState skipState;

    UFUNCTION()
    void OnRep_syncState(const FS_TimeSyncState& previous);

    UFUNCTION()
    void OnRep_calendar(const FS_CalendarState& previous);

    UFUNCTION()
    void OnRep_sleepState(const FS_SleepWaitState& previous);

    UFUNCTION()
    void OnRep_skipState(const FS_SkipState& previous);
};