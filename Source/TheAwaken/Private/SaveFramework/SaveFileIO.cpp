#include "SaveFramework/SaveFileIO.h"
#include "SaveFramework/SaveConstants.h"
#include "SaveFramework/02Tags/T_SaveFramework.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace{
    constexpr int32 envelopePrefixBytes = sizeof(uint32) + sizeof(int32) + sizeof(int32);   // magic + envelope version + header length
    constexpr int32 maxHeaderBytes = 64 * 1024;
}

bool FSaveFileIO::serializeSave(USaveGame* save, TArray<uint8>& payload){
    payload.Reset();
    return save && UGameplayStatics::SaveGameToMemory(save, payload);   // game thread only
}

void FSaveFileIO::buildFileBytes(FS_SaveFileHeader& header, const TArray<uint8>& payload, TArray<uint8>& file){
    header.envelopeVersion = AwakenSave::envelopeVersion;
    header.schemaVersion = AwakenSave::schemaVersion;
    header.buildVersion = FApp::GetBuildVersion();
    header.writtenAtUtc = FDateTime::UtcNow();
    header.payloadSize = payload.Num();
    header.payloadCrc = FCrc::MemCrc32(payload.GetData(), payload.Num());

    TArray<uint8> headerBytes;
    FMemoryWriter headerWriter(headerBytes);
    headerWriter << header;

    file.Reset();
    FMemoryWriter writer(file);
    uint32 magic = AwakenSave::fileMagic;
    int32 envelopeVersion = AwakenSave::envelopeVersion;
    int32 headerLen = headerBytes.Num();
    writer << magic << envelopeVersion << headerLen;
    writer.Serialize(headerBytes.GetData(), headerBytes.Num());
    writer.Serialize(const_cast<uint8*>(payload.GetData()), payload.Num());
}

FGameplayTag FSaveFileIO::parseHeader(const TArray<uint8>& file, FS_SaveFileHeader& header, int32& payloadOffset){
    if(file.Num() < envelopePrefixBytes){
        return T_Save_FailReason_CorruptFile;
    }

    FMemoryReader prefix(file);
    uint32 magic = 0; int32 envelopeVersion = 0; int32 headerLen = 0;
    prefix << magic << envelopeVersion << headerLen;
    if(magic != AwakenSave::fileMagic || envelopeVersion < 1 || headerLen <= 0 || headerLen > maxHeaderBytes
        || static_cast<int64>(envelopePrefixBytes) + headerLen > file.Num()){
        return T_Save_FailReason_CorruptFile;
    }
    // written by a newer game
    if(envelopeVersion > AwakenSave::envelopeVersion){
        return T_Save_FailReason_SchemaTooNew;
    }

    // A private copy of the header bytes: parsing can never run into the payload, and unknown trailing bytes are ignored.
    const TArray<uint8> headerBytes(file.GetData() + envelopePrefixBytes, headerLen);
    FMemoryReader headerReader(headerBytes);
    header = FS_SaveFileHeader();
    header.envelopeVersion = envelopeVersion;
    headerReader << header;
    if(headerReader.IsError() || headerReader.IsCriticalError()){
        return T_Save_FailReason_CorruptFile;
    }

    payloadOffset = envelopePrefixBytes + headerLen;
    return FGameplayTag();
}

FGameplayTag FSaveFileIO::readHeaderFromDisk(const FString& path, FS_SaveFileHeader& header){
    TUniquePtr<FArchive> reader(IFileManager::Get().CreateFileReader(*path));
    if(!reader){
        return T_Save_FailReason_FileMissing;
    }

    const int32 toRead = static_cast<int32>(FMath::Min<int64>(reader->TotalSize(), envelopePrefixBytes + maxHeaderBytes));   // TotalSize: the engine's spelling
    TArray<uint8> prefix;
    prefix.SetNumUninitialized(toRead);
    reader->Serialize(prefix.GetData(), toRead);

    int32 offset = 0;
    return parseHeader(prefix, header, offset);
}

FGameplayTag FSaveFileIO::validateFile(const TArray<uint8>& file, FS_SaveFileHeader& header, int32& payloadOffset, int32 minSchema){
    const FGameplayTag parsed = parseHeader(file, header, payloadOffset);
    if(parsed.IsValid()){
        return parsed;
    }

    if(header.schemaVersion > AwakenSave::schemaVersion){
        return T_Save_FailReason_SchemaTooNew;
    }
    if(header.schemaVersion < minSchema){
        return T_Save_FailReason_MigrationMissing;
    }
    if(header.payloadSize < 0 || static_cast<int64>(payloadOffset) + header.payloadSize != file.Num()){
        return T_Save_FailReason_CorruptFile;
    }
    if(FCrc::MemCrc32(file.GetData() + payloadOffset, header.payloadSize) != header.payloadCrc){
        return T_Save_FailReason_ChecksumMismatch;
    }
    return FGameplayTag();
}