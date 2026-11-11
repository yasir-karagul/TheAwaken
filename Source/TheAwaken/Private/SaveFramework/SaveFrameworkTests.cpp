// Private/SaveFramework/Tests/SaveFrameworkTests.cpp
//
// Automation tests for the Save framework. They cover guide test plan 1 (envelope), 2 (bag rule), 3 (both or neither),
// 4 (recovery), 9 (cut a commit at every file operation), 24 (I/O faults) and one check for open item 19 (slot names).
//
// They need NO world and NO other framework: they only drive the pure functions and the file layer, on plain byte arrays and
// on temporary folders under Intermediate/SaveAutomation (each test deletes its own folder at the end). Tests 9 and 24 put a
// fault-injecting file system on top of the engine's platform file chain while they run.
//
// Run (Output Log "Cmd" box, no Play needed):   Automation RunTests Awaken.Save
// Or: Tools > Test Automation, type "Awaken.Save" in the filter box, tick the tests, press Start Tests.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SaveFramework/SaveFileIO.h"
#include "SaveFramework/SaveSlotStore.h"
#include "SaveFramework/SaveBagReconciler.h"
#include "SaveFramework/SaveConstants.h"
#include "SaveFramework/02Tags/T_SaveFramework.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"

namespace AwakenSaveTests{
    // ---- Small helpers ------------------------------------------------------------------------------------------------

    FString tagName(const FGameplayTag& tag){
        return tag.IsValid() ? tag.ToString() : FString(TEXT("None (success)"));
    }

    void expectTag(FAutomationTestBase& test, const TCHAR* what, const FGameplayTag& actual, const FGameplayTag& expected){
        test.TestTrue(FString::Printf(TEXT("%s (expected %s, got %s)"), what, *tagName(expected), *tagName(actual)), actual == expected);
    }

    TArray<uint8> makePayload(int32 size, int32 seed = 0){
        TArray<uint8> payload;
        payload.SetNumUninitialized(size);
        for(int32 index = 0; index < size; ++index){
            payload[index] = static_cast<uint8>(index * 31 + 7 + seed * 13);
        }
        return payload;
    }

    /** A header that matches its payload, the way buildFileBytes would fill it. */
    FS_SaveFileHeader makeGoodHeader(E_SaveScope scope, const FGuid& worldId, int32 counter, const TArray<uint8>& payload){
        FS_SaveFileHeader header;
        header.scope = scope;
        header.worldID = worldId;
        header.saveCounter = counter;
        header.schemaVersion = AwakenSave::schemaVersion;
        header.buildVersion = TEXT("automation");
        header.writtenAtUtc = FDateTime::UtcNow();
        header.payloadSize = payload.Num();
        header.payloadCrc = FCrc::MemCrc32(payload.GetData(), payload.Num());
        return header;
    }

    /** Builds a file with the same layout as FSaveFileIO::buildFileBytes, but the test chooses the magic, the envelope version, the
     *  header and the sizes. extraHeaderBytes = unknown bytes after the header. headerLenOverride >= 0 = write that length instead. */
    TArray<uint8> buildRawFile(uint32 magic, int32 envelopeVersion, FS_SaveFileHeader header, const TArray<uint8>& payload, int32 extraHeaderBytes, int32 headerLenOverride){
        TArray<uint8> headerBytes;
        FMemoryWriter headerWriter(headerBytes);
        headerWriter << header;
        for(int32 index = 0; index < extraHeaderBytes; ++index){
            headerBytes.Add(0xAB);
        }

        int32 headerLen = headerLenOverride >= 0 ? headerLenOverride : headerBytes.Num();
        TArray<uint8> file;
        FMemoryWriter writer(file);
        writer << magic << envelopeVersion << headerLen;
        writer.Serialize(headerBytes.GetData(), headerBytes.Num());
        writer.Serialize(const_cast<uint8*>(payload.GetData()), payload.Num());
        return file;
    }

    FGameplayTag validateBytes(const TArray<uint8>& file, int32 minSchema){
        FS_SaveFileHeader scratch;
        int32 scratchOffset = 0;
        return FSaveFileIO::validateFile(file, scratch, scratchOffset, minSchema);
    }

    // ---- Sets on disk -------------------------------------------------------------------------------------------------

    /** A temporary slot folder with every path built by hand (makePaths would read the real settings). Deleted in the destructor. */
    struct FTestSlot{
        FString root;
        FS_SaveSlotPaths paths;

        FTestSlot(){
            root = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir()) / TEXT("SaveAutomation") / FGuid::NewGuid().ToString();
            paths.root = root;
            paths.current = root / TEXT("Current");
            paths.backup = root / TEXT("Backup");
            paths.retired = root / TEXT("Retired");
            paths.refused = root / TEXT("Refused");
            paths.refusedCharacters = root / TEXT("RefusedCharacters");
            paths.staging = root / TEXT("Staging_1");
            IFileManager::Get().MakeDirectory(*root, true);
        }

        ~FTestSlot(){
            IFileManager::Get().DeleteDirectory(*root, false, true);
        }

