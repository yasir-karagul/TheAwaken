#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CosmeticEventsEnvironmentPresentationFramework/01Structs/S_CosmeticEventsEnvironmentPresentationFramework.h"
#include "CosmeticEventRelayComponent.generated.h"

// The client call of the cosmetic channel, on every PlayerController (gap G2: B19 3.4 lists no relay there, so the RPC lives in B21's own files).
// A per-player Client call is used instead of a multicast, because a multicast would reach clients outside the radius.
UCLASS(ClassGroup=(Awaken))
class THEAWAKEN_API UCosmeticEventRelayComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UCosmeticEventRelayComponent();

    // Unreliable and never resent (B21 A.2). _Implementation is the suffix Unreal generates, an exception to Doc02 20.1 that 20.5 allows.
    UFUNCTION(Client, Unreliable)
    void clientReceiveCosmeticEvent(const FS_CosmeticEvent& event);
};
