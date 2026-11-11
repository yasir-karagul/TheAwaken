// Private/SaveFramework/SaveSmokeTest.cpp
//
// DEV-ONLY. Delete this file and SaveSmokeTest.h once your real frameworks are in and tested.
//
// Console command:  Awaken.Save.SmokeTest      (type it in the Output Log "Cmd" box while Play-In-Editor is running)
//
// What it does, in one sentence: it plays the part of a tiny game (one fake player, one fake framework) and drives the real
// USaveSubsystem through new world -> join -> save -> save -> reload -> restore -> save, then damages a file on purpose and
// checks that the Backup set takes over. Every check logs PASS or FAIL under the log category LogAwakenSave.
#include "SaveFramework/SaveSmokeTest.h"   // MUST be the first include (Unreal's first-include rule)
#include "SaveFramework/SaveSubsystem.h"
#include "SaveFramework/SaveSlotStore.h"
#include "SaveFramework/SaveFileIO.h"
#include "SaveFramework/SaveConstants.h"
#include "SaveFramework/AwakenSaveSettings.h"
#include "SaveFramework/02Tags/T_SaveFramework.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

// =====================================================================================================================
// Part 1. The fake framework objects (declared in SaveSmokeTest.h)
// =====================================================================================================================

FName USaveSmokeTestParticipant::getSaveKey_Implementation() const{
    return key;
}

E_SaveScope USaveSmokeTestParticipant::getSaveScope_Implementation() const{
    return scope;
}

int32 USaveSmokeTestParticipant::getSnapshotVersion_Implementation() const{
    return version;
}

APlayerState* USaveSmokeTestParticipant::getSaveOwner_Implementation() const{
    return scope == E_SaveScope::Character ? ownerPlayer.Get() : nullptr;
}

bool USaveSmokeTestParticipant::getSnapshotForSave_Implementation(FInstancedStruct& snapshot) const{
    // A query: it only reads. True + a valid struct = "write this".
    if(scope == E_SaveScope::World){
        snapshot = FInstancedStruct::Make(worldData);
    }else{
        snapshot = FInstancedStruct::Make(characterData);
    }
    return true;
}

bool USaveSmokeTestParticipant::restoreFromSnapshot_Implementation(const FInstancedStruct& snapshot, int32 savedVersion){
    ++restoreCalls;
    lastSavedVersion = savedVersion;

    // GetPtr, not Get: a wrong or missing struct type gives null (refuse the restore) instead of an assert.
    if(scope == E_SaveScope::World){
        const FS_SmokeWorldRecord* record = snapshot.GetPtr<FS_SmokeWorldRecord>();
        if(!record){
            return false;
        }
        worldData = *record;
        return true;
    }

    const FS_SmokeCharacterRecord* record = snapshot.GetPtr<FS_SmokeCharacterRecord>();
    if(!record){
        return false;
    }
    characterData = *record;
    return true;
}

bool USaveSmokeTestParticipant::migrateSnapshotStep_Implementation(int32 fromVersion, const FInstancedStruct& oldSnapshot, FInstancedStruct& newSnapshot) const{
    // The only step this fake knows: World snapshot, version 1 -> 2. It marks the note so the test can see the step ran.
    if(scope != E_SaveScope::World || fromVersion != 1){
        return false;
    }
    const FS_SmokeWorldRecord* old = oldSnapshot.GetPtr<FS_SmokeWorldRecord>();
    if(!old){
        return false;
    }
    FS_SmokeWorldRecord migrated = *old;
    migrated.note += TEXT("+migrated");
    newSnapshot = FInstancedStruct::Make(migrated);
    ++migrateCalls;
    return true;
}

bool USaveSmokeTestParticipant::contributeToSlotHeader_Implementation(const FS_SaveSlotHeader& display, FS_SaveSlotHeader& contribution) const{
    // World scope only: tell the slot list which day this save is on.
    if(scope != E_SaveScope::World){
        return false;
    }
    contribution = display;
    contribution.dayCount = worldData.dayCount;
    return true;
}

FGameplayTag USaveSmokeTestGate::getSaveRefusalReason_Implementation(APlayerState* requester) const{
    return refuseWith;   // invalid = allowed
}

bool USaveSmokeTestGate::shouldKeepPreviousCharacterFile_Implementation(APlayerState* player) const{
    return keepPreviousFile;
}

FString ASaveSmokeTestPlayerState::getPlayerIdentity_Implementation() const{
    return identity;
}

void ASaveSmokeTestPlayerState::getLinkedPlayerIds_Implementation(TArray<FString>& linkedIds) const{
    linkedIds = linkedPlayerIds;
}

FGuid ASaveSmokeTestPlayerState::getCharacterId_Implementation() const{
    return characterId;
}

FGuid ASaveSmokeTestPlayerState::getHeldBagId_Implementation() const{
    return heldBagId;
}

void ASaveSmokeTestPlayerState::setCharacterId_Implementation(FGuid newCharacterId){
    characterId = newCharacterId;
}

// =====================================================================================================================
// Part 2. Helpers for the test (private to this file)
// =====================================================================================================================

namespace{
    /** True while a run is in progress (the last step waits for a background write, so a run can span several frames). */
    bool smokeRunning = false;

    /** Everything one run needs. It lives on the heap, so the last (waiting) step can still reach it after the command returned. */
    struct FSmokeContext{
        int32 passed = 0;
        int32 failed = 0;
        TArray<FString> failures;

        FString slotName = TEXT("SmokeTest");
        FName worldKey = FName(TEXT("SmokeWorld"));
        FName characterKey = FName(TEXT("SmokeCharacter"));
        FString identity = TEXT("SmokeTest_PlayerA");
        FString linkedId = TEXT("SmokeTest_PlayerA_Linked");

        TWeakObjectPtr<UWorld> world;
        TWeakObjectPtr<USaveSubsystem> save;
        TWeakObjectPtr<ASaveSmokeTestPlayerState> player;
        // Strong pointers: the subsystem only holds weak ones, so without these the garbage collector could delete the fakes mid-run.
        TStrongObjectPtr<USaveSmokeTestParticipant> worldPart;
        TStrongObjectPtr<USaveSmokeTestParticipant> charPart;
        TStrongObjectPtr<USaveSmokeTestGate> gate;

