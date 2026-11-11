#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "GameplayAbilitySpec.h"
#include "AbilityLibrary.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAwakenAbility, Log, All);   // Doc B22 assigns this category to Doc R04

class UAbilitySystemComponent;
class UDA_AbilityLoadout;
class UDA_NPCProfile;
struct FS_AbilityLoadoutEntry;

// Public functions of Doc R04 for other frameworks (names PROPOSED except isActionAllowed).
UCLASS()
class THEAWAKEN_API UAbilityLibrary : public UBlueprintFunctionLibrary{
    GENERATED_BODY()
public:
    // R04 3.3, A6 step 4. Reads the same tags as the abilities, so there is no second set of rules.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Ability")
    static bool isActionAllowed(AActor* target, const FGameplayTag& actionTag);

    // A9 step 1. True while the ASC has Action.Block. Combat reads it at step 3 of its damage order.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Ability")
    static bool isBlocking(AActor* target);

    // A6 step 5. Server only. Doc R01's held item component calls it with Action.Reload on a weapon swap.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Ability")
    static void cancelAction(AActor* target, const FGameplayTag& actionTag);

    // A5 step 1, server only. Grants entries without an unlockTag. Returns how many were given.
    // deniedActions skips entries (A13: an NPC without CanDodge).
    static int32 grantLoadout(UAbilitySystemComponent* asc, const UDA_AbilityLoadout* loadout, const FGameplayTagContainer& deniedActions);
    // Gives the entries whose unlockTag matches. Doc R06 calls this through AAwakenPlayerState::onUnlockGranted.
    static int32 grantUnlockEntries(UAbilitySystemComponent* asc, const UDA_AbilityLoadout* loadout, const FGameplayTag& unlockTag, const FGameplayTagContainer& deniedActions);
    static bool grantEntry(UAbilitySystemComponent* asc, const FS_AbilityLoadoutEntry& entry);

    static FGameplayAbilitySpec* findSpecByActionTag(UAbilitySystemComponent* asc, const FGameplayTag& actionTag);

    // A13. For an NPC pawn that owns an ASC (Minimal). Grants from the profile's loadout, skipping Dodge
    // when canDodge is false and Block when canBlock is false, and binds the carry cancel on the enemy.
    // Call it once per spawn, after InitAbilityActorInfo.
    static void grantNpcLoadout(AActor* npc, const UDA_NPCProfile* profile);
};