        FString stagingFor(int32 counter) const{
            return root / FString::Printf(TEXT("Staging_%d"), counter);
        }
    };

    FS_SaveWriteJob::FS_OutFile makeOutFile(const FString& fileName, E_SaveScope scope, const FGuid& worldId, int32 counter, int32 payloadSize, const FString& identity){
        FS_SaveWriteJob::FS_OutFile out;
        out.fileName = fileName;
        out.header.scope = scope;
        out.header.worldID = worldId;
        out.header.saveCounter = counter;
        out.header.playerIdentity = identity;
        FSaveFileIO::buildFileBytes(out.header, makePayload(payloadSize, counter), out.bytes);   // also fills the CRC in out.header
        return out;
    }

    FS_SaveManifestEntry makeEntry(const FS_SaveWriteJob::FS_OutFile& file){
        FS_SaveManifestEntry entry;
        entry.fileName = file.fileName;
        entry.scope = static_cast<int32>(file.header.scope);
        entry.size = file.bytes.Num();
        entry.payloadCrc = file.header.payloadCrc;
        entry.playerIdentity = file.header.playerIdentity;
        return entry;
    }

    FS_SaveManifest makeManifest(const FGuid& worldId, int32 counter){
        FS_SaveManifest manifest;
        manifest.schemaVersion = AwakenSave::schemaVersion;
        manifest.slotName = TEXT("AutomationSlot");
        manifest.worldId = worldId.ToString();
        manifest.saveCounter = counter;
        return manifest;
    }

    /** A write job with a World file and one Character file. breakLastFile = the Character file gets a name that cannot be written
     *  (a path inside World.sav, which is a file, not a folder). */
    FS_SaveWriteJob makeJob(const FTestSlot& slot, const FGuid& worldId, int32 counter, bool keepBackup, bool breakLastFile){
        FS_SaveWriteJob job;
        job.paths = slot.paths;
        job.paths.staging = slot.stagingFor(counter);
        job.counter = counter;
        job.minSchema = 1;
        job.keepBackup = keepBackup;
        job.manifest = makeManifest(worldId, counter);

        FS_SaveWriteJob::FS_OutFile world = makeOutFile(FSaveSlotStore::worldFileName(), E_SaveScope::World, worldId, counter, 200, FString());
        FS_SaveWriteJob::FS_OutFile character = makeOutFile(FSaveSlotStore::characterFileName(TEXT("PlayerA")), E_SaveScope::Character, worldId, counter, 120, TEXT("PlayerA"));
        job.manifest.files.Add(makeEntry(world));
        job.manifest.files.Add(makeEntry(character));
        if(breakLastFile){
            character.fileName = world.fileName / character.fileName;
        }
        job.filesToWrite.Add(world);
        job.filesToWrite.Add(character);   // the LAST file of the set
        return job;
    }

    /** Writes a whole, valid set (World + one Character file per identity + manifest) straight into dir. worldSchema > 0 stamps that schema number on the World file. */
    void makeSetOf(const FString& dir, const FGuid& worldId, int32 counter, const TArray<FString>& identities, int32 worldSchema = 0){
        IFileManager::Get().MakeDirectory(*dir, true);
        FS_SaveManifest manifest = makeManifest(worldId, counter);

        FS_SaveWriteJob::FS_OutFile world = makeOutFile(FSaveSlotStore::worldFileName(), E_SaveScope::World, worldId, counter, 200, FString());
        if(worldSchema > 0){
            const TArray<uint8> payload = makePayload(200, counter);
            FS_SaveFileHeader header = makeGoodHeader(E_SaveScope::World, worldId, counter, payload);
            header.schemaVersion = worldSchema;
            world.header = header;
            world.bytes = buildRawFile(AwakenSave::fileMagic, AwakenSave::envelopeVersion, header, payload, 0, -1);
        }
        FFileHelper::SaveArrayToFile(world.bytes, *(dir / world.fileName));
        manifest.files.Add(makeEntry(world));

        for(const FString& identity : identities){
            FS_SaveWriteJob::FS_OutFile character = makeOutFile(FSaveSlotStore::characterFileName(identity), E_SaveScope::Character, worldId, counter, 120, identity);
            FFileHelper::SaveArrayToFile(character.bytes, *(dir / character.fileName));
            manifest.files.Add(makeEntry(character));
        }
        FSaveSlotStore::writeManifest(dir, manifest);
    }

    void makeSet(const FString& dir, const FGuid& worldId, int32 counter, int32 worldSchema = 0){
        makeSetOf(dir, worldId, counter, TArray<FString>{TEXT("PlayerA")}, worldSchema);
    }

    /** Flips the last byte of a file: the payload checksum no longer matches. */
    void damageFile(const FString& path){
        TArray<uint8> bytes;
        if(FFileHelper::LoadFileToArray(bytes, *path) && bytes.Num() > 0){
            bytes[bytes.Num() - 1] ^= 0xFF;
            FFileHelper::SaveArrayToFile(bytes, *path);
        }
    }

    bool dirExists(const FString& path){
        return IFileManager::Get().DirectoryExists(*path);
    }

    FGameplayTag recover(const FS_SaveSlotPaths& paths, FS_SaveManifest& manifest, bool& usedBackup){
        TArray<FS_SaveManifestEntry> bad;
        return FSaveSlotStore::recoverSlot(paths, 1, true, manifest, bad, usedBackup);
    }

    FS_BagRecord makeBag(const FGuid& bagId, const FGuid& owner){
        FS_BagRecord bag;
        bag.bagID = bagId;
        bag.ownerCharacterID = owner;
        return bag;
    }
    // ---- A file system that fails on demand ---------------------------------------------------------------------------
    //
    // Sits on top of the engine's platform file chain (FPlatformFileManager::SetPlatformFile), so every IFileManager call Save makes
    // goes through it. Only paths under rootFilter are counted or faulted, so the editor's own file traffic is never touched.
    // The counted operations are the ones that change the disk: OpenWrite, handle Write and Flush, MoveFile, DeleteFile,
    // CreateDirectory, DeleteDirectory. Reads pass through uncounted.

    enum class EFaultMode : uint8{
        Fail,            // from the Nth counted operation on, every later counted operation fails: a crash
        TruncateWrite    // the Nth operation, if it is a Write, writes only half its bytes and reports success; everything else works
    };

    enum class EFaultAction : uint8{
        Proceed,
        Fail,
        Truncate
    };

    class FFaultFileSystem;

    class FFaultFileHandle : public IFileHandle{
    public:
        FFaultFileHandle(IFileHandle* innerHandle, FFaultFileSystem* inOwner) : inner(innerHandle), owner(inOwner){}
        virtual ~FFaultFileHandle() override{ delete inner; }

        virtual int64 Tell() override{ return inner->Tell(); }
        virtual bool Seek(int64 newPosition) override{ return inner->Seek(newPosition); }
        virtual bool SeekFromEnd(int64 newPositionRelativeToEnd = 0) override{ return inner->SeekFromEnd(newPositionRelativeToEnd); }
        virtual bool Read(uint8* destination, int64 bytesToRead) override{ return inner->Read(destination, bytesToRead); }
        virtual bool ReadAt(uint8* destination, int64 bytesToRead, int64 offset) override{ return inner->ReadAt(destination, bytesToRead, offset); }
        virtual bool Truncate(int64 newSize) override{ return inner->Truncate(newSize); }
        virtual int64 Size() override{ return inner->Size(); }
        virtual bool Write(const uint8* source, int64 bytesToWrite) override;
        virtual bool Flush(const bool bFullFlush = false) override;

    private:
        IFileHandle* inner;
        FFaultFileSystem* owner;
    };

    class FFaultFileSystem : public IPlatformFile{
    public:
        // ---- settings
        FString rootFilter;                    // only paths under this folder are counted and faulted
        EFaultMode mode = EFaultMode::Fail;
        int32 failFrom = 0;                    // 1-based index of the first operation that fails; 0 = never
        FString failOpenReadContaining;        // OpenRead of a path that contains this text fails (a locked file)
        int32 failMoveTimes = 0;               // > 0: the next N MoveFile calls fail, then they work. < 0: every MoveFile call fails

        // ---- results
        int32 opCount = 0;
        int32 faultsInjected = 0;
        TArray<bool> opIsWrite;                // per counted operation: was it a handle Write?

        static FString normalizePath(const FString& path){
            FString normalized = path;
            normalized.ReplaceInline(TEXT("\\"), TEXT("/"));
            return normalized;
        }

        bool isMine(const TCHAR* path) const{
            return !rootFilter.IsEmpty() && normalizePath(path).StartsWith(rootFilter, ESearchCase::IgnoreCase);
        }

        EFaultAction nextAction(bool isWrite){
            ++opCount;
            opIsWrite.Add(isWrite);
            if(mode == EFaultMode::Fail){
                if(failFrom <= 0 || opCount < failFrom){
                    return EFaultAction::Proceed;
                }
                ++faultsInjected;
                return EFaultAction::Fail;
            }
            if(failFrom > 0 && opCount == failFrom && isWrite){
                ++faultsInjected;
                return EFaultAction::Truncate;
            }
            return EFaultAction::Proceed;
        }

        virtual bool ShouldBeUsed(IPlatformFile* inner, const TCHAR* cmdLine) const override{ return true; }
        virtual bool Initialize(IPlatformFile* inner, const TCHAR* cmdLine) override{ lower = inner; return lower != nullptr; }
        virtual IPlatformFile* GetLowerLevel() override{ return lower; }
        virtual void SetLowerLevel(IPlatformFile* newLowerLevel) override{ lower = newLowerLevel; }
        virtual const TCHAR* GetName() const override{ return TEXT("AwakenSaveFaultFile"); }

        virtual bool FileExists(const TCHAR* filename) override{ return lower->FileExists(filename); }
        virtual int64 FileSize(const TCHAR* filename) override{ return lower->FileSize(filename); }
        virtual bool IsReadOnly(const TCHAR* filename) override{ return lower->IsReadOnly(filename); }
        virtual bool SetReadOnly(const TCHAR* filename, bool newReadOnlyValue) override{ return lower->SetReadOnly(filename, newReadOnlyValue); }
        virtual FDateTime GetTimeStamp(const TCHAR* filename) override{ return lower->GetTimeStamp(filename); }
        virtual void SetTimeStamp(const TCHAR* filename, FDateTime dateTime) override{ lower->SetTimeStamp(filename, dateTime); }
        virtual FDateTime GetAccessTimeStamp(const TCHAR* filename) override{ return lower->GetAccessTimeStamp(filename); }
        virtual FString GetFilenameOnDisk(const TCHAR* filename) override{ return lower->GetFilenameOnDisk(filename); }
        virtual bool DirectoryExists(const TCHAR* directory) override{ return lower->DirectoryExists(directory); }
        virtual FFileStatData GetStatData(const TCHAR* filenameOrDirectory) override{ return lower->GetStatData(filenameOrDirectory); }
        virtual bool IterateDirectory(const TCHAR* directory, FDirectoryVisitor& visitor) override{ return lower->IterateDirectory(directory, visitor); }
        virtual bool IterateDirectoryStat(const TCHAR* directory, FDirectoryStatVisitor& visitor) override{ return lower->IterateDirectoryStat(directory, visitor); }
        virtual IAsyncReadFileHandle* OpenAsyncRead(const TCHAR* filename, bool allowWrite = false){ return lower->OpenAsyncRead(filename, allowWrite); }

        virtual IFileHandle* OpenRead(const TCHAR* filename, bool allowWrite = false) override{
            if(!failOpenReadContaining.IsEmpty() && FString(filename).Contains(failOpenReadContaining)){
                ++faultsInjected;
                return nullptr;
            }
            return lower->OpenRead(filename, allowWrite);
        }

        virtual IFileHandle* OpenWrite(const TCHAR* filename, bool append = false, bool allowRead = false) override{
            const bool mine = isMine(filename);
            if(mine && nextAction(false) == EFaultAction::Fail){
                return nullptr;
            }
            IFileHandle* innerHandle = lower->OpenWrite(filename, append, allowRead);
            return (innerHandle && mine) ? new FFaultFileHandle(innerHandle, this) : innerHandle;
        }

        virtual bool DeleteFile(const TCHAR* filename) override{
            if(isMine(filename) && nextAction(false) == EFaultAction::Fail){
                return false;
            }
            return lower->DeleteFile(filename);
        }

        virtual bool MoveFile(const TCHAR* to, const TCHAR* from) override{
            if(isMine(to) || isMine(from)){
                if(nextAction(false) == EFaultAction::Fail){
                    return false;
                }
                if(failMoveTimes != 0){
                    if(failMoveTimes > 0){
                        --failMoveTimes;
                    }
                    ++faultsInjected;
                    return false;
                }
            }
            return lower->MoveFile(to, from);
        }

        virtual bool CreateDirectory(const TCHAR* directory) override{
            if(isMine(directory) && nextAction(false) == EFaultAction::Fail){
                return false;
            }
            return lower->CreateDirectory(directory);
        }

        virtual bool DeleteDirectory(const TCHAR* directory) override{
            if(isMine(directory) && nextAction(false) == EFaultAction::Fail){
                return false;
            }
            return lower->DeleteDirectory(directory);
        }

    private:
        IPlatformFile* lower = nullptr;
    };

    inline bool FFaultFileHandle::Write(const uint8* source, int64 bytesToWrite){
        switch(owner->nextAction(true)){
            case EFaultAction::Fail:
                return false;
            case EFaultAction::Truncate:
                return inner->Write(source, bytesToWrite / 2);   // half the bytes, and a success: a silently short write
            default:
                return inner->Write(source, bytesToWrite);
        }
    }

    inline bool FFaultFileHandle::Flush(const bool bFullFlush){
        if(owner->nextAction(false) == EFaultAction::Fail){
            return false;
        }
        return inner->Flush(bFullFlush);
    }

    /** Puts the fault file system on top of the chain for the life of this object. Declare it AFTER the FTestSlot, so it is removed first. */
    class FScopedFaultFileSystem{
    public:
        FFaultFileSystem fileSystem;

        explicit FScopedFaultFileSystem(const FString& root){
            fileSystem.rootFilter = FFaultFileSystem::normalizePath(root);
            previous = &FPlatformFileManager::Get().GetPlatformFile();
            fileSystem.Initialize(previous, TEXT(""));
            FPlatformFileManager::Get().SetPlatformFile(fileSystem);
        }

        ~FScopedFaultFileSystem(){
            FPlatformFileManager::Get().SetPlatformFile(*previous);
        }

    private:
        IPlatformFile* previous = nullptr;
    };

    // ---- Test 9: cut a commit at every file operation ---------------------------------------------------------------

    /** Old state: Current = save #1 (and, with keepBackup, Backup = save #0). The commit under test writes save #2. */
    void makeOldState(const FTestSlot& slot, const FGuid& worldId, bool keepBackup){
        makeSet(slot.paths.current, worldId, 1);
        if(keepBackup){
            makeSet(slot.paths.backup, worldId, 0);
        }
    }

    void checkRecoveredSlot(FAutomationTestBase& test, const FString& label, const FTestSlot& slot, bool keepBackup, EFaultMode mode, const FGameplayTag& commitResult){
        FS_SaveManifest manifest;
        TArray<FS_SaveManifestEntry> bad;
        bool usedBackup = false;
        const FGameplayTag recovered = FSaveSlotStore::recoverSlot(slot.paths, 1, keepBackup, manifest, bad, usedBackup);   // "the next start"
        test.TestTrue(FString::Printf(TEXT("%s: recovery succeeds (got %s)"), *label, *tagName(recovered)), !recovered.IsValid());
        if(recovered.IsValid()){
            return;
        }

        test.TestTrue(FString::Printf(TEXT("%s: the slot is exactly the old set (1) or the new set (2), got %d"), *label, manifest.saveCounter), manifest.saveCounter == 1 || manifest.saveCounter == 2);
        test.TestFalse(FString::Printf(TEXT("%s: recovery did not need the Backup"), *label), usedBackup);
        if(!commitResult.IsValid()){
            test.TestEqual(FString::Printf(TEXT("%s: a commit that reported success is the new set"), *label), manifest.saveCounter, 2);
        }
        if(mode == EFaultMode::TruncateWrite){
            test.TestTrue(FString::Printf(TEXT("%s: a silently shortened write is caught (the save is refused)"), *label), commitResult.IsValid());
            test.TestEqual(FString::Printf(TEXT("%s: so the old set is still the slot"), *label), manifest.saveCounter, 1);
        }

        // Whole set, no mix: Current validates, and every file in it carries the counter of the manifest.
        test.TestTrue(FString::Printf(TEXT("%s: Current exists"), *label), dirExists(slot.paths.current));
        FS_SaveManifest whole;
        const FGameplayTag wholeResult = FSaveSlotStore::validateSet(slot.paths.current, 1, whole);
        test.TestTrue(FString::Printf(TEXT("%s: Current validates as a whole set (got %s)"), *label, *tagName(wholeResult)), !wholeResult.IsValid());
        for(const FS_SaveManifestEntry& entry : whole.files){
            TArray<uint8> bytes;
            FS_SaveFileHeader header;
            int32 offset = 0;
            if(FFileHelper::LoadFileToArray(bytes, *(slot.paths.current / entry.fileName)) && !FSaveFileIO::validateFile(bytes, header, offset, 1).IsValid()){
                test.TestEqual(FString::Printf(TEXT("%s: %s carries the manifest's counter"), *label, *entry.fileName), header.saveCounter, whole.saveCounter);
            }else{
                test.AddError(FString::Printf(TEXT("%s: %s cannot be read back"), *label, *entry.fileName));
            }
        }
    }

    void sweepCommitFaults(FAutomationTestBase& test, bool keepBackup, EFaultMode mode){
        const FGuid worldId = FGuid::NewGuid();
        const TCHAR* modeName = mode == EFaultMode::Fail ? TEXT("crash") : TEXT("short write");

        // Dry run: no fault, just count the operations of one whole commit.
        int32 totalOps = 0;
        TArray<bool> opIsWrite;
        {
            FTestSlot slot;
            makeOldState(slot, worldId, keepBackup);
            FS_SaveWriteJob job = makeJob(slot, worldId, 2, keepBackup, false);
            FScopedFaultFileSystem scoped(slot.root);
            const FGameplayTag result = FSaveSlotStore::executeWriteJob(job);
            totalOps = scoped.fileSystem.opCount;
            opIsWrite = scoped.fileSystem.opIsWrite;
            expectTag(test, TEXT("the dry-run commit succeeds"), result, FGameplayTag());
        }
        test.TestTrue(FString::Printf(TEXT("the fault file system sees the commit's operations (%d counted; if 0 it is not in the path)"), totalOps), totalOps > 10);
        test.AddInfo(FString::Printf(TEXT("%s, keepBackup %s: a commit has %d counted file operations"), modeName, keepBackup ? TEXT("on") : TEXT("off"), totalOps));

        for(int32 n = 1; n <= totalOps; ++n){
            if(mode == EFaultMode::TruncateWrite && !opIsWrite[n - 1]){
                continue;   // only a Write can be cut short
            }
            const FString label = FString::Printf(TEXT("%s, keepBackup %s, fault at operation %d of %d"), modeName, keepBackup ? TEXT("on") : TEXT("off"), n, totalOps);

            FTestSlot slot;
            makeOldState(slot, worldId, keepBackup);
            FS_SaveWriteJob job = makeJob(slot, worldId, 2, keepBackup, false);
            FGameplayTag commitResult;
            int32 injected = 0;
            {
                FScopedFaultFileSystem scoped(slot.root);
                scoped.fileSystem.mode = mode;
                scoped.fileSystem.failFrom = n;
                commitResult = FSaveSlotStore::executeWriteJob(job);
                injected = scoped.fileSystem.faultsInjected;
            }   // the fault file system is removed here: the process "restarted"
            test.TestTrue(FString::Printf(TEXT("%s: the fault was really injected"), *label), injected > 0);
            checkRecoveredSlot(test, label, slot, keepBackup, mode, commitResult);
        }
    }

}

