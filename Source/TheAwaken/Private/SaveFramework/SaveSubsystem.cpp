#include "SaveFramework/SaveSubsystem.h"
#include "SaveFramework/03Interfaces/BPI_SaveParticipant.h"
#include "SaveFramework/03Interfaces/BPI_SavePlayerSubject.h"
#include "SaveFramework/03Interfaces/BPI_SaveGateSource.h"
#include "SaveFramework/03Interfaces/BPI_SaveBagProvider.h"
#include "SaveFramework/SaveFeedbackComponent.h"
#include "SaveFramework/AwakenSaveGame.h"
#include "SaveFramework/AwakenSaveSettings.h"
#include "SaveFramework/SaveFileIO.h"
#include "SaveFramework/01Structs/S_SaveFramework.h"
#include "SaveFramework/02Tags/T_SaveFramework.h"
#include "SessionHandshakeFramework/SessionHandshakeSubsystem.h"
#include "GameFramework/GameplayMessageSubsystem.h"   // GameplayMessageRouter plugin
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "SaveFramework/SaveConstants.h"
#include "SaveFramework/SaveBagReconciler.h"
#include "Misc/Guid.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY(LogAwakenSave);

void USaveSubsystem::Initialize(FSubsystemCollectionBase& collection){
    Super::Initialize(collection);
    collection.InitializeDependency<UGameplayMessageSubsystem>();
    // The handshake asks Save who is a returning player, without a hard link from the handshake to Save (Doc00 4.2).
    if(USessionHandshakeSubsystem* handshake = collection.InitializeDependency<USessionHandshakeSubsystem>()){
        handshake->registerCharacterLookup(this);
    }
}

E_ReturningLookup USaveSubsystem::lookupCharacter_Implementation(const TArray<FString>& lookupIds) const{
    const double now = nowSeconds();
    for(const FString& id : lookupIds){
        if(id.IsEmpty()){
            continue;
        }
        // The same sources, in the same order, as resolveCharacterSource, but nothing is pinned or planned.
        const FS_HeldCharacter* held = heldStates.Find(id);
        if(held != nullptr && held->save.IsValid() && now < held->expiresAtRealTime && held->header.worldID == activeWorldID){
            return E_ReturningLookup::ReturningPlayer;
        }
        if(deletedIdentities.Contains(id)){
            continue;
        }
        const FS_CharacterFileInfo* info = characterIndex.Find(id);
        if(info != nullptr && info->header.worldID == activeWorldID){
            return E_ReturningLookup::ReturningPlayer;
        }
        // A file that is damaged or from a newer game still belongs to a character who exists: the host decides what happens to it.
        auto isUnsupported = [&id](const FS_UnsupportedCharacterFile& unsupported){
            return unsupported.entry.playerIdentity == id;
        };
        auto isRefused = [&id](const FS_SaveManifestEntry& entry){
            return entry.playerIdentity == id;
        };
        if(unsupportedCharacterFiles.ContainsByPredicate(isUnsupported) || refusedCharacterFiles.ContainsByPredicate(isRefused)){
            return E_ReturningLookup::ReturningPlayer;
        }
    }
    return E_ReturningLookup::NewPlayer;   // nothing to restore, or no slot is loaded
}

void USaveSubsystem::Deinitialize(){
    // Never cut a write that is mid-commit (Doc R13 3.11: leaving drops waiting requests, never the running write).
    pending.Reset();
    if(inFlightFuture.IsValid()){
        inFlightFuture.Wait();
    }
    Super::Deinitialize();
}

bool USaveSubsystem::isHostOrOffline() const{
    const UWorld* world = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    return world && world->GetNetMode() != NM_Client;   // Doc00 5.1: offline = local authority
}

UGameplayMessageSubsystem* USaveSubsystem::getMessages() const{
    // Not UGameplayMessageSubsystem::Get(this): it needs a world, and finishSave can arrive in the middle of a level travel.
    UGameInstance* gameInstance = GetGameInstance();
    return gameInstance ? gameInstance->GetSubsystem<UGameplayMessageSubsystem>() : nullptr;
}

// ---- Registration -------------------------------------------------------------------------------------

void USaveSubsystem::registerParticipant(UObject* participant){
    if(!isHostOrOffline() || !participant || !participant->Implements<UBPI_SaveParticipant>()){
        return;
    }

    const FName key = IBPI_SaveParticipant::Execute_getSaveKey(participant);
    if(key.IsNone()){
        UE_LOG(LogAwakenSave, Warning, TEXT("RegisterParticipant: %s has no SaveKey and will not be saved."), *GetNameSafe(participant));
        return;
    }

    // Two participants with the same (scope, key, owner) would silently overwrite each other at capture and both read the record at restore.
    const E_SaveScope scope = IBPI_SaveParticipant::Execute_getSaveScope(participant);
    const APlayerState* owner = IBPI_SaveParticipant::Execute_getSaveOwner(participant);
    auto isStale = [](const TWeakObjectPtr<UObject>& w){
        return !w.IsValid();
    };
    participants.RemoveAll(isStale);
    for(const TWeakObjectPtr<UObject>& weak : participants){
        const UObject* other = weak.Get();
        if(!other || other == participant){
            continue;
        }
        if(IBPI_SaveParticipant::Execute_getSaveKey(other) == key && IBPI_SaveParticipant::Execute_getSaveScope(other) == scope
            && IBPI_SaveParticipant::Execute_getSaveOwner(other) == owner){
            UE_LOG(LogAwakenSave, Error, TEXT("Duplicate save participant: %s and %s both use key '%s'. The second one is NOT registered."),
                *GetNameSafe(other), *GetNameSafe(participant), *key.ToString());
            return;
        }
    }
    participants.AddUnique(TWeakObjectPtr<UObject>(participant));
}

void USaveSubsystem::unregisterParticipant(UObject* participant){
    auto isThisParticipant = [participant](const TWeakObjectPtr<UObject>& w){
        return w.Get() == participant;
    };
    participants.RemoveAll(isThisParticipant);
}

void USaveSubsystem::registerGateSource(UObject* gateSource){
    if(gateSource && gateSource->Implements<UBPI_SaveGateSource>()){
        gateSources.AddUnique(TWeakObjectPtr<UObject>(gateSource));
    }
}

void USaveSubsystem::unregisterGateSource(UObject* gateSource){
    auto isThisSource = [gateSource](const TWeakObjectPtr<UObject>& w){
        return w.Get() == gateSource;
    };
    gateSources.RemoveAll(isThisSource);
}

void USaveSubsystem::setBagProvider(UObject* provider){
    bagProvider = (provider && provider->Implements<UBPI_SaveBagProvider>()) ? provider : nullptr;
}

// ---- Restore marks (server only; B06 resets them on every level change) ---------------------------------

void USaveSubsystem::markWorldRestored(bool restored){
    if(isHostOrOffline()){
        worldRestored = restored;
    }
}
void USaveSubsystem::markPlayerRestored(APlayerState* player){
    if(isHostOrOffline() && player){
        restoredPlayers.Add(TWeakObjectPtr<APlayerState>(player));
    }
}
void USaveSubsystem::clearPlayerRestored(APlayerState* player){
    restoredPlayers.Remove(TWeakObjectPtr<APlayerState>(player));
    pinnedRestores.Remove(TWeakObjectPtr<APlayerState>(player));   // leaving, deleteCharacter and a failed restore all cancel a restore in flight
}
void USaveSubsystem::clearAllRestoreMarks(){
    worldRestored = false;
    restoredPlayers.Reset();
    pinnedRestores.Reset();
}

