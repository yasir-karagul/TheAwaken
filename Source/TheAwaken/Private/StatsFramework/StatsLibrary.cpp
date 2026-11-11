#include "StatsFramework/StatsLibrary.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFrameworkClassTable/AwakenPlayerState.h"
#include "SessionHandshakeFramework/PlayerIdentityComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "StatsFramework/AS_CoreVitals.h"
#include "StatsFramework/DA_CoreVitalsDefinition.h"
#include "StatsFramework/GE_ModifyHP.h"
#include "StatsFramework/GE_ModifyStamina.h"
#include "StatsFramework/VitalsRegenComponent.h"
#include "CombatFramework/CombatComponent.h"
#include "AINPCFramework/DA_NPCProfile.h"
#include "DifficultyFramework/DifficultySubsystem.h"

DEFINE_LOG_CATEGORY(LogAwakenStats);

FString UStatsLibrary::getRoleName(const UObject* worldContext){
    const UWorld* world = worldContext != nullptr ? worldContext->GetWorld() : nullptr;
    if(world == nullptr){
        return TEXT("Offline");
    }
    if(world->GetNetMode() == NM_Client){
        return TEXT("Client");
    }
    if(world->GetNetMode() == NM_Standalone){
        return TEXT("Offline");
    }
    return TEXT("Host");
}

FString UStatsLibrary::getPlayerId(const AActor* actor){
    const APlayerState* playerState = Cast<const APlayerState>(actor);
    if(playerState == nullptr){
        const APawn* pawn = Cast<const APawn>(actor);
        if(pawn != nullptr){
            playerState = pawn->GetPlayerState();
        }
    }
    if(playerState == nullptr){
        return actor != nullptr ? actor->GetName() : FString(TEXT("None"));
    }
    const AAwakenPlayerState* awakenState = Cast<const AAwakenPlayerState>(playerState);
    const UPlayerIdentityComponent* identity = awakenState != nullptr ? awakenState->getIdentityComponent() : nullptr;
    if(identity != nullptr && identity->hasIdentity()){
        return identity->getIdentity().primaryId;
    }
    return playerState->GetPlayerName();
}

UAbilitySystemComponent* UStatsLibrary::getAbilitySystem(AActor* target){
    return UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(target);
}

const UAS_CoreVitals* UStatsLibrary::getVitals(AActor* target){
    UAbilitySystemComponent* asc = getAbilitySystem(target);
    return asc != nullptr ? asc->GetSet<UAS_CoreVitals>() : nullptr;
}

AActor* UStatsLibrary::getVitalsOwner(AActor* target){
    UAbilitySystemComponent* asc = getAbilitySystem(target);
    return asc != nullptr ? asc->GetOwner() : nullptr;
}

UCombatComponent* UStatsLibrary::findCombatComponent(AActor* owner){
    if(owner == nullptr){
        return nullptr;
    }
    UCombatComponent* combat = owner->FindComponentByClass<UCombatComponent>();
    if(combat == nullptr){
        const APlayerState* playerState = Cast<APlayerState>(owner);
        if(playerState != nullptr && playerState->GetPawn() != nullptr){
            combat = playerState->GetPawn()->FindComponentByClass<UCombatComponent>();   // Combat lives on the Character
        }
    }
    return combat;
}

bool UStatsLibrary::requireAuthority(const UAbilitySystemComponent* asc, const TCHAR* functionName){
    if(asc == nullptr){
        return false;
    }
    if(!asc->GetOwner()->HasAuthority()){
        UE_LOG(LogAwakenStats, Warning, TEXT("[%s][%s] client attempt to call server-only %s, refused"), *UStatsLibrary::getRoleName(asc->GetOwner()), *UStatsLibrary::getPlayerId(asc->GetOwner()), functionName);
        return false;
    }
    return true;
}

