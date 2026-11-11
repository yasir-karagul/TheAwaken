#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "VitalsRegenComponent.generated.h"

class UAbilitySystemComponent;
class UAS_CoreVitals;
class UDA_CoreVitalsDefinition;

// S3 step 7 and S5. Server-side timers for stamina and health regeneration, the regeneration cap and
// the low-health tag. Put it on the same actor as the ASC (the PlayerState). Name is PROPOSED.
// It re-evaluates on events only, never by polling. These callers must invoke reevaluate():
//   currentHP and currentStamina changes (the attribute set does), Action.Sprint and Stats.State.MedicineActive
//   changes (bound here), B03 on a tier change, a blood change or an Illness change, B02 on a life-state change.
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API UVitalsRegenComponent : public UActorComponent{
    GENERATED_BODY()
public:
    UVitalsRegenComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type endPlayReason) override;

    // Starts or stops the timers and refreshes the low-health tag. Server only.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Stats")
    void reevaluate();

    // S5 step 2. Percent of maxHP, the lowest limit that applies.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Stats")
    float getRegenCap() const;
    // Same, and names which limit applies: Natural, HungryOrThirsty or Blood.
    float getRegenCapWithSource(FName& source) const;

    bool isHealthRegenEligible() const;
    bool isHealthRegenRunning() const;
    bool isStaminaRegenRunning() const;

    // S5 step 6. Server only.
    void refreshLowHealthTag();
    // S9 sleep. Raises health to the cap with one modifyHP, same eligibility rules as S5.
    void regenerateToCap();

private:
    UAbilitySystemComponent* getAbilitySystem() const;
    const UAS_CoreVitals* getVitals() const;
    UDA_CoreVitalsDefinition* getDefinition() const;
    bool isPlayerOwner() const;
    float getCapHP() const;

    void updateHealthTimer();
    void updateStaminaTimer();
    void onHealthTick();
    void onStaminaTick();
    void onWatchedTagChanged(const FGameplayTag tag, int32 count);

    FTimerHandle healthTimer;
    FTimerHandle staminaTimer;
    FDelegateHandle sprintTagHandle;
    FDelegateHandle medicineTagHandle;
    bool eventsBound = false;
};
