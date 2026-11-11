// Private/DifficultyFramework/DifficultySubsystem.cpp (TEMPORARY)
#include "DifficultyFramework/DifficultySubsystem.h"

bool UDifficultySubsystem::canPlayerDie(const UObject* worldContext){
    return true;
}

float UDifficultySubsystem::getHealthFloor(const UObject* worldContext){
    return 1.0f;
}

float UDifficultySubsystem::getEnemyHealthMultiplier(const UObject* worldContext){
    return 1.0f;
}
