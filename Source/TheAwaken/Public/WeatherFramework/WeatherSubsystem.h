#pragma once
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "TimerManager.h"
#include "WeatherFramework/00Enums/E_WeatherFramework.h"
#include "WeatherFramework/01Structs/S_WeatherFramework.h"
#include "WeatherFramework/WeatherSimulation.h"
#include "TimeFramework/01Structs/S_TimeFramework.h"
#include "WeatherSubsystem.generated.h"

class UDA_WeatherTuning;
class UWeatherCarrierComponent;
class UShelterComponent;
class URainCollectorComponent;
class UTimeSubsystem;

UCLASS()
class THEAWAKEN_API UWeatherSubsystem : public UWorldSubsystem{
    GENERATED_BODY()
public:
    static UWeatherSubsystem* get(const UObject* worldContext);
    virtual bool ShouldCreateSubsystem(UObject* outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;
    virtual void OnWorldBeginPlay(UWorld& world) override;

    // The carrier registers itself in BeginPlay; the OnRep functions of the carrier call the handle functions.
    void registerCarrier(UWeatherCarrierComponent* newCarrier);
    void handleWeatherStateReplicated(const FS_WeatherState& previous);
    void handleWaterBodiesReplicated(const TArray<FS_WaterBodyState>& previous);

    // ---- Commands (host only; a client call does nothing) ----
    void initializeNewWorld();
    FGameplayTag forceWeather(E_WeatherType type, float strength);
    void advanceTo(int64 targetGameTime);
    FS_WeatherSaveRecord getSnapshotForSave() const;
    void restoreFromSnapshot(const FS_WeatherSaveRecord& record);
    void setRandomSeedForTesting(int32 seed);

    // ---- Registration ----
    void registerWaterBody(const FS_WaterBodyRegistration& registration);
    void unregisterWaterBody(const FGuid& bodyId);
    void registerShelterComponent(UShelterComponent* component);
    void unregisterShelterComponent(UShelterComponent* component);
    void registerRainCollector(URainCollectorComponent* component);
    void unregisterRainCollector(URainCollectorComponent* component);
    void registerIndoorZone(UObject* zone);
    void unregisterIndoorZone(UObject* zone);

    // ---- Queries (every machine unless marked host only) ----
    /** True once initializeNewWorld or restoreFromSnapshot has run (same idea as isClockReady in Time). */
    bool isWeatherReady() const{ return isInitialized; }

    UFUNCTION(BlueprintPure, Category = "Weather")
    E_WeatherType getCurrentWeather() const;

    UFUNCTION(BlueprintPure, Category = "Weather")
    float getRainStrength() const;

    UFUNCTION(BlueprintPure, Category = "Weather")
    float getSnowStrength() const;

    UFUNCTION(BlueprintPure, Category = "Weather")
    float getSnowLevel() const;

    UFUNCTION(BlueprintPure, Category = "Weather")
    float getIceLevel(const FGuid& bodyId) const;

    UFUNCTION(BlueprintPure, Category = "Weather")
    float getRiverStrength(const FGuid& bodyId) const;

    UFUNCTION(BlueprintPure, Category = "Weather")
    bool isWaterFreezing(const FGuid& bodyId) const;

    UFUNCTION(BlueprintPure, Category = "Weather")
    bool isWaterFrozen(const FGuid& bodyId) const;

    UFUNCTION(BlueprintPure, Category = "Weather")
    float getStrongCurrentThreshold() const;

    E_WeatherSeasonKey getCurrentSeasonKey() const;
    float getColdExposureRate(const FS_ShelterState& shelter) const;
    float getRainHoursOnDay(int64 dayNumber) const;                       // host only
    float getRainHoursInRange(int64 firstDay, int64 lastDay) const;       // host only
    bool isInsideIndoorZone(const FVector& location) const;               // host only
    const UDA_WeatherTuning* getTuning() const;

#if !UE_BUILD_SHIPPING
    // Doc B22 dev commands and the debug view call these (section 9). Non-shipping only.
    bool devForceWeather(E_WeatherType type, float strength);
    void devDump() const;
    void drawDebug() const;
#endif

private:
    template<typename messageType>
    void raise(const FGameplayTag& channel, const messageType& message) const{
        UGameplayMessageSubsystem::Get(this).BroadcastMessage(channel, message);
    }

    // lifecycle and helpers
    void loadData();
    bool isAuthority() const;
    FString roleName() const;
    UTimeSubsystem* getTime() const;
    const FS_WeatherState& readState() const;
    const TArray<FS_WaterBodyState>& readBodies() const;
    const FS_WaterBodyState* findBody(const FGuid& bodyId) const;

    // period timer (Step 10)
    void reschedulePeriodTimer();
    void onPeriodTimer(const FS_TimerHandle& handle, int64 finishGameTime);

    // stepping (Step 11)
    void flushRainFill();
    void pruneRainHistory(int64 currentDay);
    void updateStepTimer();
    void onStepTimer();

    // Time hooks (Step 12)
    void registerTimeListeners();
    void onSeasonChangedMessage(FGameplayTag channel, const FS_TimeSeasonChangedMessage& message);
    void onDayPassedMessage(FGameplayTag channel, const FS_TimeDayPassedMessage& message);
    void onSkipAnnouncedMessage(FGameplayTag channel, const FS_TimeSkipAnnouncedMessage& message);
    float deriveLevelFromCalendar(float winterDays, float snowyDays, float thawDays) const;
    void resetBodyFromCalendar(FS_WaterBodyState& body) const;
    void rederiveFromCalendar();

    // publishing and messages (Step 13)
    void publishState();
    void raiseWeatherMessages(const FS_WeatherState& previousState, const FS_WeatherState& newState);
    void raiseWaterMessages(const TArray<FS_WaterBodyState>& previousBodies, const TArray<FS_WaterBodyState>& newBodies);

    // shelter (Step 14)
    void updateShelterTimer();
    void onShelterTimer();

    // ---- data ----
    UPROPERTY()
    TObjectPtr<UDA_WeatherTuning> tuning;

    UPROPERTY()
    TObjectPtr<UWeatherCarrierComponent> carrier;

    // Everything that moves with time (Step 11). Host only.
    FWeatherSimulation simulation;

    FS_WeatherState publishedState;
    TArray<FS_WaterBodyState> publishedBodies;
    TSet<FGuid> pendingDeriveIds;
    FS_TimerHandle periodTimerHandle;
    int64 scheduledPeriodEnd = 0;
    bool isInitialized = false;

    // Replicated state that arrived before the carrier registered (an OnRep can run before BeginPlay)
    bool weatherArrivedEarly = false;
    bool bodiesArrivedEarly = false;

    // timers and listeners (host)
    FTimerHandle stepTimerHandle;
    FTimerHandle shelterTimerHandle;
    FGameplayMessageListenerHandle seasonListener;
    FGameplayMessageListenerHandle dayListener;
    FGameplayMessageListenerHandle skipListener;

    TArray<TWeakObjectPtr<UShelterComponent>> shelterComponents;
    TArray<TWeakObjectPtr<URainCollectorComponent>> rainCollectors;
    TArray<TWeakObjectPtr<UObject>> indoorZones;

    FS_WeatherState emptyState;
    TArray<FS_WaterBodyState> emptyBodies;

#if !UE_BUILD_SHIPPING
    FTimerHandle debugTimerHandle;
#endif
};
