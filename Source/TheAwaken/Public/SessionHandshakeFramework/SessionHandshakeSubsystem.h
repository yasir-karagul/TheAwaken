#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "Containers/Ticker.h"
#include "SessionHandshakeFramework/00Enums/E_SessionHandshakeFramework.h"
#include "SessionHandshakeFramework/01Structs/S_SessionHandshakeFramework.h"
#include "SessionHandshakeSubsystem.generated.h"

class APlayerState;
class UAwakenHandshakeSettings;
class UGameplayMessageSubsystem;
class UPlatformAuthProvider;
struct FS_PendingJoin;
struct FUniqueNetIdRepl;

DECLARE_LOG_CATEGORY_EXTERN(LogAwakenHandshake, Log, All);   // Doc B22 3.6 names the category LogAwakenHandshake

UCLASS()
class THEAWAKEN_API USessionHandshakeSubsystem : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;

    // ---- Mode (Doc B05 3.1) ----
    /** COMMAND. B20 or B06 calls it when the player picks Play Offline or Host Co-op, and when a guest connects. The mode never changes while a world runs. */
    UFUNCTION(BlueprintCallable, Category = "Handshake")
    void setSessionMode(E_SessionMode newMode, bool isHost);

    UFUNCTION(BlueprintPure, Category = "Handshake")
    E_SessionMode getSessionMode() const;

    /** QUERY. True on the machine that decides joins: a co-op host or a future dedicated server. */
    UFUNCTION(BlueprintPure, Category = "Handshake")
    bool isCheckingPeers() const;

    // ---- Local identity (Doc B05 3.5) ----
    UFUNCTION(BlueprintPure, Category = "Handshake")
    FS_PlayerIdentity getLocalIdentity() const;

    /** COMMAND. Ask the platform who is signed in and update the identity and the cache. B20 calls it when it starts hosting or joining. */
    UFUNCTION(BlueprintCallable, Category = "Handshake")
    FS_PlayerIdentity refreshLocalIdentity();

    /** COMMAND, server only. The host's own player or the offline player: identity now, platform name later in co-op. */
    UFUNCTION(BlueprintCallable, Category = "Handshake")
    void applyLocalPlayer(APlayerState* playerState);

    // ---- Build info (Doc B05 3.3) ----
    UFUNCTION(BlueprintPure, Category = "Handshake")
    FS_PeerBuildInfo getLocalBuildInfo() const;

    /** QUERY. Invalid tag means compatible. Both the build number AND the content hash must match; there is no override (Doc B05 3.3).
     *  A guest calls it too, for the courtesy pre-check of Doc B20 3.3. */
    UFUNCTION(BlueprintPure, Category = "Handshake")
    FGameplayTag checkVersionCompatibility(const FS_PeerBuildInfo& peerBuildInfo) const;

    /** COMMAND. A mod or extra content adds a named piece that is folded into the content hash, so different mods never match. */
    UFUNCTION(BlueprintCallable, Category = "Handshake")
    void addContentHashContribution(FName contributorName, const FString& hash);

    // ---- Guest side ----
    /** Produce the proof of ownership to send to the host. C++ only: the callback is a TFunction. */
    void requestLocalAuthToken(TFunction<void(const FString& authToken)> onDone) const;

    /** COMMAND. B20 calls it when the host's verdict arrives down its channel. A refusal raises Session.PeerJoinRejected locally. */
    UFUNCTION(BlueprintCallable, Category = "Handshake")
    void receiveRemoteVerdict(const FS_JoinValidationResult& verdict);

    // ---- Host side (Doc B05 4.1) ----
    /** COMMAND, host only. B20 calls it with what the guest's claim carried, before requestJoinSession. */
    UFUNCTION(BlueprintCallable, Category = "Handshake")
    void submitPeerAuthToken(const FString& peerId, const FString& authToken, E_StoreKind store, const FString& storeAccountId);

    /** COMMAND, host only, async. Runs ownership, the join gates, the version check and the name lookup, in that order, and ends in
     *  Session.PeerJoinAccepted or Session.PeerJoinRejected. B20 listens to them and calls it for each guest after its capacity check. */
    UFUNCTION(BlueprintCallable, Category = "Handshake")
    void requestJoinSession(const FString& peerId, const FS_PeerBuildInfo& buildInfo);

    /** QUERY (async), C++ only. One platform ownership check. requestJoinSession uses it. */
    void verifyPeerOwnership(const FString& peerId, TFunction<void(const FS_OwnershipResult& ownership)> onDone);

    /** QUERY (async), C++ only. The platform persona name of a peer. */
    void getPlatformDisplayName(const FString& peerId, TFunction<void(const FString& displayName)> onDone) const;

    UFUNCTION(BlueprintCallable, Category = "Handshake")
    void registerJoinGate(UObject* gateSource);

    UFUNCTION(BlueprintCallable, Category = "Handshake")
    void unregisterJoinGate(UObject* gateSource);

    UFUNCTION(BlueprintCallable, Category = "Handshake")
    void registerCharacterLookup(UObject* lookupSource);

    /** QUERY. Asks the registered lookup. NotChecked when none is registered. */
    UFUNCTION(BlueprintPure, Category = "Handshake")
    E_ReturningLookup lookupReturningPlayer(const FS_PlayerIdentity& identity) const;

    /** COMMAND. A peer left, was removed or was refused: forget its token and admission and end its platform session. B20 calls it. */
    UFUNCTION(BlueprintCallable, Category = "Handshake")
    void releasePeer(const FString& peerId);

    // ---- The engine door (step 10) ----
    /** QUERY. GameMode PreLogin: true when this peer passed the check and the admission has not expired. Does not consume it. */
    UFUNCTION(BlueprintPure, Category = "Handshake")
    bool hasLiveAdmission(const FString& peerId) const;

    /** COMMAND. GameMode PostLogin: take the admission. Single use. */
    bool tryConsumeAdmission(const FString& peerId, FS_JoinValidationResult& admitted);

    /** COMMAND, server only. Writes the platform name (once) and the identity onto the PlayerState (Doc B05 3.4, 10). */
    UFUNCTION(BlueprintCallable, Category = "Handshake")
    bool applyJoinResultToPlayerState(APlayerState* playerState, const FS_JoinValidationResult& result);

    /** THE definition of peerId (Doc02 19.3, stable ids): the Epic Online Services id of the connecting player, as trimmed lower-case text (normalizePeerId). B20's claim, requestJoinSession,
     *  PreLogin and PostLogin all use this one form, and the ownership check must return the same id (step 9). With the EOS online subsystem the
     *  engine's id can be a composite string, so it is cut down here, in one place. *Verify* */
    static FString peerIdFromUniqueNetId(const FUniqueNetIdRepl& uniqueId);

    /** Trimmed lower-case text. Every entry point that takes a peer id passes it through here, so " PEER-A " and "peer-a" are one peer. */
    static FString normalizePeerId(const FString& peerId);

    /** C++ only. For tests (step 14): the provider this subsystem uses. */
    UPlatformAuthProvider* getProvider() const;

