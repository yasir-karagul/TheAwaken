// Private/GameFrameworkClassTable/AwakenPlayerState.cpp
#include "GameFrameworkClassTable/AwakenPlayerState.h"
#include "GameFrameworkClassTable/AwakenPlayerController.h"
#include "StatsFramework/AS_CoreVitals.h"
#include "StatsFramework/StatsLibrary.h"
#include "StatsFramework/VitalsRegenComponent.h"
#include "AbilityActionFramework/AbilityLibrary.h"
#include "AbilityActionFramework/DA_AbilityLoadout.h"
#include "AbilityActionFramework/DA_ActionDefinition.h"
#include "AbilityActionFramework/01Structs/S_AbilityActionFramework.h"
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"
#include "CarryEncumbranceFramework/02Tags/T_CarryEncumbranceFramework.h"
#include "DialogueFramework/02Tags/T_DialogueFramework.h"
#include "Abilities/GameplayAbility.h"
#include "SurvivalTickFramework/SurvivalMeterComponent.h"
#include "EquipmentFramework/EquipmentComponent.h"
#include "SessionHandshakeFramework/PlayerIdentityComponent.h"
#include "TimeFramework/TimeSubsystem.h"
#include "ProgressionFramework/ProgressionComponent.h"
#include "ProgressionFramework/01Structs/S_ProgressionFramework.h"
#include "ProgressionFramework/02Tags/T_ProgressionFramework.h"
#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"

AAwakenPlayerState::AAwakenPlayerState(){
    abilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    abilitySystem->SetIsReplicated(true);
    abilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
    coreVitals = CreateDefaultSubobject<UAS_CoreVitals>(TEXT("CoreVitals"));
    vitalsRegen = CreateDefaultSubobject<UVitalsRegenComponent>(TEXT("VitalsRegen"));
    survivalMeters = CreateDefaultSubobject<USurvivalMeterComponent>(TEXT("SurvivalMeters"));
    equipment = CreateDefaultSubobject<UEquipmentComponent>(TEXT("Equipment"));
    playerIdentity = CreateDefaultSubobject<UPlayerIdentityComponent>(TEXT("IdentityComponent"));
    SetNetUpdateFrequency(100.0f);
}

UAbilitySystemComponent* AAwakenPlayerState::GetAbilitySystemComponent() const{
    return abilitySystem;
}

void AAwakenPlayerState::BeginPlay(){
    Super::BeginPlay();
    abilitySystem->AbilityFailedCallbacks.AddUObject(this, &AAwakenPlayerState::onAbilityFailed);
    if(HasAuthority()){
        coreVitals->initializeFromDefinition(coreVitalsDefinition);   // S1 step 8
        bindGatingEvents();
        abilitySystem->GetGameplayAttributeValueChangeDelegate(UAS_CoreVitals::currentStaminaAttribute()).AddUObject(this, &AAwakenPlayerState::onCurrentStaminaChanged);
        vitalsRegen->reevaluate();

        // Doc R06 7: the host hears every unlock raised on this machine and gives this player the abilities gated by it.
        // A client never registers: it has no authority, and the abilities are granted on the host only.
        const TWeakObjectPtr<AAwakenPlayerState> weakThis(this);
        TFunction<void(FGameplayTag, const FS_ProgressionUnlockMessage&)> onUnlock = [weakThis](FGameplayTag channel, const FS_ProgressionUnlockMessage& message){
            if(AAwakenPlayerState* state = weakThis.Get()){
                state->onUnlockGranted(message.unlockedTag);
            }
        };
        progressionListener = UGameplayMessageSubsystem::Get(this).RegisterListener<FS_ProgressionUnlockMessage>(T_Progression_UnlockGranted.GetTag(), MoveTemp(onUnlock));
    }
}

void AAwakenPlayerState::EndPlay(const EEndPlayReason::Type endPlayReason){
    progressionListener.Unregister();
    Super::EndPlay(endPlayReason);
}

void AAwakenPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AAwakenPlayerState, cosmeticMeshIndex);
    DOREPLIFETIME_CONDITION(AAwakenPlayerState, characterId, COND_OwnerOnly);   // Save reads it on the host; the owner may show it
}

void AAwakenPlayerState::OnRep_cosmeticMeshIndex(){
    // R07 applies the mesh to this player's pawn.
}

