// Private/InteractionCoreFramework/Tests/InteractionCoreTest.cpp
// The pure rules that need no world (guide Step 12): focus choice order, the six checks in order, the lock policy table, the
// same-frame order. They call the SAME functions UInteractionComponent uses. Anything with two machines stays a manual test.
#include "Misc/AutomationTest.h"
#include "InteractionCoreFramework/InteractionRules.h"
#include "InteractionCoreFramework/02Tags/T_InteractionCoreFramework.h"
#include "InteractionFrameworkExtension/02Tags/T_InteractionFrameworkExtension.h"

// *Verify* the flag names against 5.8: newer versions renamed the context flags (same note as the Session Handshake tests).

// ---- Focus choice (case 1 and 2) ----
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionFocusChoiceTest, "Awaken.Interaction.FocusChoice", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionFocusChoiceTest::RunTest(const FString& parameters){
    TArray<FInteractionFocusCandidate> candidates;
    TestEqual(TEXT("no candidates gives no focus"), FInteractionRules::pickBestIndex(candidates), static_cast<int32>(INDEX_NONE));

    FInteractionFocusCandidate nearCentre;
    nearCentre.angleRadians = 0.05f;
    nearCentre.priority = 0;
    nearCentre.distance = 400.0f;
    FInteractionFocusCandidate offCentre;
    offCentre.angleRadians = 0.30f;
    offCentre.priority = 5;
    offCentre.distance = 100.0f;
    candidates = {offCentre, nearCentre};
    TestEqual(TEXT("the one nearest the view centre wins, even against higher priority and a shorter distance"), FInteractionRules::pickBestIndex(candidates), 1);

    FInteractionFocusCandidate lowPriority;
    lowPriority.angleRadians = 0.10f;
    lowPriority.priority = 0;
    lowPriority.distance = 100.0f;
    FInteractionFocusCandidate highPriority;
    highPriority.angleRadians = 0.10f;
    highPriority.priority = 2;
    highPriority.distance = 500.0f;
    candidates = {lowPriority, highPriority};
    TestEqual(TEXT("equal angle: the higher priority wins"), FInteractionRules::pickBestIndex(candidates), 1);

    FInteractionFocusCandidate farther = lowPriority;
    farther.distance = 300.0f;
    candidates = {farther, lowPriority};
    TestEqual(TEXT("equal angle and priority: the nearer one wins"), FInteractionRules::pickBestIndex(candidates), 1);

    FInteractionFocusCandidate withinEpsilon = lowPriority;
    withinEpsilon.angleRadians += FInteractionRules::angleEpsilon * 0.5f;
    withinEpsilon.priority = 3;
    candidates = {lowPriority, withinEpsilon};
    TestEqual(TEXT("angles closer than the tolerance count as equal"), FInteractionRules::pickBestIndex(candidates), 1);
    return true;
}

// ---- Range is inclusive (case 22) ----
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionRangeTest, "Awaken.Interaction.RangeInclusive", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionRangeTest::RunTest(const FString& parameters){
    TestTrue(TEXT("exactly range + slack is allowed (Doc02 19.9)"), FInteractionRules::isWithinRange(350.0f, 300.0f, 50.0f));
    TestFalse(TEXT("one centimetre farther is refused"), FInteractionRules::isWithinRange(351.0f, 300.0f, 50.0f));
    TestTrue(TEXT("focus uses no slack: exactly the range counts"), FInteractionRules::isWithinRange(300.0f, 300.0f, 0.0f));
    TestFalse(TEXT("focus uses no slack: beyond the range does not"), FInteractionRules::isWithinRange(300.5f, 300.0f, 0.0f));
    return true;
}

