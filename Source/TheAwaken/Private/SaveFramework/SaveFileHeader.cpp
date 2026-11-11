#include "SaveFramework/SaveFileHeader.h"
#include "SaveFramework/01Structs/S_SaveFramework.h"
FArchive& operator<<(FArchive& ar, FS_SaveFileHeader& header){
	uint8 scopeByte = static_cast<uint8>(header.scope);
	ar << scopeByte;
	header.scope = static_cast<E_SaveScope>(scopeByte);

	ar << header.schemaVersion << header.buildVersion << header.writtenAtUtc << header.worldID << header.saveCounter
	   << header.payloadSize << header.payloadCrc << header.characterID << header.playerIdentity << header.linkedIds
	   << header.heldBagID << header.dayCount;

	FString season = header.seasonTag.ToString();
	FString difficulty = header.difficultyTag.ToString();
	ar << season << difficulty;
	if(ar.IsLoading()){
		header.seasonTag = FName(*season);
		header.difficultyTag = FName(*difficulty);
	}

	// A later layout change appends its fields here, guarded by the envelope version the reader found:
	// if (header.envelopeVersion >= 2) { ar << header.newField; }
	return ar;
}