using namespace AwakenSaveTests;

// =====================================================================================================================
// Test 1. The file envelope
// =====================================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwakenSaveEnvelopeTest, "Awaken.Save.Envelope", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwakenSaveEnvelopeTest::RunTest(const FString& Parameters){
    const FGuid worldId = FGuid::NewGuid();
    const TArray<uint8> payload = makePayload(300);
    const FGameplayTag success;

    // A good file validates, and the header comes back as written.
    FS_SaveFileHeader header = makeGoodHeader(E_SaveScope::World, worldId, 7, payload);
    TArray<uint8> good;
    FSaveFileIO::buildFileBytes(header, payload, good);

    FS_SaveFileHeader parsed;
    int32 offset = 0;
    expectTag(*this, TEXT("a good file validates"), FSaveFileIO::validateFile(good, parsed, offset, 1), success);
    TestEqual(TEXT("saveCounter comes back"), parsed.saveCounter, 7);
    TestTrue(TEXT("worldID comes back"), parsed.worldID == worldId);
    TestEqual(TEXT("payload offset + payload size = file size"), offset + parsed.payloadSize, good.Num());

    // One flipped payload byte -> ChecksumMismatch.
    TArray<uint8> flipped = good;
    flipped[offset + 10] ^= 0xFF;
    expectTag(*this, TEXT("a flipped payload byte"), validateBytes(flipped, 1), T_Save_FailReason_ChecksumMismatch);

    // Truncated file -> CorruptFile (in the payload, in the header, and shorter than the prefix).
    TArray<uint8> truncatedPayload = good;
    truncatedPayload.SetNum(truncatedPayload.Num() - 5);
    expectTag(*this, TEXT("a file cut inside the payload"), validateBytes(truncatedPayload, 1), T_Save_FailReason_CorruptFile);
    TArray<uint8> truncatedHeader = good;
    truncatedHeader.SetNum(20);
    expectTag(*this, TEXT("a file cut inside the header"), validateBytes(truncatedHeader, 1), T_Save_FailReason_CorruptFile);
    TArray<uint8> shorterThanPrefix = good;
    shorterThanPrefix.SetNum(5);
    expectTag(*this, TEXT("a file shorter than the envelope prefix"), validateBytes(shorterThanPrefix, 1), T_Save_FailReason_CorruptFile);

    // Zero-byte file.
    const TArray<uint8> empty;
    expectTag(*this, TEXT("a zero-byte file"), validateBytes(empty, 1), T_Save_FailReason_CorruptFile);

    // Wrong magic.
    TArray<uint8> wrongMagic = good;
    wrongMagic[0] ^= 0xFF;
    expectTag(*this, TEXT("a wrong magic"), validateBytes(wrongMagic, 1), T_Save_FailReason_CorruptFile);

    // Written by a newer game: a newer schema, and a newer envelope.
    FS_SaveFileHeader newerSchema = makeGoodHeader(E_SaveScope::World, worldId, 1, payload);
    newerSchema.schemaVersion = AwakenSave::schemaVersion + 1;
    expectTag(*this, TEXT("a newer schema version"), validateBytes(buildRawFile(AwakenSave::fileMagic, AwakenSave::envelopeVersion, newerSchema, payload, 0, -1), 1), T_Save_FailReason_SchemaTooNew);
    FS_SaveFileHeader normal = makeGoodHeader(E_SaveScope::World, worldId, 1, payload);
    expectTag(*this, TEXT("a newer envelope version"), validateBytes(buildRawFile(AwakenSave::fileMagic, AwakenSave::envelopeVersion + 1, normal, payload, 0, -1), 1), T_Save_FailReason_SchemaTooNew);

    // Older than the oldest schema this build can migrate from.
    expectTag(*this, TEXT("a schema below minSchema"), validateBytes(good, AwakenSave::schemaVersion + 1), T_Save_FailReason_MigrationMissing);

    // Lying headers: no crash, no big allocation, just CorruptFile.
    FS_SaveFileHeader negativeSize = makeGoodHeader(E_SaveScope::World, worldId, 1, payload);
    negativeSize.payloadSize = -1;
    expectTag(*this, TEXT("a negative payload size"), validateBytes(buildRawFile(AwakenSave::fileMagic, AwakenSave::envelopeVersion, negativeSize, payload, 0, -1), 1), T_Save_FailReason_CorruptFile);
    FS_SaveFileHeader tooBigSize = makeGoodHeader(E_SaveScope::World, worldId, 1, payload);
    tooBigSize.payloadSize = payload.Num() + 10;
    expectTag(*this, TEXT("a payload size larger than the file"), validateBytes(buildRawFile(AwakenSave::fileMagic, AwakenSave::envelopeVersion, tooBigSize, payload, 0, -1), 1), T_Save_FailReason_CorruptFile);
    expectTag(*this, TEXT("an oversized header length"), validateBytes(buildRawFile(AwakenSave::fileMagic, AwakenSave::envelopeVersion, normal, payload, 0, 1000000), 1), T_Save_FailReason_CorruptFile);
    expectTag(*this, TEXT("a header length of zero"), validateBytes(buildRawFile(AwakenSave::fileMagic, AwakenSave::envelopeVersion, normal, payload, 0, 0), 1), T_Save_FailReason_CorruptFile);

    // A header with unknown trailing bytes (a later build appended fields) still reads.
    expectTag(*this, TEXT("a header with extra trailing bytes"), validateBytes(buildRawFile(AwakenSave::fileMagic, AwakenSave::envelopeVersion, normal, payload, 16, -1), 1), success);

    return true;
}

