// Progression guide, Step 11.2: the tests that need no world. One bare actor owns one bare UProgressionComponent.
// A transient actor has authority, so the host rules run. Run: Automation RunTests Awaken.Progression
//
// Covered here:   1 (seed from the starting set), 2 (exact match), 3 (every refusal except NotAuthority), 4 (story order),
//                 6 (unknown story tags), 7 first case (no Time subsystem answers day 0), 11 (what the Peaceful grant would take, without the flag),
//                 12 (save round trip), and the starting set joined on restore (open item 5).
// NOT covered:    5 and 7 second case (need Time running: PIE with Awaken.Time.SetDay), 8 (needs an expected Error line), 9 (needs the log),
//                 10, 13 to 17 (messages, Blueprint, two processes). Do those by hand, as the guide says.
//
// Content the tests read. A test whose content is missing skips itself with a warning, it does not fail:
//   Tags (Step 3.4):  Progression.Blueprint.Building.Sawmill, Progression.Story.Part1, Part2, Part3
//   Table (Step 3.3): /Game/Progression/Data/DT_StoryChain with the three rows of the guide (Part3 needs Part2 and day 1)
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "GameFramework/Actor.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "ProgressionFramework/ProgressionComponent.h"
#include "ProgressionFramework/DA_StartingUnlocks.h"
#include "ProgressionFramework/01Structs/S_ProgressionFramework.h"
#include "ProgressionFramework/02Tags/T_ProgressionFramework.h"

namespace{
    // One actor and one component, kept alive for the length of one test and then released.
    struct FProgressionRig{
        AActor* actor = nullptr;
        UProgressionComponent* progression = nullptr;
        TArray<UObject*> extras;

        FProgressionRig(){
            actor = NewObject<AActor>(GetTransientPackage());
            actor->AddToRoot();
            progression = NewObject<UProgressionComponent>(actor);
            progression->AddToRoot();
        }

        ~FProgressionRig(){
            for(UObject* extra : extras){
                extra->RemoveFromRoot();
            }
            progression->RemoveFromRoot();
            actor->RemoveFromRoot();
        }

        // Sets a private object member of the component by name (reflection), so the tests need no friend access.
        bool setMember(const TCHAR* memberName, UObject* value){
            FObjectPropertyBase* property = FindFProperty<FObjectPropertyBase>(UProgressionComponent::StaticClass(), memberName);
            if(property == nullptr){
                return false;
            }
            property->SetObjectPropertyValue_InContainer(progression, value);
            return true;
        }
    };

    FGameplayTag findTag(const TCHAR* name){
        return FGameplayTag::RequestGameplayTag(FName(name), false);
    }

    bool refusedWith(const FS_ProgressionResult& result, const FGameplayTag& reason){
        return !result.success && result.failReason.MatchesTagExact(reason);
    }
}

