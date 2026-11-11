#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "StatsFramework/01Structs/S_StatsFramework.h"
#include "StatsLibrary.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAwakenStats, Log, All);   // Doc B22 assigns this category to Doc R08

class UAbilitySystemComponent;
class UAS_CoreVitals;
class UCombatComponent;
class UDA_NPCProfile;

// Doc R08 entry points. Everything that changes state is server only and returns at once on a client (S8).
UCLASS()
class THEAWAKEN_API UStatsLibrary : public UBlueprintFunctionLibrary{
    GENERATED_BODY()
public:
    // S2. Negative delta is damage (sourceTag is a DamageType.* tag), positive is healing (Stats.Source.*).
    UFUNCTION(BlueprintCallable, Category = "Awaken|Stats")
    static void modifyHP(AActor* target, float delta, const FGameplayTag& sourceTag, AActor* instigator);

    // S3. The drain or regeneration multiplier is applied by UMMC_StaminaDelta.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Stats")
    static void modifyStamina(AActor* target, float delta, const FGameplayTag& sourceTag);

    // S3 step 6. Skips the multipliers on purpose. Gameplay never calls it, except B02 for the refill after a respawn.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Stats")
    static void setStamina(AActor* target, float value);

    // S7 step 3. Sets maxHP and keeps the health fraction.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Stats")
    static void setMaxHP(AActor* target, float value);

    // S4. Doc R05 pushes the total on every equip and unequip.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Stats")
    static void applyArmorAggregate(AActor* target, float totalArmorValue);

    // S9. Doc R13 aggregates; Stats never writes a file.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Stats")
    static FS_StatsSaveRecord getSnapshotForSave(AActor* target);
    UFUNCTION(BlueprintCallable, Category = "Awaken|Stats")
    static void restoreFromSnapshot(AActor* target, const FS_StatsSaveRecord& record);

    // S9 sleep. Call from OnSuccessfulSleep for the players registered when the skip fired.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Stats")
    static void regenerateToCap(AActor* target);

    // S7. For an NPC pawn that already owns an ASC and a UAS_CoreVitals default subobject.
    static void initializeNpcVitals(AActor* npc, const UDA_NPCProfile* profile);

    // Used by the logs of both the Stats and the Ability & Action framework: Offline, Host or Client.
    static FString getRoleName(const UObject* worldContext);
    // Used by the same logs: the primary id of the PlayerIdentityComponent (Doc B05), or a name before it is written.
    static FString getPlayerId(const AActor* actor);

    static UAbilitySystemComponent* getAbilitySystem(AActor* target);
    static const UAS_CoreVitals* getVitals(AActor* target);
    // The ASC owner: the PlayerState for a player, the pawn for an NPC.
    static AActor* getVitalsOwner(AActor* target);
    static UCombatComponent* findCombatComponent(AActor* owner);

private:
    static bool requireAuthority(const UAbilitySystemComponent* asc, const TCHAR* functionName);
    static void refreshAfterRestore(AActor* owner);
};