        bool settingsTouched = false;
        bool originalWriteAsync = true;

        // What we expect to read back after a reload.
        FS_SmokeWorldRecord expectedWorld;
        FS_SmokeCharacterRecord expectedCharacter;
        FGuid worldId;
        FGuid characterId;

        // Used by the last (background write) step.
        int32 counterBeforeAsync = 0;
        double asyncStartedAt = 0.0;
    };

    // ---- Reporting ------------------------------------------------------------------------------------------------

    bool report(FSmokeContext& ctx, bool ok, const FString& what){
        if(ok){
            ++ctx.passed;
            UE_LOG(LogAwakenSave, Display, TEXT("[SmokeTest] PASS  %s"), *what);
        }else{
            ++ctx.failed;
            ctx.failures.Add(what);
            UE_LOG(LogAwakenSave, Error, TEXT("[SmokeTest] FAIL  %s"), *what);
        }
        return ok;
    }

    bool expectTrue(FSmokeContext& ctx, bool condition, const FString& what){
        return report(ctx, condition, what);
    }

    bool expectEqInt(FSmokeContext& ctx, int64 actual, int64 expected, const FString& what){
        return report(ctx, actual == expected, FString::Printf(TEXT("%s (expected %lld, got %lld)"), *what, expected, actual));
    }

    bool expectEqStr(FSmokeContext& ctx, const FString& actual, const FString& expected, const FString& what){
        // FString == is case-insensitive in Unreal; a save must come back exactly, so compare case-sensitively.
        return report(ctx, actual.Equals(expected, ESearchCase::CaseSensitive), FString::Printf(TEXT("%s (expected '%s', got '%s')"), *what, *expected, *actual));
    }

    bool expectTag(FSmokeContext& ctx, const FGameplayTag& actual, const FGameplayTag& expected, const FString& what){
        return report(ctx, actual == expected, FString::Printf(TEXT("%s (expected %s, got %s)"), *what, *expected.ToString(), *actual.ToString()));
    }

    FString reasonSuffix(const FGameplayTag& reason){
        return reason.IsValid() ? FString::Printf(TEXT(" -> %s"), *reason.ToString()) : FString();
    }

    bool expectOk(FSmokeContext& ctx, const FS_SaveResult& result, const FString& what){
        return report(ctx, result.success, what + reasonSuffix(result.failReasonTag));
    }

    void section(const FString& title){
        UE_LOG(LogAwakenSave, Display, TEXT("[SmokeTest] ---- %s ----"), *title);
    }

    void showOnScreen(const FString& message, const FColor& color){
        if(GEngine){
            GEngine->AddOnScreenDebugMessage(-1, 30.0f, color, message);
        }
    }

    // ---- Test data ------------------------------------------------------------------------------------------------

    FS_SmokeWorldRecord makeWorldRecord(){
        FS_SmokeWorldRecord record;
        record.gameTimeSeconds = 5000000000LL;   // more than 32 bits: proves int64 survives
        record.dayCount = 7;
        record.note = TEXT("Hello save");
        record.numbers.Add(1);
        record.numbers.Add(2);
        record.numbers.Add(3);
        record.numbers.Add(5);
        record.numbers.Add(8);
        record.location = FVector(100.5, -20.25, 300.0);
        record.notSaved = 99;
        return record;
    }

    FS_SmokeCharacterRecord makeCharacterRecord(){
        FS_SmokeCharacterRecord record;
        record.level = 12;
        record.health = 77.5f;
        record.displayName = TEXT("Tester");
        FS_SmokeItem axe;
        axe.itemId = FName(TEXT("Axe"));
        axe.count = 1;
        record.items.Add(axe);
        FS_SmokeItem rope;
        rope.itemId = FName(TEXT("Rope"));
        rope.count = 3;
        record.items.Add(rope);
        record.position = FVector(1.0, 2.0, 3.0);
        record.someId = FGuid::NewGuid();
        record.notSaved = 99;
        return record;
    }

    void expectWorldEqual(FSmokeContext& ctx, const FS_SmokeWorldRecord& actual, const FS_SmokeWorldRecord& expected, const FString& label){
        expectEqInt(ctx, actual.gameTimeSeconds, expected.gameTimeSeconds, label + TEXT(": int64 gameTimeSeconds"));
        expectEqInt(ctx, actual.dayCount, expected.dayCount, label + TEXT(": int32 dayCount"));
        expectEqStr(ctx, actual.note, expected.note, label + TEXT(": FString note"));
        expectTrue(ctx, actual.numbers == expected.numbers, label + TEXT(": TArray<int32> numbers"));
        expectTrue(ctx, actual.location.Equals(expected.location, 0.001), label + TEXT(": FVector location"));
        // The Transient field must NOT have been saved: it comes back as the default, 0.
        expectEqInt(ctx, actual.notSaved, 0, label + TEXT(": Transient field comes back as default (not saved)"));
    }

    void expectCharacterEqual(FSmokeContext& ctx, const FS_SmokeCharacterRecord& actual, const FS_SmokeCharacterRecord& expected, const FString& label){
        expectEqInt(ctx, actual.level, expected.level, label + TEXT(": int32 level"));
        expectTrue(ctx, FMath::IsNearlyEqual(actual.health, expected.health, 0.001f), label + FString::Printf(TEXT(": float health (expected %.3f, got %.3f)"), expected.health, actual.health));
        expectEqStr(ctx, actual.displayName, expected.displayName, label + TEXT(": FString displayName"));
        expectTrue(ctx, actual.position.Equals(expected.position, 0.001), label + TEXT(": FVector position"));
        expectTrue(ctx, actual.someId == expected.someId && actual.someId.IsValid(), label + TEXT(": FGuid someId"));
        expectEqInt(ctx, actual.items.Num(), expected.items.Num(), label + TEXT(": struct array item count"));
        if(actual.items.Num() == expected.items.Num()){
            for(int32 index = 0; index < expected.items.Num(); ++index){
                const bool same = actual.items[index].itemId == expected.items[index].itemId && actual.items[index].count == expected.items[index].count;
                expectTrue(ctx, same, label + FString::Printf(TEXT(": nested struct item %d (FName + int32)"), index));
            }
        }
        expectEqInt(ctx, actual.notSaved, 0, label + TEXT(": Transient field comes back as default (not saved)"));
    }

