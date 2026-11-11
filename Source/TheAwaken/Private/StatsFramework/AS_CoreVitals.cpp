#include "StatsFramework/AS_CoreVitals.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "StatsFramework/StatsLibrary.h"
#include "StatsFramework/DA_CoreVitalsDefinition.h"
#include "StatsFramework/VitalsRegenComponent.h"
#include "StatsFramework/01Structs/S_StatsFramework.h"
#include "StatsFramework/02Tags/T_StatsFramework.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"
#include "CombatFramework/CombatComponent.h"
#include "DifficultyFramework/DifficultySubsystem.h"
#include "CarryEncumbranceFramework/02Tags/T_CarryEncumbranceFramework.h"
#include "DialogueFramework/02Tags/T_DialogueFramework.h"

namespace{
    // Picks the DamageType.* or Stats.Source.* tag out of an effect's asset tags, for messages and logs.
    FGameplayTag pickSourceTag(const FGameplayTagContainer& tags){
        const FGameplayTag damageRoot = FGameplayTag::RequestGameplayTag(FName("DamageType"), false);
        const FGameplayTag sourceRoot = FGameplayTag::RequestGameplayTag(FName("Stats.Source"), false);
        for(const FGameplayTag& tag : tags){
            if(damageRoot.IsValid() && tag.MatchesTag(damageRoot)){
                return tag;
            }
            if(sourceRoot.IsValid() && tag.MatchesTag(sourceRoot)){
                return tag;
            }
        }
        return FGameplayTag();
    }
}

void UAS_CoreVitals::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION_NOTIFY(UAS_CoreVitals, currentHP, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAS_CoreVitals, maxHP, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAS_CoreVitals, currentStamina, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAS_CoreVitals, maxStamina, COND_None, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAS_CoreVitals, staminaDrainMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
    DOREPLIFETIME_CONDITION_NOTIFY(UAS_CoreVitals, staminaRegenMultiplier, COND_OwnerOnly, REPNOTIFY_Always);
}

void UAS_CoreVitals::OnRep_currentHP(const FGameplayAttributeData& oldValue){
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_CoreVitals, currentHP, oldValue);
    raiseHPChanged(FGameplayTag());
}

void UAS_CoreVitals::OnRep_maxHP(const FGameplayAttributeData& oldValue){
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_CoreVitals, maxHP, oldValue);
    raiseHPChanged(FGameplayTag());
}

void UAS_CoreVitals::OnRep_currentStamina(const FGameplayAttributeData& oldValue){
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_CoreVitals, currentStamina, oldValue);
    raiseStaminaChanged();
}

void UAS_CoreVitals::OnRep_maxStamina(const FGameplayAttributeData& oldValue){
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_CoreVitals, maxStamina, oldValue);
    raiseStaminaChanged();
}

void UAS_CoreVitals::OnRep_staminaDrainMultiplier(const FGameplayAttributeData& oldValue){
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_CoreVitals, staminaDrainMultiplier, oldValue);
}

void UAS_CoreVitals::OnRep_staminaRegenMultiplier(const FGameplayAttributeData& oldValue){
    GAMEPLAYATTRIBUTE_REPNOTIFY(UAS_CoreVitals, staminaRegenMultiplier, oldValue);
}

void UAS_CoreVitals::initializeFromDefinition(UDA_CoreVitalsDefinition* newDefinition){
    UAbilitySystemComponent* asc = GetOwningAbilitySystemComponent();
    if(asc == nullptr || newDefinition == nullptr || !GetOwningActor()->HasAuthority()){
        return;
    }
    definition = newDefinition;
    asc->SetNumericAttributeBase(maxHPAttribute(), newDefinition->maxHP);
    asc->SetNumericAttributeBase(maxStaminaAttribute(), newDefinition->maxStamina);
    asc->SetNumericAttributeBase(staminaDrainMultiplierAttribute(), 1.0f);
    asc->SetNumericAttributeBase(staminaRegenMultiplierAttribute(), 1.0f);
    // Health and stamina start full (R08, Index A106 S3-F15).
    asc->SetNumericAttributeBase(currentHPAttribute(), newDefinition->maxHP);
    asc->SetNumericAttributeBase(currentStaminaAttribute(), newDefinition->maxStamina);
}

