// Public/GameFrameworkClassTable/AwakenPlayerState.h
#pragma once
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "CheckpointRespawnFramework/00Enums/E_CheckpointRespawnFramework.h"
#include "SaveFramework/03Interfaces/BPI_SavePlayerSubject.h"
#include "TimeFramework/03Interfaces/BPI_SleepEligibility.h"
#include "AwakenPlayerState.generated.h"

class UAbilitySystemComponent;
class UAS_CoreVitals;
class USurvivalMeterComponent;
class UEquipmentComponent;
class UPlayerIdentityComponent;
class UGameplayAbility;
class UVitalsRegenComponent;
class UDA_AbilityLoadout;
class UDA_ActionDefinition;
class UDA_CoreVitalsDefinition;

UCLASS()
class THEAWAKEN_API AAwakenPlayerState : public APlayerState, public IAbilitySystemInterface, public IBPI_SavePlayerSubject, public IBPI_SleepEligibility{
    GENERATED_BODY()
public:
    AAwakenPlayerState();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION()
    void OnRep_cosmeticMeshIndex();

    UPROPERTY(ReplicatedUsing=OnRep_cosmeticMeshIndex)
    int32 cosmeticMeshIndex = 0;

    UPlayerIdentityComponent* getIdentityComponent() const{
        return playerIdentity;
    }

    // ---- Stats framework (Doc R08) and Ability & Action framework (Doc R04) ----
    UPROPERTY(EditDefaultsOnly, Category="Awaken|Stats")
    TObjectPtr<UDA_CoreVitalsDefinition> coreVitalsDefinition;

    UPROPERTY(EditDefaultsOnly, Category="Awaken|Ability")
    TObjectPtr<UDA_AbilityLoadout> abilityLoadout;

    // Index R04-R08-IG31: Dodge and Block are not in the player's loadout. Awaken.Action.Grant looks here after the player's loadout.
    // Point it at your enemy loadout, or at one that lists Dodge and Block.
    UPROPERTY(EditDefaultsOnly, Category="Awaken|Ability")
    TObjectPtr<UDA_AbilityLoadout> devGrantLoadout;

    // A3 step 4. Grants the loadout once, so a respawned pawn does not grant twice. Server only.
    void grantLoadoutOnce();
    // A5 step 1. Called on the host for every Progression.UnlockGranted message (BeginPlay registers the listener). Server only.
    void onUnlockGranted(const FGameplayTag& unlockTag);
    // Dev command helper: grants one action from the loadout (including unlock-gated ones) or from devGrantLoadout.
    bool grantAction(const FGameplayTag& actionTag);
    const UDA_ActionDefinition* findDefinitionByTag(const FGameplayTag& actionTag) const;

    // TEMPORARY until Doc B02 owns the life state.
    E_PlayerLifeState getLifeState() const;
    void setLifeState(E_PlayerLifeState newState);

    // ---- IBPI_SavePlayerSubject (Save guide, Step 5): what Save reads to key and name a Character file. ----
    virtual FString getPlayerIdentity_Implementation() const override;
    virtual void getLinkedPlayerIds_Implementation(TArray<FString>& linkedIds) const override;
    virtual FGuid getCharacterId_Implementation() const override;
    virtual FGuid getHeldBagId_Implementation() const override;
    virtual void setCharacterId_Implementation(FGuid newCharacterId) override;

    // ---- IBPI_SleepEligibility (Time guide, 11.1): the one adapter through which Doc R12 judges a player. ----
    virtual E_SleepCountState getSleepCountState_Implementation() override;
    virtual bool canRegisterToSleep_Implementation() override;
    virtual bool hasAwareHostileNearby_Implementation() override;

private:
    void bindGatingEvents();
    void onBlockingTagChanged(const FGameplayTag tag, int32 count);
    void onIncapacitatedChanged(const FGameplayTag tag, int32 count);
    void onCurrentStaminaChanged(const FOnAttributeChangeData& data);
    void onAbilityFailed(const UGameplayAbility* ability, const FGameplayTagContainer& reason);

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UAbilitySystemComponent> abilitySystem;

    UPROPERTY()
    TObjectPtr<UAS_CoreVitals> coreVitals;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UVitalsRegenComponent> vitalsRegen;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USurvivalMeterComponent> survivalMeters;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UEquipmentComponent> equipment;

    // Doc B05 step 7: the platform identity (primary id) is written once by the handshake. It is the key of the Character save, not saved itself.
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UPlayerIdentityComponent> playerIdentity;

    // Doc R13 3.10: a new unique id per character. Save writes it through setCharacterId (beginCharacterRestore) and stores it in the Character file header.
    UPROPERTY(Replicated)
    FGuid characterId;

    // Doc R01 sets this when a bag is held; invalid means Bagless. Server only, not replicated.
    UPROPERTY()
    FGuid heldBagId;

    bool abilitiesGranted = false;
    // The host-side listener of Progression.UnlockGranted. Registered in BeginPlay on the authority, released in EndPlay.
    FGameplayMessageListenerHandle progressionListener;
    E_PlayerLifeState lifeState = E_PlayerLifeState::Healthy;
    FGameplayTagContainer cancelOnBlockTags;
    FGameplayTagContainer cancelOnIncapacitatedTags;
};
