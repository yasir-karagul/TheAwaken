#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SaveFramework/01Structs/S_SaveFramework.h"

class USaveGame;

class THEAWAKEN_API FSaveFileIO{
public:
	static bool serializeSave(USaveGame* save, TArray<uint8>& payload);

	/** Fills schema, build, time, size and CRC in Header, then builds the whole file. */
	static void buildFileBytes(FS_SaveFileHeader& header, const TArray<uint8>& payload, TArray<uint8>& file);

	/** Header only; the payload is not checked. Invalid tag = success. */
	static FGameplayTag parseHeader(const TArray<uint8>& file, FS_SaveFileHeader& header, int32& payloadOffset);

	/** Reads just the first bytes of a file: used by the slot list and the character index. */
	static FGameplayTag readHeaderFromDisk(const FString& path, FS_SaveFileHeader& header);

	/** Header, size, schema and checksum (Doc R13 3.11: a newer schema or a failed checksum is refused). */
	static FGameplayTag validateFile(const TArray<uint8>& file, FS_SaveFileHeader& header, int32& payloadOffset, int32 minSchema);
};