#include "SessionHandshakeFramework/SessionHandshakeSubsystem.h"
#include "SessionHandshakeFramework/AwakenHandshakeSettings.h"
#include "SessionHandshakeFramework/NullPlatformAuthProvider.h"
#include "SessionHandshakeFramework/PlatformAuthProvider.h"
#include "SessionHandshakeFramework/PlayerIdentityComponent.h"
#include "SessionHandshakeFramework/02Tags/T_SessionHandshakeFramework.h"
#include "SessionHandshakeFramework/03Interfaces/BPI_JoinGateSource.h"
#include "SessionHandshakeFramework/03Interfaces/BPI_CharacterLookupSource.h"
#include "GameFramework/GameplayMessageSubsystem.h"   // GameplayMessageRouter plugin
#include "GameFramework/PlayerState.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

DEFINE_LOG_CATEGORY(LogAwakenHandshake);

/** One join attempt in flight. Lives only on the host and only until the verdict. */
struct FS_PendingJoin{
    FGuid attemptId;
    FString peerId;
    FS_PeerBuildInfo buildInfo;
    FS_PlayerIdentity identity;
    E_StoreKind store = E_StoreKind::None;
    bool waitingForName = false;
    FTSTicker::FDelegateHandle timeoutHandle;
};

void USessionHandshakeSubsystem::Initialize(FSubsystemCollectionBase& collection){
    collection.InitializeDependency<UGameplayMessageSubsystem>();   // same as USaveSubsystem: the message subsystem exists before this one
    Super::Initialize(collection);
    createProvider(UAwakenHandshakeSettings::get());
    loadLocalBuildInfo();
    rebuildLocalIdentity(false);   // reads the local cache only. A cache read is not a platform call (Doc B05 3.1)
}

void USessionHandshakeSubsystem::Deinitialize(){
    resetHostState();
    provider = nullptr;
    Super::Deinitialize();
}

void USessionHandshakeSubsystem::createProvider(const UAwakenHandshakeSettings* settings){
    UClass* providerType = settings->providerClass.LoadSynchronous();
    if(providerType == nullptr){
#if UE_BUILD_SHIPPING
        UE_LOG(LogAwakenHandshake, Error, TEXT("No platform provider class is set. Every co-op join will be refused."));
        return;
#else
        if(!settings->allowNullProvider){
            UE_LOG(LogAwakenHandshake, Error, TEXT("No platform provider class is set and the Null provider is not allowed. Every co-op join will be refused."));
            return;
        }
        UE_LOG(LogAwakenHandshake, Warning, TEXT("Using the Null platform provider: every peer passes the ownership check."));
        providerType = UNullPlatformAuthProvider::StaticClass();
#endif
    }
    provider = NewObject<UPlatformAuthProvider>(this, providerType);
}

UGameplayMessageSubsystem* USessionHandshakeSubsystem::getMessages() const{
    // Not UGameplayMessageSubsystem::Get(this): that needs a world, and a verdict can arrive in the middle of a level travel.
    UGameInstance* gameInstance = GetGameInstance();
    return gameInstance != nullptr ? gameInstance->GetSubsystem<UGameplayMessageSubsystem>() : nullptr;
}

void USessionHandshakeSubsystem::setSessionMode(E_SessionMode newMode, bool isHost){
    sessionMode = newMode;
    hostingAuthority = isHost && newMode != E_SessionMode::Offline;
    if(!hostingAuthority){
        resetHostState();
    }
}

E_SessionMode USessionHandshakeSubsystem::getSessionMode() const{
    return sessionMode;
}

bool USessionHandshakeSubsystem::isCheckingPeers() const{
    return hostingAuthority && sessionMode != E_SessionMode::Offline;
}

void USessionHandshakeSubsystem::resetHostState(){
    for(const TPair<FString, TSharedPtr<FS_PendingJoin>>& entry : pendingJoins){
        if(entry.Value.IsValid()){
            FTSTicker::GetCoreTicker().RemoveTicker(entry.Value->timeoutHandle);
        }
    }
    for(const TPair<FString, FTSTicker::FDelegateHandle>& timer : admissionTimers){
        FTSTicker::GetCoreTicker().RemoveTicker(timer.Value);
    }
    pendingJoins.Reset();
    admissions.Reset();
    admissionTimers.Reset();
    peerTokens.Reset();
}

