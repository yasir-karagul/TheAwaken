#include "StatsFramework/VitalsRegenComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/PlayerState.h"
#include "StatsFramework/StatsLibrary.h"
#include "StatsFramework/AS_CoreVitals.h"
#include "StatsFramework/DA_CoreVitalsDefinition.h"
#include "StatsFramework/02Tags/T_StatsFramework.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"
#include "GameFrameworkClassTable/AwakenPlayerState.h"
#include "CheckpointRespawnFramework/00Enums/E_CheckpointRespawnFramework.h"
#include "SurvivalTickFramework/SurvivalMeterComponent.h"

UVitalsRegenComponent::UVitalsRegenComponent(){
    PrimaryComponentTick.bCanEverTick = false;
}

UAbilitySystemComponent* UVitalsRegenComponent::getAbilitySystem() const{
    return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
}

const UAS_CoreVitals* UVitalsRegenComponent::getVitals() const{
    UAbilitySystemComponent* asc = getAbilitySystem();
    return asc != nullptr ? asc->GetSet<UAS_CoreVitals>() : nullptr;
}

UDA_CoreVitalsDefinition* UVitalsRegenComponent::getDefinition() const{
    const UAS_CoreVitals* vitals = getVitals();
    return vitals != nullptr ? vitals->definition.Get() : nullptr;
}

bool UVitalsRegenComponent::isPlayerOwner() const{
    return GetOwner() != nullptr && GetOwner()->IsA<APlayerState>();
}

void UVitalsRegenComponent::BeginPlay(){
    Super::BeginPlay();
    if(GetOwner() == nullptr || !GetOwner()->HasAuthority()){
        return;
    }
    UAbilitySystemComponent* asc = getAbilitySystem();
    if(asc == nullptr){
        return;
    }
    sprintTagHandle = asc->RegisterGameplayTagEvent(T_Action_Sprint, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UVitalsRegenComponent::onWatchedTagChanged);
    medicineTagHandle = asc->RegisterGameplayTagEvent(T_Stats_State_MedicineActive, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UVitalsRegenComponent::onWatchedTagChanged);
    eventsBound = true;
    reevaluate();
}

void UVitalsRegenComponent::EndPlay(const EEndPlayReason::Type endPlayReason){
    if(eventsBound){
        UAbilitySystemComponent* asc = getAbilitySystem();
        if(asc != nullptr){
            asc->RegisterGameplayTagEvent(T_Action_Sprint, EGameplayTagEventType::NewOrRemoved).Remove(sprintTagHandle);
            asc->RegisterGameplayTagEvent(T_Stats_State_MedicineActive, EGameplayTagEventType::NewOrRemoved).Remove(medicineTagHandle);
        }
        eventsBound = false;
    }
    if(GetWorld() != nullptr){
        GetWorld()->GetTimerManager().ClearTimer(healthTimer);
        GetWorld()->GetTimerManager().ClearTimer(staminaTimer);
    }
    Super::EndPlay(endPlayReason);
}

void UVitalsRegenComponent::onWatchedTagChanged(const FGameplayTag tag, int32 count){
    reevaluate();
}

float UVitalsRegenComponent::getRegenCapWithSource(FName& source) const{
    const UDA_CoreVitalsDefinition* definition = getDefinition();
    if(definition == nullptr){
        source = FName("None");
        return 0.0f;
    }
    const USurvivalMeterComponent* survival = GetOwner()->FindComponentByClass<USurvivalMeterComponent>();
    float cap = definition->naturalRegenCapPercent;
    source = FName("Natural");
    if(survival != nullptr && survival->isHungryOrThirsty() && definition->hungryThirstyRegenCapPercent < cap){
        cap = definition->hungryThirstyRegenCapPercent;
        source = FName("HungryOrThirsty");
    }
    if(survival != nullptr && survival->hasBlood() && definition->bloodRegenCapPercent < cap){
        cap = definition->bloodRegenCapPercent;
        source = FName("Blood");
    }
    return cap;
}

float UVitalsRegenComponent::getRegenCap() const{
    FName source;
    return getRegenCapWithSource(source);
}

float UVitalsRegenComponent::getCapHP() const{
    const UAS_CoreVitals* vitals = getVitals();
    return vitals != nullptr ? vitals->getMaxHP() * getRegenCap() * 0.01f : 0.0f;
}