// ---- The gate (Doc R13 3.2, 3.11) ---------------------------------------------------------------------

FGameplayTag USaveSubsystem::evaluateSaveGate(APlayerState* requester) const{
    if(!isHostOrOffline()){
        return T_Save_FailReason_NotAuthority;
    }
    if(activeSlotName.IsEmpty()){
        return T_Save_FailReason_NoActiveSlot;
    }

    // Saving is refused until the host's restore, and this player's own restore, have finished.
    if(!requester || !worldRestored || !restoredPlayers.Contains(TWeakObjectPtr<APlayerState>(requester))){
        return T_Save_FailReason_RestoreNotFinished;
    }

    // A save is written under the requester's identity, so a player without one cannot save (a gate refusal, not a failed save).
    if(!requester->Implements<UBPI_SavePlayerSubject>() || IBPI_SavePlayerSubject::Execute_getPlayerIdentity(requester).IsEmpty()){
        return T_Save_FailReason_NoIdentity;
    }

    // With no gate source every rule (aware enemy, Bagless, Downed) is off. Say so once instead of failing silently.
    if(gateSources.IsEmpty()){
        static bool logged = false;   // once per run. An ensure would vanish in Shipping.
        if(!logged){
            logged = true;
            UE_LOG(LogAwakenSave, Error, TEXT("EvaluateSaveGate: no IBPI_SaveGateSource is registered, so no save rule is being checked."));
        }
    }

    // The checks run when the request is made, not when the write starts (Doc R13 3.11): a request that waits behind a running save is not asked again.
    for(const TWeakObjectPtr<UObject>& weak : gateSources){
        if(const UObject* source = weak.Get()){
            const FGameplayTag reason = IBPI_SaveGateSource::Execute_getSaveRefusalReason(source, requester);
            // fail fast: the first refusal wins (Doc02 sec. 6)
            if(reason.IsValid()){
                return reason;
            }
        }
    }
    return FGameplayTag();
}

bool USaveSubsystem::shouldKeepPreviousFile(APlayerState* player) const{
    for(const TWeakObjectPtr<UObject>& weak : gateSources){
        if(const UObject* source = weak.Get()){
            if(IBPI_SaveGateSource::Execute_shouldKeepPreviousCharacterFile(source, player)){
                return true;
            }
        }
    }
    return false;
}

// ---- Feedback (Doc R13 5: Save.Completed goes only to the requester) ----------------------------------

void USaveSubsystem::notifyLocalFeedback(FGameplayTag eventTag, APlayerState* player, FGameplayTag failReasonTag, const APlayerController* localController){
    // The delegate is for the local player's UI only. The caller passes the controller: on a client, Player->GetPlayerController()
    // depends on the order in which the PlayerState and the controller replicated.
    if(localController && localController->IsLocalController()){
        onSaveEvent.Broadcast(eventTag, failReasonTag);
    }

    if(UGameplayMessageSubsystem* messages = getMessages()){
        FS_SaveMessage message;
        message.player = player;   // the UI shows a notice only when this is its own player
        message.failReasonTag = failReasonTag;
        messages->BroadcastMessage(eventTag, message);
    }
}

void USaveSubsystem::deliverToRequester(APlayerState* requester, FGameplayTag eventTag, FGameplayTag failReasonTag){
    // the player left; the save still finishes
    if(!requester){
        return;
    }
    const APlayerController* playerController = requester->GetPlayerController();   // on the host this exists for every connected player
    if(!playerController){
        return;
    }

    if(playerController->IsLocalController()){
        notifyLocalFeedback(eventTag, requester, failReasonTag, playerController);   // the host's own UI, and every server-side listener
        return;
    }

    notifyLocalFeedback(eventTag, requester, failReasonTag, nullptr);   // server-side listeners only: the delegate is not for this player
    if(USaveFeedbackComponent* feedback = playerController->FindComponentByClass<USaveFeedbackComponent>()){
        feedback->clientSaveEvent(eventTag, failReasonTag);   // then local on the requesting client
    }else{
        UE_LOG(LogAwakenSave, Warning, TEXT("%s has no USaveFeedbackComponent: this player gets no save feedback."), *GetNameSafe(playerController));
    }
}

bool USaveSubsystem::hasPendingSaves() const{
    return writeInFlight || !pending.IsEmpty();
}
bool USaveSubsystem::slotHasFiles(const FString& slotName) const{
    return FSaveSlotStore::slotHasFiles(slotName);
}

TArray<FString> USaveSubsystem::getRefusedCharacterIdentities() const{
    TArray<FString> identities;
    for(const FS_SaveManifestEntry& entry : refusedCharacterFiles){
        identities.AddUnique(entry.playerIdentity);
    }
    return identities;
}

// =====================================================================================================
// Step 9: the save queue
// =====================================================================================================


FS_SaveResult USaveSubsystem::requestSaveSession(APlayerState* requester){
    // The checks run when the request is made, on the server, for the requester only (Doc R13 3.11 and section 5).
    const FGameplayTag refusal = evaluateSaveGate(requester);
    if(refusal.IsValid()){
        deliverToRequester(requester, T_Save_Failed, refusal);
        return FS_SaveResult::fail(refusal);
    }

    pending.AddUnique(TWeakObjectPtr<APlayerState>(requester));
    pumpQueue();
    return FS_SaveResult::ok(committedCounter);   // queued, not yet written: Save.Completed says when it is
}

void USaveSubsystem::cancelPendingRequests(){
    pending.Reset();   // "leave anyway" drops waiting requests, never the write already running (Doc R13 3.11)
}

void USaveSubsystem::pumpQueue(){
    if(!writeInFlight && !pending.IsEmpty()){
        startSave();
    }
}

void USaveSubsystem::startSave(){
    writeInFlight = true;
    inFlightRequesters = MoveTemp(pending);   // the merge: one follow-up save for everyone who waited
    pending.Reset();

    TSharedPtr<FS_SaveWriteJob, ESPMode::ThreadSafe> job = MakeShared<FS_SaveWriteJob, ESPMode::ThreadSafe>();
    inFlightJob = job;

    const FGameplayTag captureFailure = captureSet(*job, committedCounter + 1);   // game thread: reads every UObject and the settings
    if(captureFailure.IsValid()){
        finishSave(captureFailure);   // the real reason, not always WriteFailed
        return;
    }
    for(const TWeakObjectPtr<APlayerState>& requester : inFlightRequesters){
        deliverToRequester(requester.Get(), T_Save_Started, FGameplayTag());
    }

    if(!UAwakenSaveSettings::get()->writeAsync){
        finishSave(FSaveSlotStore::executeWriteJob(*job));   // fast tests only: the write finishes inside the call. Ship and test with writeAsync = true.
        return;
    }

    TWeakObjectPtr<USaveSubsystem> weakThis(this);
    auto finishOnGameThread = [weakThis](FGameplayTag result){
        if(USaveSubsystem* self = weakThis.Get()){
            self->finishSave(result);
        }
    };
    auto writeOnWorker = [job, finishOnGameThread](){
        // Worker thread: the job holds paths and bytes only. No UObject, no settings.
        const FGameplayTag result = FSaveSlotStore::executeWriteJob(*job);
        AsyncTask(ENamedThreads::GameThread, [finishOnGameThread, result](){
            finishOnGameThread(result);
        });
    };
    inFlightFuture = Async(EAsyncExecution::ThreadPool, writeOnWorker);
}