private:
    // ---- set up ----
    void createProvider(const UAwakenHandshakeSettings* settings);
    void loadLocalBuildInfo();
    void rebuildLocalIdentity(bool askPlatform);
    UGameplayMessageSubsystem* getMessages() const;
    void resetHostState();

    // ---- identity cache ----
    FString getIdentityCachePath() const;
    bool readIdentityCache(FS_IdentityCache& cache) const;
    bool writeIdentityCache(const FS_IdentityCache& cache) const;
    FString makePlaceholderName(const FS_PlayerIdentity& identity) const;

    // ---- the join pipeline (step 9) ----
    void armTimeout(const TSharedPtr<FS_PendingJoin>& pending, float seconds);
    bool onAttemptTimeout(float deltaTime, FString peerId, FGuid attemptId);
    void onOwnershipDone(const FString& peerId, const FGuid& attemptId, const FS_OwnershipResult& ownership);
    void onNameDone(const FString& peerId, const FGuid& attemptId, const FString& displayName);
    void runGatesAndVersion(const TSharedPtr<FS_PendingJoin>& pending);
    void completeSuccess(const TSharedPtr<FS_PendingJoin>& pending, const FString& displayName);
    void completeFailure(const FString& peerId, const FGameplayTag& reason);
    void cleanupAttempt(const FString& peerId);
    void broadcastVerdict(const FS_JoinValidationResult& result);
    void pruneExpiredAdmissions();
    bool onAdmissionExpired(float deltaTime, FString peerId);

    UPROPERTY(Transient)
    TObjectPtr<UPlatformAuthProvider> provider;

    E_SessionMode sessionMode = E_SessionMode::Offline;
    bool hostingAuthority = false;

    FS_PlayerIdentity localIdentity;
    FS_PeerBuildInfo baseBuildInfo;
    TMap<FName, FString> contentContributions;

    TArray<TWeakObjectPtr<UObject>> joinGates;
    TWeakObjectPtr<UObject> characterLookup;

    TMap<FString, FS_PeerAuthToken> peerTokens;
    TMap<FString, FS_AdmissionRecord> admissions;
    TMap<FString, FTSTicker::FDelegateHandle> admissionTimers;
    TMap<FString, TSharedPtr<FS_PendingJoin>> pendingJoins;
};
