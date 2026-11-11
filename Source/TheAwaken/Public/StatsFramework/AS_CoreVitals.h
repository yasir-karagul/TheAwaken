#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "AS_CoreVitals.generated.h"

class UDA_CoreVitalsDefinition;
class UAbilitySystemComponent;
class UGameplayMessageSubsystem;
struct FGameplayEffectModCallbackData;

// Doc R08 S1. Hand-written camelCase accessors instead of the GAS macro (gap 18).
UCLASS()
class THEAWAKEN_API UAS_CoreVitals : public UAttributeSet{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_currentHP)
    FGameplayAttributeData currentHP;
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_maxHP)
    FGameplayAttributeData maxHP;
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_currentStamina)
    FGameplayAttributeData currentStamina;
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_maxStamina)
    FGameplayAttributeData maxStamina;
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_staminaDrainMultiplier)
    FGameplayAttributeData staminaDrainMultiplier;
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_staminaRegenMultiplier)
    FGameplayAttributeData staminaRegenMultiplier;
    // Not replicated: Combat reads it on the authority.
    UPROPERTY(BlueprintReadOnly)
    FGameplayAttributeData aggregatedArmor;
    // Meta attribute, never replicated, always read back to 0.
    UPROPERTY(BlueprintReadOnly)
    FGameplayAttributeData incomingHP;

    UPROPERTY()
    TObjectPtr<UDA_CoreVitalsDefinition> definition;

    // Server only. An Essential NPC stops at the health floor (S7 step 4).
    bool essential = false;

    float getCurrentHP() const{
        return currentHP.GetCurrentValue();
    }
    float getMaxHP() const{
        return maxHP.GetCurrentValue();
    }
    float getCurrentStamina() const{
        return currentStamina.GetCurrentValue();
    }
    float getMaxStamina() const{
        return maxStamina.GetCurrentValue();
    }
    float getStaminaDrainMultiplier() const{
        return staminaDrainMultiplier.GetCurrentValue();
    }
    float getStaminaRegenMultiplier() const{
        return staminaRegenMultiplier.GetCurrentValue();
    }
    float getAggregatedArmor() const{
        return aggregatedArmor.GetCurrentValue();
    }

    static FGameplayAttribute currentHPAttribute(){
        return FGameplayAttribute(FindFieldChecked<FProperty>(UAS_CoreVitals::StaticClass(), GET_MEMBER_NAME_CHECKED(UAS_CoreVitals, currentHP)));
    }
    static FGameplayAttribute maxHPAttribute(){
        return FGameplayAttribute(FindFieldChecked<FProperty>(UAS_CoreVitals::StaticClass(), GET_MEMBER_NAME_CHECKED(UAS_CoreVitals, maxHP)));
    }
    static FGameplayAttribute currentStaminaAttribute(){
        return FGameplayAttribute(FindFieldChecked<FProperty>(UAS_CoreVitals::StaticClass(), GET_MEMBER_NAME_CHECKED(UAS_CoreVitals, currentStamina)));
    }
    static FGameplayAttribute maxStaminaAttribute(){
        return FGameplayAttribute(FindFieldChecked<FProperty>(UAS_CoreVitals::StaticClass(), GET_MEMBER_NAME_CHECKED(UAS_CoreVitals, maxStamina)));
    }
    static FGameplayAttribute staminaDrainMultiplierAttribute(){
        return FGameplayAttribute(FindFieldChecked<FProperty>(UAS_CoreVitals::StaticClass(), GET_MEMBER_NAME_CHECKED(UAS_CoreVitals, staminaDrainMultiplier)));
    }
    static FGameplayAttribute staminaRegenMultiplierAttribute(){
        return FGameplayAttribute(FindFieldChecked<FProperty>(UAS_CoreVitals::StaticClass(), GET_MEMBER_NAME_CHECKED(UAS_CoreVitals, staminaRegenMultiplier)));
    }
    static FGameplayAttribute aggregatedArmorAttribute(){
        return FGameplayAttribute(FindFieldChecked<FProperty>(UAS_CoreVitals::StaticClass(), GET_MEMBER_NAME_CHECKED(UAS_CoreVitals, aggregatedArmor)));
    }
    static FGameplayAttribute incomingHPAttribute(){
        return FGameplayAttribute(FindFieldChecked<FProperty>(UAS_CoreVitals::StaticClass(), GET_MEMBER_NAME_CHECKED(UAS_CoreVitals, incomingHP)));
    }

    // S1 step 8, server only. Initialisation is one of the two allowed direct writes (S2 step 8).
    void initializeFromDefinition(UDA_CoreVitalsDefinition* newDefinition);
    // S7, server only. For an NPC pawn: maxHP and armor from the profile, stamina only when used.
    void initializeNpc(UDA_CoreVitalsDefinition* newDefinition, float newMaxHP, float armor, bool usesStamina, bool isEssential);

    // S10. Raise only on the machine that owns the player; both functions check this themselves.
    void raiseHPChanged(const FGameplayTag& sourceTag) const;
    void raiseStaminaChanged() const;
    bool isLocalPlayerTarget() const;
    UGameplayMessageSubsystem* getMessages() const;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual bool PreGameplayEffectExecute(FGameplayEffectModCallbackData& data) override;
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& data) override;
    virtual void PostAttributeChange(const FGameplayAttribute& attribute, float oldValue, float newValue) override;

    UFUNCTION()
    void OnRep_currentHP(const FGameplayAttributeData& oldValue);
    UFUNCTION()
    void OnRep_maxHP(const FGameplayAttributeData& oldValue);
    UFUNCTION()
    void OnRep_currentStamina(const FGameplayAttributeData& oldValue);
    UFUNCTION()
    void OnRep_maxStamina(const FGameplayAttributeData& oldValue);
    UFUNCTION()
    void OnRep_staminaDrainMultiplier(const FGameplayAttributeData& oldValue);
    UFUNCTION()
    void OnRep_staminaRegenMultiplier(const FGameplayAttributeData& oldValue);

private:
    bool isHealthLossRefused(FName& reason) const;
    bool isFloorProtected(float& floorValue) const;
    void handleIncomingHP(const FGameplayEffectModCallbackData& data);
    void handleCurrentStamina();
    void clampCurrentToMaximums();
};