    // ---- Disk inspection ------------------------------------------------------------------------------------------

    /** Looks at the slot folder the way a human would: which files are there, what the manifest and the headers say, and that nothing
     *  half-finished is left behind. expectedBackupCounter < 0 = do not check Backup. expectedDayCount < 0 = do not check the day. */
    void checkSlotOnDisk(FSmokeContext& ctx, const FString& label, int32 expectedCounter, int32 expectedBackupCounter, int32 expectedDayCount){
        const FS_SaveSlotPaths paths = FSaveSlotStore::makePaths(ctx.slotName, 0);
        IFileManager& fileManager = IFileManager::Get();
        const FString worldFile = FSaveSlotStore::worldFileName();
        const FString characterFile = FSaveSlotStore::characterFileName(ctx.identity);
        const bool keepBackup = UAwakenSaveSettings::get()->keepBackupSet;

        expectTrue(ctx, fileManager.FileExists(*(paths.current / worldFile)), label + TEXT(": Current/World.sav exists"));
        expectTrue(ctx, fileManager.FileExists(*(paths.current / characterFile)), label + TEXT(": Current/Char_<identity>.sav exists"));
        expectTrue(ctx, fileManager.FileExists(*(paths.current / FSaveSlotStore::manifestFileName())), label + TEXT(": Current/Manifest.json exists"));

        FS_SaveManifest manifest;
        const FGameplayTag manifestReason = FSaveSlotStore::readManifest(paths.current, manifest);
        if(expectTrue(ctx, !manifestReason.IsValid(), label + TEXT(": Current/Manifest.json can be read") + reasonSuffix(manifestReason))){
            expectEqInt(ctx, manifest.saveCounter, expectedCounter, label + TEXT(": manifest saveCounter"));
            expectEqInt(ctx, manifest.files.Num(), 2, label + TEXT(": manifest lists 2 files (World + one Character)"));
            expectEqStr(ctx, manifest.slotName, ctx.slotName, label + TEXT(": manifest slotName"));
            expectEqStr(ctx, manifest.worldId, ctx.worldId.ToString(), label + TEXT(": manifest worldId"));
        }

        FS_SaveFileHeader worldHeader;
        const FGameplayTag worldReason = FSaveFileIO::readHeaderFromDisk(paths.current / worldFile, worldHeader);
        if(expectTrue(ctx, !worldReason.IsValid(), label + TEXT(": World.sav header can be read") + reasonSuffix(worldReason))){
            expectTrue(ctx, worldHeader.scope == E_SaveScope::World, label + TEXT(": World header scope is World"));
            expectEqInt(ctx, worldHeader.saveCounter, expectedCounter, label + TEXT(": World header saveCounter"));
            expectTrue(ctx, worldHeader.worldID == ctx.worldId, label + TEXT(": World header worldID"));
            expectEqInt(ctx, worldHeader.schemaVersion, AwakenSave::schemaVersion, label + TEXT(": World header schemaVersion"));
            expectTrue(ctx, worldHeader.payloadSize > 0, label + TEXT(": World header payloadSize > 0"));
            if(expectedDayCount >= 0){
                expectEqInt(ctx, worldHeader.dayCount, expectedDayCount, label + TEXT(": World header dayCount (from the participant's contributeToSlotHeader)"));
            }
        }

        FS_SaveFileHeader characterHeader;
        const FGameplayTag characterReason = FSaveFileIO::readHeaderFromDisk(paths.current / characterFile, characterHeader);
        if(expectTrue(ctx, !characterReason.IsValid(), label + TEXT(": Char file header can be read") + reasonSuffix(characterReason))){
            expectTrue(ctx, characterHeader.scope == E_SaveScope::Character, label + TEXT(": Char header scope is Character"));
            expectEqStr(ctx, characterHeader.playerIdentity, ctx.identity, label + TEXT(": Char header playerIdentity"));
            expectTrue(ctx, characterHeader.characterID == ctx.characterId && ctx.characterId.IsValid(), label + TEXT(": Char header characterID is the player's CharacterID"));
            expectTrue(ctx, characterHeader.linkedIds.Contains(ctx.linkedId), label + TEXT(": Char header linkedIds holds the linked id"));
            expectEqInt(ctx, characterHeader.saveCounter, expectedCounter, label + TEXT(": Char header saveCounter (same set, same number)"));
            expectTrue(ctx, characterHeader.worldID == ctx.worldId, label + TEXT(": Char header worldID"));
            expectTrue(ctx, !characterHeader.heldBagID.IsValid(), label + TEXT(": Char header heldBagID empty (Bagless)"));
        }

        if(keepBackup && expectedBackupCounter >= 0){
            expectTrue(ctx, fileManager.DirectoryExists(*paths.backup), label + TEXT(": Backup folder exists (keepBackupSet is on)"));
            FS_SaveManifest backupManifest;
            const FGameplayTag backupReason = FSaveSlotStore::readManifest(paths.backup, backupManifest);
            if(expectTrue(ctx, !backupReason.IsValid(), label + TEXT(": Backup/Manifest.json can be read") + reasonSuffix(backupReason))){
                expectEqInt(ctx, backupManifest.saveCounter, expectedBackupCounter, label + TEXT(": Backup holds the PREVIOUS set (its saveCounter)"));
            }
        }else if(!keepBackup){
            expectTrue(ctx, !fileManager.DirectoryExists(*paths.backup), label + TEXT(": keepBackupSet is off, so no Backup folder"));
        }

        // Nothing half-finished may stay behind: only Current, Backup and Refused (+ RefusedCharacters) are allowed next to each other.
        TArray<FString> folders;
        fileManager.FindFiles(folders, *(paths.root / TEXT("*")), false, true);
        TArray<FString> unexpected;
        for(const FString& folder : folders){
            const bool known = folder.Equals(TEXT("Current"), ESearchCase::IgnoreCase) || folder.Equals(TEXT("Backup"), ESearchCase::IgnoreCase)
                || folder.Equals(TEXT("Refused"), ESearchCase::IgnoreCase) || folder.Equals(TEXT("RefusedCharacters"), ESearchCase::IgnoreCase);
            if(!known){
                unexpected.Add(folder);
            }
        }
        expectTrue(ctx, unexpected.IsEmpty(), label + FString::Printf(TEXT(": no leftover Staging_*/Retired/Restoring folder (found: %s)"), *FString::Join(unexpected, TEXT(", "))));
    }