void USaveSubsystem::finishSave(FGameplayTag failReason){
    const TSharedPtr<FS_SaveWriteJob, ESPMode::ThreadSafe> job = inFlightJob;
    const bool ok = !failReason.IsValid() && job.IsValid();
    bool anyFileRefused = false;

    if(ok){
        committedCounter = job->counter;   // advances only when the set committed
        loadedWorld = inFlightWorldSave;   // what was captured is now what is on disk
        for(const FS_SaveWriteJob::FS_OutFile& file : job->filesToWrite){
            if(file.header.scope == E_SaveScope::Character){
                const FString& identity = file.header.playerIdentity;
                characterIndex.Add(identity, FS_CharacterFileInfo{ file.header, file.fileName, file.bytes.Num() });
                if(const TObjectPtr<UAwakenCharacterSave>* captured = inFlightCharacterSaves.Find(identity)){
                    loadedCharacterSaves.Add(identity, *captured);
                }
            }else{
                loadedWorldHeader = file.header;
            }
        }

        // A carried-over file that failed its check on the worker was left out of the set: it is refused now, and only that file.
        for(const FString& skipped : job->skippedIdentities){
            if(const FS_CharacterFileInfo* info = characterIndex.Find(skipped)){
                FS_SaveManifestEntry entry;
                entry.fileName = info->fileName; entry.scope = static_cast<int32>(E_SaveScope::Character);
                entry.size = info->fileSize; entry.payloadCrc = info->header.payloadCrc; entry.playerIdentity = skipped;
                refusedCharacterFiles.Add(entry);
            }
            characterIndex.Remove(skipped);
            loadedCharacterSaves.Remove(skipped);
            anyFileRefused = true;
        }

        // Only what THIS committed set actually left out is forgotten (the lists were fixed at capture time).
        for(const FString& newIdentity : job->honouredRekeys){
            FString oldIdentity;
            if(pendingRekeys.RemoveAndCopyValue(newIdentity, oldIdentity)){
                characterIndex.Remove(oldIdentity);
                loadedCharacterSaves.Remove(oldIdentity);
            }
        }
        for(const FString& deleted : job->honouredDeletes){
            deletedIdentities.Remove(deleted);
            if(!inFlightCharacterSaves.Contains(deleted)){
                characterIndex.Remove(deleted);
                loadedCharacterSaves.Remove(deleted);
            }
        }
        UE_LOG(LogAwakenSave, Log, TEXT("Save committed: slot %s, counter %d, %d file(s) written."),
            *activeSlotName, committedCounter, job->filesToWrite.Num());
    }else{
        UE_LOG(LogAwakenSave, Warning, TEXT("Save failed: %s. The previous files are untouched."), *failReason.ToString());
    }

    TArray<TWeakObjectPtr<APlayerState>> requesters = MoveTemp(inFlightRequesters);
    inFlightRequesters.Reset();
    inFlightJob.Reset();
    inFlightWorldSave = nullptr;
    inFlightCharacterSaves.Reset();
    inFlightFuture = TFuture<void>();
    writeInFlight = false;

    // host-only notice
    if(anyFileRefused){
        notifyLocalFeedback(T_Save_CharacterFileRefused, nullptr, FGameplayTag(), nullptr);
    }
    for(const TWeakObjectPtr<APlayerState>& requester : requesters){
        deliverToRequester(requester.Get(), ok ? T_Save_Completed : T_Save_Failed, ok ? FGameplayTag() : failReason);
    }
    pumpQueue();   // requests that arrived during the write run now, merged into one
}

// =====================================================================================================
// Step 9: capture (game thread only)
// =====================================================================================================


bool USaveSubsystem::addFileToJob(FS_SaveWriteJob& job, FS_SaveFileHeader& header, USaveGame* save, const FString& fileName) const{
    TArray<uint8> payload, fileBytes;
    if(!FSaveFileIO::serializeSave(save, payload)){
        return false;
    }
    FSaveFileIO::buildFileBytes(header, payload, fileBytes);   // fills schema, build, time, size, CRC

    FS_SaveManifestEntry& entry = job.manifest.files.AddDefaulted_GetRef();
    entry.fileName = fileName;
    entry.scope = static_cast<int32>(header.scope);
    entry.size = fileBytes.Num();
    entry.payloadCrc = header.payloadCrc;
    entry.playerIdentity = header.playerIdentity;

    FS_SaveWriteJob::FS_OutFile& writeFile = job.filesToWrite.AddDefaulted_GetRef();
    writeFile.fileName = fileName;
    writeFile.bytes = MoveTemp(fileBytes);
    writeFile.header = header;
    return true;
}

bool USaveSubsystem::captureCharacterFile(APlayerState* player, int32 counter, FS_SaveFileHeader& header, UAwakenCharacterSave*& save) const{
    if(!player || !player->Implements<UBPI_SavePlayerSubject>()){
        return false;
    }

    header = FS_SaveFileHeader();
    header.scope = E_SaveScope::Character;
    header.playerIdentity = IBPI_SavePlayerSubject::Execute_getPlayerIdentity(player);
    // the gate refuses this earlier (NoIdentity)
    if(header.playerIdentity.IsEmpty()){
        return false;
    }
    IBPI_SavePlayerSubject::Execute_getLinkedPlayerIds(player, header.linkedIds);
    header.characterID = IBPI_SavePlayerSubject::Execute_getCharacterId(player);
    if(!header.characterID.IsValid()){
        // beginCharacterRestore puts the id on the PlayerState. Never write a file with an invalid id: bag ownership would fail silently.
        UE_LOG(LogAwakenSave, Warning, TEXT("Capture skipped %s: no CharacterID yet."), *GetNameSafe(player));
        return false;
    }
    header.heldBagID = IBPI_SavePlayerSubject::Execute_getHeldBagId(player);
    header.worldID = activeWorldID;
    header.saveCounter = counter;

    save = NewObject<UAwakenCharacterSave>(GetTransientPackage());
    TSet<FName> handledKeys;
    for(const TWeakObjectPtr<UObject>& weak : participants){
        UObject* p = weak.Get();
        if(!p || IBPI_SaveParticipant::Execute_getSaveScope(p) != E_SaveScope::Character){
            continue;
        }
        if(IBPI_SaveParticipant::Execute_getSaveOwner(p) != player){
            continue;
        }

        FInstancedStruct snapshot;
        if(IBPI_SaveParticipant::Execute_getSnapshotForSave(p, snapshot)){   // true = this participant decided; false = keep the previous record
            const FName key = IBPI_SaveParticipant::Execute_getSaveKey(p);
            handledKeys.Add(key);
            // true + empty = delete on purpose
            if(snapshot.IsValid()){
                save->setSnapshot(key, IBPI_SaveParticipant::Execute_getSnapshotVersion(p), snapshot);
            }
        }
    }

    // Records nobody handled (a framework that is off right now, or a newer one) are carried forward, never dropped.
    // The previous file is found by the player's identity, or by the identity a pending re-key replaces. A file of another
    // worldID (before Start Again) or a deleted character is not a "previous" state: nothing is carried from it.
    const FString& identity = header.playerIdentity;
    const FString* previousKey = &identity;
    if(!characterIndex.Contains(identity)){
        if(const FString* old = pendingRekeys.Find(identity)){
            previousKey = old;
        }
    }
    const FS_CharacterFileInfo* previousInfo = characterIndex.Find(*previousKey);
    const TObjectPtr<UAwakenCharacterSave>* previous = loadedCharacterSaves.Find(*previousKey);
    if(previousInfo && previous && *previous && previousInfo->header.worldID == activeWorldID && !deletedIdentities.Contains(*previousKey)){
        save->carryForward(*previous, handledKeys);
    }
    return true;
}

