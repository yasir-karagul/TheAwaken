#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SaveFramework/01Structs/S_SaveFramework.h"

class THEAWAKEN_API FSaveSlotStore{
public:
    // ---- Game thread only: these read the settings ----
    static FS_SaveSlotPaths makePaths(const FString& slotName, int32 counter);
    static bool slotHasFiles(const FString& slotName);   // any of Current, Backup, Retired, Refused or a Staging_*
    static void listSlotNames(TArray<FString>& slotNames);
    static void deleteSlot(const FString& slotName);
    /** The one rule for a slot name: not empty, not "." or "..", no trailing dot or space, at most 64 characters, not a Windows
     *  reserved device name, and unchanged by FPaths::MakeValidFileName. Used by deleteSlot and by the subsystem's slot functions. */
    static bool isUsableSlotName(const FString& slotName);
    /** SchemaTooNew or MigrationMissing: the file was written by another version of the game. It is not damaged. */
    static bool isVersionReason(const FGameplayTag& reason);
    static FString worldFileName(){
        return TEXT("World.sav");
    }
    static FString manifestFileName(){
        return TEXT("Manifest.json");
    }
    static FString characterFileName(const FString& playerIdentity);   // Char_<hex>.sav: any character is safe in a file name

    /** At load. Decides from what is on disk (the table below). Current and Backup are never deleted by it. */
    static FGameplayTag recoverSlot(const FS_SaveSlotPaths& paths, int32 minSchema, bool keepBackup, FS_SaveManifest& manifest,
                                    TArray<FS_SaveManifestEntry>& badCharacterFiles, bool& usedBackup);

    // ---- From here on: paths and primitives only. No UObject, no settings. Safe on any thread. ----
    static FGameplayTag writeManifest(const FString& dir, const FS_SaveManifest& manifest);
    static FGameplayTag readManifest(const FString& dir, FS_SaveManifest& manifest);

    /** Full check of a set. A bad World file or manifest refuses the set. With badCharacterFiles (load), a bad Character
     *  file is listed and skipped instead; without it (the read-back of a staged set) any bad file refuses the set.
     *  A file that exists but cannot be read gives IoError: a lock is never judged as corruption.
     *  A World file or manifest that fails only on version returns that tag, and the caller parks nothing. A Character file that fails
     *  only on version passes (when its size is the manifest's): it is kept in the set as opaque bytes. */
    static FGameplayTag validateSet(const FString& dir, int32 minSchema, FS_SaveManifest& manifest,
                                    TArray<FS_SaveManifestEntry>* badCharacterFiles = nullptr);

    /** Worker thread. Write, carry over, manifest LAST, read back, commit. Fills Job.skippedIdentities. */
    static FGameplayTag executeWriteJob(FS_SaveWriteJob& job);
};