// ---- Local identity (Doc B05 3.5, Doc R13 3.10) -----------------------------------------------------------

FString USessionHandshakeSubsystem::getIdentityCachePath() const{
    const UAwakenHandshakeSettings* settings = UAwakenHandshakeSettings::get();
    return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / settings->identityFolder / settings->identityFileName);
}

bool USessionHandshakeSubsystem::readIdentityCache(FS_IdentityCache& cache) const{
    const FString path = getIdentityCachePath();
    if(!IFileManager::Get().FileExists(*path)){
        return false;   // first run, or the file was lost: a new local id is made (Doc B05 3.5, known limit)
    }
    FString text;
    if(FFileHelper::LoadFileToString(text, *path) && FJsonObjectConverter::JsonObjectStringToUStruct(text, &cache, 0, 0)){
        return true;
    }
    // The file exists but cannot be read. Keep it for support, never overwrite it, and start again.
    UE_LOG(LogAwakenHandshake, Error, TEXT("The identity file %s is unreadable. It is kept as .corrupt and a new local id is made."), *path);
    IFileManager::Get().Move(*(path + TEXT(".corrupt")), *path, true, true);
    cache = FS_IdentityCache();
    return false;
}

bool USessionHandshakeSubsystem::writeIdentityCache(const FS_IdentityCache& cache) const{
    FString text;
    if(!FJsonObjectConverter::UStructToJsonObjectString(cache, text)){
        return false;
    }
    const FString finalPath = getIdentityCachePath();
    const FString tempPath = finalPath + TEXT(".tmp");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(finalPath), true);
    if(!FFileHelper::SaveStringToFile(text, *tempPath)){
        return false;
    }
    return IFileManager::Get().Move(*finalPath, *tempPath, true, true);   // write aside, then swap: a crash never leaves half a file
}

void USessionHandshakeSubsystem::rebuildLocalIdentity(bool askPlatform){
    const UAwakenHandshakeSettings* settings = UAwakenHandshakeSettings::get();

    FS_IdentityCache cache;
    bool cacheChanged = !readIdentityCache(cache);
    if(cache.localId.IsEmpty()){
        cache.localId = settings->localIdPrefix + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        cacheChanged = true;
    }

    // Offline never asks the platform. Only B20 does, when it starts hosting or joining (refreshLocalIdentity).
    const FString livePlatformId = (askPlatform && provider != nullptr) ? provider->getLocalAccountId() : FString();

    FS_PlayerIdentity identity;
    if(!livePlatformId.IsEmpty()){
        identity.primaryId = livePlatformId;
        identity.kind = E_PlayerIdKind::PlatformAccount;
        if(cache.localIdLinkedTo.IsEmpty()){
            // The first time this person hosts or joins with a platform account: the local id is linked to it (Doc B05 3.5).
            cache.localIdLinkedTo = livePlatformId;
            cacheChanged = true;
        }
        if(!cache.cachedPlatformId.Equals(livePlatformId, ESearchCase::IgnoreCase)){
            cache.cachedPlatformId = livePlatformId;   // the last platform account id on this machine
            cacheChanged = true;
        }
    }else if(!cache.cachedPlatformId.IsEmpty()){
        identity.primaryId = cache.cachedPlatformId;
        identity.kind = E_PlayerIdKind::CachedPlatformAccount;
    }else{
        identity.primaryId = cache.localId;
        identity.kind = E_PlayerIdKind::LocalFallback;
    }

    // The local id is a linked id only for the account it was linked to, so a second account on the same PC
    // never finds the first account's character.
    if(identity.kind != E_PlayerIdKind::LocalFallback){
        const bool localIdBelongsHere = cache.localIdLinkedTo.IsEmpty() || cache.localIdLinkedTo.Equals(identity.primaryId, ESearchCase::IgnoreCase);
        if(localIdBelongsHere){
            identity.linkedIds.AddUnique(cache.localId);
        }
    }

    if(cacheChanged && !writeIdentityCache(cache)){
        UE_LOG(LogAwakenHandshake, Error, TEXT("The identity file could not be written. The identity works for this run only."));
    }

    localIdentity = identity;
}

