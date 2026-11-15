#include "CraftingFramework/CraftingProgressionBridge.h"
#include "ProgressionFramework/ProgressionComponent.h"

bool UCraftingProgressionBridge::isUnlocked_Implementation(FGameplayTag unlockTag){
    // The bridge is outered to UCraftingServices, so it reaches the game instance's current world through its outers.
    const UProgressionComponent* progression = UProgressionComponent::findProgression(this);
    return progression != nullptr && progression->isUnlocked(unlockTag);
}