FGameplayTag USaveSubsystem::captureSet(FS_SaveWriteJob& job, int32 counter){
    UWorld* world = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if(!world){
        return T_Save_FailReason_NoActiveSlot;
    }

    const UAwakenSaveSettings* settings = UAwakenSaveSettings::get();
    purgeExpiredHeldStates();

    // Every path and setting is read HERE, on the game thread. The worker gets plain strings.
    job.paths = FSaveSlotStore::makePaths(activeSlotName, counter);
    job.counter = counter;
    job.minSchema = settings->minSupportedSchemaVersion;
    job.keepBackup = settings->keepBackupSet;
    job.manifest.schemaVersion = AwakenSave::schemaVersion;
    job.manifest.slotName = activeSlotName;
    job.manifest.worldId = activeWorldID.ToString();
    job.manifest.saveCounter = counter;

    inFlightWorldSave = nullptr;
    inFlightCharacterSaves.Reset();

    // ---- 1. The World file ----
    UAwakenWorldSave* worldSave = NewObject<UAwakenWorldSave>(GetTransientPackage());
    TSet<FName> handledWorldKeys;
    FS_SaveSlotHeader display;
    for(const TWeakObjectPtr<UObject>& weak : participants){
        UObject* p = weak.Get();
        if(!p || IBPI_SaveParticipant::Execute_getSaveScope(p) != E_SaveScope::World){
            continue;
        }

        FInstancedStruct snapshot;
        if(IBPI_SaveParticipant::Execute_getSnapshotForSave(p, snapshot)){
            const FName key = IBPI_SaveParticipant::Execute_getSaveKey(p);
            handledWorldKeys.Add(key);
            if(snapshot.IsValid()){
                worldSave->setSnapshot(key, IBPI_SaveParticipant::Execute_getSnapshotVersion(p), snapshot);
            }
        }
        FS_SaveSlotHeader contribution = display;
        if(IBPI_SaveParticipant::Execute_contributeToSlotHeader(p, display, contribution)){
            display = contribution;
        }
    }
    if(UObject* bags = bagProvider.Get()){
        IBPI_SaveBagProvider::Execute_getBagRecordsForSave(bags, worldSave->bags);
    }else if(loadedWorld){
        // no bag system right now: keep the bags that were saved
        worldSave->bags = loadedWorld->bags;
    }
    if(loadedWorld){
        worldSave->carryForward(loadedWorld, handledWorldKeys);
    }

    FS_SaveFileHeader worldHeader;
    worldHeader.scope = E_SaveScope::World;
    worldHeader.worldID = activeWorldID;
    worldHeader.saveCounter = counter;
    worldHeader.dayCount = display.dayCount;
    worldHeader.seasonTag = display.seasonTag.GetTagName();
    worldHeader.difficultyTag = display.difficultyTag.GetTagName();
    if(!addFileToJob(job, worldHeader, worldSave, FSaveSlotStore::worldFileName())){
        return T_Save_FailReason_WriteFailed;
    }
    inFlightWorldSave = worldSave;

    // ---- 2. Every connected player who is eligible (Doc R13 3.3) ----
    TSet<FString> written;
    if(const AGameStateBase* gameState = world->GetGameState()){
        for(APlayerState* player : gameState->PlayerArray){
            // Restore not finished, Bagless, Downed or Restrained: keep the previous file. It is carried over below.
            if(!player || !restoredPlayers.Contains(TWeakObjectPtr<APlayerState>(player)) || shouldKeepPreviousFile(player)){
                continue;
            }

            FS_SaveFileHeader header;
            UAwakenCharacterSave* characterSave = nullptr;
            // logged inside; the previous file is carried
            if(!captureCharacterFile(player, counter, header, characterSave)){
                continue;
            }
            if(!addFileToJob(job, header, characterSave, FSaveSlotStore::characterFileName(header.playerIdentity))){
                return T_Save_FailReason_WriteFailed;
            }
            inFlightCharacterSaves.Add(header.playerIdentity, characterSave);
            written.Add(header.playerIdentity);
        }
    }

    // ---- 3. Players who are away: their held live state is written as their Character file (Doc R13 3.3) ----
    for(TPair<FString, FS_HeldCharacter>& pair : heldStates){
        FS_HeldCharacter& held = pair.Value;
        if(held.keepPreviousFile || written.Contains(pair.Key) || !held.save.IsValid()){
            continue;
        }

        FS_SaveFileHeader header = held.header;   // same set, same counter, same worldID as every other file of this save
        header.worldID = activeWorldID;
        header.saveCounter = counter;
        if(!addFileToJob(job, header, held.save.Get(), FSaveSlotStore::characterFileName(pair.Key))){
            return T_Save_FailReason_WriteFailed;
        }
        inFlightCharacterSaves.Add(pair.Key, held.save.Get());
        written.Add(pair.Key);
    }

    // ---- 4. Everyone else keeps their previous file: copy it into the new set, so the set is whole ----
    for(const TPair<FString, FS_CharacterFileInfo>& pair : characterIndex){
        if(written.Contains(pair.Key) || deletedIdentities.Contains(pair.Key)){
            continue;
        }

        // A re-keyed file is left out ONLY when the file under the new identity is in this same set. If that player is
        // Bagless, Downed, not restored yet or gone, the old file is the only copy of their progress: it stays.
        bool replaced = false;
        for(const TPair<FString, FString>& rekey : pendingRekeys){
            if(rekey.Value == pair.Key && written.Contains(rekey.Key)){
                replaced = true;
                break;
            }
        }
        if(replaced){
            continue;
        }

        FS_SaveManifestEntry entry;
        entry.fileName = pair.Value.fileName;
        entry.scope = static_cast<int32>(E_SaveScope::Character);
        entry.size = pair.Value.fileSize;
        entry.payloadCrc = pair.Value.header.payloadCrc;
        entry.playerIdentity = pair.Key;
        job.filesToCarryOver.Add(entry);
        job.manifest.files.Add(entry);
    }

    // Files written by another version of the game cannot be read here, but they are not damaged: copy them as they are.
    for(const FS_UnsupportedCharacterFile& unsupported : unsupportedCharacterFiles){
        job.filesToCarryOver.Add(unsupported.entry);
        job.manifest.files.Add(unsupported.entry);
    }

    // What this set leaves out, fixed now. finishSave forgets exactly this, and only if the set committed.
    for(const TPair<FString, FString>& rekey : pendingRekeys){
        if(written.Contains(rekey.Key)){
            job.honouredRekeys.Add(rekey.Key);
        }
    }
    job.honouredDeletes = deletedIdentities.Array();
    return FGameplayTag();
}

// =====================================================================================================
// Step 9: held states
// =====================================================================================================


