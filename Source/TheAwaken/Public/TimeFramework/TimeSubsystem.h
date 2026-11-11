#pragma once
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "TimerManager.h"
#include "TimeFramework/00Enums/E_TimeFramework.h"
#include "TimeFramework/01Structs/S_TimeFramework.h"
#include "TimeFramework/04Delegates/D_TimeFramework.h"
#include "TimeSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAwakenTime, Log, All);

class UDA_TimeSettings;
class UTimeCarrierComponent;

UCLASS()
class THEAWAKEN_API UTimeSubsystem : public UWorldSubsystem{
    GENERATED_BODY()
public:
    static UTimeSubsystem* get(const UObject* worldContext);
    virtual bool ShouldCreateSubsystem(UObject* outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& world) override;

    void registerCarrier(UTimeCarrierComponent* newCarrier);
    void handleSyncReplicated(const FS_TimeSyncState& previous);
    void handleCalendarReplicated(const FS_CalendarState& previous);
    void handleSleepStateReplicated();
    void handleSkipStateReplicated(const FS_SkipState& previous);

    UFUNCTION(BlueprintPure)
    int64 getGameTime() const;

    UFUNCTION(BlueprintPure)
    float getCurrentTimeOfDay() const;

    UFUNCTION(BlueprintPure)
    E_DayNightPhase getCurrentPhase() const;

    UFUNCTION(BlueprintPure)
    int64 getDaysPassed() const;

    UFUNCTION(BlueprintPure)
    E_Season getCurrentSeason() const;

    UFUNCTION(BlueprintPure)
    int32 getDayInSeason() const;

    UFUNCTION(BlueprintPure)
    bool isSnowyBlockActive() const;

    UFUNCTION(BlueprintPure)
    bool isGroupSleepSkipRunning() const;

    UFUNCTION(BlueprintPure)
    bool isSleepWindowOpen() const;

    UFUNCTION(BlueprintPure)
    float getRealSecondsPerGameDay() const;

    /** R12-IG4. 0 or less means idle detection is off. The PlayerState sleep adapter reads it. */
    UFUNCTION(BlueprintPure)
    float getAwayIdleSeconds() const;

    /** True once initializeNewWorld or restoreFromSnapshot has run (R12-IG14). */
    bool isClockReady() const{ return clockReady; }

    /** Host only: the Snowy draw exists on the host only (R12 3.5). Weather reads them at world creation (B14 3.7). 0 when no draw is valid. */
    int32 getSnowyBlockStartDay() const{ return snowyDraw.isValid ? snowyDraw.startDay : 0; }
    int32 getSnowyBlockLengthDays() const{ return snowyDraw.isValid ? snowyDraw.lengthDays : 0; }

    // Sleep API, callable from Blueprint. The commands change state on the host only:
    // on a client requestSleepTimeSkip returns Time.FailReason.NotAuthority and cancelSleepWait does nothing.
    UFUNCTION(BlueprintPure, Category = "Time|Sleep")
    TArray<APlayerState*> getSkipRegisteredPlayers() const;

    UFUNCTION(BlueprintCallable, Category = "Time|Sleep")
    FGameplayTag requestSleepTimeSkip(APlayerState* player, const FVector& sleepObjectLocation);

    UFUNCTION(BlueprintCallable, Category = "Time|Sleep")
    void cancelSleepWait(APlayerState* player);

    FS_TimerHandle scheduleAt(int64 finishGameTime, const FD_TimeDue& callback);
    FS_TimerHandle scheduleAtDayStart(int64 finishDay, const FD_TimeDue& callback);
    void cancelTimer(const FS_TimerHandle& handle);

    void initializeNewWorld();
    FS_TimeSaveRecord getSnapshotForSave() const;
    void restoreFromSnapshot(const FS_TimeSaveRecord& record);

#if !UE_BUILD_SHIPPING
    // Doc B22 dev commands and the debug view call these (Step 15). Non-shipping only.
    bool devSkipTo(int64 toGameTime);
    void drawDebug() const;

    // Added for the console commands of TimeCheatCommands.cpp (not in the guide's code).
    bool devSetTimeOfDay(int32 hour, int32 minute);
    bool devSkipToMorning();
    bool devSetDay(int64 day);
    bool devSetSeason(E_Season season, bool snowy);
    bool devSimulateRegister(int32 playerIndex);
#endif

private:
    template<typename messageType>
    void raise(const FGameplayTag& channel, const messageType& message) const{
        UGameplayMessageSubsystem::Get(this).BroadcastMessage(channel, message);
    }

    bool isAuthority() const;
    FString roleName() const;
    double serverWorldSeconds() const;
    int64 extrapolatedGameTime() const;
    FS_TimeCalendarConfig makeCalendarConfig() const;
    E_DayNightPhase phaseForTime(int32 timeOfDaySeconds) const;
    void publishSync(int64 newGameTime, bool isJump);
    void onClockStep();
    void processTo(int64 target);
    void localWatch();

    void runRollover();
    void applyCalendar(const FS_CalendarState& next);

    FS_TimerHandle addTimer(int64 finishGameTime, E_TimerKind kind, const FD_TimeDue& callback);
    void fireTimers(int64 upTo, E_TimerKind kind);

    E_SleepCountState countStateOf(APlayerState* player) const;
    bool isSleepWindowOpenAt(int32 timeOfDaySeconds) const;
    void refreshSleepState();
    void evaluateGroupSkip();
    void onSleepMonitor();
    void clearRegistrations();
    void beginGroupSkip();
    void onSkipFadeDone();
    void executeSkip(int64 toGameTime, E_SkipKind kind);

    UPROPERTY()
    TObjectPtr<UDA_TimeSettings> settings;

    UPROPERTY()
    TObjectPtr<UTimeCarrierComponent> carrier;

    // Server only
    int64 processedGameTime = 0;
    bool clockReady = false;   // set by initializeNewWorld or restoreFromSnapshot (R12-IG14)
    FS_SnowyDraw snowyDraw;
    FRandomStream randomStream;
    E_SkipKind skipKind = E_SkipKind::None;
    bool skipRunning = false;   // true only inside executeSkip, which runs in one frame
    bool hasPendingCalendar = false;
    FS_CalendarState pendingCalendarPrevious;
    TArray<FS_SleepRegistration> registrations;
    TArray<TWeakObjectPtr<APlayerState>> skipRegisteredPlayers;
    TArray<FS_ScheduledTimer> timers;
    FTimerHandle sleepMonitorTimer;
    FTimerHandle skipFadeTimer;

    // Every machine
    FTimerHandle clockTimer;
    int32 watchedSkipSerial = -1;
    int32 watchedHour = -1;
    E_DayNightPhase watchedPhase = E_DayNightPhase::Day;
    bool firstWatch = true;
    bool calendarArrivedEarly = false;
    bool sleepArrivedEarly = false;
    bool skipArrivedEarly = false;
};