// The "huge string length" case of test 1, on its own: depending on the engine, reading a lying FString length may write an Error
// line to the log. If this test fails only because of such a log line, paste the line and we add it as an expected error.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwakenSaveEnvelopeHugeStringTest, "Awaken.Save.EnvelopeHugeString", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwakenSaveEnvelopeHugeStringTest::RunTest(const FString& Parameters){
    // The header bytes are: scope, schemaVersion, then the buildVersion string, whose length says "about 268 million characters".
    TArray<uint8> headerBytes;
    FMemoryWriter headerWriter(headerBytes);
    uint8 scopeByte = 0;
    int32 schemaVersion = AwakenSave::schemaVersion;
    int32 hugeLength = 0x10000000;
    headerWriter << scopeByte << schemaVersion << hugeLength;
    for(int32 index = 0; index < 16; ++index){
        uint8 filler = 0x41;
        headerWriter << filler;
    }

    TArray<uint8> file;
    FMemoryWriter writer(file);
    uint32 magic = AwakenSave::fileMagic;
    int32 envelopeVersion = AwakenSave::envelopeVersion;
    int32 headerLen = headerBytes.Num();
    writer << magic << envelopeVersion << headerLen;
    writer.Serialize(headerBytes.GetData(), headerBytes.Num());

    expectTag(*this, TEXT("a header whose string length is huge"), validateBytes(file, 1), T_Save_FailReason_CorruptFile);
    return true;
}