void USaveSubsystem::storeHeldCharacter(APlayerState* player){
    if(!isHostOrOffline() || !player){
        return;
    }
    pinnedRestores.Remove(TWeakObjectPtr<APlayerState>(player));   // a restore in progress ends with the player

    // A player who dropped before their join restore finished has no held state (Doc R13 3.3).
    if(!restoredPlayers.Contains(TWeakObjectPtr<APlayerState>(player))){
        return;
    }

    FS_HeldCharacter held;
    UAwakenCharacterSave* characterSave = nullptr;
    if(!captureCharacterFile(player, committedCounter, held.header, characterSave)){
        return;
    }

    held.save.Reset(characterSave);
    held.expiresAtRealTime = nowSeconds() + UAwakenSaveSettings::get()->awayHoldSeconds;
    // The same exclusions apply to the state the player was in when they dropped. They still return to this live
    // state inside the hold, but it is never written over their previous file.
    held.keepPreviousFile = shouldKeepPreviousFile(player);
    const FString identity = held.header.playerIdentity;   // copied first: the next line moves Held
    heldStates.Add(identity, MoveTemp(held));
    clearPlayerRestored(player);   // the player is gone: their mark must not outlive their PlayerState
}

void USaveSubsystem::discardHeldCharacter(const FString& playerIdentity){
    heldStates.Remove(playerIdentity);
}

void USaveSubsystem::purgeExpiredHeldStates(){
    const double now = nowSeconds();
    for(auto it = heldStates.CreateIterator(); it; ++it){
        // the known limit of Doc R13 3.3: nothing is written when the hold ends
        if(now >= it->Value.expiresAtRealTime){
            it.RemoveCurrent();
        }
    }
}

// =====================================================================================================
// Step 9: load
// =====================================================================================================


void USaveSubsystem::resetLoadedState(){
    loadedWorld = nullptr;
    loadedWorldHeader = FS_SaveFileHeader();
    loadedCharacterSaves.Reset();
    characterIndex.Reset();
    refusedCharacterFiles.Reset();
    pendingRekeys.Reset();
    deletedIdentities.Reset();
    heldStates.Reset();
    unsupportedCharacterFiles.Reset();
    pinnedRestores.Reset();
    restoredPlayers.Reset();
    pending.Reset();
    worldRestored = false;
    lastLoadUsedBackup = false;
    committedCounter = 0;
    activeSlotName.Reset();
    activeWorldID.Invalidate();
}

void USaveSubsystem::unloadSlot(){
    if(!isHostOrOffline()){
        return;
    }
    if(writeInFlight){
        UE_LOG(LogAwakenSave, Warning, TEXT("UnloadSlot ignored: a save is still being written."));
        return;
    }
    resetLoadedState();
}

namespace{
    /** A slot name must survive FPaths::MakeValidFileName unchanged, so the name in the list is the name that was typed. */
    bool isUsableSlotName(const FString& slotName){
        return !slotName.IsEmpty() && FPaths::MakeValidFileName(slotName) == slotName;
    }
}

FS_SaveResult USaveSubsystem::beginNewWorld(const FString& slotName){
    if(!isHostOrOffline()){
        return FS_SaveResult::fail(T_Save_FailReason_NotAuthority);
    }
    // the worker may still be writing the old slot
    if(writeInFlight){
        return FS_SaveResult::fail(T_Save_FailReason_SaveInProgress);
    }
    if(!isUsableSlotName(slotName)){
        return FS_SaveResult::fail(T_Save_FailReason_SlotNameInvalid);
    }
    // never a silent overwrite: B06 deletes the slot first, after the confirmation that names it
    if(FSaveSlotStore::slotHasFiles(slotName)){
        return FS_SaveResult::fail(T_Save_FailReason_SlotNotEmpty);
    }

    resetLoadedState();
    activeSlotName = slotName;
    activeWorldID = FGuid::NewGuid();   // a world gets its worldID when it is created (Doc R13 3.10)
    return FS_SaveResult::ok(0);
}

/** Atomic: everything is read into locals first. A failure at any point changes nothing, so the slot that was loaded before stays loaded
 *  and a half-loaded slot can never pass the NoActiveSlot gate. */
FS_SaveResult USaveSubsystem::loadSlot(const FString& slotName){
    if(!isHostOrOffline()){
        return FS_SaveResult::fail(T_Save_FailReason_NotAuthority);
    }
    // recoverSlot would delete the Staging folder the worker is writing
    if(writeInFlight){
        return FS_SaveResult::fail(T_Save_FailReason_SaveInProgress);
    }
    if(!isUsableSlotName(slotName)){
        return FS_SaveResult::fail(T_Save_FailReason_SlotNameInvalid);
    }

    // never saved: Continue starts the opening state
    if(!FSaveSlotStore::slotHasFiles(slotName)){
        return beginNewWorld(slotName);
    }

    const UAwakenSaveSettings* settings = UAwakenSaveSettings::get();
    const FS_SaveSlotPaths paths = FSaveSlotStore::makePaths(slotName, 0);
    FS_SaveManifest manifest;
    TArray<FS_SaveManifestEntry> badFiles;   // Character files that failed their check: set aside one by one, the set still loads
    bool usedBackup = false;
    const FGameplayTag reason = FSaveSlotStore::recoverSlot(paths, settings->minSupportedSchemaVersion, settings->keepBackupSet, manifest, badFiles, usedBackup);
    // B06 offers: try again, delete this slot, start a new game
    if(reason.IsValid()){
        return FS_SaveResult::fail(reason);
    }

    UAwakenWorldSave* newWorld = nullptr;
    FS_SaveFileHeader newWorldHeader;
    TMap<FString, FS_CharacterFileInfo> newIndex;
    TMap<FString, TObjectPtr<UAwakenCharacterSave>> newSaves;
    TArray<FS_UnsupportedCharacterFile> newUnsupported;   // another game version wrote them: carried untouched

    for(const FS_SaveManifestEntry& entry : manifest.files){
        auto isSameFile = [&entry](const FS_SaveManifestEntry& bad){
            return bad.fileName == entry.fileName;
        };
        if(badFiles.ContainsByPredicate(isSameFile)){
            continue;
        }

        TArray<uint8> bytes;
        FS_SaveFileHeader header;
        int32 payloadOffset = 0;
        // it validated a moment ago: a read that fails now is a lock
        if(!FFileHelper::LoadFileToArray(bytes, *(paths.current / entry.fileName))){
            return FS_SaveResult::fail(T_Save_FailReason_IoError);
        }
        const FGameplayTag bad = FSaveFileIO::validateFile(bytes, header, payloadOffset, settings->minSupportedSchemaVersion);
        const TArray<uint8> payload = bad.IsValid() ? TArray<uint8>() : TArray<uint8>(bytes.GetData() + payloadOffset, header.payloadSize);
        USaveGame* loaded = bad.IsValid() ? nullptr : UGameplayStatics::LoadGameFromMemory(payload);

        if(entry.scope == static_cast<int32>(E_SaveScope::World)){
            newWorld = Cast<UAwakenWorldSave>(loaded);
            // the World file is the slot
            if(!newWorld){
                return FS_SaveResult::fail(bad.IsValid() ? bad : T_Save_FailReason_CorruptFile);
            }
            newWorldHeader = header;
        }else if(UAwakenCharacterSave* characterSave = Cast<UAwakenCharacterSave>(loaded)){
            newIndex.Add(header.playerIdentity, FS_CharacterFileInfo{ header, entry.fileName, bytes.Num() });
            newSaves.Add(header.playerIdentity, characterSave);
        }else if(FSaveSlotStore::isVersionReason(bad)){
            // not damaged: never set aside
            newUnsupported.Add(FS_UnsupportedCharacterFile{ entry, bad });
        }else{
            // damaged, or not a Character save: refused alone
            badFiles.Add(entry);
        }
    }
    if(!newWorld){
        return FS_SaveResult::fail(T_Save_FailReason_ManifestInvalid);
    }

    // ---- Everything is good: only now does the loaded state change. ----
    resetLoadedState();
    activeSlotName = slotName;
    lastLoadUsedBackup = usedBackup;
    loadedWorld = newWorld;
    loadedWorldHeader = newWorldHeader;
    activeWorldID = newWorldHeader.worldID;
    committedCounter = newWorldHeader.saveCounter;
    characterIndex = MoveTemp(newIndex);
    loadedCharacterSaves = MoveTemp(newSaves);
    refusedCharacterFiles = badFiles;
    unsupportedCharacterFiles = MoveTemp(newUnsupported);

    // Keep a copy of every refused Character file (best effort). B06 and B20 ask the host what to do for each identity.
    if(!badFiles.IsEmpty()){
        IFileManager& fileManager = IFileManager::Get();
        fileManager.MakeDirectory(*paths.refusedCharacters, true);
        for(const FS_SaveManifestEntry& bad : badFiles){
            fileManager.Copy(*(paths.refusedCharacters / bad.fileName), *(paths.current / bad.fileName));
        }
        UE_LOG(LogAwakenSave, Warning, TEXT("%d Character file(s) failed their check and were set aside. The World file loaded."), badFiles.Num());
        notifyLocalFeedback(T_Save_CharacterFileRefused, nullptr, FGameplayTag(), nullptr);   // host-only notice
    }
    return FS_SaveResult::ok(committedCounter);
}

