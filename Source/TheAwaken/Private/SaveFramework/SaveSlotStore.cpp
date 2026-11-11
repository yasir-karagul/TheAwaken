#include "SaveFramework/SaveSlotStore.h"
#include "SaveFramework/SaveFileIO.h"
#include "SaveFramework/SaveSubsystem.h"          // LogAwakenSave (Step 8)
#include "SaveFramework/AwakenSaveSettings.h"
#include "SaveFramework/02Tags/T_SaveFramework.h"
#include "JsonObjectConverter.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace{
    /** Write, then flush to disk before the handle closes. Verify on 5.8: IFileHandle::Flush(true) is a full flush. */
    bool writeBytesDurably(const FString& path, const uint8* data, int64 num){
        IPlatformFile& platformFile = FPlatformFileManager::Get().GetPlatformFile();
        TUniquePtr<IFileHandle> handle(platformFile.OpenWrite(*path, /*bAppend*/ false, /*bAllowRead*/ false));
        return handle && handle->Write(data, num) && handle->Flush(true);
    }

    /** A lock (antivirus, indexer) fails a read for a moment. Try again before calling it a failure.
     *  At most 3 tries, 50 ms apart: a load on the game thread waits about 0.15 s for one locked file. */
    bool loadWithRetry(const FString& path, TArray<uint8>& bytes){
        for(int32 attempt = 0; attempt < 3; ++attempt){
            if(FFileHelper::LoadFileToArray(bytes, *path)){
                return true;
            }
            // really missing
            if(!IFileManager::Get().FileExists(*path)){
                return false;
            }
            FPlatformProcess::Sleep(0.05f);
        }
        return false;
    }

    /** Folder renames can fail for a moment under a lock on Windows. IFileManager::Move takes (Dest, Src). */
    bool moveDir(const FString& dest, const FString& src){
        for(int32 attempt = 0; attempt < 5; ++attempt){
            if(IFileManager::Get().Move(*dest, *src, true, true, false, /*bDoNotRetryOrError*/ true)){
                return true;
            }
            FPlatformProcess::Sleep(0.05f);
        }
        return false;
    }

    /** One manifest entry against its file. Invalid tag = the file is exactly what the manifest says. */
    FGameplayTag validateEntry(const FString& dir, const FS_SaveManifestEntry& entry, int32 minSchema, FS_SaveFileHeader& header, TArray<uint8>* bytes = nullptr, bool* versionOnly = nullptr){
        const FString path = dir / entry.fileName;
        TArray<uint8> local;
        TArray<uint8>& loadedBytes = bytes ? *bytes : local;
        if(!loadWithRetry(path, loadedBytes)){
            return IFileManager::Get().FileExists(*path) ? T_Save_FailReason_IoError : T_Save_FailReason_FileMissing;
        }

        int32 offset = 0;
        const FGameplayTag reason = FSaveFileIO::validateFile(loadedBytes, header, offset, minSchema);
        if(versionOnly && FSaveSlotStore::isVersionReason(reason)){
            // Written by another version of the game: not damaged. The caller keeps the file as it is, if it has the size the manifest says.
            *versionOnly = true;
            return loadedBytes.Num() == entry.size ? FGameplayTag() : T_Save_FailReason_CorruptFile;
        }
        if(reason.IsValid()){
            return reason;
        }
        if(loadedBytes.Num() != entry.size || static_cast<int64>(header.payloadCrc) != entry.payloadCrc || static_cast<int32>(header.scope) != entry.scope){
            return T_Save_FailReason_ManifestInvalid;   // not the file the manifest describes
        }
        return FGameplayTag();
    }

    /** Staging_<n> folders, newest first. */
    TArray<FString> findStagingDirs(const FS_SaveSlotPaths& p){
        TArray<FString> names;
        IFileManager::Get().FindFiles(names, *(p.root / TEXT("Staging_*")), false, true);
        auto newestFirst = [](const FString& a, const FString& b){
            return FCString::Atoi(*a.RightChop(8)) > FCString::Atoi(*b.RightChop(8));
        };
        names.Sort(newestFirst);   // "Staging_" = 8 characters
        for(FString& name : names){
            name = p.root / name;
        }
        return names;
    }

    /** Last step of a commit: Retired becomes Backup (the old Backup goes first), or is deleted. Current is already in place here,
     *  so a failure only leaves a folder for the next start to settle. */
    void settleRetired(const FS_SaveSlotPaths& p, bool keepBackup){
        IFileManager& fileManager = IFileManager::Get();
        if(!fileManager.DirectoryExists(*p.retired)){
            return;
        }
        if(keepBackup){
            fileManager.DeleteDirectory(*p.backup, false, true);
            if(!moveDir(p.backup, p.retired)){
                UE_LOG(LogAwakenSave, Warning, TEXT("Could not rotate Retired into Backup; it stays for the next start."));
            }
        }else{
            fileManager.DeleteDirectory(*p.retired, false, true);
        }
    }

    /** Current -> Retired, Staging -> Current (THE commit), then settle. Nothing is deleted before the new set is in place. */
    bool commitStage(const FS_SaveSlotPaths& p, bool keepBackup){
        IFileManager& fileManager = IFileManager::Get();
        const bool hadCurrent = fileManager.DirectoryExists(*p.current);

        if(hadCurrent){
            fileManager.DeleteDirectory(*p.retired, false, true);   // leftover of an earlier, interrupted commit
            if(!moveDir(p.retired, p.current)){
                return false;
            }
        }
        if(!moveDir(p.current, p.staging)){
            // roll back
            if(hadCurrent){
                moveDir(p.current, p.retired);
            }
            return false;
        }
        if(hadCurrent){
            settleRetired(p, keepBackup);
        }
        return true;
    }

    /** The folder that holds every slot: <Saved>/SaveGames/<RootFolder>. Reads the settings: game thread only. */
    FString slotsRootFolder(){
        return FPaths::ProjectSavedDir() / TEXT("SaveGames") / UAwakenSaveSettings::get()->saveRootFolder;
    }
}