// S5 step 4. A limit only stops regeneration; it never lowers health already above it.
bool UVitalsRegenComponent::isHealthRegenEligible() const{
    const UAS_CoreVitals* vitals = getVitals();
    UAbilitySystemComponent* asc = getAbilitySystem();
    if(vitals == nullptr || asc == nullptr || !isPlayerOwner() || getDefinition() == nullptr){
        return false;
    }
    if(vitals->getCurrentHP() <= 0.0f){
        return false;
    }
    // No regeneration while Downed; it continues while Restrained (R08 item 31 d).
    const AAwakenPlayerState* playerState = Cast<AAwakenPlayerState>(GetOwner());
    if(playerState != nullptr && playerState->getLifeState() == E_PlayerLifeState::Downed){
        return false;
    }
    const USurvivalMeterComponent* survival = GetOwner()->FindComponentByClass<USurvivalMeterComponent>();
    if(survival != nullptr && survival->isIll()){
        return false;
    }
    // A player counts as having taken medicine while the medicine's own timed effect runs (item 31 b).
    if(asc->HasMatchingGameplayTag(T_Stats_State_MedicineActive)){
        return false;
    }
    return vitals->getCurrentHP() < getCapHP();
}

bool UVitalsRegenComponent::isHealthRegenRunning() const{
    return GetWorld() != nullptr && GetWorld()->GetTimerManager().IsTimerActive(healthTimer);
}

bool UVitalsRegenComponent::isStaminaRegenRunning() const{
    return GetWorld() != nullptr && GetWorld()->GetTimerManager().IsTimerActive(staminaTimer);
}

void UVitalsRegenComponent::reevaluate(){
    if(GetOwner() == nullptr || !GetOwner()->HasAuthority() || getDefinition() == nullptr){
        return;
    }
    refreshLowHealthTag();
    updateHealthTimer();
    updateStaminaTimer();
}

void UVitalsRegenComponent::updateHealthTimer(){
    if(GetWorld() == nullptr){
        return;
    }
    FTimerManager& timers = GetWorld()->GetTimerManager();
    const bool shouldRun = isHealthRegenEligible();
    const bool running = timers.IsTimerActive(healthTimer);
    if(shouldRun && !running){
        timers.SetTimer(healthTimer, this, &UVitalsRegenComponent::onHealthTick, FMath::Max(0.05f, getDefinition()->regenTickSeconds), true);
        UE_LOG(LogAwakenStats, Log, TEXT("[%s][%s] health regeneration started"), *UStatsLibrary::getRoleName(this), *UStatsLibrary::getPlayerId(GetOwner()));
    }else if(!shouldRun && running){
        timers.ClearTimer(healthTimer);
        UE_LOG(LogAwakenStats, Log, TEXT("[%s][%s] health regeneration stopped"), *UStatsLibrary::getRoleName(this), *UStatsLibrary::getPlayerId(GetOwner()));
    }
}

void UVitalsRegenComponent::updateStaminaTimer(){
    const UAS_CoreVitals* vitals = getVitals();
    UAbilitySystemComponent* asc = getAbilitySystem();
    if(GetWorld() == nullptr || vitals == nullptr || asc == nullptr){
        return;
    }
    FTimerManager& timers = GetWorld()->GetTimerManager();
    // Stamina regenerates below max and pauses while the player has Action.Sprint (S3 step 7).
    const bool shouldRun = vitals->getMaxStamina() > 0.0f && vitals->getCurrentStamina() < vitals->getMaxStamina() && !asc->HasMatchingGameplayTag(T_Action_Sprint);
    const bool running = timers.IsTimerActive(staminaTimer);
    if(shouldRun && !running){
        timers.SetTimer(staminaTimer, this, &UVitalsRegenComponent::onStaminaTick, FMath::Max(0.05f, getDefinition()->regenTickSeconds), true);
        UE_LOG(LogAwakenStats, Log, TEXT("[%s][%s] stamina regeneration started"), *UStatsLibrary::getRoleName(this), *UStatsLibrary::getPlayerId(GetOwner()));
    }else if(!shouldRun && running){
        timers.ClearTimer(staminaTimer);
        UE_LOG(LogAwakenStats, Log, TEXT("[%s][%s] stamina regeneration stopped"), *UStatsLibrary::getRoleName(this), *UStatsLibrary::getPlayerId(GetOwner()));
    }
}