void UStatsLibrary::modifyHP(AActor* target, float delta, const FGameplayTag& sourceTag, AActor* instigator){
    UAbilitySystemComponent* asc = getAbilitySystem(target);
    if(!requireAuthority(asc, TEXT("modifyHP"))){
        return;
    }
    FGameplayEffectContextHandle context = asc->MakeEffectContext();
    context.AddInstigator(instigator, instigator);
    FGameplayEffectSpecHandle spec = asc->MakeOutgoingSpec(UGE_ModifyHP::StaticClass(), 1.0f, context);
    if(!spec.IsValid()){
        return;
    }
    if(sourceTag.IsValid()){
        spec.Data->AddDynamicAssetTag(sourceTag);
    }
    spec.Data->SetSetByCallerMagnitude(FName("delta"), delta);
    asc->ApplyGameplayEffectSpecToSelf(*spec.Data.Get());
}

void UStatsLibrary::modifyStamina(AActor* target, float delta, const FGameplayTag& sourceTag){
    UAbilitySystemComponent* asc = getAbilitySystem(target);
    if(!requireAuthority(asc, TEXT("modifyStamina")) || delta == 0.0f){
        return;
    }
    FGameplayEffectSpecHandle spec = asc->MakeOutgoingSpec(UGE_ModifyStamina::StaticClass(), 1.0f, asc->MakeEffectContext());
    if(!spec.IsValid()){
        return;
    }
    if(sourceTag.IsValid()){
        spec.Data->AddDynamicAssetTag(sourceTag);
    }
    spec.Data->SetSetByCallerMagnitude(FName("delta"), delta);
    asc->ApplyGameplayEffectSpecToSelf(*spec.Data.Get());
    UE_LOG(LogAwakenStats, Verbose, TEXT("[%s][%s] modifyStamina %.2f, source %s"), *UStatsLibrary::getRoleName(asc->GetOwner()), *UStatsLibrary::getPlayerId(asc->GetOwner()), delta, *sourceTag.ToString());
}

void UStatsLibrary::setStamina(AActor* target, float value){
    UAbilitySystemComponent* asc = getAbilitySystem(target);
    if(!requireAuthority(asc, TEXT("setStamina"))){
        return;
    }
    const UAS_CoreVitals* vitals = asc->GetSet<UAS_CoreVitals>();
    if(vitals == nullptr){
        return;
    }
    asc->SetNumericAttributeBase(UAS_CoreVitals::currentStaminaAttribute(), FMath::Clamp(value, 0.0f, vitals->getMaxStamina()));
    vitals->raiseStaminaChanged();
    UVitalsRegenComponent* regen = asc->GetOwner()->FindComponentByClass<UVitalsRegenComponent>();
    if(regen != nullptr){
        regen->reevaluate();
    }
    UE_LOG(LogAwakenStats, Log, TEXT("[%s][%s] setStamina %.2f"), *UStatsLibrary::getRoleName(asc->GetOwner()), *UStatsLibrary::getPlayerId(asc->GetOwner()), vitals->getCurrentStamina());
}

void UStatsLibrary::setMaxHP(AActor* target, float value){
    UAbilitySystemComponent* asc = getAbilitySystem(target);
    if(!requireAuthority(asc, TEXT("setMaxHP"))){
        return;
    }
    const UAS_CoreVitals* vitals = asc->GetSet<UAS_CoreVitals>();
    if(vitals == nullptr || value <= 0.0f){
        return;
    }
    const float oldMax = vitals->getMaxHP();
    const float fraction = oldMax > 0.0f ? vitals->getCurrentHP() / oldMax : 1.0f;
    asc->SetNumericAttributeBase(UAS_CoreVitals::maxHPAttribute(), value);
    asc->SetNumericAttributeBase(UAS_CoreVitals::currentHPAttribute(), FMath::Clamp(fraction * value, 0.0f, value));
    vitals->raiseHPChanged(FGameplayTag());
}

void UStatsLibrary::applyArmorAggregate(AActor* target, float totalArmorValue){
    UAbilitySystemComponent* asc = getAbilitySystem(target);
    if(!requireAuthority(asc, TEXT("applyArmorAggregate"))){
        return;
    }
    asc->SetNumericAttributeBase(UAS_CoreVitals::aggregatedArmorAttribute(), totalArmorValue);
}