FS_SaveSlotPaths FSaveSlotStore::makePaths(const FString& slotName, int32 counter){
    check(IsInGameThread());   // it reads the settings
    FS_SaveSlotPaths p;
    p.root = slotsRootFolder() / FPaths::MakeValidFileName(slotName);
    p.current = p.root / TEXT("Current");
    p.backup = p.root / TEXT("Backup");
    p.retired = p.root / TEXT("Retired");
    p.refused = p.root / TEXT("Refused");
    p.refusedCharacters = p.root / TEXT("RefusedCharacters");
    p.staging = p.root / FString::Printf(TEXT("Staging_%d"), counter);
    return p;
}

FGameplayTag FSaveSlotStore::writeManifest(const FString& dir, const FS_SaveManifest& manifest){
    FString json;
    if(!FJsonObjectConverter::UStructToJsonObjectString(manifest, json)){
        return T_Save_FailReason_ManifestInvalid;
    }
    const FTCHARToUTF8 utf8(*json);
    return writeBytesDurably(dir / manifestFileName(), reinterpret_cast<const uint8*>(utf8.Get()), utf8.Length())
        ? FGameplayTag() : T_Save_FailReason_WriteFailed;
}

FGameplayTag FSaveSlotStore::readManifest(const FString& dir, FS_SaveManifest& manifest){
    const FString path = dir / manifestFileName();
    TArray<uint8> bytes;
    if(!loadWithRetry(path, bytes)){
        return IFileManager::Get().FileExists(*path) ? T_Save_FailReason_IoError : T_Save_FailReason_ManifestInvalid;
    }

    FString json;
    FFileHelper::BufferToString(json, bytes.GetData(), bytes.Num());
    return FJsonObjectConverter::JsonObjectStringToUStruct(json, &manifest, 0, 0) ? FGameplayTag() : T_Save_FailReason_ManifestInvalid;
}