// =====================================================================================================
// Step 14: restore primitives
// =====================================================================================================

/** Migrate one snapshot up to the participant's current version, then apply it (Doc R13 3.11). */
FS_SaveResult USaveSubsystem::applySnapshot(UObject* participant, const FS_SnapshotRecord& record) const{
    // the struct was renamed or removed since this file was written
    if(!record.data.IsValid()){
        return FS_SaveResult::fail(T_Save_FailReason_SnapshotTypeMissing);
    }

    const int32 current = FMath::Max(1, IBPI_SaveParticipant::Execute_getSnapshotVersion(participant));
    int32 version = FMath::Max(1, record.snapshotVersion);
    if(version > current){
        return FS_SaveResult::fail(T_Save_FailReason_SchemaTooNew);
    }

    FInstancedStruct work = record.data;
    while(version < current){
        FInstancedStruct next;
        if(!IBPI_SaveParticipant::Execute_migrateSnapshotStep(participant, version, work, next) || !next.IsValid()){
            return FS_SaveResult::fail(T_Save_FailReason_MigrationMissing);   // a missing or failed step refuses the restore
        }
        work = MoveTemp(next);
        ++version;
    }
    return IBPI_SaveParticipant::Execute_restoreFromSnapshot(participant, work, record.snapshotVersion)
        ? FS_SaveResult::ok() : FS_SaveResult::fail(T_Save_FailReason_RestoreFailed);
}

FS_SaveResult USaveSubsystem::restoreWorldParticipant(FName saveKey){
    // a new world: the level's default state stands (Doc B06 3.4 step 6)
    if(!loadedWorld){
        return FS_SaveResult::ok();
    }

    const FS_SnapshotRecord* record = loadedWorld->findSnapshot(saveKey);
    if(!record){
        UE_LOG(LogAwakenSave, Verbose, TEXT("No snapshot for '%s' in this file (a newer framework, or nothing to save)."), *saveKey.ToString());
        return FS_SaveResult::ok();
    }

    bool found = false;
    for(const TWeakObjectPtr<UObject>& weak : participants){
        UObject* p = weak.Get();
        if(!p || IBPI_SaveParticipant::Execute_getSaveScope(p) != E_SaveScope::World){
            continue;
        }
        if(IBPI_SaveParticipant::Execute_getSaveKey(p) != saveKey){
            continue;
        }

        found = true;
        const FS_SaveResult result = applySnapshot(p, *record);
        if(!result.success){
            return result;
        }
    }
    if(!found){
        // A record exists but nobody is registered for it yet. Returning Ok here would let the participant register late and the
        // next save write its default state over the real one. B06 must restore after every framework has registered.
        UE_LOG(LogAwakenSave, Error, TEXT("Snapshot '%s' exists but no World participant is registered for it."), *saveKey.ToString());
        return FS_SaveResult::fail(T_Save_FailReason_ParticipantNotRegistered);
    }
    return FS_SaveResult::ok();
}

FS_SaveResult USaveSubsystem::restoreWorldBags(){   // Doc B06 World step 11
    if(!loadedWorld || loadedWorld->bags.IsEmpty()){
        return FS_SaveResult::ok();
    }

    UObject* bags = bagProvider.Get();
    // bags exist in the file and nobody can take them
    if(!bags){
        return FS_SaveResult::fail(T_Save_FailReason_ParticipantNotRegistered);
    }
    IBPI_SaveBagProvider::Execute_restoreBagRecords(bags, loadedWorld->bags);
    return FS_SaveResult::ok();
}

/** Where this player restores from: the live state held for them, else their Character file, else (host's choice) a refused file.
 *  Reads only: nothing is applied, nothing is remembered, so it gives the same answer every time until the state changes. */
bool USaveSubsystem::resolveCharacterSource(APlayerState* player, FS_CharacterRestorePlan& plan, FS_SaveFileHeader& header, UAwakenCharacterSave*& source) const{
    plan = FS_CharacterRestorePlan();
    header = FS_SaveFileHeader();
    source = nullptr;
    if(!player || !player->Implements<UBPI_SavePlayerSubject>()){
        return false;
    }

    TArray<FString> ids;
    ids.Add(IBPI_SavePlayerSubject::Execute_getPlayerIdentity(player));
    TArray<FString> linked;
    IBPI_SavePlayerSubject::Execute_getLinkedPlayerIds(player, linked);
    for(const FString& id : linked){
        ids.AddUnique(id);
    }
    auto isBlank = [](const FString& id){
        return id.IsEmpty();
    };
    ids.RemoveAll(isBlank);

    // 1. The live state held for a player who returns inside the hold (Doc B06 P2, Doc B20 3.8).
    const double now = nowSeconds();
    for(const FString& id : ids){
        const FS_HeldCharacter* held = heldStates.Find(id);
        if(held && held->save.IsValid() && now < held->expiresAtRealTime && held->header.worldID == activeWorldID){   // another worldID counts as no file (ST-04)
            source = held->save.Get(); header = held->header;
            plan.source = E_SaveRestoreSource::HeldState; plan.sourceIdentity = id; plan.characterId = held->header.characterID;
            return true;
        }
    }

    // 2. The Character file, loaded and checked when the slot loaded. The lookup matches ANY linked id; the file is re-keyed
    //    by the next save that includes this player (Doc R13 3.10, S1-F04). A deleted character has no file any more.
    for(const FString& id : ids){
        if(deletedIdentities.Contains(id)){
            continue;
        }
        const FS_CharacterFileInfo* info = characterIndex.Find(id);
        const TObjectPtr<UAwakenCharacterSave>* saved = loadedCharacterSaves.Find(id);
        if(!info || !saved || !*saved || info->header.worldID != activeWorldID){
            continue;
        }

        source = *saved; header = info->header;
        plan.source = E_SaveRestoreSource::File; plan.sourceIdentity = id; plan.characterId = info->header.characterID;
        return true;
    }

    // 3. A Character file written by another game version. It is not damaged, so it is neither restored nor replaced:
    //    this player cannot restore (FileUnsupported) and capture carries the file untouched.
    for(const FString& id : ids){
        auto isUnsupported = [&id](const FS_UnsupportedCharacterFile& u){
            return u.entry.playerIdentity == id;
        };
        if(unsupportedCharacterFiles.ContainsByPredicate(isUnsupported)){
            plan.source = E_SaveRestoreSource::FileUnsupported; plan.sourceIdentity = id;
            return false;
        }
    }

    // 4. A file exists for this player but failed its check at load. Nothing is restored from it: the host chooses (Step 14).
    for(const FString& id : ids){
        auto isRefused = [&id](const FS_SaveManifestEntry& e){
            return e.playerIdentity == id;
        };
        if(refusedCharacterFiles.ContainsByPredicate(isRefused)){
            plan.source = E_SaveRestoreSource::FileRefused; plan.sourceIdentity = id;
            return false;
        }
    }
    return false;
}

