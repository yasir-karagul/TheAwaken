#include "ProgressionFramework/ProgressionSaveParticipant.h"
#include "ProgressionFramework/ProgressionComponent.h"
#include "GameFramework/Actor.h"

UProgressionSaveParticipant::UProgressionSaveParticipant(){
    saveKey = TEXT("Progression");   // never rename or reuse after release (Doc02 19.3)
    scope = E_SaveScope::World;      // shared by all players, Doc R13 3.4
    snapshotVersion = 1;
}

UProgressionComponent* UProgressionSaveParticipant::findProgressionComponent() const{
    // Found through the owner, not through AAwakenGameState: this framework does not depend on the Class Table (Doc00 §7).
    const AActor* owner = GetOwner();
    return owner != nullptr ? owner->FindComponentByClass<UProgressionComponent>() : nullptr;
}

bool UProgressionSaveParticipant::getSnapshotForSave_Implementation(FInstancedStruct& snapshot) const{
    const UProgressionComponent* progression = findProgressionComponent();
    if(progression == nullptr || !progression->isReady()){
        // false means "keep the record the file already has" (IBPI_SaveParticipant). A set that is not seeded yet is empty,
        // and writing it would erase a real set.
        return false;
    }
    snapshot = FInstancedStruct::Make(progression->getSnapshotForSave());
    return true;
}

bool UProgressionSaveParticipant::restoreFromSnapshot_Implementation(const FInstancedStruct& snapshot, int32 /*savedVersion*/){
    // GetPtr, not Get: a missing or renamed struct returns null and refuses the restore instead of crashing.
    const FS_ProgressionSaveRecord* record = snapshot.GetPtr<FS_ProgressionSaveRecord>();
    UProgressionComponent* progression = findProgressionComponent();
    if(record == nullptr || progression == nullptr){
        return false;
    }
    return progression->restoreFromSnapshot(*record);
}