FGameplayTag FSaveSlotStore::validateSet(const FString& dir, int32 minSchema, FS_SaveManifest& manifest, TArray<FS_SaveManifestEntry>* badCharacterFiles){
    manifest = FS_SaveManifest();
    if(badCharacterFiles){
        badCharacterFiles->Reset();
    }

    const FGameplayTag manifestReason = readManifest(dir, manifest);
    if(manifestReason.IsValid()){
        return manifestReason;
    }
    if(manifest.files.IsEmpty()){
        return T_Save_FailReason_ManifestInvalid;
    }

    bool hasWorld = false;
    for(const FS_SaveManifestEntry& entry : manifest.files){
        FS_SaveFileHeader header;
        bool versionOnly = false;   // a Character file of another version passes; a World file of another version returns its tag
        const FGameplayTag reason = validateEntry(dir, entry, minSchema, header, nullptr,
            entry.scope == static_cast<int32>(E_SaveScope::World) ? nullptr : &versionOnly);
        const bool world = entry.scope == static_cast<int32>(E_SaveScope::World);

        // never judge a file by a failed read
        if(reason == T_Save_FailReason_IoError){
            return reason;
        }
        if(reason.IsValid()){
            // the World file is the slot: a bad one refuses the set
            if(world || !badCharacterFiles){
                return reason;
            }
            badCharacterFiles->Add(entry);   // a Character file is set aside alone
            continue;
        }
        hasWorld |= world;
    }
    return hasWorld ? FGameplayTag() : T_Save_FailReason_ManifestInvalid;
}

FGameplayTag FSaveSlotStore::recoverSlot(const FS_SaveSlotPaths& p, int32 minSchema, bool keepBackup, FS_SaveManifest& manifest,
                                         TArray<FS_SaveManifestEntry>& bad, bool& usedBackup){
    check(IsInGameThread());
    IFileManager& fileManager = IFileManager::Get();
    usedBackup = false;
    manifest = FS_SaveManifest();
    bad.Reset();

    const TArray<FString> staged = findStagingDirs(p);
    fileManager.DeleteDirectory(*(p.root / TEXT("Restoring")), false, true);   // a leftover fallback copy is never a state, and its name does not match Staging_*

    // 1. Current is whole: it wins. Everything else on disk is the leftover of an interrupted save.
    if(fileManager.DirectoryExists(*p.current)){
        const FGameplayTag reason = validateSet(p.current, minSchema, manifest, &bad);
        if(!reason.IsValid()){
            // never committed (Doc R13 3.11)
            for(const FString& dir : staged){
                fileManager.DeleteDirectory(*dir, false, true);
            }
            settleRetired(p, keepBackup);
            return FGameplayTag();
        }
        // a lock is not corruption, and another version's file is not damage: park nothing, fall back to nothing
        if(reason == T_Save_FailReason_IoError || isVersionReason(reason)){
            return reason;
        }

        // Current was read fine and failed its check: set it aside as a whole. One refused set per slot (Doc R13 3.11).
        fileManager.DeleteDirectory(*p.refused, false, true);
        // nothing is deleted when the move fails
        if(!moveDir(p.refused, p.current)){
            return T_Save_FailReason_SlotUnreadable;
        }
    }

    // 2. Roll forward. A Staging set that validates was fully written and read back before its rename: as good as committed.
    for(const FString& dir : staged){
        const FGameplayTag reason = validateSet(dir, minSchema, manifest, &bad);
        // nothing deleted, nothing promoted
        if(reason == T_Save_FailReason_IoError || isVersionReason(reason)){
            return reason;
        }
        if(!reason.IsValid()){
            if(moveDir(p.current, dir)){
                for(const FString& other : findStagingDirs(p)){
                    fileManager.DeleteDirectory(*other, false, true);
                }
                settleRetired(p, keepBackup);
                return FGameplayTag();
            }
            return T_Save_FailReason_IoError;   // valid but could not be promoted: a lock, not corruption. Nothing is deleted.
        }
    }
    // partial, never committed
    for(const FString& dir : staged){
        fileManager.DeleteDirectory(*dir, false, true);
    }

    // 3. A commit that was cut right after Current was set aside: put it back.
    if(fileManager.DirectoryExists(*p.retired)){
        const FGameplayTag reason = validateSet(p.retired, minSchema, manifest, &bad);
        if(reason == T_Save_FailReason_IoError || isVersionReason(reason)){
            return reason;
        }
        if(!reason.IsValid() && moveDir(p.current, p.retired)){
            return FGameplayTag();
        }
    }

    // 4. The backup set AS A WHOLE, through a staging copy so a crash here never leaves a half-written Current.
    if(fileManager.DirectoryExists(*p.backup)){
        const FGameplayTag reason = validateSet(p.backup, minSchema, manifest, &bad);
        if(reason == T_Save_FailReason_IoError || isVersionReason(reason)){
            return reason;
        }
        if(!reason.IsValid()){
            const FString copy = p.root / TEXT("Restoring");
            fileManager.DeleteDirectory(*copy, false, true);
            if(FPlatformFileManager::Get().GetPlatformFile().CopyDirectoryTree(*copy, *p.backup, true)
                && !validateSet(copy, minSchema, manifest, &bad).IsValid()
                && moveDir(p.current, copy)){
                usedBackup = true;   // B16 shows the one-time notice on the loading cover (Doc R13 3.11)
                return FGameplayTag();
            }
            fileManager.DeleteDirectory(*copy, false, true);
        }
    }
    return T_Save_FailReason_SlotUnreadable;   // B06 offers: try again, delete this slot, start a new game
}