void UAS_CoreVitals::initializeNpc(UDA_CoreVitalsDefinition* newDefinition, float newMaxHP, float armor, bool usesStamina, bool isEssential){
    UAbilitySystemComponent* asc = GetOwningAbilitySystemComponent();
    if(asc == nullptr || !GetOwningActor()->HasAuthority()){
        return;
    }
    definition = newDefinition;
    essential = isEssential;
    asc->SetNumericAttributeBase(maxHPAttribute(), newMaxHP);
    asc->SetNumericAttributeBase(currentHPAttribute(), newMaxHP);
    asc->SetNumericAttributeBase(aggregatedArmorAttribute(), armor);
    asc->SetNumericAttributeBase(staminaDrainMultiplierAttribute(), 1.0f);
    asc->SetNumericAttributeBase(staminaRegenMultiplierAttribute(), 1.0f);
    const float staminaMax = (usesStamina && newDefinition != nullptr) ? newDefinition->maxStamina : 0.0f;
    asc->SetNumericAttributeBase(maxStaminaAttribute(), staminaMax);
    asc->SetNumericAttributeBase(currentStaminaAttribute(), staminaMax);
}

bool UAS_CoreVitals::isLocalPlayerTarget() const{
    const APlayerState* playerState = Cast<APlayerState>(GetOwningActor());
    if(playerState == nullptr){
        return false;
    }
    const APlayerController* controller = playerState->GetPlayerController();
    return controller != nullptr && controller->IsLocalController();
}

UGameplayMessageSubsystem* UAS_CoreVitals::getMessages() const{
    // Not UGameplayMessageSubsystem::Get(...): it asserts when the game instance is already gone (world teardown).
    const AActor* owner = GetOwningActor();
    const UWorld* world = owner != nullptr ? owner->GetWorld() : nullptr;
    UGameInstance* gameInstance = world != nullptr ? world->GetGameInstance() : nullptr;
    return gameInstance != nullptr ? gameInstance->GetSubsystem<UGameplayMessageSubsystem>() : nullptr;
}

void UAS_CoreVitals::raiseHPChanged(const FGameplayTag& sourceTag) const{
    if(!isLocalPlayerTarget()){
        return;
    }
    FS_HPChangedMessage message;
    message.playerId = UStatsLibrary::getPlayerId(GetOwningActor());
    message.newValue = getCurrentHP();
    message.maxValue = getMaxHP();
    message.sourceTag = sourceTag;
    UGameplayMessageSubsystem* messages = getMessages();
    if(messages != nullptr){
        messages->BroadcastMessage(T_Stats_HPChanged, message);
    }
}

void UAS_CoreVitals::raiseStaminaChanged() const{
    if(!isLocalPlayerTarget()){
        return;
    }
    FS_StaminaChangedMessage message;
    message.playerId = UStatsLibrary::getPlayerId(GetOwningActor());
    message.newValue = getCurrentStamina();
    message.maxValue = getMaxStamina();
    UGameplayMessageSubsystem* messages = getMessages();
    if(messages != nullptr){
        messages->BroadcastMessage(T_Stats_StaminaChanged, message);
    }
}

bool UAS_CoreVitals::isHealthLossRefused(FName& reason) const{
    UAbilitySystemComponent* asc = GetOwningAbilitySystemComponent();
    if(asc == nullptr){
        return false;
    }
    if(asc->HasMatchingGameplayTag(T_Stats_Dev_God)){
        reason = FName("Stats.Dev.God");
        return true;
    }
    if(asc->HasMatchingGameplayTag(T_Dialogue_State_Active)){
        reason = FName("Dialogue.State.Active");
        return true;
    }
    if(asc->HasMatchingGameplayTag(T_Player_State_Incapacitated)){
        reason = FName("Player.State.Incapacitated");
        return true;
    }
    if(getCurrentHP() <= 0.0f){
        reason = FName("HealthAlreadyZero");
        return true;
    }
    return false;
}