// ---- Tests 2 and 3: exact match and every refusal ---------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProgressionRefusalsTest, "Awaken.Progression.Refusals", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProgressionRefusalsTest::RunTest(const FString& parameters){
    const FGameplayTag sawmill = findTag(TEXT("Progression.Blueprint.Building.Sawmill"));
    if(!sawmill.IsValid()){
        AddWarning(TEXT("Skipped: add the content tag Progression.Blueprint.Building.Sawmill (guide Step 3.4)."));
        return true;
    }

    FProgressionRig rig;
    UProgressionComponent* progression = rig.progression;

    // Before seeding: NotReady, and nothing changed.
    TestFalse(TEXT("not ready before seeding"), progression->isReady());
    TestTrue(TEXT("grant before seeding gives NotReady"), refusedWith(progression->grantUnlock(sawmill), T_Progression_FailReason_NotReady.GetTag()));
    TestTrue(TEXT("the set is still empty"), progression->getAllUnlocked().IsEmpty());

    // Seed. No starting set is assigned, so the world starts empty.
    TestTrue(TEXT("seedNewWorld succeeds on the host"), progression->seedNewWorld());
    TestTrue(TEXT("ready after seeding"), progression->isReady());
    TestFalse(TEXT("a second seedNewWorld is ignored"), progression->seedNewWorld());

    // InvalidTag: empty, a root itself, and a tag outside the four roots.
    TestTrue(TEXT("an empty tag gives InvalidTag"), refusedWith(progression->grantUnlock(FGameplayTag()), T_Progression_FailReason_InvalidTag.GetTag()));
    TestTrue(TEXT("a root itself gives InvalidTag"), refusedWith(progression->grantUnlock(T_Progression_Blueprint.GetTag()), T_Progression_FailReason_InvalidTag.GetTag()));
    TestTrue(TEXT("the Discovery root itself gives InvalidTag"), refusedWith(progression->grantUnlock(T_Progression_Discovery.GetTag()), T_Progression_FailReason_InvalidTag.GetTag()));
    TestTrue(TEXT("a tag outside the five roots gives InvalidTag"), refusedWith(progression->grantUnlock(T_Progression_UnlockGranted.GetTag()), T_Progression_FailReason_InvalidTag.GetTag()));
    TestTrue(TEXT("every refusal left the set empty"), progression->getAllUnlocked().IsEmpty());

    // A real grant, then the same grant again.
    const FS_ProgressionResult first = progression->grantUnlock(sawmill);
    TestTrue(TEXT("the first grant succeeds"), first.success && !first.failReason.IsValid());
    // Open item 8: a discovery tag is grantable. Only checked when you have added one (Progression.Discovery.NPC.TestSurvivor).
    const FGameplayTag discovery = findTag(TEXT("Progression.Discovery.NPC.TestSurvivor"));
    if(discovery.IsValid()){
        TestTrue(TEXT("a discovery tag is granted"), progression->grantUnlock(discovery).success);
    }
    TestTrue(TEXT("the second grant gives AlreadyUnlocked"), refusedWith(progression->grantUnlock(sawmill), T_Progression_FailReason_AlreadyUnlocked.GetTag()));
    TestEqual(TEXT("the set holds the tag once"), progression->getAllUnlocked().Num(), discovery.IsValid() ? 2 : 1);

    // Test 2: exact match. A parent of a held tag is not unlocked.
    TestTrue(TEXT("the granted tag is unlocked"), progression->isUnlocked(sawmill));
    TestFalse(TEXT("an empty tag is never unlocked"), progression->isUnlocked(FGameplayTag()));
    const FGameplayTag group = findTag(TEXT("Progression.Blueprint.Building"));
    if(group.IsValid()){
        TestFalse(TEXT("the parent Progression.Blueprint.Building is not unlocked by its child"), progression->isUnlocked(group));
    }
    return true;
}

// ---- Test 1: the starting set ----------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProgressionStartingSetTest, "Awaken.Progression.StartingSet", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProgressionStartingSetTest::RunTest(const FString& parameters){
    const FGameplayTag sawmill = findTag(TEXT("Progression.Blueprint.Building.Sawmill"));
    if(!sawmill.IsValid()){
        AddWarning(TEXT("Skipped: add the content tag Progression.Blueprint.Building.Sawmill (guide Step 3.4)."));
        return true;
    }
    const FGameplayTag part1 = findTag(TEXT("Progression.Story.Part1"));

    FProgressionRig rig;
    UDA_StartingUnlocks* starting = NewObject<UDA_StartingUnlocks>(GetTransientPackage());
    starting->AddToRoot();
    rig.extras.Add(starting);
    starting->startingTags.AddTag(sawmill);
    if(part1.IsValid()){
        starting->startingTags.AddTag(part1);   // a story tag in the asset must be skipped with one log line
    }
    if(!rig.setMember(TEXT("loadedStartingUnlocks"), starting)){
        AddError(TEXT("The member loadedStartingUnlocks was not found on UProgressionComponent. Was it renamed?"));
        return false;
    }

    TestTrue(TEXT("seedNewWorld succeeds"), rig.progression->seedNewWorld());
    TestTrue(TEXT("ready after seeding"), rig.progression->isReady());
    TestTrue(TEXT("the starting tag is unlocked"), rig.progression->isUnlocked(sawmill));
    if(part1.IsValid()){
        TestFalse(TEXT("a story tag in the starting set is skipped"), rig.progression->isUnlocked(part1));
    }
    TestEqual(TEXT("only the starting tag is in the set"), rig.progression->getAllUnlocked().Num(), 1);
    return true;
}