FGameplayTag FSaveSlotStore::executeWriteJob(FS_SaveWriteJob& job){
    IFileManager& fileManager = IFileManager::Get();
    const FS_SaveSlotPaths& p = job.paths;
    auto fail = [&](const FGameplayTag& reason){
        fileManager.DeleteDirectory(*p.staging, false, true);   // the previous files stay (Doc R13 3.11)
        return reason;
    };

    fileManager.DeleteDirectory(*p.staging, false, true);
    if(!fileManager.MakeDirectory(*p.staging, true)){
        return fail(T_Save_FailReason_WriteFailed);
    }

    for(const FS_SaveWriteJob::FS_OutFile& file : job.filesToWrite){
        if(!writeBytesDurably(p.staging / file.fileName, file.bytes.GetData(), file.bytes.Num())){
            return fail(T_Save_FailReason_WriteFailed);
        }
    }

    // Carried-over files: each one is checked before it enters the new set. A bad one is set aside alone and left out of the set.
    for(const FS_SaveManifestEntry& entry : job.filesToCarryOver){
        FS_SaveFileHeader header;
        TArray<uint8> bytes;
        bool versionOnly = false;   // another game version wrote it: carried as opaque bytes, never judged
        const FGameplayTag reason = validateEntry(p.current, entry, job.minSchema, header, &bytes, &versionOnly);
        if(reason == T_Save_FailReason_IoError){
            return fail(reason);
        }
        if(reason.IsValid()){
            job.skippedIdentities.Add(entry.playerIdentity);
            auto sameFile = [&entry](const FS_SaveManifestEntry& e){
                return e.fileName == entry.fileName;
            };
            job.manifest.files.RemoveAll(sameFile);
            fileManager.MakeDirectory(*p.refusedCharacters, true);
            fileManager.Copy(*(p.refusedCharacters / entry.fileName), *(p.current / entry.fileName));   // best effort; the original stays in Current and Backup
            continue;
        }
        if(!writeBytesDurably(p.staging / entry.fileName, bytes.GetData(), bytes.Num())){
            return fail(T_Save_FailReason_WriteFailed);
        }
    }

    // LAST
    if(writeManifest(p.staging, job.manifest).IsValid()){
        return fail(T_Save_FailReason_WriteFailed);
    }

    FS_SaveManifest readBack;
    // catches a truncated write
    if(validateSet(p.staging, job.minSchema, readBack).IsValid()){
        return fail(T_Save_FailReason_WriteFailed);
    }

    if(commitStage(p, job.keepBackup)){
        return FGameplayTag();
    }
    // rolled back cleanly
    if(fileManager.DirectoryExists(*p.current)){
        return fail(T_Save_FailReason_WriteFailed);
    }
    return T_Save_FailReason_WriteFailed;   // Current is gone: leave Staging and Retired exactly as they are; recoverSlot rolls forward or back
}