// =====================================================================================================================
// Test 2. The bag rule
// =====================================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwakenSaveBagRuleTest, "Awaken.Save.BagRule", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwakenSaveBagRuleTest::RunTest(const FString& Parameters){
    const FGuid characterId = FGuid::NewGuid();
    const FGuid heldBag = FGuid::NewGuid();
    const FGuid droppedBag = FGuid::NewGuid();
    const FGuid strangerId = FGuid::NewGuid();
    const FGuid noBag;   // invalid = none

    auto expectDecision = [this](const TCHAR* what, const FS_SaveBagDecision& decision, E_SaveBagOutcome expected, const FGuid& expectedRemove){
        TestTrue(FString::Printf(TEXT("%s: outcome"), what), decision.outcome == expected);
        TestTrue(FString::Printf(TEXT("%s: worldBagToRemove"), what), decision.worldBagToRemove == expectedRemove);
    };

    TArray<FS_BagRecord> worldHasHeld;
    worldHasHeld.Add(makeBag(heldBag, characterId));
    TArray<FS_BagRecord> worldHasOtherOnly;
    worldHasOtherOnly.Add(makeBag(droppedBag, characterId));
    const TArray<FS_BagRecord> worldEmpty;

    // The four rows of the table in Step 10.
    expectDecision(TEXT("row 1: world newer, world holds the file's bag"), FSaveBagReconciler::evaluate(characterId, heldBag, worldHasHeld, true), E_SaveBagOutcome::WorldBagWinsPlayerBagless, noBag);
    expectDecision(TEXT("row 2: world newer, no such bag"), FSaveBagReconciler::evaluate(characterId, heldBag, worldHasOtherOnly, true), E_SaveBagOutcome::NoConflict, noBag);
    expectDecision(TEXT("row 3: file newer or equal, world holds the file's bag"), FSaveBagReconciler::evaluate(characterId, heldBag, worldHasHeld, false), E_SaveBagOutcome::SavedStateWinsRemoveWorldBag, heldBag);
    expectDecision(TEXT("row 4: file newer or equal, no such bag"), FSaveBagReconciler::evaluate(characterId, heldBag, worldHasOtherOnly, false), E_SaveBagOutcome::NoConflict, noBag);

    // The file holds no bag, or the character is unknown, or the world has no bags: nothing can be duplicated.
    expectDecision(TEXT("file holds no bag (world newer)"), FSaveBagReconciler::evaluate(characterId, noBag, worldHasHeld, true), E_SaveBagOutcome::NoConflict, noBag);
    expectDecision(TEXT("file holds no bag (file newer)"), FSaveBagReconciler::evaluate(characterId, noBag, worldHasHeld, false), E_SaveBagOutcome::NoConflict, noBag);
    expectDecision(TEXT("invalid characterId"), FSaveBagReconciler::evaluate(FGuid(), heldBag, worldHasHeld, false), E_SaveBagOutcome::NoConflict, noBag);
    expectDecision(TEXT("empty world"), FSaveBagReconciler::evaluate(characterId, heldBag, worldEmpty, false), E_SaveBagOutcome::NoConflict, noBag);

    // Two world bags of this character, only one equals the file's bag: exactly that one is removed.
    TArray<FS_BagRecord> worldHasTwo;
    worldHasTwo.Add(makeBag(droppedBag, characterId));
    worldHasTwo.Add(makeBag(heldBag, characterId));
    expectDecision(TEXT("two world bags, one matches"), FSaveBagReconciler::evaluate(characterId, heldBag, worldHasTwo, false), E_SaveBagOutcome::SavedStateWinsRemoveWorldBag, heldBag);

    // A bagID is unique: the owner on the world record is not compared.
    TArray<FS_BagRecord> worldBagOtherOwner;
    worldBagOtherOwner.Add(makeBag(heldBag, strangerId));
    expectDecision(TEXT("matching bagID, different owner on the world record"), FSaveBagReconciler::evaluate(characterId, heldBag, worldBagOtherOwner, false), E_SaveBagOutcome::SavedStateWinsRemoveWorldBag, heldBag);

    // A character who dropped an earlier bag and now holds a different one, while the world is newer: keeps the one it holds.
    expectDecision(TEXT("dropped an earlier bag, holds a different one, world newer"), FSaveBagReconciler::evaluate(characterId, heldBag, worldHasOtherOnly, true), E_SaveBagOutcome::NoConflict, noBag);

    // A held state after an intervening save is never older than the world: the caller passes false.
    expectDecision(TEXT("held state (worldNewerThanFile = false)"), FSaveBagReconciler::evaluate(characterId, heldBag, worldHasHeld, false), E_SaveBagOutcome::SavedStateWinsRemoveWorldBag, heldBag);

    return true;
}

