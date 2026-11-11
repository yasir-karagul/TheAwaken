#pragma once
#include "CoreMinimal.h"
#include "E_SessionHandshakeFramework.generated.h"

/** Doc B05 5. Fixed set that drives the 3.1 branch. DedicatedServer is future: it only changes which machine asks. */
UENUM(BlueprintType)
enum class E_SessionMode : uint8{
    Offline,
    P2PCoop,
    DedicatedServer
};

/** Which kind of id is the primary id of a player (Doc B05 3.5). A local id is marked so it cannot be mistaken for a platform id. */
UENUM(BlueprintType)
enum class E_PlayerIdKind : uint8{
    PlatformAccount,
    CachedPlatformAccount,
    LocalFallback
};

/** Which store verified a peer. Steam and Epic check ownership differently (Doc B05 3.5, review B05-02). */
UENUM(BlueprintType)
enum class E_StoreKind : uint8{
    None,
    Steam,
    Epic
};

/** What the Character-save lookup found for a joining identity (Doc B05 amendment, Index A26 to A45). */
UENUM(BlueprintType)
enum class E_ReturningLookup : uint8{
    NotChecked,
    NewPlayer,
    ReturningPlayer
};