    // ---- Calling Save the way Game Flow (B06) will -------------------------------------------------------------------

    bool saveNow(FSmokeContext& ctx, const FString& label, int32 expectedCounter){
        USaveSubsystem* save = ctx.save.Get();
        const FS_SaveResult result = save->requestSaveSession(ctx.player.Get());
        const bool accepted = expectOk(ctx, result, label + TEXT(": requestSaveSession accepted"));
        expectTrue(ctx, !save->hasPendingSaves(), label + TEXT(": the write finished inside the call (writeAsync is off in this part)"));
        // A failed write leaves the counter where it was. The reason is in the LogAwakenSave line "Save failed: <tag>" just above.
        expectEqInt(ctx, save->getWorldSaveCounter(), expectedCounter, label + TEXT(": world save counter after the save"));
        return accepted;
    }

    /** The restore protocol of Step 14, in B06's order: world first, then the player (preview, begin, participants, finish). */
    FS_CharacterRestorePlan restoreAll(FSmokeContext& ctx, const FString& label, E_SaveRestoreSource expectedSource){
        USaveSubsystem* save = ctx.save.Get();
        ASaveSmokeTestPlayerState* player = ctx.player.Get();

        expectOk(ctx, save->restoreWorldParticipant(ctx.worldKey), label + TEXT(": restoreWorldParticipant('SmokeWorld')"));
        expectOk(ctx, save->restoreWorldBags(), label + TEXT(": restoreWorldBags"));
        save->markWorldRestored(true);

        const FS_CharacterRestorePlan plan = save->previewCharacterRestore(player);
        expectEqInt(ctx, static_cast<int32>(plan.source), static_cast<int32>(expectedSource), label + TEXT(": previewCharacterRestore source (0 None, 1 HeldState, 2 File)"));
        expectTrue(ctx, plan.characterId.IsValid(), label + TEXT(": the plan carries a valid characterId"));
        if(expectedSource == E_SaveRestoreSource::File){
            const FS_CharacterRestorePlan again = save->previewCharacterRestore(player);
            expectTrue(ctx, again.source == plan.source && again.characterId == plan.characterId, label + TEXT(": previewCharacterRestore is read-only (a second call gives the same answer)"));
            expectEqStr(ctx, plan.sourceIdentity, ctx.identity, label + TEXT(": plan sourceIdentity"));
            expectTrue(ctx, !plan.noticeTag.IsValid(), label + TEXT(": no 'world older than character' notice"));
            expectEqInt(ctx, static_cast<int32>(plan.bagOutcome), static_cast<int32>(E_SaveBagOutcome::NoConflict), label + TEXT(": bag rule says NoConflict (no bags in this test)"));
        }

        expectOk(ctx, save->beginCharacterRestore(player, plan), label + TEXT(": beginCharacterRestore"));
        expectTrue(ctx, player->characterId == plan.characterId, label + TEXT(": the CharacterID reached the PlayerState (setCharacterId was called)"));
        expectOk(ctx, save->restorePlayerParticipant(player, ctx.characterKey), label + TEXT(": restorePlayerParticipant('SmokeCharacter')"));
        save->finishCharacterRestore(player, plan, true);
        expectTag(ctx, save->evaluateSaveGate(player), FGameplayTag(), label + TEXT(": the gate now allows this player to save"));
        return plan;
    }

    /** Wipes what the fakes hold, so that a value that comes back must have come from the file. */
    void forgetEverything(FSmokeContext& ctx){
        ctx.worldPart->worldData = FS_SmokeWorldRecord();
        ctx.charPart->characterData = FS_SmokeCharacterRecord();
        if(ASaveSmokeTestPlayerState* player = ctx.player.Get()){
            player->characterId = FGuid();
        }
    }

    // ---- Finding the game ------------------------------------------------------------------------------------------

    bool isGameWorld(const UWorld* world){
        return world && world->GetGameInstance() && (world->WorldType == EWorldType::PIE || world->WorldType == EWorldType::Game);
    }

    /** The Cmd box passes a world, but it is not always the Play-In-Editor one. Look for the right one if it is not. */
    UWorld* findGameWorld(UWorld* hint){
        if(isGameWorld(hint)){
            return hint;
        }
        if(GEngine){
            for(const FWorldContext& context : GEngine->GetWorldContexts()){
                if(isGameWorld(context.World())){
                    return context.World();
                }
            }
        }
        return nullptr;
    }

    // ---- Ending a run ----------------------------------------------------------------------------------------------

    void cleanUp(FSmokeContext& ctx){
        if(USaveSubsystem* save = ctx.save.Get()){
            if(ctx.worldPart.IsValid()){
                save->unregisterParticipant(ctx.worldPart.Get());
            }
            if(ctx.charPart.IsValid()){
                save->unregisterParticipant(ctx.charPart.Get());
            }
            if(ctx.gate.IsValid()){
                save->unregisterGateSource(ctx.gate.Get());
            }
            // Leave nothing loaded: the next run (or the real game) starts clean. The files on disk stay.
            if(!save->hasPendingSaves() && save->getActiveSlotName() == ctx.slotName){
                save->unloadSlot();
            }
        }
        if(ASaveSmokeTestPlayerState* player = ctx.player.Get()){
            player->Destroy();
        }
        if(ctx.settingsTouched){
            GetMutableDefault<UAwakenSaveSettings>()->writeAsync = ctx.originalWriteAsync;
            ctx.settingsTouched = false;
        }
    }

