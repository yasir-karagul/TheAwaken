// Public/DifficultyFramework/DifficultySubsystem.h (TEMPORARY)
#pragma once
#include "Subsystems/WorldSubsystem.h"
#include "DifficultySubsystem.generated.h"

// TEMPORARY until Doc B13 is built. The three functions below are what the Stats framework reads (S2, S7).
// When the real subsystem exists, keep the names or change the call sites in AS_CoreVitals.cpp and StatsLibrary.cpp.
UCLASS()
class THEAWAKEN_API UDifficultySubsystem : public UWorldSubsystem{
    GENERATED_BODY()
public:
    static bool canPlayerDie(const UObject* worldContext);
    static float getHealthFloor(const UObject* worldContext);
    static float getEnemyHealthMultiplier(const UObject* worldContext);
};