FS_PlayerIdentity USessionHandshakeSubsystem::getLocalIdentity() const{
    return localIdentity;
}

FS_PlayerIdentity USessionHandshakeSubsystem::refreshLocalIdentity(){
    rebuildLocalIdentity(true);
    return localIdentity;
}

FString USessionHandshakeSubsystem::makePlaceholderName(const FS_PlayerIdentity& identity) const{
    return UAwakenHandshakeSettings::get()->placeholderNamePrefix + TEXT("-") + identity.primaryId.Right(4);
}

// ---- Build info (Doc B05 3.3) -----------------------------------------------------------------------------

void USessionHandshakeSubsystem::loadLocalBuildInfo(){
    const UAwakenHandshakeSettings* settings = UAwakenHandshakeSettings::get();
    const FString path = FPaths::ProjectDir() / settings->buildInfoRelativePath;

    FString text;
    if(FFileHelper::LoadFileToString(text, *path) && FJsonObjectConverter::JsonObjectStringToUStruct(text, &baseBuildInfo, 0, 0) && baseBuildInfo.isComplete()){
        return;
    }
#if WITH_EDITOR
    baseBuildInfo.buildNumber = settings->editorBuildNumber;
    baseBuildInfo.contentHash = settings->editorContentHash;
#else
    baseBuildInfo = FS_PeerBuildInfo();
    UE_LOG(LogAwakenHandshake, Error, TEXT("No usable build info file at %s. Every co-op join will be refused."), *path);
#endif
}

FS_PeerBuildInfo USessionHandshakeSubsystem::getLocalBuildInfo() const{
    FS_PeerBuildInfo info = baseBuildInfo;
    if(contentContributions.Num() == 0){
        return info;
    }
    // Fold every named contribution into the hash in a fixed order, so the same mods give the same hash on every machine.
    TArray<FName> names;
    contentContributions.GetKeys(names);
    names.Sort(FNameLexicalLess());
    FString combined = info.contentHash;
    for(const FName& name : names){
        combined += TEXT("|") + name.ToString() + TEXT(":") + contentContributions[name];
    }
    info.contentHash = FMD5::HashAnsiString(*combined);
    return info;
}

void USessionHandshakeSubsystem::addContentHashContribution(FName contributorName, const FString& hash){
    contentContributions.Add(contributorName, hash);
}

FGameplayTag USessionHandshakeSubsystem::checkVersionCompatibility(const FS_PeerBuildInfo& peerBuildInfo) const{
    const FS_PeerBuildInfo local = getLocalBuildInfo();
    if(!local.isComplete()){
        UE_LOG(LogAwakenHandshake, Error, TEXT("This build has no complete build info, so no peer can match it."));
        return T_Session_JoinFailReason_VersionMismatch.GetTag();
    }
    if(!peerBuildInfo.isComplete()){
        return T_Session_JoinFailReason_VersionMismatch.GetTag();   // both fields are required (Doc B05 6)
    }
    const bool sameBuild = peerBuildInfo.buildNumber == local.buildNumber;
    const bool sameContent = peerBuildInfo.contentHash.Equals(local.contentHash, ESearchCase::IgnoreCase);
    if(sameBuild && sameContent){
        return FGameplayTag();
    }
    return T_Session_JoinFailReason_VersionMismatch.GetTag();   // one rule for a different build and for different mods (Doc B05 3.3)
}

// ---- The join pipeline (Doc B05 4.1) ----------------------------------------------------------------------