// ---- IBPI_SavePlayerSubject ------------------------------------------------------------------------------

FString AAwakenPlayerState::getPlayerIdentity_Implementation() const{
    // Empty until the handshake has written it. Save refuses a player with an empty id (NoIdentity), so a missing component fails closed.
    return playerIdentity != nullptr ? playerIdentity->getIdentity().primaryId : FString();
}

void AAwakenPlayerState::getLinkedPlayerIds_Implementation(TArray<FString>& linkedIds) const{
    linkedIds.Reset();
    if(playerIdentity != nullptr){
        linkedIds = playerIdentity->getIdentity().linkedIds;
    }
}

FGuid AAwakenPlayerState::getCharacterId_Implementation() const{
    return characterId;
}

FGuid AAwakenPlayerState::getHeldBagId_Implementation() const{
    // Doc R01 owns the bag and will set heldBagId. Until then it stays invalid, which Save reads as Bagless.
    return heldBagId;
}

void AAwakenPlayerState::setCharacterId_Implementation(FGuid newCharacterId){
    if(HasAuthority()){
        characterId = newCharacterId;
    }
}

// ---- IBPI_SleepEligibility -------------------------------------------------------------------------------

E_SleepCountState AAwakenPlayerState::getSleepCountState_Implementation(){
    // Downed, Restrained, Arriving (B02) and Bagless (R01) join this list when those frameworks exist.
    // Until then the only state besides Counted is Away, from the input heartbeat of Doc R12 11.1 (R12-IG13).
    if(!HasAuthority()){
        return E_SleepCountState::Counted;   // only the host judges
    }
    const UTimeSubsystem* time = UTimeSubsystem::get(this);
    const float awayIdleSeconds = time != nullptr ? time->getAwayIdleSeconds() : 0.0f;
    if(awayIdleSeconds <= 0.0f){
        return E_SleepCountState::Counted;   // idle detection is off until the designer sets awayIdleSeconds
    }
    const AAwakenPlayerController* controller = Cast<AAwakenPlayerController>(GetPlayerController());
    if(controller == nullptr || controller->getSecondsSinceLastInput() > awayIdleSeconds){
        return E_SleepCountState::Away;   // a player we cannot reach is never counted
    }
    return E_SleepCountState::Counted;
}

bool AAwakenPlayerState::canRegisterToSleep_Implementation(){
    // False while in a dialogue (Dialogue.State.Active, R11) or a scene: those frameworks add the check.
    return true;
}

bool AAwakenPlayerState::hasAwareHostileNearby_Implementation(){
    // Forwards to IsPlayerDetected of Doc B12 3.22 once the AI framework exists.
    return false;
}

// ---- Stats (Doc R08) and Ability & Action (Doc R04) -------------------------------------------------------

E_PlayerLifeState AAwakenPlayerState::getLifeState() const{
    return lifeState;
}

void AAwakenPlayerState::setLifeState(E_PlayerLifeState newState){
    lifeState = newState;
    vitalsRegen->reevaluate();
}

const UDA_ActionDefinition* AAwakenPlayerState::findDefinitionByTag(const FGameplayTag& actionTag) const{
    FGameplayAbilitySpec* spec = UAbilityLibrary::findSpecByActionTag(abilitySystem, actionTag);
    return spec != nullptr ? Cast<UDA_ActionDefinition>(spec->SourceObject.Get()) : nullptr;
}

void AAwakenPlayerState::grantLoadoutOnce(){
    if(!HasAuthority() || abilitiesGranted || abilityLoadout == nullptr){
        return;
    }
    abilitiesGranted = true;
    UAbilityLibrary::grantLoadout(abilitySystem, abilityLoadout, FGameplayTagContainer());

    // Catch-up (Doc R06). grantLoadout skips every unlock-gated entry, and the unlock message is raised only for a tag that enters the set later.
    // So a player who joins after an unlock, or into a loaded or Peaceful world, gets the gated entries whose unlock is already in the set here.
    // grantEntry skips an ability the player already has, so this is safe to repeat.
    const UProgressionComponent* progression = UProgressionComponent::findProgression(this);
    if(progression != nullptr){
        for(const FS_AbilityLoadoutEntry& entry : abilityLoadout->entries){
            if(entry.unlockTag.IsValid() && progression->isUnlocked(entry.unlockTag)){
                UAbilityLibrary::grantEntry(abilitySystem, entry);
            }
        }
    }
}