// ---- Open item 5: a starting tag added after the save was made joins on restore ------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProgressionStartingSetOnRestoreTest, "Awaken.Progression.StartingSetOnRestore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProgressionStartingSetOnRestoreTest::RunTest(const FString& parameters){
    const FGameplayTag sawmill = findTag(TEXT("Progression.Blueprint.Building.Sawmill"));
    if(!sawmill.IsValid()){
        AddWarning(TEXT("Skipped: add the content tag Progression.Blueprint.Building.Sawmill (guide Step 3.4)."));
        return true;
    }

    FProgressionRig rig;
    UDA_StartingUnlocks* starting = NewObject<UDA_StartingUnlocks>(GetTransientPackage());
    starting->AddToRoot();
    rig.extras.Add(starting);
    starting->startingTags.AddTag(sawmill);
    if(!rig.setMember(TEXT("loadedStartingUnlocks"), starting)){
        AddError(TEXT("The member loadedStartingUnlocks was not found on UProgressionComponent. Was it renamed?"));
        return false;
    }

    // A save made before the patch: it does not hold the starting tag.
    const FS_ProgressionSaveRecord oldSave;
    TestTrue(TEXT("the restore succeeds"), rig.progression->restoreFromSnapshot(oldSave));
    TestTrue(TEXT("the starting tag joined the restored set"), rig.progression->isUnlocked(sawmill));
    TestEqual(TEXT("nothing else was added"), rig.progression->getAllUnlocked().Num(), 1);
    return true;
}

// ---- Tests 4, 6 and 7 (first case): the story chain --------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProgressionStoryChainTest, "Awaken.Progression.StoryChain", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProgressionStoryChainTest::RunTest(const FString& parameters){
    const FGameplayTag part1 = findTag(TEXT("Progression.Story.Part1"));
    const FGameplayTag part2 = findTag(TEXT("Progression.Story.Part2"));
    const FGameplayTag part3 = findTag(TEXT("Progression.Story.Part3"));
    if(!part1.IsValid() || !part2.IsValid() || !part3.IsValid()){
        AddWarning(TEXT("Skipped: add the content tags Progression.Story.Part1, Part2 and Part3 (guide Step 3.4)."));
        return true;
    }
    // Quiet load: a missing asset returns null with no log line, and the test skips itself.
    UDataTable* table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Progression/Data/DT_StoryChain.DT_StoryChain"), nullptr, LOAD_NoWarn | LOAD_Quiet);
    if(table == nullptr){
        AddWarning(TEXT("Skipped: create /Game/Progression/Data/DT_StoryChain (a DataTable of S_StoryChainRow) with the three rows of guide Step 3.3."));
        return true;
    }

    FProgressionRig rig;
    UProgressionComponent* progression = rig.progression;
    if(!rig.setMember(TEXT("loadedStoryChain"), table)){
        AddError(TEXT("The member loadedStoryChain was not found on UProgressionComponent. Was it renamed?"));
        return false;
    }
    TestTrue(TEXT("seedNewWorld succeeds"), progression->seedNewWorld());

    // Test 4: order.
    TestTrue(TEXT("Part2 before Part1 gives PrerequisiteMissing"), refusedWith(progression->grantUnlock(part2), T_Progression_FailReason_PrerequisiteMissing.GetTag()));
    TestTrue(TEXT("the refusal changed nothing"), progression->getAllUnlocked().IsEmpty());
    TestTrue(TEXT("Part1 succeeds (a first part has no prerequisite)"), progression->grantUnlock(part1).success);
    TestTrue(TEXT("Part2 succeeds after Part1"), progression->grantUnlock(part2).success);

    // Test 7, first case: this bare actor has no Time subsystem, so the day reads 0 and Part3 (minimum day 1) is too early.
    TestTrue(TEXT("Part3 is TooEarly on day 0"), refusedWith(progression->grantUnlock(part3), T_Progression_FailReason_TooEarly.GetTag()));
    TestFalse(TEXT("Part3 is not in the set"), progression->isUnlocked(part3));
    TestFalse(TEXT("Story.Complete is not in the set"), progression->isUnlocked(T_Progression_Story_Complete.GetTag()));

    // Test 6: a story tag with no row, and Story.Complete granted directly.
    TestTrue(TEXT("a direct grant of Story.Complete gives UnknownStoryPart"), refusedWith(progression->grantUnlock(T_Progression_Story_Complete.GetTag()), T_Progression_FailReason_UnknownStoryPart.GetTag()));
    TestEqual(TEXT("the set holds exactly Part1 and Part2"), progression->getAllUnlocked().Num(), 2);
    return true;
}