void USessionHandshakeSubsystem::submitPeerAuthToken(const FString& peerIdArgument, const FString& authToken, E_StoreKind store, const FString& storeAccountId){
    const FString peerId = normalizePeerId(peerIdArgument);
    if(!isCheckingPeers() || peerId.IsEmpty()){
        return;
    }
    const UAwakenHandshakeSettings* settings = UAwakenHandshakeSettings::get();
    if(authToken.Len() > settings->maxAuthTokenLength || storeAccountId.Len() > settings->maxExternalIdLength){
        UE_LOG(LogAwakenHandshake, Warning, TEXT("The claim of %s is too long and is ignored. Its ownership check will fail."), *peerId);
        return;
    }
    FS_PeerAuthToken entry;
    entry.token = authToken;
    entry.store = store;
    entry.storeAccountId = storeAccountId;
    peerTokens.Add(peerId, entry);
}

void USessionHandshakeSubsystem::requestLocalAuthToken(TFunction<void(const FString& authToken)> onDone) const{
    if(provider == nullptr){
        onDone(FString());
        return;
    }
    provider->requestLocalAuthToken(onDone);
}

void USessionHandshakeSubsystem::verifyPeerOwnership(const FString& peerId, TFunction<void(const FS_OwnershipResult& ownership)> onDone){
    if(provider == nullptr){
        onDone(FS_OwnershipResult());   // no provider: fail closed
        return;
    }
    const FS_PeerAuthToken* tokenEntry = peerTokens.Find(peerId);
    provider->verifyPeerOwnership(peerId, tokenEntry != nullptr ? *tokenEntry : FS_PeerAuthToken(), onDone);
}

void USessionHandshakeSubsystem::getPlatformDisplayName(const FString& peerId, TFunction<void(const FString& displayName)> onDone) const{
    if(provider == nullptr){
        onDone(FString());
        return;
    }
    provider->queryDisplayName(peerId, onDone);
}

void USessionHandshakeSubsystem::requestJoinSession(const FString& peerIdArgument, const FS_PeerBuildInfo& buildInfo){
    const FString peerId = normalizePeerId(peerIdArgument);
    if(sessionMode == E_SessionMode::Offline){
        // Doc B05 3.1: offline runs no checks and makes no platform call.
        FS_JoinValidationResult offlineResult;
        offlineResult.peerId = peerId;
        offlineResult.allowed = true;
        offlineResult.identity = localIdentity;
        broadcastVerdict(offlineResult);   // offline there is no door; the message only tells a caller the answer
        return;
    }
    if(!isCheckingPeers()){
        UE_LOG(LogAwakenHandshake, Error, TEXT("requestJoinSession was called on a machine that is not the host. A guest never validates a peer."));
        return;
    }

    pruneExpiredAdmissions();
    if(pendingJoins.Contains(peerId)){
        UE_LOG(LogAwakenHandshake, Verbose, TEXT("%s is already being checked. This request joins that check."), *peerId);
        return;
    }
    if(peerId.IsEmpty() || provider == nullptr){
        completeFailure(peerId, T_Session_JoinFailReason_OwnershipUnverified.GetTag());
        return;
    }

    TSharedPtr<FS_PendingJoin> pending = MakeShared<FS_PendingJoin>();
    pending->attemptId = FGuid::NewGuid();
    pending->peerId = peerId;
    pending->buildInfo = buildInfo;
    const FS_PeerAuthToken* tokenEntry = peerTokens.Find(peerId);
    pending->store = tokenEntry != nullptr ? tokenEntry->store : E_StoreKind::None;
    pendingJoins.Add(peerId, pending);
    armTimeout(pending, UAwakenHandshakeSettings::get()->ownershipTimeoutSeconds);

    // 1. Ownership. A provider may answer before this function returns, so everything above is already in place.
    const FGuid attemptId = pending->attemptId;
    TWeakObjectPtr<USessionHandshakeSubsystem> weakThis(this);
    TFunction<void(const FS_OwnershipResult& ownership)> onOwnership = [weakThis, peerId, attemptId](const FS_OwnershipResult& ownership){
        if(weakThis.IsValid()){
            weakThis->onOwnershipDone(peerId, attemptId, ownership);
        }
    };
    verifyPeerOwnership(peerId, onOwnership);
}