// =====================================================================================================================
// Test 3. Both or neither
// =====================================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwakenSaveBothOrNeitherTest, "Awaken.Save.BothOrNeither", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwakenSaveBothOrNeitherTest::RunTest(const FString& Parameters){
    FTestSlot slot;
    const FGuid worldId = FGuid::NewGuid();
    const FGameplayTag success;
    const FString currentWorldFile = slot.paths.current / FSaveSlotStore::worldFileName();

    // Save #1 into an empty slot.
    FS_SaveWriteJob first = makeJob(slot, worldId, 1, true, false);
    expectTag(*this, TEXT("save #1 into an empty slot"), FSaveSlotStore::executeWriteJob(first), success);
    TestTrue(TEXT("after save #1: Current exists"), dirExists(slot.paths.current));
    TestFalse(TEXT("after save #1: no Staging folder is left"), dirExists(slot.stagingFor(1)));

    FS_SaveManifest manifestBefore;
    expectTag(*this, TEXT("after save #1: the manifest reads"), FSaveSlotStore::readManifest(slot.paths.current, manifestBefore), success);
    TestEqual(TEXT("after save #1: manifest saveCounter"), manifestBefore.saveCounter, 1);
    TArray<uint8> worldBytesBefore;
    TestTrue(TEXT("after save #1: World.sav can be read"), FFileHelper::LoadFileToArray(worldBytesBefore, *currentWorldFile));

    // Save #2: the LAST file cannot be written. Neither file of the new set may reach Current.
    FS_SaveWriteJob broken = makeJob(slot, worldId, 2, true, true);
    expectTag(*this, TEXT("save #2 with an unwritable last file"), FSaveSlotStore::executeWriteJob(broken), T_Save_FailReason_WriteFailed);
    TestTrue(TEXT("after the failed save: Current still exists"), dirExists(slot.paths.current));
    TestFalse(TEXT("after the failed save: the Staging folder is gone"), dirExists(slot.stagingFor(2)));
    TestFalse(TEXT("after the failed save: no Retired folder"), dirExists(slot.paths.retired));

    FS_SaveManifest manifestAfter;
    expectTag(*this, TEXT("after the failed save: Current's manifest reads"), FSaveSlotStore::readManifest(slot.paths.current, manifestAfter), success);
    TestEqual(TEXT("after the failed save: Current is still save #1"), manifestAfter.saveCounter, 1);
    TArray<uint8> worldBytesAfter;
    TestTrue(TEXT("after the failed save: World.sav can be read"), FFileHelper::LoadFileToArray(worldBytesAfter, *currentWorldFile));
    TestTrue(TEXT("after the failed save: World.sav is byte for byte the old one"), worldBytesAfter == worldBytesBefore);
    FS_SaveManifest checked;
    expectTag(*this, TEXT("after the failed save: Current still validates as a whole set"), FSaveSlotStore::validateSet(slot.paths.current, 1, checked), success);

    // Save #3 works again, and the old Current became the Backup.
    FS_SaveWriteJob third = makeJob(slot, worldId, 3, true, false);
    expectTag(*this, TEXT("save #3 after the failure"), FSaveSlotStore::executeWriteJob(third), success);
    FS_SaveManifest currentManifest;
    FS_SaveManifest backupManifest;
    FSaveSlotStore::readManifest(slot.paths.current, currentManifest);
    FSaveSlotStore::readManifest(slot.paths.backup, backupManifest);
    TestEqual(TEXT("after save #3: Current is save #3"), currentManifest.saveCounter, 3);
    TestEqual(TEXT("after save #3: Backup is save #1 (the failed #2 never existed)"), backupManifest.saveCounter, 1);
    TestFalse(TEXT("after save #3: no Retired folder"), dirExists(slot.paths.retired));

    return true;
}

