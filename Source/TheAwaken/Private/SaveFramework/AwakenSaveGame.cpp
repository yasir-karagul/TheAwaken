#include "SaveFramework/AwakenSaveGame.h"
#include "SaveFramework/SaveSubsystem.h"      // LogAwakenSave

void UAwakenSaveGameBase::setSnapshot(FName key, int32 version, const FInstancedStruct& data){
	auto matchesKey = [key](const FS_SnapshotRecord& r){
		return r.saveKey == key;
	};
	FS_SnapshotRecord* existing = snapshots.FindByPredicate(matchesKey);
	FS_SnapshotRecord& rec = existing ? *existing : snapshots.AddDefaulted_GetRef();
	rec.saveKey = key;
	rec.snapshotVersion = FMath::Max(1, version);
	rec.data = data;
}

const FS_SnapshotRecord* UAwakenSaveGameBase::findSnapshot(FName key) const{
	auto matchesKey = [key](const FS_SnapshotRecord& r){
		return r.saveKey == key;
	};
	return snapshots.FindByPredicate(matchesKey);
}

void UAwakenSaveGameBase::carryForward(const UAwakenSaveGameBase* previous, const TSet<FName>& handledKeys){
	if(!previous){
		return;
	}
	for(const FS_SnapshotRecord& old : previous->snapshots){
		if(handledKeys.Contains(old.saveKey) || findSnapshot(old.saveKey)){
			continue;
		}
		if(!old.data.IsValid()){
			UE_LOG(LogAwakenSave, Warning, TEXT("Record '%s' has no struct type any more and cannot be carried forward."), *old.saveKey.ToString());
			continue;
		}
		snapshots.Add(old);
	}
}