    void finishRun(const TSharedRef<FSmokeContext>& ctxRef){
        FSmokeContext& ctx = ctxRef.Get();
        const FS_SaveSlotPaths paths = FSaveSlotStore::makePaths(ctx.slotName, 0);
        cleanUp(ctx);

        const bool allPassed = ctx.failed == 0;
        const FString summary = allPassed
            ? FString::Printf(TEXT("SAVE SMOKE TEST: ALL PASSED (%d passed, 0 failed)"), ctx.passed)
            : FString::Printf(TEXT("SAVE SMOKE TEST: FAILED (%d passed, %d failed)"), ctx.passed, ctx.failed);

        if(allPassed){
            UE_LOG(LogAwakenSave, Display, TEXT("[SmokeTest] %s"), *summary);
        }else{
            UE_LOG(LogAwakenSave, Error, TEXT("[SmokeTest] %s"), *summary);
            for(const FString& failure : ctx.failures){
                UE_LOG(LogAwakenSave, Error, TEXT("[SmokeTest]   FAILED: %s"), *failure);
            }
        }
        UE_LOG(LogAwakenSave, Display, TEXT("[SmokeTest] Files left on disk for you to look at: %s"), *FPaths::ConvertRelativePathToFull(paths.root));

        showOnScreen(summary, allPassed ? FColor::Green : FColor::Red);
        smokeRunning = false;
    }

    // =================================================================================================================
    // Part 3. The test itself
    // =================================================================================================================

