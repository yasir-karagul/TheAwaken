#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "UObject/StrongObjectPtr.h"
#include "SessionHandshakeFramework/NullPlatformAuthProvider.h"
#include "SessionHandshakeFramework/SessionHandshakeSubsystem.h"
#include "SessionHandshakeFramework/02Tags/T_SessionHandshakeFramework.h"

namespace{

    /** A game instance with the handshake subsystem and the Null provider. The Null provider answers at once. */
    struct FS_PipelineFixture{
        TStrongObjectPtr<UGameInstance> gameInstance;
        USessionHandshakeSubsystem* handshake = nullptr;
        UNullPlatformAuthProvider* provider = nullptr;
        bool accepted = false;
        int32 rejectedCount = 0;
        FGameplayTag rejection;
        bool rejectionWasLocal = false;
        FGameplayMessageListenerHandle acceptedHandle;
        FGameplayMessageListenerHandle rejectedHandle;

        bool create(){
            gameInstance.Reset(NewObject<UGameInstance>(GEngine));
            gameInstance->InitializeStandalone();
            handshake = gameInstance->GetSubsystem<USessionHandshakeSubsystem>();
            UGameplayMessageSubsystem* messages = gameInstance->GetSubsystem<UGameplayMessageSubsystem>();
            if(handshake == nullptr || messages == nullptr){
                return false;
            }
            provider = Cast<UNullPlatformAuthProvider>(handshake->getProvider());
            handshake->setSessionMode(E_SessionMode::P2PCoop, true);

            TFunction<void(FGameplayTag, const FS_PeerJoinAcceptedMessage&)> onAccepted = [this](FGameplayTag channel, const FS_PeerJoinAcceptedMessage& message){
                accepted = true;
            };
            acceptedHandle = messages->RegisterListener<FS_PeerJoinAcceptedMessage>(T_Session_PeerJoinAccepted, MoveTemp(onAccepted));

            TFunction<void(FGameplayTag, const FS_PeerJoinRejectedMessage&)> onRejected = [this](FGameplayTag channel, const FS_PeerJoinRejectedMessage& message){
                rejectedCount++;
                rejection = message.failReasonTag;
                rejectionWasLocal = message.localToRejectedPeer;
            };
            rejectedHandle = messages->RegisterListener<FS_PeerJoinRejectedMessage>(T_Session_PeerJoinRejected, MoveTemp(onRejected));
            return provider != nullptr;
        }

        void join(const FString& peerId, const FS_PeerBuildInfo& build){
            accepted = false;
            rejectedCount = 0;
            rejection = FGameplayTag();
            handshake->requestJoinSession(peerId, build);
        }
    };

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionHandshakePipelineTest, "Awaken.SessionHandshake.Pipeline", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSessionHandshakePipelineTest::RunTest(const FString& parameters){
    FS_PipelineFixture fixture;
    if(!TestTrue(TEXT("the fixture is created"), fixture.create())){
        return false;
    }
    const FS_PeerBuildInfo goodBuild = fixture.handshake->getLocalBuildInfo();

    // 5. A matching peer passes, and its admission is single use.
    const FString peerId = TEXT("peer-aaaa");
    fixture.join(peerId, goodBuild);
    TestTrue(TEXT("a matching peer is accepted"), fixture.accepted);
    TestTrue(TEXT("it has a live admission"), fixture.handshake->hasLiveAdmission(peerId));
    TestTrue(TEXT("peer ids are trimmed and case-blind"), fixture.handshake->hasLiveAdmission(TEXT("  PEER-AAAA ")));
    FS_JoinValidationResult admitted;
    TestTrue(TEXT("the admission can be taken once"), fixture.handshake->tryConsumeAdmission(peerId, admitted));
    TestFalse(TEXT("and not twice"), fixture.handshake->tryConsumeAdmission(peerId, admitted));
    TestEqual(TEXT("the identity is the verified id"), admitted.identity.primaryId, peerId);

    // 2. Ownership refused.
    fixture.provider->verifiesOwnership = false;
    fixture.join(TEXT("peer-bbbb"), goodBuild);
    TestTrue(TEXT("ownership refused"), fixture.rejection == T_Session_JoinFailReason_OwnershipUnverified.GetTag());
    TestFalse(TEXT("no admission after a refusal"), fixture.handshake->hasLiveAdmission(TEXT("peer-bbbb")));
    TestFalse(TEXT("the host copy is not local to the rejected peer"), fixture.rejectionWasLocal);
    fixture.provider->verifiesOwnership = true;

    // 3. The verified id must equal the peer id.
    fixture.provider->accountIdOverride = TEXT("someone-else");
    fixture.join(TEXT("peer-cccc"), goodBuild);
    TestTrue(TEXT("a different verified id is refused"), fixture.rejection == T_Session_JoinFailReason_OwnershipUnverified.GetTag());
    fixture.provider->accountIdOverride = FString();

    // 4. Build number AND content hash.
    FS_PeerBuildInfo otherBuildNumber = goodBuild;
    otherBuildNumber.buildNumber += 1;
    fixture.join(TEXT("peer-dddd"), otherBuildNumber);
    TestTrue(TEXT("a different build number is refused"), fixture.rejection == T_Session_JoinFailReason_VersionMismatch.GetTag());

    FS_PeerBuildInfo otherHash = goodBuild;
    otherHash.contentHash += TEXT("-x");
    fixture.join(TEXT("peer-eeee"), otherHash);
    TestTrue(TEXT("a different content hash is refused"), fixture.rejection == T_Session_JoinFailReason_VersionMismatch.GetTag());

    FS_PeerBuildInfo noHash = goodBuild;
    noHash.contentHash = FString();
    fixture.join(TEXT("peer-ffff"), noHash);
    TestTrue(TEXT("an empty content hash is refused"), fixture.rejection == T_Session_JoinFailReason_VersionMismatch.GetTag());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSessionHandshakeModesTest, "Awaken.SessionHandshake.Modes", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FSessionHandshakeModesTest::RunTest(const FString& parameters){
    FS_PipelineFixture fixture;
    if(!TestTrue(TEXT("the fixture is created"), fixture.create())){
        return false;
    }

    // 1. Offline runs no check and makes no platform call.
    fixture.handshake->setSessionMode(E_SessionMode::Offline, false);
    fixture.provider->platformCallCount = 0;
    fixture.join(TEXT("peer-aaaa"), FS_PeerBuildInfo());
    TestTrue(TEXT("offline: accepted at once"), fixture.accepted);
    TestEqual(TEXT("offline: no platform call"), fixture.provider->platformCallCount, 0);

    // 6. A guest raises the rejection locally; an acceptance raises nothing on the guest.
    fixture.handshake->setSessionMode(E_SessionMode::P2PCoop, false);
    FS_JoinValidationResult refusal;
    refusal.peerId = TEXT("me");
    refusal.allowed = false;
    refusal.failReasonTag = T_Session_JoinFailReason_Banned.GetTag();
    fixture.rejectedCount = 0;
    fixture.handshake->receiveRemoteVerdict(refusal);
    TestEqual(TEXT("one local rejection"), fixture.rejectedCount, 1);
    TestTrue(TEXT("it is local to the rejected peer"), fixture.rejectionWasLocal);

    FS_JoinValidationResult acceptance;
    acceptance.allowed = true;
    fixture.rejectedCount = 0;
    fixture.handshake->receiveRemoteVerdict(acceptance);
    TestEqual(TEXT("an acceptance raises no rejection"), fixture.rejectedCount, 0);
    return true;
}