void AAwakenPlayerState::onUnlockGranted(const FGameplayTag& unlockTag){
    if(!HasAuthority() || abilityLoadout == nullptr){
        return;
    }
    UAbilityLibrary::grantUnlockEntries(abilitySystem, abilityLoadout, unlockTag, FGameplayTagContainer());
}

bool AAwakenPlayerState::grantAction(const FGameplayTag& actionTag){
    if(!HasAuthority()){
        return false;
    }
    const UDA_AbilityLoadout* sources[2] = {abilityLoadout, devGrantLoadout};
    for(const UDA_AbilityLoadout* loadout : sources){
        if(loadout == nullptr){
            continue;
        }
        for(const FS_AbilityLoadoutEntry& entry : loadout->entries){
            if(entry.actionTag == actionTag){
                return UAbilityLibrary::grantEntry(abilitySystem, entry);
            }
        }
    }
    UE_LOG(LogAwakenAbility, Warning, TEXT("[%s][%s] grantAction %s: no entry in the loadout or the dev grant loadout"), *UStatsLibrary::getRoleName(this), *UStatsLibrary::getPlayerId(this), *actionTag.ToString());
    return false;
}

// A6 step 2. ActivationBlockedTags only stops an ability from starting. A tag that arrives while an ability runs needs a cancel.
// The handlers act only on the authority; the cancel replicates to the owning client.
void AAwakenPlayerState::bindGatingEvents(){
    cancelOnBlockTags.Reset();
    cancelOnBlockTags.AddTag(T_Action_Dodge);
    cancelOnBlockTags.AddTag(T_Action_Sprint);
    cancelOnBlockTags.AddTag(T_Action_Block);
    cancelOnBlockTags.AddTag(T_Action_Reload);
    cancelOnBlockTags.AddTag(T_Action_WeaponCharge);
    cancelOnIncapacitatedTags = cancelOnBlockTags;
    cancelOnIncapacitatedTags.AddTag(T_Action_Crouch);
    abilitySystem->RegisterGameplayTagEvent(T_Player_State_Incapacitated, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AAwakenPlayerState::onIncapacitatedChanged);
    abilitySystem->RegisterGameplayTagEvent(T_Carry_State_Active, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AAwakenPlayerState::onBlockingTagChanged);
    abilitySystem->RegisterGameplayTagEvent(T_Dialogue_State_Active, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AAwakenPlayerState::onBlockingTagChanged);
}

void AAwakenPlayerState::onBlockingTagChanged(const FGameplayTag tag, int32 count){
    if(HasAuthority() && count > 0){
        abilitySystem->CancelAbilities(&cancelOnBlockTags);
    }
}

void AAwakenPlayerState::onIncapacitatedChanged(const FGameplayTag tag, int32 count){
    if(HasAuthority() && count > 0){
        abilitySystem->CancelAbilities(&cancelOnIncapacitatedTags);
    }
}

// A8 step 5. The Sprint latch clears on the server when stamina rises above restartThreshold.
void AAwakenPlayerState::onCurrentStaminaChanged(const FOnAttributeChangeData& data){
    if(!HasAuthority() || !abilitySystem->HasMatchingGameplayTag(T_Action_State_SprintExhausted)){
        return;
    }
    const UDA_ActionDefinition* sprint = findDefinitionByTag(T_Action_Sprint);
    const float threshold = sprint != nullptr ? sprint->restartThreshold : 0.0f;
    if(data.NewValue > threshold){
        abilitySystem->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(T_Action_State_SprintExhausted));
        UE_LOG(LogAwakenAbility, Log, TEXT("[%s][%s] sprint exhaustion latch cleared"), *UStatsLibrary::getRoleName(this), *UStatsLibrary::getPlayerId(this));
    }
}

// A4 step 5. One line per refusal with the reason tag and the machine role.
void AAwakenPlayerState::onAbilityFailed(const UGameplayAbility* ability, const FGameplayTagContainer& reason){
    UE_LOG(LogAwakenAbility, Log, TEXT("[%s][%s] activation refused: %s, reason %s"), *UStatsLibrary::getRoleName(this), *UStatsLibrary::getPlayerId(this), *GetNameSafe(ability), *reason.ToStringSimple());
}