    /** Steps 0 to 12. Everything here is synchronous: writeAsync is switched off so each write finishes inside the call.
     *  Returns false when the run cannot go on (a fatal check failed). */
    bool runMainPhases(FSmokeContext& ctx){
        USaveSubsystem* save = ctx.save.Get();
        UWorld* world = ctx.world.Get();
        AGameStateBase* gameState = world ? world->GetGameState() : nullptr;
        if(!save || !world || !gameState){
            return false;
        }

        UAwakenSaveSettings* settings = GetMutableDefault<UAwakenSaveSettings>();
        ctx.originalWriteAsync = settings->writeAsync;
        ctx.settingsTouched = true;
        settings->writeAsync = false;   // restored in cleanUp

        // ---- 0. Clean start -----------------------------------------------------------------------------------------
        section(TEXT("0. Clean start: delete the SmokeTest slot from an earlier run"));
        save->deleteSlot(ctx.slotName);
        if(!expectTrue(ctx, !save->slotHasFiles(ctx.slotName), TEXT("0: the SmokeTest slot folder is gone before the run (if this fails, a file may be open in another program)"))){
            return false;
        }

        // ---- 1. Set up the fakes ------------------------------------------------------------------------------------
        section(TEXT("1. Set up a fake gate, a fake player and two fake participants"));
        ctx.gate.Reset(NewObject<USaveSmokeTestGate>(GetTransientPackage()));
        save->registerGateSource(ctx.gate.Get());

        FActorSpawnParameters spawnParams;
        spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ASaveSmokeTestPlayerState* player = world->SpawnActor<ASaveSmokeTestPlayerState>(spawnParams);
        if(!expectTrue(ctx, player != nullptr, TEXT("1: the fake PlayerState could be spawned"))){
            return false;
        }
        ctx.player = player;
        player->identity = ctx.identity;
        player->linkedPlayerIds.Add(ctx.linkedId);

        // A PlayerState adds itself to the GameState's PlayerArray when it spawns. Make sure, because capture walks that array.
        bool inPlayerArray = false;
        for(APlayerState* known : gameState->PlayerArray){
            if(known == player){
                inPlayerArray = true;
                break;
            }
        }
        if(!inPlayerArray){
            gameState->AddPlayerState(player);
        }

        ctx.worldPart.Reset(NewObject<USaveSmokeTestParticipant>(GetTransientPackage()));
        ctx.worldPart->key = ctx.worldKey;
        ctx.worldPart->scope = E_SaveScope::World;
        ctx.worldPart->version = 1;
        ctx.worldPart->worldData = makeWorldRecord();

        ctx.charPart.Reset(NewObject<USaveSmokeTestParticipant>(GetTransientPackage()));
        ctx.charPart->key = ctx.characterKey;
        ctx.charPart->scope = E_SaveScope::Character;
        ctx.charPart->version = 1;
        ctx.charPart->ownerPlayer = player;
        ctx.charPart->characterData = makeCharacterRecord();

        save->registerParticipant(ctx.worldPart.Get());
        save->registerParticipant(ctx.charPart.Get());

        // ---- 2. A new world -----------------------------------------------------------------------------------------
        section(TEXT("2. beginNewWorld on a slot that has no files"));
        if(!expectOk(ctx, save->beginNewWorld(ctx.slotName), TEXT("2: beginNewWorld('SmokeTest')"))){
            return false;
        }
        expectEqStr(ctx, save->getActiveSlotName(), ctx.slotName, TEXT("2: active slot name"));
        expectTrue(ctx, save->getActiveWorldID().IsValid(), TEXT("2: the new world got a worldID"));
        expectEqInt(ctx, save->getWorldSaveCounter(), 0, TEXT("2: a new world starts at save counter 0"));
        ctx.worldId = save->getActiveWorldID();

        // ---- 3. The gate before any restore -------------------------------------------------------------------------
        section(TEXT("3. The gate refuses a save before the restore has finished"));
        expectTag(ctx, save->evaluateSaveGate(player), T_Save_FailReason_RestoreNotFinished, TEXT("3: evaluateSaveGate before restore"));
        const FS_SaveResult tooEarly = save->requestSaveSession(player);
        expectTrue(ctx, !tooEarly.success, TEXT("3: requestSaveSession is refused before restore"));
        expectTag(ctx, tooEarly.failReasonTag, T_Save_FailReason_RestoreNotFinished, TEXT("3: refusal reason"));
        expectTrue(ctx, !save->slotHasFiles(ctx.slotName), TEXT("3: a refused request wrote nothing"));

        // ---- 4. Join as a new character -----------------------------------------------------------------------------
        section(TEXT("4. The player joins a new world (no file yet): the restore protocol runs"));
        restoreAll(ctx, TEXT("4"), E_SaveRestoreSource::None);
        expectTrue(ctx, player->characterId.IsValid(), TEXT("4: the new character has a CharacterID"));
        ctx.characterId = player->characterId;

        // ---- 5. A rule from another framework refuses ---------------------------------------------------------------
        section(TEXT("5. A registered gate source can refuse a save"));
        ctx.gate->refuseWith = T_Save_FailReason_SaveInProgress;   // any tag will do: Save passes the gate's answer through
        expectTag(ctx, save->evaluateSaveGate(player), T_Save_FailReason_SaveInProgress, TEXT("5: evaluateSaveGate returns the gate source's tag"));
        const FS_SaveResult refused = save->requestSaveSession(player);
        expectTrue(ctx, !refused.success, TEXT("5: requestSaveSession is refused by the gate source"));
        expectTag(ctx, refused.failReasonTag, T_Save_FailReason_SaveInProgress, TEXT("5: refusal reason comes from the gate source"));
        expectTrue(ctx, !save->slotHasFiles(ctx.slotName), TEXT("5: a refused request wrote nothing"));
        ctx.gate->refuseWith = FGameplayTag();   // allow again

        // ---- 6. Two saves, then look at the disk --------------------------------------------------------------------
        section(TEXT("6. Save #1, change the data, save #2, then inspect the folder"));
        saveNow(ctx, TEXT("6a save #1"), 1);

        ctx.worldPart->worldData.dayCount = 8;
        ctx.worldPart->worldData.numbers.Add(13);
        ctx.charPart->characterData.level = 13;
        FS_SmokeItem torch;
        torch.itemId = FName(TEXT("Torch"));
        torch.count = 2;
        ctx.charPart->characterData.items.Add(torch);
        ctx.expectedWorld = ctx.worldPart->worldData;
        ctx.expectedCharacter = ctx.charPart->characterData;

        saveNow(ctx, TEXT("6b save #2"), 2);
        checkSlotOnDisk(ctx, TEXT("6c after save #2"), 2, 1, ctx.expectedWorld.dayCount);

        // ---- 7. The slot list ---------------------------------------------------------------------------------------
        section(TEXT("7. listSlotHeaders (the slot-selection screen)"));
        TArray<FS_SaveSlotHeader> headers;
        save->listSlotHeaders(headers);
        auto isSmokeSlot = [&ctx](const FS_SaveSlotHeader& header){
            return header.slotName == ctx.slotName;
        };
        const FS_SaveSlotHeader* mine = headers.FindByPredicate(isSmokeSlot);
        if(expectTrue(ctx, mine != nullptr, TEXT("7: the SmokeTest slot is in the list"))){
            expectTrue(ctx, mine->hasFiles, TEXT("7: hasFiles is true"));
            expectTrue(ctx, !mine->unreadableReason.IsValid(), TEXT("7: no unreadableReason") + reasonSuffix(mine->unreadableReason));
            expectEqInt(ctx, mine->dayCount, ctx.expectedWorld.dayCount, TEXT("7: slot header dayCount"));
            expectEqInt(ctx, mine->schemaVersion, AwakenSave::schemaVersion, TEXT("7: slot header schemaVersion"));
            const FTimespan age = FDateTime::UtcNow() - mine->savedAtUtc;
            expectTrue(ctx, FMath::Abs(age.GetTotalSeconds()) < 600.0, FString::Printf(TEXT("7: slot header savedAtUtc is recent (%.0f s ago)"), age.GetTotalSeconds()));
        }

        // ---- 8. Reload and restore ----------------------------------------------------------------------------------
        section(TEXT("8. unloadSlot, forget everything, loadSlot, restore: the values must come back from the files"));
        save->unloadSlot();
        expectTrue(ctx, save->getActiveSlotName().IsEmpty(), TEXT("8: unloadSlot cleared the active slot"));
        forgetEverything(ctx);
        if(!expectOk(ctx, save->loadSlot(ctx.slotName), TEXT("8: loadSlot('SmokeTest')"))){
            return false;
        }
        expectEqInt(ctx, save->getWorldSaveCounter(), 2, TEXT("8: the loaded slot is at counter 2"));
        expectTrue(ctx, save->getActiveWorldID() == ctx.worldId, TEXT("8: the worldID is the one the world was created with"));
        expectTrue(ctx, !save->didLastLoadUseBackup(), TEXT("8: the load did not need the Backup"));
        restoreAll(ctx, TEXT("8"), E_SaveRestoreSource::File);
        expectWorldEqual(ctx, ctx.worldPart->worldData, ctx.expectedWorld, TEXT("8 world data"));
        expectCharacterEqual(ctx, ctx.charPart->characterData, ctx.expectedCharacter, TEXT("8 character data"));
        expectTrue(ctx, player->characterId == ctx.characterId, TEXT("8: the returning player got the SAME CharacterID back"));

        // ---- 9. Save after the reload -------------------------------------------------------------------------------
        section(TEXT("9. Save again after a reload"));
        saveNow(ctx, TEXT("9 save #3"), 3);
        checkSlotOnDisk(ctx, TEXT("9 after save #3"), 3, 2, ctx.expectedWorld.dayCount);

        // ---- 10. A record nobody claims survives a save -------------------------------------------------------------
        section(TEXT("10. A World record whose participant is not registered must survive a save (carry forward)"));
        save->unregisterParticipant(ctx.worldPart.Get());
        saveNow(ctx, TEXT("10a save with no World participant registered"), 4);
        save->unloadSlot();
        forgetEverything(ctx);
        if(!expectOk(ctx, save->loadSlot(ctx.slotName), TEXT("10b: loadSlot"))){
            return false;
        }
        expectEqInt(ctx, save->getWorldSaveCounter(), 4, TEXT("10b: the loaded slot is at counter 4"));

        UE_LOG(LogAwakenSave, Display, TEXT("[SmokeTest] The next line, an Error from LogAwakenSave about 'no World participant is registered', is EXPECTED: it is the late-registration guard doing its job."));
        const FS_SaveResult late = save->restoreWorldParticipant(ctx.worldKey);
        expectTrue(ctx, !late.success, TEXT("10c: restoring a record with no registered participant is refused"));
        expectTag(ctx, late.failReasonTag, T_Save_FailReason_ParticipantNotRegistered, TEXT("10c: refusal reason"));

        save->registerParticipant(ctx.worldPart.Get());
        restoreAll(ctx, TEXT("10d"), E_SaveRestoreSource::File);
        expectWorldEqual(ctx, ctx.worldPart->worldData, ctx.expectedWorld, TEXT("10d world data (the record was carried forward)"));
        expectCharacterEqual(ctx, ctx.charPart->characterData, ctx.expectedCharacter, TEXT("10d character data"));

        // ---- 11. Migration ------------------------------------------------------------------------------------------
        section(TEXT("11. Migration: a version-1 record is restored by a participant that is now at version 2"));
        save->unregisterParticipant(ctx.worldPart.Get());
        TStrongObjectPtr<USaveSmokeTestParticipant> worldPartV2;
        worldPartV2.Reset(NewObject<USaveSmokeTestParticipant>(GetTransientPackage()));
        worldPartV2->key = ctx.worldKey;
        worldPartV2->scope = E_SaveScope::World;
        worldPartV2->version = 2;
        save->registerParticipant(worldPartV2.Get());
        expectOk(ctx, save->restoreWorldParticipant(ctx.worldKey), TEXT("11: restoreWorldParticipant through a 1 -> 2 migration"));
        expectEqInt(ctx, worldPartV2->migrateCalls, 1, TEXT("11: migrateSnapshotStep ran once"));
        expectEqInt(ctx, worldPartV2->lastSavedVersion, 1, TEXT("11: the participant was told the record was saved at version 1"));
        expectEqStr(ctx, worldPartV2->worldData.note, ctx.expectedWorld.note + TEXT("+migrated"), TEXT("11: the migrated data reached restoreFromSnapshot"));
        save->unregisterParticipant(worldPartV2.Get());
        save->registerParticipant(ctx.worldPart.Get());

        // ---- 12. Damage Current on purpose --------------------------------------------------------------------------
        section(TEXT("12. Damage Current/World.sav on purpose: the slot must come back from Backup"));
        if(!UAwakenSaveSettings::get()->keepBackupSet){
            // Without a Backup set there is nothing to fall back to, so this step would only prove that damage is fatal.
            UE_LOG(LogAwakenSave, Display, TEXT("[SmokeTest] Step 12 skipped: keepBackupSet is off in Project Settings > Awaken > Save."));
        }else{
            const FS_SaveSlotPaths paths = FSaveSlotStore::makePaths(ctx.slotName, 0);
            const FString worldPath = paths.current / FSaveSlotStore::worldFileName();
            TArray<uint8> bytes;
            const bool loaded = FFileHelper::LoadFileToArray(bytes, *worldPath);
            if(expectTrue(ctx, loaded && bytes.Num() > 0, TEXT("12: World.sav could be read for damaging"))){
                bytes.Last() = static_cast<uint8>(~bytes.Last());   // flip every bit of the last byte (inside the payload)
                expectTrue(ctx, FFileHelper::SaveArrayToFile(bytes, *worldPath), TEXT("12: World.sav written back with one damaged byte"));
            }

            save->unloadSlot();
            forgetEverything(ctx);
            UE_LOG(LogAwakenSave, Display, TEXT("[SmokeTest] A Warning/Error from LogAwakenSave about a failed check or the Backup in the next lines is EXPECTED."));
            if(!expectOk(ctx, save->loadSlot(ctx.slotName), TEXT("12: loadSlot succeeds although Current is damaged"))){
                return false;
            }
            expectTrue(ctx, save->didLastLoadUseBackup(), TEXT("12: didLastLoadUseBackup is true"));
            expectEqInt(ctx, save->getWorldSaveCounter(), 3, TEXT("12: the slot loaded from the Backup set (counter 3, not the damaged 4)"));
            expectTrue(ctx, IFileManager::Get().DirectoryExists(*paths.refused), TEXT("12: the damaged set was parked in Refused"));
            restoreAll(ctx, TEXT("12"), E_SaveRestoreSource::File);
            expectWorldEqual(ctx, ctx.worldPart->worldData, ctx.expectedWorld, TEXT("12 world data (read from the Backup set)"));
            expectCharacterEqual(ctx, ctx.charPart->characterData, ctx.expectedCharacter, TEXT("12 character data (read from the Backup set)"));
        }
        return true;
    }

