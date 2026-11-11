// Public/GameFrameworkClassTable/AwakenServerProbeSubsystem.h (TEMPORARY)
#pragma once
#include "GameFrameworkClassTable/AwakenServerWorldSubsystem.h"
#include "AwakenServerProbeSubsystem.generated.h"

UCLASS()
class THEAWAKEN_API UAwakenServerProbeSubsystem : public UAwakenServerWorldSubsystem{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
};
