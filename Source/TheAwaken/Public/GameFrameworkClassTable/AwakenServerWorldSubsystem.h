// Public/GameFrameworkClassTable/AwakenServerWorldSubsystem.h
#pragma once
#include "Subsystems/WorldSubsystem.h"
#include "AwakenServerWorldSubsystem.generated.h"

UCLASS(Abstract)
class THEAWAKEN_API UAwakenServerWorldSubsystem : public UWorldSubsystem{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* outer) const override;
};