    /** Step 13 start: a save with writeAsync ON (the way the game really writes). Returns after the request; the check follows later. */
    void startAsyncPhase(const TSharedRef<FSmokeContext>& ctxRef);

    /** Step 13 end: called every frame until the background write has finished. Returns true to be called again. */
    bool tickAsyncPhase(const TSharedRef<FSmokeContext>& ctxRef){
        FSmokeContext& ctx = ctxRef.Get();
        USaveSubsystem* save = ctx.save.Get();
        if(!save || !ctx.world.IsValid() || !ctx.player.IsValid()){
            report(ctx, false, TEXT("13: Play-In-Editor stopped before the background save finished"));
            finishRun(ctxRef);
            return false;
        }

        if(save->hasPendingSaves()){
            if(FPlatformTime::Seconds() - ctx.asyncStartedAt > 20.0){
                report(ctx, false, TEXT("13: the background save did not finish within 20 seconds"));
                finishRun(ctxRef);
                return false;
            }
            return true;   // still writing: look again next frame
        }

        const int32 expectedCounter = ctx.counterBeforeAsync + 1;
        expectEqInt(ctx, save->getWorldSaveCounter(), expectedCounter, TEXT("13: world save counter after the background save"));
        checkSlotOnDisk(ctx, TEXT("13 after the background save"), expectedCounter, ctx.counterBeforeAsync, ctx.expectedWorld.dayCount);

        // And once more from the files, to prove the background-written set is a real one.
        save->unloadSlot();
        forgetEverything(ctx);
        if(expectOk(ctx, save->loadSlot(ctx.slotName), TEXT("13: loadSlot of the set written in the background"))){
            restoreAll(ctx, TEXT("13"), E_SaveRestoreSource::File);
            expectWorldEqual(ctx, ctx.worldPart->worldData, ctx.expectedWorld, TEXT("13 world data"));
            expectCharacterEqual(ctx, ctx.charPart->characterData, ctx.expectedCharacter, TEXT("13 character data (level 99 was written in the background)"));
        }
        finishRun(ctxRef);
        return false;
    }

