#pragma once

#include "CoreMinimal.h"
#include "E_CameraFramework.generated.h"

// PROPOSED (Open items, row 18). A closed set that drives branching, so an enum (Doc02 3).
// The mode is derived from two flags in UFirstPersonCameraComponent::refreshViewMode, never set directly.
UENUM(BlueprintType)
enum class E_CameraViewMode : uint8{
    FirstPerson,
    SceneYielded,
    BagView
};

// Broadcast by UFirstPersonCameraComponent when the derived view mode changes. Local only, never replicated.
// Used by the debug view, and by the Character to hide or show the held item while the mode is BagView.
// Declared here, next to the enum it uses: UHT does not register a delegate that sits alone in a header (see 04Delegates/D_CameraFramework.h).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FD_CameraViewModeChanged, E_CameraViewMode, oldMode, E_CameraViewMode, newMode);