/** Doc B06 P2 and P4: choose the source, run the bag rule. READ ONLY: safe to call twice. A new character's id is made here,
 *  so B06 passes the SAME plan to Begin and Finish. */
FS_CharacterRestorePlan USaveSubsystem::previewCharacterRestore(APlayerState* player){
    FS_CharacterRestorePlan plan;
    if(!isHostOrOffline() || !player || !player->Implements<UBPI_SavePlayerSubject>()){
        return plan;
    }

    FS_SaveFileHeader header;
    UAwakenCharacterSave* source = nullptr;
    if(!resolveCharacterSource(player, plan, header, source)){
        plan.characterId = FGuid::NewGuid();   // a new character, a refused file (the host chooses) or an unsupported one; nothing is applied until beginCharacterRestore
        return plan;   // B20 grants the starting kit once (Doc R13 3.8)
    }

    // P4: the bag rule. A "newer" world has a higher counter within the same worldID (Doc R13 3.11).
    // A held state is the live state of that player: it is newer than the world, so the world is never "newer" than it.
    const bool worldNewer = plan.source == E_SaveRestoreSource::File && header.saveCounter < committedCounter;
    TArray<FS_BagRecord> worldBags;
    if(UObject* bags = bagProvider.Get()){
        IBPI_SaveBagProvider::Execute_getBagRecordsForSave(bags, worldBags);
    }
    const FS_SaveBagDecision decision = FSaveBagReconciler::evaluate(header.characterID, header.heldBagID, worldBags, worldNewer);
    plan.bagOutcome = decision.outcome;
    plan.worldBagToRemove = decision.worldBagToRemove;

    // The character is AHEAD of the world: accepted, one notice at join (Doc R13 3.11, Index A92).
    if(plan.source == E_SaveRestoreSource::File && header.saveCounter > committedCounter){
        plan.noticeTag = T_Save_WorldOlderThanCharacter;
    }
    return plan;
}

/** COMMAND. Call once per player, before their participants restore. */
FS_SaveResult USaveSubsystem::beginCharacterRestore(APlayerState* player, const FS_CharacterRestorePlan& plan){
    if(!isHostOrOffline()){
        return FS_SaveResult::fail(T_Save_FailReason_NotAuthority);
    }
    if(!player || !player->Implements<UBPI_SavePlayerSubject>()){
        return FS_SaveResult::fail(T_Save_FailReason_NoIdentity);
    }

    const FString identity = IBPI_SavePlayerSubject::Execute_getPlayerIdentity(player);
    if(identity.IsEmpty()){
        return FS_SaveResult::fail(T_Save_FailReason_NoIdentity);
    }

    // 0. Check the plan and fix the source BEFORE anything changes. A file from another game version is never restored or replaced.
    if(plan.source == E_SaveRestoreSource::FileUnsupported){
        return FS_SaveResult::fail(T_Save_FailReason_SchemaTooNew);
    }

    FS_CharacterRestorePlan resolved;
    FS_SaveFileHeader resolvedHeader;
    UAwakenCharacterSave* source = nullptr;
    const bool hasSource = resolveCharacterSource(player, resolved, resolvedHeader, source);
    const bool planHasSource = plan.source == E_SaveRestoreSource::HeldState || plan.source == E_SaveRestoreSource::File;
    if(planHasSource != hasSource || (hasSource && resolved.source != plan.source)){
        // The hold ran out, or another player's save purged it, between Preview and Begin. B06 previews again.
        return FS_SaveResult::fail(T_Save_FailReason_RestoreFailed);
    }

    // The restore reads from this object until Finish, so it cannot be purged or replaced half way. A new character pins nothing.
    // players who left
    for(auto it = pinnedRestores.CreateIterator(); it; ++it){
        if(!it.Key().IsValid()){
            it.RemoveCurrent();
        }
    }
    FS_PinnedRestore pinned;
    if(planHasSource){
        pinned.source.Reset(source);
    }
    pinnedRestores.Add(TWeakObjectPtr<APlayerState>(player), MoveTemp(pinned));

    // 1. The characterID goes onto the PlayerState: the saved one, or a fresh one. Bag ownership and every later file depend on it.
    IBPI_SavePlayerSubject::Execute_setCharacterId(player, plan.characterId.IsValid() ? plan.characterId : FGuid::NewGuid());

    // 2. The bag rule's answer: the file's bag wins, so the duplicate lying in the world goes.
    //    WorldBagWinsPlayerBagless needs nothing here: B06 skips the Inventory snapshot and the player stays Bagless.
    if(plan.bagOutcome == E_SaveBagOutcome::SavedStateWinsRemoveWorldBag && plan.worldBagToRemove.IsValid()){
        if(UObject* bags = bagProvider.Get()){
            IBPI_SaveBagProvider::Execute_removeBagRecord(bags, plan.worldBagToRemove);
        }else{
            UE_LOG(LogAwakenSave, Error, TEXT("Bag rule: no IBPI_SaveBagProvider, the duplicate world bag %s stays."), *plan.worldBagToRemove.ToString());
        }
    }

    // 3. A file found under another linked id is replaced by one under this identity. That is only REMEMBERED here:
    //    the old file is left out of a set only when the new one is in the same set (captureSet step 4).
    if(plan.source != E_SaveRestoreSource::None && plan.sourceIdentity != identity && characterIndex.Contains(plan.sourceIdentity)){
        pendingRekeys.Add(identity, plan.sourceIdentity);
    }

    // 4. The host chose "new character" for a refused file: the choice is made. The bad file stays in refusedCharacters.
    if(plan.source == E_SaveRestoreSource::FileRefused){
        auto isPlanSource = [&plan](const FS_SaveManifestEntry& e){
            return e.playerIdentity == plan.sourceIdentity;
        };
        refusedCharacterFiles.RemoveAll(isPlanSource);
    }
    return FS_SaveResult::ok();
}