void USessionHandshakeSubsystem::armTimeout(const TSharedPtr<FS_PendingJoin>& pending, float seconds){
    FTSTicker::GetCoreTicker().RemoveTicker(pending->timeoutHandle);
    // Real time, not the game clock (Doc02 19.4: short gameplay timers use real time). *Verify* FTickerDelegate against your 5.8 headers.
    pending->timeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &USessionHandshakeSubsystem::onAttemptTimeout, pending->peerId, pending->attemptId), seconds);
}

bool USessionHandshakeSubsystem::onAttemptTimeout(float deltaTime, FString peerId, FGuid attemptId){
    const TSharedPtr<FS_PendingJoin> pending = pendingJoins.FindRef(peerId);
    if(!pending.IsValid() || pending->attemptId != attemptId){
        return false;   // a stale timer of an attempt that already ended
    }
    pending->timeoutHandle.Reset();   // this one-shot ticker is finished
    if(pending->waitingForName){
        UE_LOG(LogAwakenHandshake, Warning, TEXT("The name lookup of %s timed out. A placeholder name is used."), *peerId);
        completeSuccess(pending, FString());
    }else{
        UE_LOG(LogAwakenHandshake, Log, TEXT("The ownership check of %s timed out."), *peerId);
        completeFailure(peerId, T_Session_JoinFailReason_Timeout.GetTag());
    }
    return false;
}

void USessionHandshakeSubsystem::onOwnershipDone(const FString& peerId, const FGuid& attemptId, const FS_OwnershipResult& ownership){
    if(!ensureMsgf(IsInGameThread(), TEXT("A platform provider called back off the game thread. The callback is ignored."))){
        return;
    }
    const TSharedPtr<FS_PendingJoin> pending = pendingJoins.FindRef(peerId);
    if(!pending.IsValid() || pending->attemptId != attemptId){
        return;   // too late: the attempt already ended
    }
    // The id the platform verified must be the id this connection uses. Otherwise a guest could connect as one account with a valid
    // token of another, and a ban or a save would hit the wrong id (Doc B05 3.5).
    if(!ownership.verified || ownership.accountId.IsEmpty() || !ownership.accountId.Equals(peerId, ESearchCase::IgnoreCase)){
        completeFailure(peerId, T_Session_JoinFailReason_OwnershipUnverified.GetTag());
        return;
    }
    // The identity is the id the platform verified, never an id the guest claimed (Doc B05 3.5).
    pending->identity.primaryId = normalizePeerId(ownership.accountId);   // the save key is trimmed lower-case text
    pending->identity.kind = E_PlayerIdKind::PlatformAccount;
    pending->identity.linkedIds.Reset();
    if(ownership.store != E_StoreKind::None){
        pending->store = ownership.store;
    }
    runGatesAndVersion(pending);
}

void USessionHandshakeSubsystem::runGatesAndVersion(const TSharedPtr<FS_PendingJoin>& pending){
    // 2. Join gates: B20's ban check on the verified id.
    for(const TWeakObjectPtr<UObject>& weakGate : joinGates){
        UObject* gate = weakGate.Get();
        if(gate == nullptr){
            continue;
        }
        const FGameplayTag refusal = IBPI_JoinGateSource::Execute_evaluateJoin(gate, pending->identity);
        if(refusal.IsValid()){
            completeFailure(pending->peerId, refusal);
            return;
        }
    }

    // 3. Build number and content hash.
    const FGameplayTag versionRefusal = checkVersionCompatibility(pending->buildInfo);
    if(versionRefusal.IsValid()){
        completeFailure(pending->peerId, versionRefusal);
        return;
    }

    // 4. The platform name. It never refuses the join.
    pending->waitingForName = true;
    armTimeout(pending, UAwakenHandshakeSettings::get()->displayNameTimeoutSeconds);
    const FString peerId = pending->peerId;
    const FGuid attemptId = pending->attemptId;
    TWeakObjectPtr<USessionHandshakeSubsystem> weakThis(this);
    TFunction<void(const FString& displayName)> onName = [weakThis, peerId, attemptId](const FString& displayName){
        if(weakThis.IsValid()){
            weakThis->onNameDone(peerId, attemptId, displayName);
        }
    };
    getPlatformDisplayName(peerId, onName);
}

