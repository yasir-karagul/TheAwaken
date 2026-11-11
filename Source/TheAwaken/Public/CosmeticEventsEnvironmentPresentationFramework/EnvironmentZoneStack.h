#pragma once

#include "CoreMinimal.h"
#include "CosmeticEventsEnvironmentPresentationFramework/01Structs/S_CosmeticEventsEnvironmentPresentationFramework.h"

// One zone on the stack. weight moves toward targetWeight: 1 after a push, 0 after a pop.
struct FS_ZoneEntry{
    FGuid zoneId;
    FS_EnvironmentZoneSettings settings;
    float weight = 0.0f;
    float targetWeight = 1.0f;
    float blendSeconds = 0.0f;
};

// The fog values a zone can change.
struct FS_ZoneFog{
    float density = 0.0f;
    float heightFalloff = 0.0f;
    FLinearColor inscatteringColor = FLinearColor::Black;
    float startDistance = 0.0f;
    float maxOpacity = 1.0f;
};

// The stack of active zones (B21 B.7). No world, no components: the manager reads the layers and applies them.
//
// Rules:
//  - The most recently pushed zone is on top and wins, so zones nest.
//  - pop removes the zone with that id from any position. The others keep their order. An unknown id is ignored.
//  - push of an id already in the stack updates its settings and does not add a second entry.
//  - A popped zone stays in the list while its weight falls to 0, so it fades out; it is no longer "active".
//  - The look is the layers applied in stack order: global, then each zone lerped in by its weight. So when the top zone is popped the look
//    fades into the zone below it, and a zone below a full-weight top zone changes nothing.
class THEAWAKEN_API FEnvironmentZoneStack{
public:
    // defaultBlendSeconds is used when the zone's own blendSeconds is 0. jumpToFull puts the weight at 1 at once (hard start).
    void push(const FGuid& zoneId, const FS_EnvironmentZoneSettings& settings, float defaultBlendSeconds, bool jumpToFull);

    void pop(const FGuid& zoneId);

    void advance(float deltaSeconds);

    // Every weight jumps to its target and finished fades are removed.
    void snapAll();

    void clear();

    bool isMoving() const;

    int32 num() const{
        return entries.Num();
    }

    // The ids of the zones that are on the stack and not popped, bottom to top.
    TArray<FGuid> getActiveIds() const;

    const TArray<FS_ZoneEntry>& getEntries() const{
        return entries;
    }

    // The two topmost zones that are visible (weight above a small limit). The manager draws them with two post-process components, so a
    // fading top zone blends into the zone below instead of into the global look.
    void getPostProcessLayers(const FS_ZoneEntry*& lower, const FS_ZoneEntry*& upper) const;

    // 1 outside every hide-sky zone, 0 deep inside one.
    float layerSkyVisibility() const;

    // Lerps the fog toward each zone that overrides fog, bottom to top, by the zone's weight.
    void layerFog(FS_ZoneFog& fog) const;

private:
    TArray<FS_ZoneEntry> entries;
};
