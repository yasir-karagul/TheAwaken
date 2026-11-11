#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"

class UFirstPersonCameraComponent;
class UScreenEffectComponent;

// The ONE file pair that knows the payload types of other frameworks' messages (Doc00 4.2, Open items row 12).
// The components never include another framework's header; they only expose plain handler functions
// (onCosmeticEvent, onSceneStarted, handleSourceValue, ...). The functions here connect the two.
namespace CameraMessageBindings{
    // Listeners of the camera rig: Cosmetic.Received, Cinematic.SceneStarted / SceneEnded, UI.Screen.Opened / Closed.
    void bindFirstPersonCamera(UFirstPersonCameraComponent* camera, TArray<FGameplayMessageListenerHandle>& handles);

    // One listener per distinct source tag of the loaded mapping rows.
    void bindScreenEffects(UScreenEffectComponent* effects, const TArray<FGameplayTag>& sourceTags, TArray<FGameplayMessageListenerHandle>& handles);

    // The start-up snapshot: reads blood amount, meter tiers and the low-health tag once and applies them with snap = true.
    // Returns true when the state was valid and the snapshot ran (so the retry stops), false when it must be tried again.
    bool applyStartupSnapshot(UScreenEffectComponent* effects);
}
