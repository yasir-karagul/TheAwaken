#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "SaveFramework/SaveParticipantComponent.h"
#include "ProgressionSaveParticipant.generated.h"

class UProgressionComponent;

UCLASS(ClassGroup = (Save), meta = (BlueprintSpawnableComponent))
class THEAWAKEN_API UProgressionSaveParticipant : public USaveParticipantComponent{
    GENERATED_BODY()

public:
    UProgressionSaveParticipant();

    // BlueprintNativeEvent overrides. The _Implementation suffix is Unreal's (Doc02 §20.5).
    virtual bool getSnapshotForSave_Implementation(FInstancedStruct& snapshot) const override;
    virtual bool restoreFromSnapshot_Implementation(const FInstancedStruct& snapshot, int32 savedVersion) override;

private:
    UProgressionComponent* findProgressionComponent() const;
};