bool UAS_CoreVitals::isFloorProtected(float& floorValue) const{
    const AActor* owner = GetOwningActor();
    if(owner == nullptr){
        return false;
    }
    if(owner->IsA<APlayerState>()){
        if(!UDifficultySubsystem::canPlayerDie(owner)){
            floorValue = UDifficultySubsystem::getHealthFloor(owner);
            return true;
        }
        return false;
    }
    if(essential){
        floorValue = UDifficultySubsystem::getHealthFloor(owner);
        return true;
    }
    return false;
}

bool UAS_CoreVitals::PreGameplayEffectExecute(FGameplayEffectModCallbackData& data){
    if(!Super::PreGameplayEffectExecute(data)){
        return false;
    }
    if(data.EvaluatedData.Attribute == incomingHPAttribute() && data.EvaluatedData.Magnitude < 0.0f){
        FName reason;
        if(isHealthLossRefused(reason)){
            UE_LOG(LogAwakenStats, Log, TEXT("[%s][%s] health loss refused: %s"), *UStatsLibrary::getRoleName(GetOwningActor()), *UStatsLibrary::getPlayerId(GetOwningActor()), *reason.ToString());
            return false;
        }
    }
    return true;
}

void UAS_CoreVitals::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& data){
    Super::PostGameplayEffectExecute(data);
    const FGameplayAttribute attribute = data.EvaluatedData.Attribute;
    if(attribute == incomingHPAttribute()){
        handleIncomingHP(data);
    }else if(attribute == currentHPAttribute()){
        // S2 step 6: an effect broke the authoring rule of A11. Warn, clamp, do nothing else.
        UE_LOG(LogAwakenStats, Warning, TEXT("[%s][%s] effect %s wrote currentHP directly. Fix the effect (A11): health changes go through incomingHP."), *UStatsLibrary::getRoleName(GetOwningActor()), *UStatsLibrary::getPlayerId(GetOwningActor()), *GetNameSafe(data.EffectSpec.Def));
        UAbilitySystemComponent* asc = GetOwningAbilitySystemComponent();
        if(asc != nullptr){
            asc->SetNumericAttributeBase(currentHPAttribute(), FMath::Clamp(getCurrentHP(), 0.0f, getMaxHP()));
        }
    }else if(attribute == currentStaminaAttribute()){
        handleCurrentStamina();
    }else if(attribute == maxStaminaAttribute() || attribute == maxHPAttribute()){
        clampCurrentToMaximums();
    }
}

void UAS_CoreVitals::handleIncomingHP(const FGameplayEffectModCallbackData& data){
    UAbilitySystemComponent* asc = GetOwningAbilitySystemComponent();
    if(asc == nullptr){
        return;
    }
    AActor* owner = GetOwningActor();
    const float delta = data.EvaluatedData.Magnitude;
    const float previousHP = getCurrentHP();
    asc->SetNumericAttributeBase(incomingHPAttribute(), 0.0f);

    float result = FMath::Clamp(previousHP + delta, 0.0f, getMaxHP());
    float floorValue = 0.0f;
    if(delta < 0.0f && isFloorProtected(floorValue)){
        result = FMath::Max(result, floorValue);
    }
    asc->SetNumericAttributeBase(currentHPAttribute(), result);

    FGameplayTagContainer assetTags;
    data.EffectSpec.GetAllAssetTags(assetTags);
    const FGameplayTag sourceTag = pickSourceTag(assetTags);

    if(delta != 0.0f && sourceTag != T_Stats_Source_Regeneration.GetTag()){
        UE_LOG(LogAwakenStats, Log, TEXT("[%s][%s] health %s by %.2f (%.2f -> %.2f), source %s"), *UStatsLibrary::getRoleName(owner), *UStatsLibrary::getPlayerId(owner), delta < 0.0f ? TEXT("lost") : TEXT("gained"), FMath::Abs(delta), previousHP, result, *sourceTag.ToString());
    }

    // Depletion fires once, only from a previous health above 0 (S2 step 5.5).
    if(delta < 0.0f && previousHP > 0.0f && result <= 0.0f){
        UE_LOG(LogAwakenStats, Log, TEXT("[%s][%s] health reached 0, damage tags: %s"), *UStatsLibrary::getRoleName(owner), *UStatsLibrary::getPlayerId(owner), *assetTags.ToStringSimple());
        UCombatComponent* combat = UStatsLibrary::findCombatComponent(owner);
        if(combat != nullptr){
            combat->onHealthDepleted(data.EffectSpec.GetEffectContext(), assetTags);
        }
    }

    raiseHPChanged(sourceTag);
    UVitalsRegenComponent* regen = owner != nullptr ? owner->FindComponentByClass<UVitalsRegenComponent>() : nullptr;
    if(regen != nullptr){
        regen->reevaluate();
    }
}