// ---- Player state (case 5 and 18) ----
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionStateTest, "Awaken.Interaction.PlayerState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionStateTest::RunTest(const FString& parameters){
    TestTrue(TEXT("a healthy player may interact"), FInteractionRules::stateAllows(false, false, false));
    TestFalse(TEXT("an incapacitated player may not"), FInteractionRules::stateAllows(true, false, false));
    TestTrue(TEXT("an incapacitated player may still Escape"), FInteractionRules::stateAllows(true, false, true));
    TestFalse(TEXT("a scene blocks everything"), FInteractionRules::stateAllows(false, true, false));
    TestFalse(TEXT("a scene blocks Escape too"), FInteractionRules::stateAllows(true, true, true));
    return true;
}

// ---- Hands (case 6) ----
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionHandsTest, "Awaken.Interaction.Hands", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionHandsTest::RunTest(const FString& parameters){
    TestTrue(TEXT("free hands always pass"), FInteractionRules::handsAllow(false, false));
    TestFalse(TEXT("carrying blocks an interaction that is not in the list"), FInteractionRules::handsAllow(true, false));
    TestTrue(TEXT("carrying passes for a tag in allowedWhileCarrying"), FInteractionRules::handsAllow(true, true));
    return true;
}

// ---- Lock policy table (cases 12 and 13) ----
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionLockPolicyTest, "Awaken.Interaction.LockPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionLockPolicyTest::RunTest(const FString& parameters){
    TestTrue(TEXT("None: anyone, even with another holder"), FInteractionRules::lockAllows(E_InteractionLockPolicy::None, true));
    TestTrue(TEXT("Shared: two players at once"), FInteractionRules::lockAllows(E_InteractionLockPolicy::Shared, true));
    TestTrue(TEXT("Exclusive: free when nobody holds it"), FInteractionRules::lockAllows(E_InteractionLockPolicy::Exclusive, false));
    TestFalse(TEXT("Exclusive: refused when a different player holds it"), FInteractionRules::lockAllows(E_InteractionLockPolicy::Exclusive, true));
    return true;
}

// ---- Same-frame order (cases 9, 10, 11, 18, 25) ----
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionSameFrameTest, "Awaken.Interaction.SameFrameOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionSameFrameTest::RunTest(const FString& parameters){
    const double endTime = 10.0;
    TestTrue(TEXT("a hit in an earlier frame cancels the hold"), FInteractionRules::shouldCancelOnDamage(false, false, 5.0, endTime));
    TestFalse(TEXT("a hit once the hold is due never cancels it (case 10)"), FInteractionRules::shouldCancelOnDamage(false, true, 10.0, endTime));
    TestFalse(TEXT("a hit whose time has reached the end time is ignored even before the flag is set (case 25)"), FInteractionRules::shouldCancelOnDamage(false, false, 10.0, endTime));
    TestFalse(TEXT("a hit after the end time is ignored too"), FInteractionRules::shouldCancelOnDamage(false, false, 10.2, endTime));
    TestTrue(TEXT("a hit just before the end time still cancels"), FInteractionRules::shouldCancelOnDamage(false, false, 9.99, endTime));
    TestFalse(TEXT("damage never cancels Escape (case 18)"), FInteractionRules::shouldCancelOnDamage(true, false, 1.0, endTime));
    return true;
}