void USessionHandshakeSubsystem::onNameDone(const FString& peerId, const FGuid& attemptId, const FString& displayName){
    if(!ensureMsgf(IsInGameThread(), TEXT("A platform provider called back off the game thread. The callback is ignored."))){
        return;
    }
    const TSharedPtr<FS_PendingJoin> pending = pendingJoins.FindRef(peerId);
    if(!pending.IsValid() || pending->attemptId != attemptId){
        return;
    }
    completeSuccess(pending, displayName);
}

// ---- Verdict, admission, PlayerState (Doc B05 4.2) --------------------------------------------------------

void USessionHandshakeSubsystem::completeSuccess(const TSharedPtr<FS_PendingJoin>& pending, const FString& displayName){
    FS_JoinValidationResult result;
    result.peerId = pending->peerId;
    result.allowed = true;
    result.identity = pending->identity;
    result.platformDisplayName = displayName.IsEmpty() ? makePlaceholderName(result.identity) : displayName;
    result.returningLookup = lookupReturningPlayer(result.identity);

    FS_AdmissionRecord admission;
    admission.result = result;
    admission.expiresAt = FPlatformTime::Seconds() + UAwakenHandshakeSettings::get()->admissionLifetimeSeconds;

    const FString peerId = pending->peerId;
    cleanupAttempt(peerId);
    admissions.Add(peerId, admission);   // state changes happen last (Doc02 6)
    // A timer ends the platform session of a peer that never connects, at the moment the admission runs out (not at the next join).
    FTSTicker::GetCoreTicker().RemoveTicker(admissionTimers.FindRef(peerId));
    admissionTimers.Add(peerId, FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &USessionHandshakeSubsystem::onAdmissionExpired, peerId), UAwakenHandshakeSettings::get()->admissionLifetimeSeconds));
    broadcastVerdict(result);
}

void USessionHandshakeSubsystem::completeFailure(const FString& peerIdArgument, const FGameplayTag& reason){
    const FString peerId = peerIdArgument;   // a copy: cleanupAttempt removes the entry the argument may point into
    cleanupAttempt(peerId);
    if(provider != nullptr && !peerId.IsEmpty()){
        provider->releasePeer(peerId);
    }
    FS_JoinValidationResult result;
    result.peerId = peerId;
    result.allowed = false;
    result.failReasonTag = reason;
    UE_LOG(LogAwakenHandshake, Log, TEXT("The join of %s is refused: %s"), *peerId, *reason.ToString());
    broadcastVerdict(result);   // a failed join writes nothing to any save (Doc B20 3.4)
}

void USessionHandshakeSubsystem::cleanupAttempt(const FString& peerIdArgument){
    const FString peerId = peerIdArgument;
    const TSharedPtr<FS_PendingJoin>* found = pendingJoins.Find(peerId);
    if(found != nullptr && found->IsValid()){
        FTSTicker::GetCoreTicker().RemoveTicker((*found)->timeoutHandle);
    }
    pendingJoins.Remove(peerId);
    peerTokens.Remove(peerId);   // the token is needed only until the ownership check ends
}

void USessionHandshakeSubsystem::broadcastVerdict(const FS_JoinValidationResult& result){
    if(UGameplayMessageSubsystem* messages = getMessages()){
        if(result.allowed){
            FS_PeerJoinAcceptedMessage message;   // Server only (Doc B05 8)
            message.peerId = result.peerId;
            message.platformDisplayName = result.platformDisplayName;
            messages->BroadcastMessage(T_Session_PeerJoinAccepted, message);
        }else{
            FS_PeerJoinRejectedMessage message;   // Server: B20 listens to this and sends the verdict to the guest (step 13)
            message.localToRejectedPeer = false;   // the host's copy: no join-failed screen here
            message.peerId = result.peerId;
            message.failReasonTag = result.failReasonTag;
            messages->BroadcastMessage(T_Session_PeerJoinRejected, message);
        }
    }
}