bool FSaveSlotStore::isVersionReason(const FGameplayTag& reason){
    return reason == T_Save_FailReason_SchemaTooNew || reason == T_Save_FailReason_MigrationMissing;
}

bool FSaveSlotStore::slotHasFiles(const FString& slotName){
    const FS_SaveSlotPaths p = makePaths(slotName, 0);
    IFileManager& fileManager = IFileManager::Get();
    return fileManager.DirectoryExists(*p.current) || fileManager.DirectoryExists(*p.backup) || fileManager.DirectoryExists(*p.retired)
        || fileManager.DirectoryExists(*p.refused) || !findStagingDirs(p).IsEmpty();
}

void FSaveSlotStore::listSlotNames(TArray<FString>& slotNames){
    check(IsInGameThread());   // it reads the settings
    slotNames.Reset();
    IFileManager::Get().FindFiles(slotNames, *(slotsRootFolder() / TEXT("*")), /*Files*/ false, /*Directories*/ true);
    slotNames.Sort();
}

void FSaveSlotStore::deleteSlot(const FString& slotName){
    // A name like "", "." or ".." would resolve to the root folder of ALL slots, or to its parent. Refuse it: beginNewWorld never lets such a name in.
    if(!isUsableSlotName(slotName)){
        return;
    }
    const FS_SaveSlotPaths p = makePaths(slotName, 0);
    IFileManager::Get().DeleteDirectory(*p.root, false, true);
}

bool FSaveSlotStore::isUsableSlotName(const FString& slotName){
    // "." and ".." survive FPaths::MakeValidFileName (confirmed by the test Awaken.Save.SlotNameSafety on 5.8): they would name the
    // slots folder itself or its parent. A trailing dot or space is dropped by Windows, so "World." and "World" would be the same folder.
    if(slotName.IsEmpty() || slotName == TEXT(".") || slotName == TEXT("..") || slotName.EndsWith(TEXT(".")) || slotName.EndsWith(TEXT(" "))){
        return false;
    }
    // PROPOSED limit: a slot folder plus its files must stay under the Windows path limit.
    if(slotName.Len() > 64){
        return false;
    }
    // Windows reserved device names cannot be folders ("CON", "NUL", "COM1", "con.txt" ...).
    int32 dotIndex = INDEX_NONE;
    slotName.FindChar(TEXT('.'), dotIndex);
    const FString stem = (dotIndex == INDEX_NONE ? slotName : slotName.Left(dotIndex)).TrimEnd().ToUpper();
    static const TCHAR* reservedNames[] = {TEXT("CON"), TEXT("PRN"), TEXT("AUX"), TEXT("NUL"),
        TEXT("COM1"), TEXT("COM2"), TEXT("COM3"), TEXT("COM4"), TEXT("COM5"), TEXT("COM6"), TEXT("COM7"), TEXT("COM8"), TEXT("COM9"),
        TEXT("LPT1"), TEXT("LPT2"), TEXT("LPT3"), TEXT("LPT4"), TEXT("LPT5"), TEXT("LPT6"), TEXT("LPT7"), TEXT("LPT8"), TEXT("LPT9")};
    for(const TCHAR* reserved : reservedNames){
        if(stem == reserved){
            return false;
        }
    }
    // The name in the list must be the name that was typed.
    return FPaths::MakeValidFileName(slotName) == slotName;
}

FString FSaveSlotStore::characterFileName(const FString& playerIdentity){
    const FTCHARToUTF8 utf8(*playerIdentity);
    return TEXT("Char_") + BytesToHex(reinterpret_cast<const uint8*>(utf8.Get()), utf8.Length()) + TEXT(".sav");
}