// =====================================================================================================================
// Test 4. Recovery (the cases that need no fault injection)
// =====================================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwakenSaveRecoveryTest, "Awaken.Save.Recovery", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwakenSaveRecoveryTest::RunTest(const FString& Parameters){
    const FGuid worldId = FGuid::NewGuid();
    const FGameplayTag success;

    // A. Current damaged, Backup good: the Backup set takes over as a whole, and the damaged set is parked in Refused.
    {
        FTestSlot slot;
        makeSet(slot.paths.current, worldId, 2);
        makeSet(slot.paths.backup, worldId, 1);
        damageFile(slot.paths.current / FSaveSlotStore::worldFileName());
        FS_SaveManifest manifest;
        bool usedBackup = false;
        expectTag(*this, TEXT("A: damaged Current, good Backup"), recover(slot.paths, manifest, usedBackup), success);
        TestTrue(TEXT("A: usedBackup is true"), usedBackup);
        TestEqual(TEXT("A: the loaded set is the Backup (counter 1)"), manifest.saveCounter, 1);
        TestTrue(TEXT("A: Current exists again"), dirExists(slot.paths.current));
        TestTrue(TEXT("A: the damaged set was parked in Refused"), dirExists(slot.paths.refused));
        TestTrue(TEXT("A: the Backup is still there"), dirExists(slot.paths.backup));
        TestFalse(TEXT("A: no Restoring folder is left"), dirExists(slot.root / TEXT("Restoring")));
    }

    // B. A complete Staging set and no Current: it is promoted (the commit was cut right before the rename).
    {
        FTestSlot slot;
        makeSet(slot.stagingFor(2), worldId, 2);
        FS_SaveManifest manifest;
        bool usedBackup = false;
        expectTag(*this, TEXT("B: complete Staging, no Current"), recover(slot.paths, manifest, usedBackup), success);
        TestEqual(TEXT("B: the staged set (counter 2) is now Current"), manifest.saveCounter, 2);
        TestTrue(TEXT("B: Current exists"), dirExists(slot.paths.current));
        TestFalse(TEXT("B: the Staging folder is gone"), dirExists(slot.stagingFor(2)));
        TestFalse(TEXT("B: usedBackup is false"), usedBackup);
    }

    // B2. A complete Staging set next to a whole Current: Current wins, the staged set was never committed.
    {
        FTestSlot slot;
        makeSet(slot.paths.current, worldId, 1);
        makeSet(slot.stagingFor(2), worldId, 2);
        FS_SaveManifest manifest;
        bool usedBackup = false;
        expectTag(*this, TEXT("B2: whole Current and a staged set"), recover(slot.paths, manifest, usedBackup), success);
        TestEqual(TEXT("B2: Current (counter 1) wins"), manifest.saveCounter, 1);
        TestFalse(TEXT("B2: the Staging folder is deleted"), dirExists(slot.stagingFor(2)));
    }

    // C. Only a valid Retired set (the commit was cut after Current was set aside): it is put back.
    {
        FTestSlot slot;
        makeSet(slot.paths.retired, worldId, 3);
        FS_SaveManifest manifest;
        bool usedBackup = false;
        expectTag(*this, TEXT("C: only a valid Retired set"), recover(slot.paths, manifest, usedBackup), success);
        TestEqual(TEXT("C: the Retired set (counter 3) is Current again"), manifest.saveCounter, 3);
        TestTrue(TEXT("C: Current exists"), dirExists(slot.paths.current));
        TestFalse(TEXT("C: the Retired folder is gone"), dirExists(slot.paths.retired));
    }

    // D. Current and Backup both damaged: SlotUnreadable, and the Refused set survives a second recovery.
    {
        FTestSlot slot;
        makeSet(slot.paths.current, worldId, 2);
        makeSet(slot.paths.backup, worldId, 1);
        damageFile(slot.paths.current / FSaveSlotStore::worldFileName());
        damageFile(slot.paths.backup / FSaveSlotStore::worldFileName());
        FS_SaveManifest manifest;
        bool usedBackup = false;
        expectTag(*this, TEXT("D: everything damaged"), recover(slot.paths, manifest, usedBackup), T_Save_FailReason_SlotUnreadable);
        TestTrue(TEXT("D: the damaged Current is kept in Refused"), dirExists(slot.paths.refused));
        TestTrue(TEXT("D: the damaged Backup is not deleted"), dirExists(slot.paths.backup));
        expectTag(*this, TEXT("D: a second recovery gives the same answer"), recover(slot.paths, manifest, usedBackup), T_Save_FailReason_SlotUnreadable);
        TestTrue(TEXT("D: the Refused set survives the second recovery"), dirExists(slot.paths.refused));
    }

    // E. Current fails ONLY on version (a newer game wrote it): not parked, and the older Backup is not loaded in its place.
    {
        FTestSlot slot;
        makeSet(slot.paths.current, worldId, 2, AwakenSave::schemaVersion + 1);
        makeSet(slot.paths.backup, worldId, 1);
        FS_SaveManifest manifest;
        bool usedBackup = false;
        expectTag(*this, TEXT("E: Current written by a newer game"), recover(slot.paths, manifest, usedBackup), T_Save_FailReason_SchemaTooNew);
        TestTrue(TEXT("E: Current is still there"), dirExists(slot.paths.current));
        TestFalse(TEXT("E: nothing was parked in Refused"), dirExists(slot.paths.refused));
        TestFalse(TEXT("E: the Backup was not loaded in its place"), usedBackup);
    }

    // F. A leftover Restoring folder is deleted and never promoted.
    {
        FTestSlot slot;
        makeSet(slot.paths.current, worldId, 1);
        makeSet(slot.root / TEXT("Restoring"), worldId, 9);
        FS_SaveManifest manifest;
        bool usedBackup = false;
        expectTag(*this, TEXT("F: whole Current and a leftover Restoring folder"), recover(slot.paths, manifest, usedBackup), success);
        TestEqual(TEXT("F: Current (counter 1) is what loads"), manifest.saveCounter, 1);
        TestFalse(TEXT("F: the Restoring folder is deleted"), dirExists(slot.root / TEXT("Restoring")));
    }
    {
        FTestSlot slot;
        makeSet(slot.root / TEXT("Restoring"), worldId, 9);
        FS_SaveManifest manifest;
        bool usedBackup = false;
        expectTag(*this, TEXT("F2: only a leftover Restoring folder"), recover(slot.paths, manifest, usedBackup), T_Save_FailReason_SlotUnreadable);
        TestFalse(TEXT("F2: Restoring was not promoted to Current"), dirExists(slot.paths.current));
    }

    return true;
}

// =====================================================================================================================
// Test 9. Cut a commit at every file operation
// =====================================================================================================================
//
// For every counted file operation of one commit (save #2 over save #1): the commit runs with a fault from that operation on, the
// fault file system is removed (the process "restarted"), recoverSlot runs, and the slot must be exactly save #1 or exactly save #2:
// never a mix, never empty. Run with keepBackupSet on and off. A second sweep cuts each Write short instead (a silent partial write):
// the read-back must catch it and the old set must stay.
// This test takes a minute or two: failed renames wait 50 ms between tries, as the real code does.

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwakenSaveCommitFaultTest, "Awaken.Save.CommitFaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwakenSaveCommitFaultTest::RunTest(const FString& Parameters){
    sweepCommitFaults(*this, true, EFaultMode::Fail);
    sweepCommitFaults(*this, false, EFaultMode::Fail);
    sweepCommitFaults(*this, true, EFaultMode::TruncateWrite);
    sweepCommitFaults(*this, false, EFaultMode::TruncateWrite);
    return true;
}

