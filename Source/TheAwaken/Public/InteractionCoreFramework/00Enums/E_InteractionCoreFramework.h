// Public/InteractionCoreFramework/00Enums/E_InteractionCoreFramework.h
// Doc B18 section 5. Content grows through tags, not enum values (Doc02 20).
#pragma once

#include "CoreMinimal.h"
#include "E_InteractionCoreFramework.generated.h"

// What one press of IA_Interact means for an object (B18 3.2).
UENUM(BlueprintType)
enum class E_InteractionKind : uint8{
    Press UMETA(DisplayName = "Press"),
    Hold UMETA(DisplayName = "Hold"),
    Grab UMETA(DisplayName = "Grab")
};

// What happens when two players use one thing (B18 3.7).
UENUM(BlueprintType)
enum class E_InteractionLockPolicy : uint8{
    None UMETA(DisplayName = "None"),
    Shared UMETA(DisplayName = "Shared"),
    Exclusive UMETA(DisplayName = "Exclusive")
};

// Why a hold ended early. PROPOSED: B18 names a reason in the message but not its values. Never saved (B18 section 8).
// Refused is an addition of this code (PROPOSED): canActivate refusing at completion needs a reason that is not one of the others.
UENUM(BlueprintType)
enum class E_InteractionHoldCancelReason : uint8{
    Released UMETA(DisplayName = "Released"),
    OutOfRange UMETA(DisplayName = "Out Of Range"),
    NoLineOfSight UMETA(DisplayName = "No Line Of Sight"),
    Damaged UMETA(DisplayName = "Damaged"),
    Downed UMETA(DisplayName = "Downed"),
    InUse UMETA(DisplayName = "In Use"),
    Scene UMETA(DisplayName = "Scene"),
    Disconnected UMETA(DisplayName = "Disconnected"),
    Refused UMETA(DisplayName = "Refused")
};

// Internal result of the server's six checks (B18 3.5). Not reflected: nothing outside the component needs it.
enum class E_InteractionRequestVerdict : uint8{
    Pass,      // all six checks passed
    Ignore,    // check 1: no prompt existed, so nothing is answered
    Refuse     // a later check failed; failReason holds the tag
};