void UAS_CoreVitals::handleCurrentStamina(){
    UAbilitySystemComponent* asc = GetOwningAbilitySystemComponent();
    if(asc == nullptr){
        return;
    }
    AActor* owner = GetOwningActor();
    const float rawValue = getCurrentStamina();
    const float clamped = FMath::Clamp(rawValue, 0.0f, getMaxStamina());
    if(clamped != rawValue){
        asc->SetNumericAttributeBase(currentStaminaAttribute(), clamped);
    }
    if(clamped <= 0.0f){
        UE_LOG(LogAwakenStats, Log, TEXT("[%s][%s] stamina reached 0"), *UStatsLibrary::getRoleName(owner), *UStatsLibrary::getPlayerId(owner));
    }else if(clamped >= getMaxStamina()){
        UE_LOG(LogAwakenStats, Log, TEXT("[%s][%s] stamina full again"), *UStatsLibrary::getRoleName(owner), *UStatsLibrary::getPlayerId(owner));
    }
    raiseStaminaChanged();
    UVitalsRegenComponent* regen = owner != nullptr ? owner->FindComponentByClass<UVitalsRegenComponent>() : nullptr;
    if(regen != nullptr){
        regen->reevaluate();
    }
}

void UAS_CoreVitals::clampCurrentToMaximums(){
    UAbilitySystemComponent* asc = GetOwningAbilitySystemComponent();
    if(asc == nullptr){
        return;
    }
    if(getCurrentStamina() > getMaxStamina()){
        asc->SetNumericAttributeBase(currentStaminaAttribute(), getMaxStamina());
    }
    if(getCurrentHP() > getMaxHP()){
        asc->SetNumericAttributeBase(currentHPAttribute(), getMaxHP());
    }
}

// S3 step 4: duration and infinite effects change a maximum without calling the execute hooks.
// Verify in 5.8 that this hook fires for modifier changes from duration and infinite effects.
void UAS_CoreVitals::PostAttributeChange(const FGameplayAttribute& attribute, float oldValue, float newValue){
    Super::PostAttributeChange(attribute, oldValue, newValue);
    UAbilitySystemComponent* asc = GetOwningAbilitySystemComponent();
    if(asc == nullptr || !asc->GetOwner()->HasAuthority()){
        return;
    }
    if(attribute == maxStaminaAttribute() && getCurrentStamina() > newValue){
        UE_LOG(LogAwakenStats, Log, TEXT("[%s][%s] maxStamina changed to %.2f, current stamina clamped"), *UStatsLibrary::getRoleName(GetOwningActor()), *UStatsLibrary::getPlayerId(GetOwningActor()), newValue);
        asc->SetNumericAttributeBase(currentStaminaAttribute(), newValue);
    }
    if(attribute == maxHPAttribute() && getCurrentHP() > newValue){
        UE_LOG(LogAwakenStats, Log, TEXT("[%s][%s] maxHP changed to %.2f, current health clamped"), *UStatsLibrary::getRoleName(GetOwningActor()), *UStatsLibrary::getPlayerId(GetOwningActor()), newValue);
        asc->SetNumericAttributeBase(currentHPAttribute(), newValue);
    }
}