void UVitalsRegenComponent::onHealthTick(){
    const UAS_CoreVitals* vitals = getVitals();
    const UDA_CoreVitalsDefinition* definition = getDefinition();
    if(vitals == nullptr || definition == nullptr){
        return;
    }
    if(!isHealthRegenEligible()){
        updateHealthTimer();
        return;
    }
    const float amount = FMath::Min(definition->healthRegenPerSecond * definition->regenTickSeconds, getCapHP() - vitals->getCurrentHP());
    UE_LOG(LogAwakenStats, Verbose, TEXT("[%s][%s] health regeneration tick %.3f"), *UStatsLibrary::getRoleName(this), *UStatsLibrary::getPlayerId(GetOwner()), amount);
    if(amount > 0.0f){
        // The hook re-evaluates after the write, so the timer stops by itself at the cap.
        UStatsLibrary::modifyHP(GetOwner(), amount, T_Stats_Source_Regeneration, GetOwner());
    }
}

void UVitalsRegenComponent::onStaminaTick(){
    const UDA_CoreVitalsDefinition* definition = getDefinition();
    if(definition == nullptr){
        return;
    }
    UE_LOG(LogAwakenStats, Verbose, TEXT("[%s][%s] stamina regeneration tick"), *UStatsLibrary::getRoleName(this), *UStatsLibrary::getPlayerId(GetOwner()));
    // The regeneration multiplier is applied by UMMC_StaminaDelta.
    UStatsLibrary::modifyStamina(GetOwner(), definition->staminaRegenPerSecond * definition->regenTickSeconds, T_Stats_Source_Regeneration);
}

// S5 step 6. On when health is at or below lowHealthPercent of maxHP, or when B03's isIll() is true.
void UVitalsRegenComponent::refreshLowHealthTag(){
    if(GetOwner() == nullptr || !GetOwner()->HasAuthority()){
        return;
    }
    UAbilitySystemComponent* asc = getAbilitySystem();
    const UAS_CoreVitals* vitals = getVitals();
    const UDA_CoreVitalsDefinition* definition = getDefinition();
    if(asc == nullptr || vitals == nullptr || definition == nullptr){
        return;
    }
    const USurvivalMeterComponent* survival = GetOwner()->FindComponentByClass<USurvivalMeterComponent>();
    const float threshold = definition->lowHealthPercent * 0.01f * vitals->getMaxHP();
    const bool isLow = vitals->getCurrentHP() <= threshold || (survival != nullptr && survival->isIll());
    const bool hasTag = asc->HasMatchingGameplayTag(T_Stats_State_LowHealth);
    if(isLow && !hasTag){
        // 5.8: no AddReplicatedLooseGameplayTag. The loose tag is the server-side state; the minimal
        // replication tag (ASC is in Mixed mode) is what carries it to the clients.
        asc->AddLooseGameplayTag(T_Stats_State_LowHealth);
        asc->AddMinimalReplicationGameplayTag(T_Stats_State_LowHealth);
        UE_LOG(LogAwakenStats, Log, TEXT("[%s][%s] low-health tag added"), *UStatsLibrary::getRoleName(this), *UStatsLibrary::getPlayerId(GetOwner()));
    }else if(!isLow && hasTag){
        asc->RemoveLooseGameplayTag(T_Stats_State_LowHealth);
        asc->RemoveMinimalReplicationGameplayTag(T_Stats_State_LowHealth);
        UE_LOG(LogAwakenStats, Log, TEXT("[%s][%s] low-health tag removed"), *UStatsLibrary::getRoleName(this), *UStatsLibrary::getPlayerId(GetOwner()));
    }
}

void UVitalsRegenComponent::regenerateToCap(){
    const UAS_CoreVitals* vitals = getVitals();
    if(vitals == nullptr || GetOwner() == nullptr || !GetOwner()->HasAuthority() || !isHealthRegenEligible()){
        return;
    }
    const float amount = getCapHP() - vitals->getCurrentHP();
    if(amount > 0.0f){
        UStatsLibrary::modifyHP(GetOwner(), amount, T_Stats_Source_Sleep, GetOwner());
    }
}