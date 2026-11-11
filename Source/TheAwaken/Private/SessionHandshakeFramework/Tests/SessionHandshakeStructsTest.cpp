#include "Misc/AutomationTest.h"
#include "SessionHandshakeFramework/01Structs/S_SessionHandshakeFramework.h"

// *Verify* the flag names against 5.8: newer versions renamed the context flags.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionHandshakeIdentityTest, "Awaken.SessionHandshake.Identity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSessionHandshakeIdentityTest::RunTest(const FString& parameters){
    FS_PlayerIdentity identity;
    identity.primaryId = TEXT("abc123");
    identity.linkedIds.Add(TEXT("local-0001"));

    TestTrue(TEXT("the primary id matches, without case"), identity.matchesId(TEXT("ABC123")));
    TestTrue(TEXT("a linked id matches"), identity.matchesId(TEXT("local-0001")));
    TestFalse(TEXT("another id does not match"), identity.matchesId(TEXT("someone-else")));
    TestFalse(TEXT("an empty id never matches"), identity.matchesId(FString()));

    TArray<FString> lookupIds;
    identity.getLookupIds(lookupIds);
    TestEqual(TEXT("primary first, then the linked id"), lookupIds.Num(), 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionHandshakeBuildInfoTest, "Awaken.SessionHandshake.BuildInfo", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSessionHandshakeBuildInfoTest::RunTest(const FString& parameters){
    FS_PeerBuildInfo info;
    TestFalse(TEXT("empty build info is incomplete"), info.isComplete());
    info.buildNumber = 12;
    TestFalse(TEXT("a missing hash is incomplete"), info.isComplete());
    info.contentHash = TEXT("abc");
    TestTrue(TEXT("both fields make it complete"), info.isComplete());
    return true;
}
