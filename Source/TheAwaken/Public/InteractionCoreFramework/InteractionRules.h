// Public/InteractionCoreFramework/InteractionRules.h
// The pure rules of Doc B18, with no world and no actors. UInteractionComponent calls these at runtime and
// Tests/InteractionCoreTest.cpp calls the same functions, so the tests exercise the real logic.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Templates/Function.h"
#include "InteractionCoreFramework/00Enums/E_InteractionCoreFramework.h"

// One focus candidate (B18 3.1).
struct FInteractionFocusCandidate{
    float angleRadians = 0.0f;   // angle between the view direction and the direction to the object
    int32 priority = 0;          // getInteractionPriority
    float distance = 0.0f;       // eye to the closest point of the object's collision
};

// The six server checks of B18 3.5, as callable steps. The component fills it with lazy lambdas, so a check that is never
// reached is never evaluated. A step left unset passes. validate() runs them in the fixed order and the FIRST failure wins.
struct FInteractionRequestProbe{
    TFunction<bool()> targetUsable;                  // 1  exists, implements the interface, isInteractableBy
    TFunction<bool()> stateAllows;                   // 2  player state
    TFunction<bool()> inRange;                       // 3a range from the host's eye, inclusive
    TFunction<bool()> hasLineOfSight;                // 3b clear line
    TFunction<bool()> lockAllows;                    // 4  lock policy
    TFunction<bool()> handsAllow;                    // 5  hands
    TFunction<bool(FGameplayTag&)> objectAllows;     // 6  the object's own canInteract, refusal tag in the argument
};

struct THEAWAKEN_API FInteractionRules{
    // Two angles closer than this count as equal, so the priority tie-break can apply. This is float-comparison tolerance
    // (about 0.006 degrees), not a design value.
    static constexpr float angleEpsilon = 0.0001f;

    // B18 3.1: smallest angle wins; a tie goes to the higher priority, then to the nearer one. INDEX_NONE for no candidates.
    static int32 pickBestIndex(const TArray<FInteractionFocusCandidate>& candidates);

    // Inclusive (Doc02 19.9): exactly range + slack is allowed, one centimetre more is not.
    static bool isWithinRange(float distance, float range, float slack);

    // Check 2. A scene blocks everything. Incapacitated (Downed, Restrained, Arriving) blocks everything except Interaction.Escape.
    // Revive needs no exemption: the REVIVER must be healthy, so the normal rule already applies to them (Doc B02 3.5).
    static bool stateAllows(bool isIncapacitated, bool isInScene, bool isEscape);

    // Check 5. Free hands, or an interaction whose tag is in allowedWhileCarrying.
    static bool handsAllow(bool isCarrying, bool isTagAllowedWhileCarrying);

    // Check 4 and the lock component. None and Shared always allow. Exclusive allows unless a DIFFERENT player holds it.
    static bool lockAllows(E_InteractionLockPolicy policy, bool hasOtherHolder);

    // Step 9 same-frame order. A hit cancels a hold only if the hold is not due yet and its end time has not been reached:
    // a hit in the completion frame never cancels it (Doc02 19.9). Interaction.Escape is never cancelled by damage.
    static bool shouldCancelOnDamage(bool isEscapeHold, bool isDue, double now, double endTime);

    // B18 3.5. Runs the probe in order and stops at the first failure. Ignore means check 1 failed and nothing is answered.
    static E_InteractionRequestVerdict validate(const FInteractionRequestProbe& probe, FGameplayTag& failReason);
};