// ---- The six checks in order (cases 3, 4, 5, 6, 12) ----
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInteractionSixChecksTest, "Awaken.Interaction.SixChecksInOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FInteractionSixChecksTest::RunTest(const FString& parameters){
    // A probe whose steps all pass and record that they ran. Each case flips one step to fail.
    TArray<FString> ran;
    auto makeProbe = [&ran](int32 failingStep, FGameplayTag objectReason) -> FInteractionRequestProbe{
        FInteractionRequestProbe probe;
        probe.targetUsable = [&ran, failingStep]() -> bool{ ran.Add(TEXT("1")); return failingStep != 1; };
        probe.stateAllows = [&ran, failingStep]() -> bool{ ran.Add(TEXT("2")); return failingStep != 2; };
        probe.inRange = [&ran, failingStep]() -> bool{ ran.Add(TEXT("3a")); return failingStep != 3; };
        probe.hasLineOfSight = [&ran, failingStep]() -> bool{ ran.Add(TEXT("3b")); return failingStep != 4; };
        probe.lockAllows = [&ran, failingStep]() -> bool{ ran.Add(TEXT("4")); return failingStep != 5; };
        probe.handsAllow = [&ran, failingStep]() -> bool{ ran.Add(TEXT("5")); return failingStep != 6; };
        probe.objectAllows = [&ran, failingStep, objectReason](FGameplayTag& reason) -> bool{
            ran.Add(TEXT("6"));
            reason = objectReason;
            return failingStep != 7;
        };
        return probe;
    };

    FGameplayTag reason;

    ran.Reset();
    TestTrue(TEXT("all checks pass"), FInteractionRules::validate(makeProbe(0, FGameplayTag()), reason) == E_InteractionRequestVerdict::Pass);
    TestEqual(TEXT("all six ran, in order"), FString::Join(ran, TEXT(",")), FString(TEXT("1,2,3a,3b,4,5,6")));
    TestFalse(TEXT("no refusal tag on a pass"), reason.IsValid());

    ran.Reset();
    TestTrue(TEXT("check 1 failing is ignored, not refused"), FInteractionRules::validate(makeProbe(1, FGameplayTag()), reason) == E_InteractionRequestVerdict::Ignore);
    TestEqual(TEXT("nothing after check 1 ran"), FString::Join(ran, TEXT(",")), FString(TEXT("1")));
    TestFalse(TEXT("an ignored request names no reason"), reason.IsValid());

    ran.Reset();
    TestTrue(TEXT("check 2 refuses"), FInteractionRules::validate(makeProbe(2, FGameplayTag()), reason) == E_InteractionRequestVerdict::Refuse);
    TestTrue(TEXT("check 2 reason is NotAllowedInState"), reason == T_Interaction_FailReason_NotAllowedInState.GetTag());
    TestEqual(TEXT("first failure wins: nothing after check 2 ran"), FString::Join(ran, TEXT(",")), FString(TEXT("1,2")));

    ran.Reset();
    FInteractionRules::validate(makeProbe(3, FGameplayTag()), reason);
    TestTrue(TEXT("check 3 range reason is OutOfRange"), reason == T_Interaction_FailReason_OutOfRange.GetTag());

    ran.Reset();
    FInteractionRules::validate(makeProbe(4, FGameplayTag()), reason);
    TestTrue(TEXT("check 3 line reason is NoLineOfSight"), reason == T_Interaction_FailReason_NoLineOfSight.GetTag());

    ran.Reset();
    FInteractionRules::validate(makeProbe(5, FGameplayTag()), reason);
    TestTrue(TEXT("check 4 reason is InUse"), reason == T_Interaction_FailReason_InUse.GetTag());

    ran.Reset();
    FInteractionRules::validate(makeProbe(6, FGameplayTag()), reason);
    TestTrue(TEXT("check 5 reason is HandsFull"), reason == T_Interaction_FailReason_HandsFull.GetTag());
    TestEqual(TEXT("the object's own check never ran after hands refused"), FString::Join(ran, TEXT(",")), FString(TEXT("1,2,3a,3b,4,5")));

    ran.Reset();
    FInteractionRules::validate(makeProbe(7, T_Interaction_FailReason_EnemyAware.GetTag()), reason);
    TestTrue(TEXT("check 6 returns the object's own tag"), reason == T_Interaction_FailReason_EnemyAware.GetTag());

    ran.Reset();
    FInteractionRules::validate(makeProbe(7, FGameplayTag()), reason);
    TestTrue(TEXT("check 6 with no tag falls back to NotAllowedInState: never a bare false (Doc02 6)"), reason == T_Interaction_FailReason_NotAllowedInState.GetTag());

    // A forged request is refused by the same rules (cases 3 and 4): range fails before line is even looked at.
    ran.Reset();
    FInteractionRules::validate(makeProbe(3, FGameplayTag()), reason);
    TestEqual(TEXT("a far target stops at the range check"), FString::Join(ran, TEXT(",")), FString(TEXT("1,2,3a")));
    return true;
}