FS_SaveResult USaveSubsystem::restorePlayerParticipant(APlayerState* player, FName saveKey){
    // The source was fixed at beginCharacterRestore. Looking it up again here could find nothing (the hold ran out half way),
    // read as "a new character", and the half-restored player would be saved over their file.
    const FS_PinnedRestore* pinned = pinnedRestores.Find(TWeakObjectPtr<APlayerState>(player));
    // Begin was refused or never called
    if(!pinned){
        return FS_SaveResult::fail(T_Save_FailReason_RestoreFailed);
    }
    UAwakenCharacterSave* source = pinned->source.Get();
    // a new character: nothing to restore
    if(!source){
        return FS_SaveResult::ok();
    }

    const FS_SnapshotRecord* record = source->findSnapshot(saveKey);
    if(!record){
        return FS_SaveResult::ok();
    }

    bool found = false;
    for(const TWeakObjectPtr<UObject>& weak : participants){
        UObject* p = weak.Get();
        if(!p || IBPI_SaveParticipant::Execute_getSaveScope(p) != E_SaveScope::Character){
            continue;
        }
        if(IBPI_SaveParticipant::Execute_getSaveKey(p) != saveKey || IBPI_SaveParticipant::Execute_getSaveOwner(p) != player){
            continue;
        }

        found = true;
        const FS_SaveResult result = applySnapshot(p, *record);
        if(!result.success){
            return result;
        }
    }
    if(!found){
        UE_LOG(LogAwakenSave, Error, TEXT("Snapshot '%s' exists for %s but no participant is registered for it yet."), *saveKey.ToString(), *GetNameSafe(player));
        return FS_SaveResult::fail(T_Save_FailReason_ParticipantNotRegistered);
    }
    return FS_SaveResult::ok();
}

void USaveSubsystem::finishCharacterRestore(APlayerState* player, const FS_CharacterRestorePlan& plan, bool succeeded){
    if(!isHostOrOffline() || !player){
        return;
    }

    // Begin pinned this player's source. No pin = Begin was refused or never called, so nothing was restored: never mark them restored.
    const bool pinned = pinnedRestores.Remove(TWeakObjectPtr<APlayerState>(player)) > 0;
    if(succeeded && !pinned){
        UE_LOG(LogAwakenSave, Error, TEXT("Finish(true) for %s without a successful Begin. They stay NOT restored."), *GetNameSafe(player));
        clearPlayerRestored(player);
        return;
    }

    if(!succeeded){
        // A half-restored character must never be saved over their file: they stay NOT restored, the gate refuses them,
        // captureSet skips them, and their previous file is carried untouched (Doc R13 3.11: a load failure never overwrites a save).
        UE_LOG(LogAwakenSave, Error, TEXT("Restore failed for %s. Their file is left untouched and they cannot save."), *GetNameSafe(player));
        clearPlayerRestored(player);
        return;
    }

    // the live pawn replaces the held state
    if(plan.source == E_SaveRestoreSource::HeldState){
        discardHeldCharacter(plan.sourceIdentity);
    }
    markPlayerRestored(player);   // from now on a save may include this player (Doc B06 P5)
    // once, local to that player
    if(plan.noticeTag.IsValid()){
        deliverToRequester(player, plan.noticeTag, FGameplayTag());
    }
}

// =====================================================================================================
// Step 14: delete, Start Again, slot list
// =====================================================================================================

FS_SaveResult USaveSubsystem::deleteSlot(const FString& slotName){
    if(!isHostOrOffline()){
        return FS_SaveResult::fail(T_Save_FailReason_NotAuthority);
    }
    if(writeInFlight){
        return FS_SaveResult::fail(T_Save_FailReason_SaveInProgress);
    }
    if(!isUsableSlotName(slotName)){
        return FS_SaveResult::fail(T_Save_FailReason_SlotNameInvalid);
    }

    // B06 calls this only after the confirmation that names the slot (Doc B06 3.1). Nothing is ever deleted automatically.
    FSaveSlotStore::deleteSlot(slotName);
    if(slotName == activeSlotName){
        resetLoadedState();
    }
    return FS_SaveResult::ok();
}

/** Fresh character: acts on the RUNNING game. The files on disk change at the next save (Doc R13 3.8, PROPOSED reading).
 *  B06 then runs the restore again for this player: previewCharacterRestore finds no file, so they get a new characterID. */
void USaveSubsystem::deleteCharacter(APlayerState* player){
    if(!isHostOrOffline() || !player || !player->Implements<UBPI_SavePlayerSubject>()){
        return;
    }

    const FGuid oldCharacterId = IBPI_SavePlayerSubject::Execute_getCharacterId(player);
    const FString identity = IBPI_SavePlayerSubject::Execute_getPlayerIdentity(player);
    TArray<FString> linked;
    IBPI_SavePlayerSubject::Execute_getLinkedPlayerIds(player, linked);
    linked.AddUnique(identity);

    // lost for good
    if(UObject* bags = bagProvider.Get()){
        IBPI_SaveBagProvider::Execute_removeBagsOwnedBy(bags, oldCharacterId);
    }

    for(const FString& id : linked){
        heldStates.Remove(id);
        // the old file is not carried into the next set
        if(characterIndex.Contains(id)){
            deletedIdentities.Add(id);
        }
    }
    pendingRekeys.Remove(identity);
    clearPlayerRestored(player);   // not saved again until the fresh restore has run
}

/** Start Again by the host: a new worldID makes every older Character file and held state count as "no file". Writes no save. */
FS_SaveResult USaveSubsystem::startAgain(){
    if(!isHostOrOffline()){
        return FS_SaveResult::fail(T_Save_FailReason_NotAuthority);
    }
    // finishSave would write the old counter into the new world
    if(writeInFlight){
        return FS_SaveResult::fail(T_Save_FailReason_SaveInProgress);
    }
    if(activeSlotName.IsEmpty()){
        return FS_SaveResult::fail(T_Save_FailReason_NoActiveSlot);
    }

    activeWorldID = FGuid::NewGuid();
    committedCounter = 0;
    loadedWorld = nullptr;
    heldStates.Reset();
    pendingRekeys.Reset();
    deletedIdentities.Reset();
    characterIndex.Reset();   // the old world's Character files are not carried into the new world's sets
    loadedCharacterSaves.Reset();
    refusedCharacterFiles.Reset();
    unsupportedCharacterFiles.Reset();
    clearAllRestoreMarks();   // B06 restores the new world from scratch
    return FS_SaveResult::ok(0);
}

void USaveSubsystem::listSlotHeaders(TArray<FS_SaveSlotHeader>& headers) const{
    headers.Reset();
    TArray<FString> names;
    FSaveSlotStore::listSlotNames(names);

    for(const FString& name : names){
        FS_SaveSlotHeader slot;
        slot.slotName = name;

        const FS_SaveSlotPaths paths = FSaveSlotStore::makePaths(name, 0);
        FS_SaveFileHeader header;
        const FString worldFile = FSaveSlotStore::worldFileName();
        FGameplayTag reason = FSaveFileIO::readHeaderFromDisk(paths.current / worldFile, header);
        // Backup's header stands in for a damaged Current only. A Current that a newer game wrote is not damaged: loadSlot will not fall back to Backup for it.
        if(reason.IsValid() && !FSaveSlotStore::isVersionReason(reason) && !FSaveFileIO::readHeaderFromDisk(paths.backup / worldFile, header).IsValid()){
            reason = FGameplayTag();
        }
        if(!reason.IsValid()){
            slot.hasFiles = true;
            slot.savedAtUtc = header.writtenAtUtc;
            slot.schemaVersion = header.schemaVersion;
            slot.dayCount = header.dayCount;
            slot.seasonTag = FGameplayTag::RequestGameplayTag(header.seasonTag, false);
            slot.difficultyTag = FGameplayTag::RequestGameplayTag(header.difficultyTag, false);
        }else if(FSaveSlotStore::slotHasFiles(name)){
            // Files exist but no header could be read: a newer game's envelope, or damage. That is NOT "never saved", and the UI
            // must not offer a new game over it. The reason says which one to show.
            slot.hasFiles = true;
            slot.unreadableReason = reason;
        }
        headers.Add(slot);   // hasFiles false => the UI shows "New, not saved" (R10 key UI.SlotList.NeverSaved)
    }
}