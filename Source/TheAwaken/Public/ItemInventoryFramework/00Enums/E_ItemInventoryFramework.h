#pragma once

#include "CoreMinimal.h"
#include "E_ItemInventoryFramework.generated.h"

// R01 3.2 (decision D4): the two item kinds.
UENUM(BlueprintType)
enum class E_ItemKind : uint8{
    Stackable,     // an id and a quantity, no state of its own
    Instanced      // one physical item with its own id and state; never stacks
};

// R01 3.1 (decision D8, Index A116). None is for items that are not weapons (I5).
UENUM(BlueprintType)
enum class E_WeaponType : uint8{
    None,
    Melee,
    Bow,
    Crossbow,
    Firearm,
    Thrown
};

// R01 3.4: the three inventory types. A death bag is a Personal inventory with a flag, not a fourth type.
UENUM(BlueprintType)
enum class E_InventoryType : uint8{
    Personal,
    Container,
    PortableContainer
};

// R01 3.1, 3.2a: the kinds of entry in an item's ConsumeEffects list.
UENUM(BlueprintType)
enum class E_ConsumeEffectKind : uint8{
    ModifyMeter,      // becomes a ModifyMeter call (Doc B03)
    IllnessChance,    // a chance to register an illness source (Doc B03 3.4)
    HealthLoss        // an HP loss through DamageType.Illness, every time (Index A96)
};