void USessionHandshakeSubsystem::receiveRemoteVerdict(const FS_JoinValidationResult& verdict){
    if(verdict.allowed){
        return;   // Session.PeerJoinAccepted is server only (Doc B05 8). The guest goes on to the world state of B20 3.4
    }
    if(UGameplayMessageSubsystem* messages = getMessages()){
        FS_PeerJoinRejectedMessage message;   // local to the joining machine: the join-failed screen of Doc B17 listens
        message.localToRejectedPeer = true;   // local to the owning player (Doc B05 8)
        message.peerId = verdict.peerId;
        message.failReasonTag = verdict.failReasonTag;
        messages->BroadcastMessage(T_Session_PeerJoinRejected, message);
    }
}

void USessionHandshakeSubsystem::registerJoinGate(UObject* gateSource){
    if(gateSource == nullptr || !gateSource->GetClass()->ImplementsInterface(UBPI_JoinGateSource::StaticClass())){
        UE_LOG(LogAwakenHandshake, Warning, TEXT("registerJoinGate: %s does not implement BPI_JoinGateSource."), *GetNameSafe(gateSource));
        return;
    }
    joinGates.AddUnique(gateSource);
}

void USessionHandshakeSubsystem::unregisterJoinGate(UObject* gateSource){
    joinGates.Remove(gateSource);
}

void USessionHandshakeSubsystem::registerCharacterLookup(UObject* lookupSource){
    if(lookupSource == nullptr || !lookupSource->GetClass()->ImplementsInterface(UBPI_CharacterLookupSource::StaticClass())){
        UE_LOG(LogAwakenHandshake, Warning, TEXT("registerCharacterLookup: %s does not implement BPI_CharacterLookupSource."), *GetNameSafe(lookupSource));
        return;
    }
    characterLookup = lookupSource;
}

E_ReturningLookup USessionHandshakeSubsystem::lookupReturningPlayer(const FS_PlayerIdentity& identity) const{
    UObject* source = characterLookup.Get();
    if(source == nullptr){
        return E_ReturningLookup::NotChecked;   // Save is not registered (yet): B06 asks Save directly
    }
    TArray<FString> lookupIds;
    identity.getLookupIds(lookupIds);
    return IBPI_CharacterLookupSource::Execute_lookupCharacter(source, lookupIds);
}

void USessionHandshakeSubsystem::releasePeer(const FString& peerIdArgument){
    const FString peerId = normalizePeerId(peerIdArgument);
    cleanupAttempt(peerId);   // a check still in flight dies quietly: the peer is gone
    FTSTicker::GetCoreTicker().RemoveTicker(admissionTimers.FindRef(peerId));
    admissionTimers.Remove(peerId);
    admissions.Remove(peerId);
    if(provider != nullptr && !peerId.IsEmpty()){
        provider->releasePeer(peerId);
    }
}

// ---- The engine door -------------------------------------------------------------------------------------

bool USessionHandshakeSubsystem::onAdmissionExpired(float deltaTime, FString peerId){
    admissionTimers.Remove(peerId);
    if(admissions.Remove(peerId) > 0 && provider != nullptr){
        provider->releasePeer(peerId);   // the peer never connected: end its platform session now
    }
    return false;
}

void USessionHandshakeSubsystem::pruneExpiredAdmissions(){
    const double now = FPlatformTime::Seconds();
    for(auto iterator = admissions.CreateIterator(); iterator; ++iterator){
        if(iterator.Value().expiresAt < now){
            if(provider != nullptr){
                provider->releasePeer(iterator.Key());   // the peer never connected: end its platform session
            }
            iterator.RemoveCurrent();
        }
    }
}

bool USessionHandshakeSubsystem::hasLiveAdmission(const FString& peerIdArgument) const{
    const FS_AdmissionRecord* record = admissions.Find(normalizePeerId(peerIdArgument));
    return record != nullptr && FPlatformTime::Seconds() <= record->expiresAt;
}

