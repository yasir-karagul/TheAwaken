#include "SaveFramework/02Tags/T_SaveFramework.h"

UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_Started,   "Save.Started",   "A save capture began. Delivery: server, then local to the requesting player (Doc02 19.2)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_Completed, "Save.Completed", "The set was committed");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_Failed,    "Save.Failed",    "The save was refused or the write failed; see FailReasonTag");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_WorldOlderThanCharacter, "Save.WorldOlderThanCharacter", "Raised once at join when the world is older than the character");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_CharacterFileRefused, "Save.CharacterFileRefused", "Host only: a Character file failed its check and was set aside; the rest of the set is fine");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_Save, "Interaction.Save", "Marks the Save Point's hold-to-activate interaction");

UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_NotAuthority,   "Save.FailReason.NotAuthority",   "A server-only call was made on a client");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_NoActiveSlot,   "Save.FailReason.NoActiveSlot",   "No slot is active (B06 has not selected one)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_RestoreNotFinished, "Save.FailReason.RestoreNotFinished", "The host restore has not finished (Loading.Gate.SaveRestored)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_WriteFailed,    "Save.FailReason.WriteFailed",    "A file could not be written or read back");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_ManifestInvalid,"Save.FailReason.ManifestInvalid","The manifest is missing or unreadable");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_FileMissing,    "Save.FailReason.FileMissing",    "The manifest lists a file that is not there");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_CorruptFile,    "Save.FailReason.CorruptFile",    "Bad magic, bad header or wrong size");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_ChecksumMismatch,"Save.FailReason.ChecksumMismatch","The payload checksum does not match the header");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_SchemaTooNew,   "Save.FailReason.SchemaTooNew",   "The file was written by a newer game");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_MigrationMissing,"Save.FailReason.MigrationMissing","No migration step exists for an older snapshot");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_SlotUnreadable, "Save.FailReason.SlotUnreadable", "Current and Backup sets are both refused (Doc R13 3.11 recovery choices)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_NoIdentity,     "Save.FailReason.NoIdentity",     "A player has no PlayerIdentity yet");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_SaveInProgress, "Save.FailReason.SaveInProgress", "A slot change was refused because a write is running");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_RestoreFailed, "Save.FailReason.RestoreFailed", "A restore step failed; that player or the world counts as not restored");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_ParticipantNotRegistered, "Save.FailReason.ParticipantNotRegistered", "A record exists for a key and no registered participant claims it yet");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_SnapshotTypeMissing, "Save.FailReason.SnapshotTypeMissing", "A record's struct type no longer exists (renamed or removed)");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_IoError, "Save.FailReason.IoError", "A file exists but could not be read: a lock, not corruption. Nothing is parked or deleted.");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_SlotNameInvalid, "Save.FailReason.SlotNameInvalid", "The slot name is not a valid folder name");
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Save_FailReason_SlotNotEmpty, "Save.FailReason.SlotNotEmpty", "A new world was asked for in a slot that has files; delete the slot first");