// ---- Test 11 (selection only): what the Peaceful grant would take ------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProgressionEveryUnlockTest, "Awaken.Progression.EveryUnlockTag", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProgressionEveryUnlockTest::RunTest(const FString& parameters){
    const FGameplayTag sawmill = findTag(TEXT("Progression.Blueprint.Building.Sawmill"));
    if(!sawmill.IsValid()){
        AddWarning(TEXT("Skipped: add the content tag Progression.Blueprint.Building.Sawmill (guide Step 3.4)."));
        return true;
    }

    FProgressionRig rig;
    const FGameplayTagContainer everything = rig.progression->getEveryUnlockTag();
    const UGameplayTagsManager& tagManager = UGameplayTagsManager::Get();

    TestTrue(TEXT("a leaf under Progression.Blueprint is in the list"), everything.HasTagExact(sawmill));
    bool anyStory = false;
    bool anyDiscovery = false;
    bool anyGroup = false;
    for(const FGameplayTag& tag : everything){
        if(tag.MatchesTag(T_Progression_Story.GetTag())){
            anyStory = true;
        }
        if(tag.MatchesTag(T_Progression_Discovery.GetTag())){
            anyDiscovery = true;
        }
        if(!tagManager.RequestGameplayTagChildren(tag).IsEmpty()){
            anyGroup = true;
        }
    }
    TestFalse(TEXT("no Progression.Story tag is in the list"), anyStory);
    TestFalse(TEXT("no Progression.Discovery tag is in the list"), anyDiscovery);
    TestFalse(TEXT("no grouping tag is in the list (leaves only)"), anyGroup);
    return true;
}

// ---- Test 12: save round trip -------------------------------------------------------------------------------------

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProgressionSaveRoundTripTest, "Awaken.Progression.SaveRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FProgressionSaveRoundTripTest::RunTest(const FString& parameters){
    const FGameplayTag sawmill = findTag(TEXT("Progression.Blueprint.Building.Sawmill"));
    if(!sawmill.IsValid()){
        AddWarning(TEXT("Skipped: add the content tag Progression.Blueprint.Building.Sawmill (guide Step 3.4)."));
        return true;
    }

    FProgressionRig source;
    FProgressionRig target;
    FProgressionRig unseeded;

    TestTrue(TEXT("source seeds"), source.progression->seedNewWorld());
    TestTrue(TEXT("source grants"), source.progression->grantUnlock(sawmill).success);

    FS_ProgressionSaveRecord record = source.progression->getSnapshotForSave();
    TestEqual(TEXT("the record holds one name"), record.unlockedTags.Num(), 1);

    // A name that is not a tag any more: skipped, named once in the log, and the load goes on.
    record.unlockedTags.Add(FName(TEXT("Progression.Blueprint.NoSuchTagForTheTest")));

    TestTrue(TEXT("restore into a fresh component succeeds"), target.progression->restoreFromSnapshot(record));
    TestTrue(TEXT("the restored component is ready"), target.progression->isReady());
    const FGameplayTagContainer sourceSet = source.progression->getAllUnlocked();
    const FGameplayTagContainer targetSet = target.progression->getAllUnlocked();
    TestEqual(TEXT("both sets have the same size"), targetSet.Num(), sourceSet.Num());
    TestTrue(TEXT("both sets hold the same tags"), targetSet.HasAllExact(sourceSet) && sourceSet.HasAllExact(targetSet));
    TestFalse(TEXT("a second restore is refused"), target.progression->restoreFromSnapshot(record));

    // The participant writes nothing for a set that is not seeded (it checks isReady): an empty set must never erase a real one.
    TestFalse(TEXT("an unseeded component is not ready, so the participant will not write it"), unseeded.progression->isReady());
    return true;
}

#endif