bool USessionHandshakeSubsystem::tryConsumeAdmission(const FString& peerIdArgument, FS_JoinValidationResult& admitted){
    const FString peerId = normalizePeerId(peerIdArgument);
    const FS_AdmissionRecord* record = admissions.Find(peerId);
    if(record == nullptr){
        return false;
    }
    const bool alive = FPlatformTime::Seconds() <= record->expiresAt;
    if(alive){
        admitted = record->result;
        admitted.returningLookup = lookupReturningPlayer(admitted.identity);   // asked now, not at the verdict: B20 3.4 step 8 looks after the connection
    }
    admissions.Remove(peerId);   // single use; an expired record is dropped too
    FTSTicker::GetCoreTicker().RemoveTicker(admissionTimers.FindRef(peerId));
    admissionTimers.Remove(peerId);
    return alive;
}

FString USessionHandshakeSubsystem::normalizePeerId(const FString& peerId){
    return peerId.TrimStartAndEnd().ToLower();
}

UPlatformAuthProvider* USessionHandshakeSubsystem::getProvider() const{
    return provider;
}

FString USessionHandshakeSubsystem::peerIdFromUniqueNetId(const FUniqueNetIdRepl& uniqueId){
    if(!uniqueId.IsValid()){
        return FString();
    }
    // The peer id is the Epic Online Services id text. With the EOS online subsystem ToString() can give a composite
    // "<epic account>|<product user>" string: keep the product user part. *Verify* against your online subsystem.
    // Not uniqueId.ToString(): FUniqueNetIdWrapper::ToString is not exported to this module (link error LNK2019).
    // FUniqueNetId::ToString is virtual, so calling it through the pointer needs no extra module.
    const TSharedPtr<const FUniqueNetId> netId = uniqueId.GetUniqueNetId();
    if(!netId.IsValid()){
        return FString();
    }
    const FString text = netId->ToString();
    FString epicAccountPart;
    FString productUserPart;
    if(text.Split(TEXT("|"), &epicAccountPart, &productUserPart)){
        return normalizePeerId(productUserPart);
    }
    return normalizePeerId(text);
}

bool USessionHandshakeSubsystem::applyJoinResultToPlayerState(APlayerState* playerState, const FS_JoinValidationResult& result){
    if(playerState == nullptr || !playerState->HasAuthority() || !result.allowed){
        return false;
    }
    UPlayerIdentityComponent* component = playerState->FindComponentByClass<UPlayerIdentityComponent>();
    if(component == nullptr){
        UE_LOG(LogAwakenHandshake, Error, TEXT("The PlayerState %s has no UPlayerIdentityComponent. Add it (step 7)."), *GetNameSafe(playerState));
        return false;
    }
    // The identity is written once (Doc B05 3.4). The same id again is not an error: a reconnect can reuse a PlayerState (Doc B20 3.8).
    const bool hadIdentity = component->hasIdentity();
    if(!component->setIdentityServer(result.identity)){
        UE_LOG(LogAwakenHandshake, Error, TEXT("%s already has a different identity. Nothing is written."), *GetNameSafe(playerState));
        return false;
    }
    if(!hadIdentity){
        playerState->SetPlayerName(result.platformDisplayName);   // the name is written once
    }
    return true;
}

void USessionHandshakeSubsystem::applyLocalPlayer(APlayerState* playerState){
    if(playerState == nullptr || !playerState->HasAuthority()){
        return;
    }
    UPlayerIdentityComponent* component = playerState->FindComponentByClass<UPlayerIdentityComponent>();
    if(component == nullptr){
        UE_LOG(LogAwakenHandshake, Error, TEXT("The PlayerState %s has no UPlayerIdentityComponent. Add it (step 7)."), *GetNameSafe(playerState));
        return;
    }
    component->setIdentityServer(localIdentity);

    if(sessionMode != E_SessionMode::P2PCoop || provider == nullptr){
        return;   // offline runs no platform call, so the name stays what the engine gave (see Open items)
    }
    const FString localAccountId = provider->getLocalAccountId();
    if(localAccountId.IsEmpty()){
        return;
    }
    TWeakObjectPtr<APlayerState> weakPlayerState(playerState);
    TFunction<void(const FString& displayName)> onName = [weakPlayerState](const FString& displayName){
        if(weakPlayerState.IsValid() && !displayName.IsEmpty()){
            weakPlayerState->SetPlayerName(displayName);
        }
    };
    provider->queryDisplayName(localAccountId, onName);
}