// =====================================================================================================================
// Test 24 (and the read-error case of test 4). I/O faults in carry-over, rename and recovery
// =====================================================================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwakenSaveIoFaultTest, "Awaken.Save.IoFaults", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwakenSaveIoFaultTest::RunTest(const FString& Parameters){
    const FGuid worldId = FGuid::NewGuid();
    const FGameplayTag success;

    // A. An IoError while a Character file is carried over: the save fails, nothing on disk changes, and the file is neither
    //    judged damaged nor skipped.
    {
        FTestSlot slot;
        TArray<FString> identities;
        identities.Add(TEXT("PlayerA"));
        identities.Add(TEXT("PlayerB"));
        makeSetOf(slot.paths.current, worldId, 1, identities);

        FS_SaveManifest currentManifest;
        expectTag(*this, TEXT("A: the old manifest reads"), FSaveSlotStore::readManifest(slot.paths.current, currentManifest), success);
        const FString fileB = FSaveSlotStore::characterFileName(TEXT("PlayerB"));
        FS_SaveManifestEntry entryB;
        bool foundB = false;
        for(const FS_SaveManifestEntry& entry : currentManifest.files){
            if(entry.fileName == fileB){
                entryB = entry;
                foundB = true;
            }
        }
        TestTrue(TEXT("A: the old manifest lists PlayerB's file"), foundB);

        FS_SaveWriteJob job = makeJob(slot, worldId, 2, true, false);   // writes World + PlayerA
        job.manifest.files.Add(entryB);
        job.filesToCarryOver.Add(entryB);                               // PlayerB is carried over from Current
        FGameplayTag result;
        int32 injected = 0;
        {
            FScopedFaultFileSystem scoped(slot.root);
            scoped.fileSystem.failOpenReadContaining = fileB;           // PlayerB's file cannot be read: a lock
            result = FSaveSlotStore::executeWriteJob(job);
            injected = scoped.fileSystem.faultsInjected;
        }
        TestTrue(TEXT("A: the read fault was injected"), injected > 0);
        expectTag(*this, TEXT("A: a carry-over IoError fails the save"), result, T_Save_FailReason_IoError);
        TestEqual(TEXT("A: the identity was not skipped"), job.skippedIdentities.Num(), 0);
        TestFalse(TEXT("A: the file was not judged damaged (no refusedCharacters folder)"), dirExists(slot.paths.refusedCharacters));
        TestFalse(TEXT("A: no Staging folder is left"), dirExists(slot.stagingFor(2)));
        TestTrue(TEXT("A: PlayerB's file is still in Current"), IFileManager::Get().FileExists(*(slot.paths.current / fileB)));
        FS_SaveManifest after;
        FSaveSlotStore::readManifest(slot.paths.current, after);
        TestEqual(TEXT("A: Current is still save #1"), after.saveCounter, 1);
    }

    // B. A folder rename that fails twice and then works is retried: the commit succeeds.
    {
        FTestSlot slot;
        makeSet(slot.paths.current, worldId, 1);
        FS_SaveWriteJob job = makeJob(slot, worldId, 2, true, false);
        FGameplayTag result;
        int32 injected = 0;
        {
            FScopedFaultFileSystem scoped(slot.root);
            scoped.fileSystem.failMoveTimes = 2;
            result = FSaveSlotStore::executeWriteJob(job);
            injected = scoped.fileSystem.faultsInjected;
        }
        TestTrue(TEXT("B: the rename fault was injected"), injected > 0);
        expectTag(*this, TEXT("B: a rename that fails twice is retried and the commit succeeds"), result, success);
        FS_SaveManifest currentManifest;
        FS_SaveManifest backupManifest;
        FSaveSlotStore::readManifest(slot.paths.current, currentManifest);
        FSaveSlotStore::readManifest(slot.paths.backup, backupManifest);
        TestEqual(TEXT("B: Current is save #2"), currentManifest.saveCounter, 2);
        TestEqual(TEXT("B: Backup is save #1"), backupManifest.saveCounter, 1);
    }

    // C. A rename lock that never lifts: the commit fails and the old set stays, intact.
    {
        FTestSlot slot;
        makeSet(slot.paths.current, worldId, 1);
        FS_SaveWriteJob job = makeJob(slot, worldId, 2, true, false);
        FGameplayTag result;
        {
            FScopedFaultFileSystem scoped(slot.root);
            scoped.fileSystem.failMoveTimes = -1;
            result = FSaveSlotStore::executeWriteJob(job);
        }
        expectTag(*this, TEXT("C: a rename lock that never lifts fails the save"), result, T_Save_FailReason_WriteFailed);
        TestTrue(TEXT("C: Current still exists"), dirExists(slot.paths.current));
        TestFalse(TEXT("C: no Retired folder"), dirExists(slot.paths.retired));
        TestFalse(TEXT("C: no Staging folder is left"), dirExists(slot.stagingFor(2)));
        FS_SaveManifest checked;
        expectTag(*this, TEXT("C: Current still validates as a whole set"), FSaveSlotStore::validateSet(slot.paths.current, 1, checked), success);
        TestEqual(TEXT("C: Current is still save #1"), checked.saveCounter, 1);
    }

    // D. Test 4: a read error on a good Current is not corruption. Nothing is parked, nothing is loaded from the Backup.
    {
        FTestSlot slot;
        makeSet(slot.paths.current, worldId, 2);
        makeSet(slot.paths.backup, worldId, 1);
        FS_SaveManifest manifest;
        bool usedBackup = false;
        FGameplayTag result;
        {
            FScopedFaultFileSystem scoped(slot.root);
            scoped.fileSystem.failOpenReadContaining = FSaveSlotStore::worldFileName();
            result = recover(slot.paths, manifest, usedBackup);
        }
        expectTag(*this, TEXT("D: a read error on Current's World file"), result, T_Save_FailReason_IoError);
        TestTrue(TEXT("D: Current is still there"), dirExists(slot.paths.current));
        TestFalse(TEXT("D: nothing was parked in Refused"), dirExists(slot.paths.refused));
        TestFalse(TEXT("D: the Backup was not loaded in its place"), usedBackup);
    }

    return true;
}

// =====================================================================================================================
// Open item 19. Slot names
// =====================================================================================================================

// Open item 19. FPaths::MakeValidFileName leaves ".", ".." and names that end in a dot or a space unchanged (the first run of this test
// showed it), so Save has its own rule: FSaveSlotStore::isUsableSlotName, used by deleteSlot and by the subsystem's slot functions.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwakenSaveSlotNameTest, "Awaken.Save.SlotNameSafety", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAwakenSaveSlotNameTest::RunTest(const FString& Parameters){
    const TCHAR* refused[] = {TEXT(""), TEXT("."), TEXT(".."), TEXT("World."), TEXT("World "), TEXT("a/b"), TEXT("..\\x"), TEXT("a:b"), TEXT("CON"), TEXT("nul"), TEXT("Com1"), TEXT("con.txt")};
    for(const TCHAR* name : refused){
        TestFalse(FString::Printf(TEXT("'%s' is refused as a slot name"), name), FSaveSlotStore::isUsableSlotName(name));
    }

    const TCHAR* allowed[] = {TEXT("My World"), TEXT("Slot1"), TEXT("Save_2026-10-18"), TEXT("Console"), TEXT("A.B")};
    for(const TCHAR* name : allowed){
        TestTrue(FString::Printf(TEXT("'%s' is a usable slot name"), name), FSaveSlotStore::isUsableSlotName(name));
    }

    TestFalse(TEXT("a 65-character name is refused (PROPOSED limit 64)"), FSaveSlotStore::isUsableSlotName(FString::ChrN(65, TEXT('a'))));
    TestTrue(TEXT("a 64-character name is allowed"), FSaveSlotStore::isUsableSlotName(FString::ChrN(64, TEXT('a'))));
    return true;
}

#endif   // WITH_DEV_AUTOMATION_TESTS