FS_StatsSaveRecord UStatsLibrary::getSnapshotForSave(AActor* target){
    FS_StatsSaveRecord record;
    const UAS_CoreVitals* vitals = getVitals(target);
    if(vitals != nullptr){
        record.currentHP = vitals->getCurrentHP();
        record.currentStamina = vitals->getCurrentStamina();
    }
    return record;
}

void UStatsLibrary::refreshAfterRestore(AActor* owner){
    const UAS_CoreVitals* vitals = getVitals(owner);
    if(vitals != nullptr){
        vitals->raiseHPChanged(FGameplayTag());
        vitals->raiseStaminaChanged();
    }
    UVitalsRegenComponent* regen = owner != nullptr ? owner->FindComponentByClass<UVitalsRegenComponent>() : nullptr;
    if(regen != nullptr){
        regen->reevaluate();
    }
}

// S9. Restore writes the base values directly so it never raises depletion. Restore timed effects and
// meter tiers BEFORE calling this, or currentStamina is clamped to the wrong maximum (gap 15).
void UStatsLibrary::restoreFromSnapshot(AActor* target, const FS_StatsSaveRecord& record){
    UAbilitySystemComponent* asc = getAbilitySystem(target);
    if(!requireAuthority(asc, TEXT("restoreFromSnapshot"))){
        return;
    }
    const UAS_CoreVitals* vitals = asc->GetSet<UAS_CoreVitals>();
    if(vitals == nullptr){
        return;
    }
    if(record.currentHP < 1.0f){
        UE_LOG(LogAwakenStats, Warning, TEXT("[%s][%s] saved health %.2f is below 1, restored as 1"), *UStatsLibrary::getRoleName(asc->GetOwner()), *UStatsLibrary::getPlayerId(asc->GetOwner()), record.currentHP);
    }
    const float hp = FMath::Clamp(record.currentHP, 1.0f, vitals->getMaxHP());
    const float stamina = FMath::Clamp(record.currentStamina, 0.0f, vitals->getMaxStamina());
    asc->SetNumericAttributeBase(UAS_CoreVitals::currentHPAttribute(), hp);
    asc->SetNumericAttributeBase(UAS_CoreVitals::currentStaminaAttribute(), stamina);
    UE_LOG(LogAwakenStats, Log, TEXT("[%s][%s] snapshot restored: health %.2f, stamina %.2f"), *UStatsLibrary::getRoleName(asc->GetOwner()), *UStatsLibrary::getPlayerId(asc->GetOwner()), hp, stamina);
    refreshAfterRestore(asc->GetOwner());
}

void UStatsLibrary::regenerateToCap(AActor* target){
    UAbilitySystemComponent* asc = getAbilitySystem(target);
    if(!requireAuthority(asc, TEXT("regenerateToCap"))){
        return;
    }
    UVitalsRegenComponent* regen = asc->GetOwner()->FindComponentByClass<UVitalsRegenComponent>();
    if(regen != nullptr){
        regen->regenerateToCap();
    }
}

// S7. maxHP and armor come from the profile; the difficulty multiplier scales maxHP once, at spawn.
void UStatsLibrary::initializeNpcVitals(AActor* npc, const UDA_NPCProfile* profile){
    UAbilitySystemComponent* asc = getAbilitySystem(npc);
    if(!requireAuthority(asc, TEXT("initializeNpcVitals")) || profile == nullptr){
        return;
    }
    UAS_CoreVitals* vitals = const_cast<UAS_CoreVitals*>(asc->GetSet<UAS_CoreVitals>());
    if(vitals == nullptr){
        return;
    }
    const float scaledMaxHP = profile->maxHP * UDifficultySubsystem::getEnemyHealthMultiplier(npc);
    vitals->initializeNpc(profile->vitalsDefinition, scaledMaxHP, profile->armor, profile->usesStamina, profile->essential);
}