    void startAsyncPhase(const TSharedRef<FSmokeContext>& ctxRef){
        FSmokeContext& ctx = ctxRef.Get();
        USaveSubsystem* save = ctx.save.Get();
        ASaveSmokeTestPlayerState* player = ctx.player.Get();
        if(!save || !player){
            finishRun(ctxRef);
            return;
        }

        section(TEXT("13. Save with writeAsync ON (the way the game really writes): the write runs on a worker thread"));
        GetMutableDefault<UAwakenSaveSettings>()->writeAsync = true;   // cleanUp puts the original value back
        ctx.charPart->characterData.level = 99;   // a marker value, so we can tell this save from the earlier ones
        ctx.expectedCharacter = ctx.charPart->characterData;
        ctx.counterBeforeAsync = save->getWorldSaveCounter();

        const FS_SaveResult result = save->requestSaveSession(player);
        expectOk(ctx, result, TEXT("13: requestSaveSession accepted"));
        if(!result.success){
            finishRun(ctxRef);
            return;
        }
        // finishSave is posted back to the game thread, so it cannot run before this function returns.
        expectTrue(ctx, save->hasPendingSaves(), TEXT("13: the write is still pending right after the request (it runs in the background)"));

        ctx.asyncStartedAt = FPlatformTime::Seconds();
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([ctxRef](float deltaTime){
            return tickAsyncPhase(ctxRef);
        }), 0.0f);
    }

    // =================================================================================================================
    // The console command
    // =================================================================================================================

    void runSaveSmokeTest(const TArray<FString>& arguments, UWorld* hintWorld){
        if(smokeRunning){
            UE_LOG(LogAwakenSave, Warning, TEXT("[SmokeTest] A run is already in progress. Wait for the green or red message."));
            return;
        }

        UWorld* world = findGameWorld(hintWorld);
        if(!world){
            UE_LOG(LogAwakenSave, Error, TEXT("[SmokeTest] Refuses to run: there is no game world. Press Play (PIE) first, then type the command again."));
            showOnScreen(TEXT("SAVE SMOKE TEST: press Play (PIE) first"), FColor::Red);
            return;
        }
        UGameInstance* gameInstance = world->GetGameInstance();
        USaveSubsystem* save = gameInstance ? gameInstance->GetSubsystem<USaveSubsystem>() : nullptr;
        if(!save){
            UE_LOG(LogAwakenSave, Error, TEXT("[SmokeTest] Refuses to run: USaveSubsystem was not found on the game instance."));
            showOnScreen(TEXT("SAVE SMOKE TEST: USaveSubsystem not found"), FColor::Red);
            return;
        }
        if(world->GetNetMode() == NM_Client){
            UE_LOG(LogAwakenSave, Error, TEXT("[SmokeTest] Refuses to run: this world is a network client. Save is server-only."));
            showOnScreen(TEXT("SAVE SMOKE TEST: this is a client, run it on the host"), FColor::Red);
            return;
        }
        if(!world->GetGameState()){
            UE_LOG(LogAwakenSave, Error, TEXT("[SmokeTest] Refuses to run: the world has no GameState."));
            showOnScreen(TEXT("SAVE SMOKE TEST: no GameState in this world"), FColor::Red);
            return;
        }

        const FString smokeSlot = TEXT("SmokeTest");
        if(!save->getActiveSlotName().IsEmpty() && save->getActiveSlotName() != smokeSlot){
            UE_LOG(LogAwakenSave, Error, TEXT("[SmokeTest] Refuses to run: the real slot '%s' is loaded. Stop PIE and start it again without loading a slot."), *save->getActiveSlotName());
            showOnScreen(TEXT("SAVE SMOKE TEST: refuses to run, a real slot is loaded"), FColor::Red);
            return;
        }
        if(save->hasPendingSaves()){
            UE_LOG(LogAwakenSave, Error, TEXT("[SmokeTest] Refuses to run: a save is in progress."));
            showOnScreen(TEXT("SAVE SMOKE TEST: a save is in progress"), FColor::Red);
            return;
        }

        smokeRunning = true;
        UE_LOG(LogAwakenSave, Display, TEXT("[SmokeTest] ================ START: Awaken.Save.SmokeTest ================"));

        const TSharedRef<FSmokeContext> ctxRef = MakeShared<FSmokeContext>();
        ctxRef->world = world;
        ctxRef->save = save;

        const bool canGoOn = runMainPhases(ctxRef.Get());
        if(canGoOn){
            startAsyncPhase(ctxRef);   // finishes later, from the ticker
        }else{
            finishRun(ctxRef);
        }
    }

    FAutoConsoleCommandWithWorldAndArgs saveSmokeTestCommand(
        TEXT("Awaken.Save.SmokeTest"),
        TEXT("Dev only. Runs the Save Framework smoke test on the slot 'SmokeTest' with a fake player. Needs Play-In-Editor with no real slot loaded."),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&runSaveSmokeTest));
}