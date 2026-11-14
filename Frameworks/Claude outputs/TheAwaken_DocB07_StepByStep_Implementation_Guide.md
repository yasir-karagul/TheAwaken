# TheAwaken — Doc B07 Step-by-Step Implementation Guide
## Carry & Encumbrance Framework (heavy items in the hands, carry slots, drop, throw, forced drops)

Prepared for Yasir · Target: Unreal Engine 5.8 · C++ core with a thin layer of Blueprints
Companion to: *TheAwaken_DocB07_CarryEncumbranceFramework_ImplementationSpec* (the spec wins on any conflict) and *TheAwaken_DocR01_StepByStep_Implementation_Guide* (this guide builds on the classes that guide created and uses the same format, task ids and rules). One small piece comes from *TheAwaken_DocR05_StepByStep_Implementation_Guide* (the cosmetic seam, see 0.4).

---

## How to use this guide

1. Work top to bottom. Each section ends with **Verify** and **Handoff**. Do not start a section until the previous one compiles and its checks pass.
2. Task ids read `C3.4` = Carry Section 3, task 4. The `C` keeps them apart from the `S` ids of the R01 guide and the `E` ids of the R05 guide. Each task cites the B07 clause (or related doc) it implements.
3. **Honesty note:** this code was written against UE 5.8 conventions and against the class and function names of the R01 guide, but **was not compiled**. Expect a few small API or include fixes. After each section, paste the first compiler error (if any) and we fix it together; do not "fix forward" across sections.
4. **This guide assumes the R01 guide's code exists** (Sections 0 to 6, 12 and 13). If you renamed something in R01 while building it, the same rename applies here. The exact R01 names this guide calls are listed in 0.4, so you can check them in one pass before you start.
5. Everything marked **PROPOSED** is implemented as written but kept easy to change: every number and every content choice lives in data (Doc00 §2.7).
6. Placeholders are the same as in R01: `AWAKEN_API` is the module export macro and `Awaken` the module name.
7. A later section sometimes adds members to a class an earlier section created. Those places say **Header additions** and show only the new lines; the earlier code stays as it is.

**What this framework is, in five sentences.** A *heavy* item (a log, a stone, a corpse) never enters the bag: it is carried in the hands, in a small fixed number of carry slots (2, a data value). While any slot is occupied the character is *carrying*: it cannot attack, block, dodge, sprint, eat, craft or open the bag, and it moves slower in a carry pose. A carry ends by dropping, throwing, placing the item into a blueprint site, or sending it along a rope; or it is *interrupted* (the player goes Downed, dies, starts to swim, falls asleep or leaves), and then the item drops where the player is. Nothing is ever destroyed by a refusal or an interrupt, and nothing about a carry is ever saved. The framework is one small component on the Character plus the generic throw pipeline that Combat reuses for thrown weapons.

---

## 0.1 Code style reminder (Doc02 §20, designer-confirmed)

The full cheat sheet is section 0.1 of the R01 guide. The short version, because every block below follows it:

| Rule | Right | Wrong |
| --- | --- | --- |
| Variables, params, members, functions | `carrySlots`, `beginCarry()` | `CarrySlots`, `BeginCarry()` |
| Booleans: no `b` prefix | `holdsActor`, `isCarryActive()` | `bHoldsActor` |
| Enum / struct / interface / delegate / tag | `E_EndCarryReason`, `FS_CarrySlotState`, `UBPI_CarryableActor` + `IBPI_CarryableActor`, `FD_CarryChanged`, `T_Carry_State_Active` | anything else |
| Keyword spacing and braces | `if(x){` … `}else{` … closing `}` alone on its line, braces always | `if (x) {`, `} else {`, `if(x) return;` |
| Indent | 4 spaces | tabs |

Names Unreal forces on us that are new in this guide (Doc02 §20.5): `PostNetReceiveVelocity`, `MovementModeChangedDelegate`, `OnProjectileStop`, and `OnRep_carrySlots` / `OnRep_itemId` (the replication-notify rule). Everything else PascalCase in the code is already on the R01 list (`BeginPlay`, `EndPlay`, `GetLifetimeReplicatedProps`, `_Implementation`, `IsDataValid`, the macros).

> The spec writes names the old way (`BeginCarry`, `S_CarrySlotState`, `GetCarriedItemsForCost`). In code they become `beginCarry`, `FS_CarrySlotState`, `getCarriedItemsForCost`. Same thing, Doc02 §20 spelling.

---

## 0.2 Decisions this guide makes where the spec is silent

B07 is a short spec. It names four functions, one struct with one field and a table of interrupts, and leaves the mechanics open. These are the choices I made so you can build without waiting. Each is small and easy to reverse. **Confirm or override any of them** (they are repeated in Appendix C as questions); none blocks Section 0.

| # | Open point | Choice in this guide | Why |
| --- | --- | --- | --- |
| C1 | The component is on the **Character** (B07 §2, Doc B19 3.3), but R01 already calls Carry through a seam that speaks **PlayerState** (`IBPI_CarryService`) | `UCarryServices`, a small game-instance subsystem, is the real `IBPI_CarryService`. It finds the component on the player's current pawn | R01's `grantOrDrop`, `pickUpLooseItem` and request gate then work unchanged, and NPCs (Doc B12) can use the same component without a PlayerState |
| C2 | What a carried item *is* while it is in the hands (B07 §10 PROPOSES an attached, replicated actor) | **Data.** A slot stores the item id; the server also keeps the full inventory entry. Each machine draws the mesh locally from the replicated slot. No actor exists while an item is carried | R01's seam hands us an *entry* and removes the loose actor itself (R01 guide S5.5). An extra replicated actor per log would only repeat what the slot already says. The server still owns the item, as §10 asks |
| C3 | `S_CarrySlotState` has one field, `ItemID` (B07 §7) | Six more fields: `carryId`, `sequence`, `holdsActor`, `carriedActor`, `lastEndReason`, `lastInterruptCause` | A client needs them to raise `Carry.Ended` with the right reason from replicated state (B07 §9), and a carried corpse is an actor, not an item id |
| C4 | "A cost consumes that many slots" and "each slot holds exactly one heavy item" (both in B07 3.1) | An item with cost N fills N slots. All N slots show the same item and share one `carryId`; the lowest of them is the item's *primary* slot. No slot ever holds a quantity | Both sentences stay true |
| C5 | "Only the item in the active carry slot is sent" (Doc B09 3.11) but nothing defines *active* | The active slot is the one filled most recently. A drop or throw request with slot `-1` uses it | The item picked up last is the one on top of the pile in the arms |
| C6 | `EndCarry(SlotIndex, Reason)` frees the slot, but who puts the item somewhere? | `Dropped` and `Interrupted`: the component spawns the loose item itself. `Placed`, `Sent` and `Thrown`: the released entry is **returned to the caller**, who owns what happens next (Building, Transport, the throw pipeline) | One function per the spec, and an item can never be lost between two frameworks |
| C7 | Who owns the throw pipeline? B07 3.4 says "reuse the existing one"; Doc R03 3.2 and Doc B11 say it is B07's | **It lives here.** `AThrownItemActor` plus `UBFL_Carry::throwEntry`. Combat calls the same function for a thrown spear with the weapon's own damage | Somebody has to build it once, and every other document already points here |
| C8 | Damage of a thrown heavy item (B07 3.4: "a distinct DamageProfile") | One new item field, `throwImpactDamage`. The hit is handed to Combat through a seam, `IBPI_ThrowImpactSink`, with a stub | Doc R03 owns the damage order; Carry only says "this hit that, with this much" |
| C9 | What the host checks on a throw request (Doc02 §19.6 lists "throw vector (B07)") | The client sends only a direction. The host uses its **own** eye position as origin and refuses a direction more than `maxThrowAngleDegrees` away from where it knows the pawn is looking | No origin from the client means no origin to validate |
| C10 | How a corpse (Doc B08) or a wounded ally (Doc B12) is carried | A second entry point, `beginCarryActor`, for any actor that implements `IBPI_CarryableActor`. The actor is attached to the carrier on the server. It can be dropped, never thrown, placed or sent | "A carried corpse is not an item stack" (Doc B09 3.11) |
| C11 | `Carry.State.Active` on the Ability System Component (B07 3.2) | A direct GAS call on the server (`AddLooseGameplayTag` plus the replicated variant). Does nothing when the pawn has no Ability System Component yet | The spec says the owner adds it. One new module dependency, no seam |
| C12 | "A carry that begins ends a running Dodge, Sprint, Block, Reload or Weapon Charge" (B07 3.2) | R01's existing seam `IBPI_ActionService::cancelAction`, once per tag | Doc R04 implements that one function for R01 anyway |
| C13 | Who triggers a forced drop | The owner of each state calls `interruptCarry(causeTag)`. Two safety nets are built in: the component drops everything when its pawn is destroyed, and when the engine's movement mode becomes swimming | B02, R07, R12 and B20 may not exist yet. Without the nets a death would delete the log |
| C14 | The interrupt cause (Doc B22 3.5 wants it in the debug view) | New tags `Carry.InterruptCause.*`; `Carry.Ended` carries the cause next to the reason | A tag is the open-ended choice of Doc02 §3 |
| C15 | The carry slowdown (Doc R07 3.5 collects "carry" as a multiplier) | One setting, `carrySpeedMultiplier`; `getSpeedMultiplier()` answers 1 when nothing is carried | R07 owns the movement stack; Carry only supplies its number |
| C16 | Where drop and throw requests travel | Server RPCs on `UCarryComponent` (the owning client owns its pawn). They reuse R01's request gate, so one rate limit covers bag and carry | Same pattern as the R05 guide (E1) |
| C17 | A heavy grant (harvest yield, refund) to a player who is Downed or swimming | Refused by `beginCarry`, so R01 lets it land loose beside the player | The item would be dropped in the same frame anyway |
| C18 | The hotbar while carrying (R01 guide, Appendix C point 6, left this to B07) | Selecting, binding and holding-from-bag are **refused** with `HandsFull` (a three-line patch to R01). The held item is not changed when a carry begins; Doc R07 hides it under the carry pose | B07 3.6: "everything that is not on this list of allowed things is refused" |
| C19 | "The owning client may show the carry pose at once" (B07 §10, PROPOSED) | Not built. The pose follows replicated state, and the server forces an immediate update | A Character replicates many times a second; prediction can be added later inside `serverPickUp` without changing this framework |
| C20 | The cosmetic events `Cosmetic.Event.Carry.Picked` / `.Dropped` (B07 amendment A89) | Raised through `IBPI_CosmeticEventSink`, the interface the R05 guide created (E1.4) | Two guides must not declare the same interface (R01 guide, I20) |
| C21 | Refusal reasons the spec does not name | Four new `Carry.FailReason.*` leaves: `NotCarryable`, `SlotEmpty`, `CannotThrow`, `InvalidThrow` | Doc02 §6: a refusal is always a tag |

---

## 0.3 Section map and dependency order

```
C0 scaffolding ─ C1 types/tags/seams ─ C2 item data ─ C3 component core
C4 begin a carry + the R01 seam ─ C5 end a carry, cost sourcing ─ C6 the throw pipeline
C7 interrupts and the refusal list ─ C8 actor carries (corpse, wounded ally)
C9 networking ─ C10 dev tools, tests, acceptance
```

Each section depends only on earlier ones.

## 0.4 What this guide uses from the R01 and R05 guides

Check these names against your real code once. If one differs, tell me and I adjust the guide.

| Piece | Used for |
| --- | --- |
| `UInventoryServices` (`get`, `registerSeam`, `getSeam`, `isIncapacitated`, `cancelAction`, `spawnLooseItem`) | the shared seam registry, the player-state check, ending actions, dropping |
| `IBPI_CarryService`, `UStubCarryService` | the seam R01 calls from `grantOrDrop`, `pickUpLooseItem` and the request gate; **this guide replaces the stub** |
| `FS_InventoryEntry`, `FS_ItemStack`, `FS_GrantResult.quantityCarried` | what a carried item is on the server |
| `UDA_ItemDefinition` (`carryCapacityCost`, `weightClassTag`, `isHeavy()`, `isPermanent()`, `isPortableContainer()`, `worldMesh`, `IsDataValid` with its `fail` helper) | item data |
| `UItemCatalogSubsystem::get` / `getDefinition` | item data |
| `UBFL_Inventory` (`isServerWorld`, `grantOrDrop`, `takeOutOfBag`) | dev commands, tests, the thrown-weapon example |
| `UInventorySettings::get()->dropForwardDistance` | where a dropped item appears |
| `UInventoryRequestComponent` (`findFor`, `gate`, `refuse`) | request gating |
| `UPlayerInventoryComponent` (`findFor`, `getPersonalInventory`), `UInventoryData::getTotalQuantity` | tests only |
| `InventoryLog::roleName`, `FInventoryTestFixture` (`addStackable`, `makePlayer`, `makeCharacterFor`, `services`, `stub`), `AInventoryTestCharacter`, `UStubLooseItemSpawner` | logging and tests |
| Tags `T_Interaction_FailReason_HandsFull`, `T_Interaction_FailReason_NotAllowedInState`, `T_Inventory_FailReason_NotServer` / `_NotFound` / `_InvalidRequest` / `_RateLimited`, `T_Action_Reload` | refusal reasons and the reload cancel |
| **From the R05 guide:** `IBPI_CosmeticEventSink` (file `EquipmentFramework/03Interfaces/BPI_CosmeticEventSink.h`, E1.4) | the two cosmetic events (C20) |

> **If you have not built the R05 guide yet:** do only its task E1.4 (the file `BPI_CosmeticEventSink.h`) before Section 1 here. Carry needs the header and nothing else from Equipment. If no implementation is registered, the two cosmetic events are simply not raised.

---

# SECTION 0 — Scaffolding

**Goal:** an empty Carry framework that compiles and logs.
**Spec:** Doc02 §14, §20.3; Doc B22 §3.6; B07 3.1 (the number of slots is tuning data).
**Depends on:** the R01 guide's Sections 0 and 1.

### Files

```
Source/Awaken/Public/CarryFramework/
    00Enums/   01Structs/   02Tags/   03Interfaces/   04Delegates/
    CarryLog.h
    CarryServices.h
    UCarrySettings.h
Source/Awaken/Private/CarryFramework/
    02Tags/   Stubs/   DevTools/   Tests/
    CarryLog.cpp
    CarryServices.cpp
    UCarrySettings.cpp
```

### C0.1 — Create the folder tree (Doc02 §20.3)

From `Source/<Module>`:

```bash
for dir in 00Enums 01Structs 02Tags 03Interfaces 04Delegates; do
  mkdir -p "Public/CarryFramework/$dir"
done
mkdir -p Private/CarryFramework/02Tags Private/CarryFramework/Stubs
mkdir -p Private/CarryFramework/DevTools Private/CarryFramework/Tests
```

### C0.2 — Module dependency for the Ability System tag (C11)

In `Awaken.Build.cs`, add to `PublicDependencyModuleNames`:

```csharp
    "GameplayAbilities",         // the Carry.State.Active loose tag on the Ability System Component (B07 3.2)
    "GameplayTasks"              // required by GameplayAbilities
```

In the editor, *Edit → Plugins*: enable **Gameplay Abilities** and restart. If Doc R04 or R08 is already in the project, both lines and the plugin are there already.

### C0.3 — Log category (Doc B22 §3.6 names it `LogAwakenCarry`)

`Public/CarryFramework/CarryLog.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "ItemInventoryFramework/InventoryLog.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAwakenCarry, Log, All);

// Every line names the machine role (Host, Client, Offline). Use: CARRY_LOG(Log, this, "text %d", value);
#define CARRY_LOG(verbosity, worldContext, format, ...) \
    UE_LOG(LogAwakenCarry, verbosity, TEXT("[%s] " format), *InventoryLog::roleName(worldContext), ##__VA_ARGS__)
```

`Private/CarryFramework/CarryLog.cpp`

```cpp
#include "CarryFramework/CarryLog.h"

DEFINE_LOG_CATEGORY(LogAwakenCarry);
```

### C0.4 — Project settings (every tuning number in one place)

`Public/CarryFramework/UCarrySettings.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UCarrySettings.generated.h"

// Project Settings -> Game -> Awaken Carry. Every value is tuning data (Doc00 2.7).
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Awaken Carry"))
class AWAKEN_API UCarrySettings : public UDeveloperSettings{
    GENERATED_BODY()

public:
    UCarrySettings();

    static const UCarrySettings* get(){
        return GetDefault<UCarrySettings>();
    }

    // B07 3.1: "1-2, per the original design intent ... the number of slots stays tuning data".
    UPROPERTY(Config, EditAnywhere, Category = "Slots", meta = (ClampMin = "1")) int32 carrySlotCount = 2;
    // Doc R07 3.5 collects this with the other speed multipliers. PROPOSED value.
    UPROPERTY(Config, EditAnywhere, Category = "Movement", meta = (ClampMin = "0.05", ClampMax = "1.0")) float carrySpeedMultiplier = 0.6f;

    // Where the item of each slot is drawn on the body. Entry 0 = slot 0. Visual only; Doc R07 owns the pose.
    UPROPERTY(Config, EditAnywhere, Category = "Visual") TArray<FName> carrySocketNames;
    // False when Doc R07 draws the carried item itself.
    UPROPERTY(Config, EditAnywhere, Category = "Visual") bool showCarriedMeshes = true;

    // B07 3.4. All PROPOSED values.
    UPROPERTY(Config, EditAnywhere, Category = "Throw", meta = (ClampMin = "0")) float throwSpeed = 900.0f;
    // How far in front of the eye a thrown item starts, so it does not start inside the thrower.
    UPROPERTY(Config, EditAnywhere, Category = "Throw", meta = (ClampMin = "0")) float throwStartDistance = 60.0f;
    UPROPERTY(Config, EditAnywhere, Category = "Throw", meta = (ClampMin = "1")) float throwCollisionRadius = 20.0f;
    // A throw that has not hit anything after this many seconds lands where it is. Nothing is ever lost in flight.
    UPROPERTY(Config, EditAnywhere, Category = "Throw", meta = (ClampMin = "0.5")) float maxThrowFlightSeconds = 8.0f;
    // Doc02 19.6: a requested throw direction further than this from the pawn's view is refused (C9).
    UPROPERTY(Config, EditAnywhere, Category = "Throw", meta = (ClampMin = "1", ClampMax = "180")) float maxThrowAngleDegrees = 60.0f;
};
```

`Private/CarryFramework/UCarrySettings.cpp`

```cpp
#include "CarryFramework/UCarrySettings.h"

UCarrySettings::UCarrySettings(){
    CategoryName = TEXT("Game");
    carrySocketNames = {TEXT("carry_01"), TEXT("carry_02")};
}
```

### C0.5 — Services subsystem skeleton (startup line)

`Public/CarryFramework/CarryServices.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CarryServices.generated.h"

UCLASS()
class AWAKEN_API UCarryServices : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static UCarryServices* get(const UObject* worldContext);
};
```

`Private/CarryFramework/CarryServices.cpp`

```cpp
#include "CarryFramework/CarryServices.h"
#include "CarryFramework/CarryLog.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

void UCarryServices::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);
    CARRY_LOG(Log, this, "Carry framework started");
}

UCarryServices* UCarryServices::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    if(gameInstance == nullptr){
        return nullptr;
    }
    return gameInstance->GetSubsystem<UCarryServices>();
}
```

### Verify
Clean Editor + Development build, zero warnings. The log shows `LogAwakenCarry: [...] Carry framework started` when a game instance starts, and *Project Settings → Game → Awaken Carry* shows `carrySlotCount = 2`.

### Handoff
Folder tree, log helper, settings and the services skeleton exist. Nothing is exported to other frameworks yet.

---

# SECTION 1 — Types, Tags, Delegates and Seams

**Goal:** every shared type in one place, plus the two interfaces and their stubs.
**Spec:** B07 §5, §6, §7, §9; Doc02 §2, §20.2; Doc00 §4.2.
**Depends on:** Section 0.

### C1.1 — Enum (B07 §5)

`Public/CarryFramework/00Enums/E_CarryFramework.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "E_CarryFramework.generated.h"

// B07 §5. Why a carry ended. Drives distinct animation and effect branches.
UENUM(BlueprintType)
enum class E_EndCarryReason : uint8{
    Placed,        // used up by a build: deposited into a blueprint site or placed (Doc B01)
    Thrown,        // went into the throw pipeline (B07 3.4)
    Dropped,       // put down by the player's own choice
    Sent,          // sent along a rope or slider (Doc B09 3.11, Index A92)
    Interrupted    // a forced drop: Downed, death, swimming, sleep, leaving (B07 3.6, Index A103)
};
```

### C1.2 — Structs

`Public/CarryFramework/01Structs/S_CarryFramework.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CarryFramework/00Enums/E_CarryFramework.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "S_CarryFramework.generated.h"

class AActor;

// ---------- Carry state (B07 §7) ----------

// One carry slot. It holds exactly one heavy item, or nothing: there is no quantity (Index A103).
// Everything after itemId is the C3 addition.
USTRUCT(BlueprintType)
struct FS_CarrySlotState{
    GENERATED_BODY()

    // The item in this slot. For a carried actor it is the label the actor gives (for example Corpse).
    UPROPERTY(BlueprintReadOnly) FName itemId;
    // One id per carried thing. An item whose cost is 2 fills two slots that share this id (C4). Invalid = the slot is free.
    UPROPERTY(BlueprintReadOnly) FGuid carryId;
    // Pickup order. The occupied slot with the highest number is the active slot (C5).
    UPROPERTY(BlueprintReadOnly) int32 sequence = 0;
    // True when the slot holds a world actor (a corpse, a wounded ally) and not an item (C10).
    UPROPERTY(BlueprintReadOnly) bool holdsActor = false;
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> carriedActor = nullptr;
    // Only meaningful once the slot is free again: why its last carry ended. A client reads these two
    // to raise Carry.Ended from replicated state (B07 §9).
    UPROPERTY(BlueprintReadOnly) E_EndCarryReason lastEndReason = E_EndCarryReason::Dropped;
    UPROPERTY(BlueprintReadOnly) FGameplayTag lastInterruptCause;

    bool isOccupied() const{
        return carryId.IsValid();
    }
};

// ---------- Command results ----------

USTRUCT(BlueprintType)
struct FS_CarryBeginResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FGameplayTag failReason;
    // The primary slot of the carried thing (of the last one, when several units were taken at once).
    UPROPERTY(BlueprintReadOnly) int32 slotIndex = INDEX_NONE;

    bool succeeded() const{
        return !failReason.IsValid();
    }
};

USTRUCT(BlueprintType)
struct FS_CarryEndResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FGameplayTag failReason;
    // The item that left the hands. For Placed, Sent and Thrown the caller now owns it (C6).
    UPROPERTY(BlueprintReadOnly) FS_InventoryEntry entry;
    // Set instead of entry when the slot held an actor.
    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> actor = nullptr;

    bool succeeded() const{
        return !failReason.IsValid();
    }
};

// ---------- Message payloads (B07 §9, Doc02 §11) ----------

// Carry.Began
USTRUCT(BlueprintType)
struct FS_CarryBeganMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> carrier = nullptr;
    UPROPERTY(BlueprintReadOnly) FName itemId;
    UPROPERTY(BlueprintReadOnly) int32 slotIndex = INDEX_NONE;
};

// Carry.Ended
USTRUCT(BlueprintType)
struct FS_CarryEndedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> carrier = nullptr;
    UPROPERTY(BlueprintReadOnly) int32 slotIndex = INDEX_NONE;
    UPROPERTY(BlueprintReadOnly) E_EndCarryReason reason = E_EndCarryReason::Dropped;
    UPROPERTY(BlueprintReadOnly) FName itemId;
    // Empty unless reason is Interrupted (C14).
    UPROPERTY(BlueprintReadOnly) FGameplayTag interruptCause;
};
```

> The payloads carry the `carrier` because a Gameplay Message is heard by every listener on the machine; animation filters by "is this my character". The spec's payloads are (ItemID, SlotIndex) and (SlotIndex, Reason); the extra fields are additions, not changes.

### C1.3 — Tags

`Public/CarryFramework/02Tags/T_CarryFramework.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

// ---- Messages (B07 §9; both are in the Tag Registry) ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_Began);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_Ended);

// ---- State (B07 §6): present while any carry slot is occupied ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_State_Active);

// ---- Weight classes (B07 §6). Heavy = a carryCapacityCost above 0; Light = a cost of 0 ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_WeightClass_Light);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_WeightClass_Heavy);

// ---- Cosmetic events (B07 amendment A89; the Tag Registry names B07 as their owner) ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Carry_Picked);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Carry_Dropped);

// ---- Refusal reasons (Doc02 §6). All four are NEW and PROPOSED: add them to the Tag Registry (C21) ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_FailReason_NotCarryable);   // the item has no carry cost, or the actor is not carryable
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_FailReason_SlotEmpty);      // nothing is carried in that slot
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_FailReason_CannotThrow);    // a carried actor can only be put down (C10)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_FailReason_InvalidThrow);   // the requested direction failed the host's check (C9)

// ---- Interrupt causes (C14). All NEW and PROPOSED: add them to the Tag Registry ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_InterruptCause_Downed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_InterruptCause_Death);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_InterruptCause_Restrained);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_InterruptCause_PlayerLeft);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_InterruptCause_Swim);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_InterruptCause_Sleep);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Carry_InterruptCause_PawnRemoved);   // the safety net of C13

// ---- Tags OWNED BY ANOTHER FRAMEWORK, referenced here (see the stub register, C10.8) ----
// TEMPORARY duplicates: remove the definitions here when Doc R04's tag header exists and include that header instead.
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_Dodge);          // Doc R04
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_Sprint);         // Doc R04
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_Block);          // Doc R04
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_WeaponCharge);   // Doc R04
// Action.Reload is already declared in R01's tag header (T_Action_Reload).

namespace CarryTags{
    // Query. The actions a beginning carry ends (B07 3.2, R04-R08-IG25): Dodge, Sprint, Block, Reload, Weapon Charge.
    AWAKEN_API TArray<FGameplayTag> getActionsEndedByCarry();
}
```

`Private/CarryFramework/02Tags/T_CarryFramework.cpp`

```cpp
#include "CarryFramework/02Tags/T_CarryFramework.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"

UE_DEFINE_GAMEPLAY_TAG(T_Carry_Began, "Carry.Began");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_Ended, "Carry.Ended");

UE_DEFINE_GAMEPLAY_TAG(T_Carry_State_Active, "Carry.State.Active");

UE_DEFINE_GAMEPLAY_TAG(T_Carry_WeightClass_Light, "Carry.WeightClass.Light");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_WeightClass_Heavy, "Carry.WeightClass.Heavy");

UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Carry_Picked, "Cosmetic.Event.Carry.Picked");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Carry_Dropped, "Cosmetic.Event.Carry.Dropped");

UE_DEFINE_GAMEPLAY_TAG(T_Carry_FailReason_NotCarryable, "Carry.FailReason.NotCarryable");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_FailReason_SlotEmpty, "Carry.FailReason.SlotEmpty");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_FailReason_CannotThrow, "Carry.FailReason.CannotThrow");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_FailReason_InvalidThrow, "Carry.FailReason.InvalidThrow");

UE_DEFINE_GAMEPLAY_TAG(T_Carry_InterruptCause_Downed, "Carry.InterruptCause.Downed");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_InterruptCause_Death, "Carry.InterruptCause.Death");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_InterruptCause_Restrained, "Carry.InterruptCause.Restrained");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_InterruptCause_PlayerLeft, "Carry.InterruptCause.PlayerLeft");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_InterruptCause_Swim, "Carry.InterruptCause.Swim");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_InterruptCause_Sleep, "Carry.InterruptCause.Sleep");
UE_DEFINE_GAMEPLAY_TAG(T_Carry_InterruptCause_PawnRemoved, "Carry.InterruptCause.PawnRemoved");

UE_DEFINE_GAMEPLAY_TAG(T_Action_Dodge, "Action.Dodge");
UE_DEFINE_GAMEPLAY_TAG(T_Action_Sprint, "Action.Sprint");
UE_DEFINE_GAMEPLAY_TAG(T_Action_Block, "Action.Block");
UE_DEFINE_GAMEPLAY_TAG(T_Action_WeaponCharge, "Action.WeaponCharge");

TArray<FGameplayTag> CarryTags::getActionsEndedByCarry(){
    return {T_Action_Dodge, T_Action_Sprint, T_Action_Block, T_Action_Reload, T_Action_WeaponCharge};
}
```

### C1.4 — Delegate

`Public/CarryFramework/04Delegates/D_CarryFramework.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "D_CarryFramework.generated.h"

class AActor;

// Raised on every machine that holds the state, after any change to the carry slots of this character.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FD_CarryChanged, AActor*, carrier);
```

> Same caveat as R01's `D_ItemInventoryFramework.h`: if Unreal Header Tool rejected a header that holds only delegates there, apply here whatever fix you settled on for that file.

### C1.5 — Interfaces

Two interfaces, one file each in `Public/CarryFramework/03Interfaces/`.

`BPI_CarryableActor.h` — **incoming.** A world actor that can be carried whole implements it: the corpse of Doc B08 and the wounded ally of Doc B12 (C10).

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CarryFramework/00Enums/E_CarryFramework.h"
#include "BPI_CarryableActor.generated.h"

class AActor;

UINTERFACE(BlueprintType)
class UBPI_CarryableActor : public UInterface{
    GENERATED_BODY()
};

class IBPI_CarryableActor{
    GENERATED_BODY()

public:
    // How many carry slots the actor fills. 0 or less = not carryable right now (for example mid-harvest).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    int32 getCarrySlotCost();

    // The name shown in logs, messages and the debug view in place of an item id (for example Corpse).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FName getCarryLabel();

    // Server. The actor is now attached to the carrier. Turn physics and collision off, pause the despawn timer,
    // refuse a harvest (Doc B08 3.2, 3.4).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void onCarryBegan(AActor* carrier);

    // Server. The actor has been detached and moved to where it was put down. Turn physics and collision back on,
    // let the despawn timer run again: it is an ordinary corpse where it lies (Index A93, UD11).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void onCarryEnded(AActor* carrier, E_EndCarryReason reason);
};
```

`BPI_ThrowImpactSink.h` — **outgoing.** Owner: Doc R03 Combat (C8).

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "BPI_ThrowImpactSink.generated.h"

class AActor;

UINTERFACE(BlueprintType)
class UBPI_ThrowImpactSink : public UInterface{
    GENERATED_BODY()
};

class IBPI_ThrowImpactSink{
    GENERATED_BODY()

public:
    // Server. A thrown item hit target. Doc R03 runs its normal damage order (a thrown item is ATTACK damage,
    // R03 3.9) with thrower as the instigator. It may change the entry (a thrown weapon loses one use, R03 3.5).
    // Returns false when the item must NOT land as a loose item (a weapon that reached zero durability breaks).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool applyThrowImpact(AActor* target, AActor* thrower, UPARAM(ref) FS_InventoryEntry& entry, float damage, FVector location);
};
```

### C1.6 — Stub implementations

`Private/CarryFramework/Stubs/CarryStubs.h`

```cpp
// TEMPORARY STUBS — the owner of each is named on its class. Delete a class when the real framework lands.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "UObject/Object.h"
#include "CarryFramework/03Interfaces/BPI_CarryableActor.h"
#include "CarryFramework/03Interfaces/BPI_ThrowImpactSink.h"
#include "CarryStubs.generated.h"

// TEMPORARY STUB — owner: Doc R03 Combat. Remembers the hits; hurts nobody; the item always lands.
UCLASS()
class UStubThrowImpactSink : public UObject, public IBPI_ThrowImpactSink{
    GENERATED_BODY()

public:
    UPROPERTY() TArray<TObjectPtr<AActor>> hitTargets;
    UPROPERTY() float totalDamage = 0.0f;

    virtual bool applyThrowImpact_Implementation(AActor* target, AActor* thrower, FS_InventoryEntry& entry, float damage, FVector location) override;
};

// TEMPORARY STUB — owners: Doc B08 (the corpse) and Doc B12 (the wounded ally). A carryable dummy for tests and the dev command.
UCLASS()
class AStubCarryableActor : public AActor, public IBPI_CarryableActor{
    GENERATED_BODY()

public:
    AStubCarryableActor();

    UPROPERTY(EditAnywhere) int32 slotCost = 1;
    UPROPERTY() int32 beganCount = 0;
    UPROPERTY() int32 endedCount = 0;
    UPROPERTY() E_EndCarryReason lastReason = E_EndCarryReason::Dropped;

    virtual int32 getCarrySlotCost_Implementation() override;
    virtual FName getCarryLabel_Implementation() override;
    virtual void onCarryBegan_Implementation(AActor* carrier) override;
    virtual void onCarryEnded_Implementation(AActor* carrier, E_EndCarryReason reason) override;
};
```

`Private/CarryFramework/Stubs/CarryStubs.cpp`

```cpp
// TEMPORARY STUBS — see CarryStubs.h for the owner of each.
#include "CarryFramework/Stubs/CarryStubs.h"
#include "Components/SceneComponent.h"

bool UStubThrowImpactSink::applyThrowImpact_Implementation(AActor* target, AActor* thrower, FS_InventoryEntry& entry, float damage, FVector location){
    hitTargets.Add(target);
    totalDamage += damage;
    return true;
}

AStubCarryableActor::AStubCarryableActor(){
    bReplicates = true;
    SetReplicatingMovement(true);
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

int32 AStubCarryableActor::getCarrySlotCost_Implementation(){
    return slotCost;
}

FName AStubCarryableActor::getCarryLabel_Implementation(){
    return TEXT("Dummy");
}

void AStubCarryableActor::onCarryBegan_Implementation(AActor* carrier){
    beganCount++;
}

void AStubCarryableActor::onCarryEnded_Implementation(AActor* carrier, E_EndCarryReason reason){
    endedCount++;
    lastReason = reason;
}
```

### C1.7 — Fill `UCarryServices`

Replace the Section 0 skeleton. Two design points:

- **One seam registry for the whole project.** Carry keeps no map of its own; it uses R01's `UInventoryServices`, like the R05 guide does. A teammate replaces a stub with one line: `UInventoryServices::get(world)->registerSeam(UBPI_ThrowImpactSink::StaticClass(), myCombatObject);`
- **This class will be the real `IBPI_CarryService`** that R01 has been calling through a stub. That part is added in Section 4.

`CarryServices.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "CarryServices.generated.h"

class AActor;
class UInventoryServices;

UCLASS()
class AWAKEN_API UCarryServices : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static UCarryServices* get(const UObject* worldContext);

    // ---- facade (Command) ----
    // Doc R03. Returns false when the item must not land.
    bool applyThrowImpact(AActor* target, AActor* thrower, FS_InventoryEntry& entry, float damage, const FVector& location) const;
    // Doc B21. Does nothing while no cosmetic sink is registered.
    void raiseCosmeticEvent(const FGameplayTag& eventTag, const FVector& location, AActor* instigator) const;

private:
    UInventoryServices* getRegistry() const;
};
```

`CarryServices.cpp`

```cpp
#include "CarryFramework/CarryServices.h"
#include "CarryFramework/CarryLog.h"
#include "CarryFramework/03Interfaces/BPI_ThrowImpactSink.h"
#include "CarryFramework/Stubs/CarryStubs.h"
#include "EquipmentFramework/03Interfaces/BPI_CosmeticEventSink.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

void UCarryServices::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);
    // R01's services must exist first: its Initialize registers its own stubs, and Section 4 replaces one of them.
    Collection.InitializeDependency<UInventoryServices>();

    UInventoryServices* registry = getRegistry();
    if(registry == nullptr){
        CARRY_LOG(Error, this, "Carry framework: UInventoryServices is missing");
        return;
    }
    // Default the outgoing seam to its stub, unless a real framework registered first.
    if(registry->getSeam(UBPI_ThrowImpactSink::StaticClass()) == nullptr){
        registry->registerSeam(UBPI_ThrowImpactSink::StaticClass(), NewObject<UStubThrowImpactSink>(this));
    }
    CARRY_LOG(Log, this, "Carry framework started");
}

UCarryServices* UCarryServices::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    if(gameInstance == nullptr){
        return nullptr;
    }
    return gameInstance->GetSubsystem<UCarryServices>();
}

UInventoryServices* UCarryServices::getRegistry() const{
    const UGameInstance* gameInstance = GetGameInstance();
    return gameInstance != nullptr ? gameInstance->GetSubsystem<UInventoryServices>() : nullptr;
}

bool UCarryServices::applyThrowImpact(AActor* target, AActor* thrower, FS_InventoryEntry& entry, float damage, const FVector& location) const{
    const UInventoryServices* registry = getRegistry();
    UObject* sink = registry != nullptr ? registry->getSeam(UBPI_ThrowImpactSink::StaticClass()) : nullptr;
    if(sink == nullptr){
        return true;   // nobody to tell: the item simply lands
    }
    return IBPI_ThrowImpactSink::Execute_applyThrowImpact(sink, target, thrower, entry, damage, location);
}

void UCarryServices::raiseCosmeticEvent(const FGameplayTag& eventTag, const FVector& location, AActor* instigator) const{
    const UInventoryServices* registry = getRegistry();
    UObject* sink = registry != nullptr ? registry->getSeam(UBPI_CosmeticEventSink::StaticClass()) : nullptr;
    if(sink != nullptr){
        IBPI_CosmeticEventSink::Execute_raiseCosmeticEvent(sink, eventTag, location, instigator, 1.0f);
    }
}
```

### Verify (Section 1)

1. **Compile** Editor + Development. Zero warnings.
2. **Tags:** *Project Settings → Gameplay Tags*, search `Carry.` — the two messages, the state tag, two weight classes, four fail reasons and seven interrupt causes show with *Native* source. `Cosmetic.Event.Carry.Picked` and `.Dropped` show as well.
3. **Seam:** a breakpoint (or a temporary log) after `Initialize`: `getSeam(UBPI_ThrowImpactSink::StaticClass())` is not null.
4. **Default-value test (tier 1):** a fresh `FS_CarrySlotState` answers `isOccupied() == false`; `CarryTags::getActionsEndedByCarry()` has five entries and contains `Action.Reload`.

### Handoff
- Every type, tag and interface exists.
- Tell the R03 owner: implement `IBPI_ThrowImpactSink` and register it. Tell the B08 and B12 owners: the corpse and the wounded NPC implement `IBPI_CarryableActor` (Section 8). Tell the R04 owner: the four `Action.*` tags here are temporary duplicates of theirs.

---

# SECTION 2 — Item Data for Heavy Items

**Goal:** a heavy item is a data asset, never code, and bad data is caught in the editor.
**Spec:** B07 3.1, §6, §8, amendments A92 and A103; R01 3.1, 3.7 step 0, 3.10; Doc B22 (the data checks).
**Depends on:** Section 1.

### What R01 already holds

The R01 guide (S2.5) already put the two spec fields on `UDA_ItemDefinition`: `carryCapacityCost` (0 = an ordinary item) and `weightClassTag`, plus the helper `isHeavy()`. R01's own data check already says a heavy item needs no bag limit and no bag slot, because it never enters the bag. This section adds one field and the checks that are Carry's.

### Files

```
Public/ItemInventoryFramework/UDA_ItemDefinition.h      (patch — you own R01)
Private/ItemInventoryFramework/UDA_ItemDefinition.cpp   (patch)
Private/ItemInventoryFramework/Tests/InventoryTestFixture.h/.cpp   (extend)
```

### C2.1 — One new field in the Carry block (C8)

In `UDA_ItemDefinition.h`, directly under the `weightClassTag` line:

```cpp
    // Damage of this item when it is thrown and hits something (B07 3.4: "a heavy blunt impact"). PROPOSED name.
    // For a Thrown WEAPON Doc R03 passes the weapon's own damage instead and ignores this field (R03 3.2).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry", meta = (ClampMin = "0")) float throwImpactDamage = 0.0f;
```

### C2.2 — Data validation

In `UDA_ItemDefinition::IsDataValid`, before `return result;` (add `#include "CarryFramework/02Tags/T_CarryFramework.h"` and `#include "CarryFramework/UCarrySettings.h"` at the top of the `.cpp`):

```cpp
    if(isHeavy()){
        if(weightClassTag != T_Carry_WeightClass_Heavy){
            fail(TEXT("An item with a carry cost must have weightClassTag = Carry.WeightClass.Heavy (B07 §6)."));
        }
        if(isPermanent()){
            fail(TEXT("The permanent Axe and the map are never carry items (B07 amendment A92, R01 3.10): carryCapacityCost must be 0."));
        }
        if(isPortableContainer()){
            fail(TEXT("A Portable Container cannot be a heavy item: a carried item is dropped loose, and a container that holds something can never be loose (R01 3.4a)."));
        }
        if(carryCapacityCost > UCarrySettings::get()->carrySlotCount){
            Context.AddWarning(FText::FromString(FString::Printf(TEXT("carryCapacityCost is %d but there are only %d carry slots: this item can never be picked up (B07 3.1)."), carryCapacityCost, UCarrySettings::get()->carrySlotCount)));
        }
        if(worldMesh.IsNull()){
            Context.AddWarning(FText::FromString(TEXT("A heavy item has no worldMesh, so nothing shows in the hands or on the ground.")));
        }
    }else{
        if(weightClassTag.IsValid() && weightClassTag != T_Carry_WeightClass_Light){
            fail(TEXT("An item with no carry cost is Carry.WeightClass.Light, or has no weight class at all (B07 §6)."));
        }
        if(throwImpactDamage > 0.0f && !isWeapon()){
            Context.AddWarning(FText::FromString(TEXT("throwImpactDamage is set on an item that is neither heavy nor a weapon. It is used only if something throws this item.")));
        }
    }
```

> Doc B22 adds two more checks that belong to **other** owners and are not built here: a recipe ingredient may not name an item with a carry cost (Doc R02), and the manual cost of a building piece may not either (Doc B01). Both exist because a heavy item can never be staged or taken from the bag.

### C2.3 — Author the starter content (Blueprint / editor)

Create or update these `DA_ItemDefinition` assets in `Content/Awaken/Items/` (the numbers are placeholders for testing, not design):

| Asset | Kind | Category | Fields |
| --- | --- | --- | --- |
| `Log` (exists from R01 S2.14) | Stackable | Material | `carryCapacityCost = 1`, `weightClassTag = Carry.WeightClass.Heavy`, `throwImpactDamage = 15`, a `worldMesh` |
| `Stone` | Stackable | Material | `carryCapacityCost = 1`, `weightClassTag = Carry.WeightClass.Heavy`, `throwImpactDamage = 20`, a `worldMesh` |
| `Trunk` | Stackable | Material | `carryCapacityCost = 2`, `weightClassTag = Carry.WeightClass.Heavy`, `throwImpactDamage = 25`, a `worldMesh` (the fallen trunk of Doc B08: it fills both hands) |
| `Rock` | Stackable | Material | `carryCapacityCost = 0`. The recipe ingredient of Doc R02: an **ordinary** item, unlike the heavy `Stone` (B07 3.1) |

Then, on the character skeleton (or the placeholder mannequin), add two sockets named `carry_01` and `carry_02`, somewhere in front of the chest. If a socket is missing the mesh is drawn at the character's root, which is ugly but harmless.

### C2.4 — Extend the test fixture

`InventoryTestFixture.h`, inside the struct:

```cpp
    // A heavy item: it has a carry cost and never enters the bag (B07 3.1).
    UDA_ItemDefinition* addHeavy(FName itemId, int32 carryCost, float throwDamage = 0.0f);
```

`InventoryTestFixture.cpp` (add `#include "CarryFramework/02Tags/T_CarryFramework.h"`):

```cpp
UDA_ItemDefinition* FInventoryTestFixture::addHeavy(FName itemId, int32 carryCost, float throwDamage){
    UDA_ItemDefinition* definition = addStackable(itemId, 1);
    definition->carryCapacityCost = carryCost;
    definition->weightClassTag = T_Carry_WeightClass_Heavy;
    definition->throwImpactDamage = throwDamage;
    return definition;
}
```

### Verify (Section 2)

1. Compile. Open `Log`: the **Carry** category shows `carryCapacityCost`, `weightClassTag` and the new `throwImpactDamage`.
2. Run `Awaken.Item.Validate` (R01 S13): zero errors on the four assets.
3. Break one on purpose: set `Log.weightClassTag` to `Light` → "must have weightClassTag = Carry.WeightClass.Heavy". Set `Trunk.carryCapacityCost = 3` → the warning "can never be picked up". Give the starter `Axe` a cost of 1 → "never carry items". Undo all three.

### Handoff
- New data field to log in the Index: `throwImpactDamage`.
- **R02** and **B01**: the two data checks named above are yours.

---

# SECTION 3 — The Carry Component (state, replication, queries, the state tag)

**Goal:** the carry slots exist on the Character, replicate to everyone, answer every query, and every machine raises the two messages and draws the carried item.
**Spec:** B07 §2, 3.1, 3.2, §4.1 (queries), §9, §10; Doc B19 3.3; Doc02 §8, §19.2; Doc R07 3.3, 3.5.
**Depends on:** Sections 0–2.

### Files

```
Public/CarryFramework/UCarryComponent.h
Private/CarryFramework/UCarryComponent.cpp
Private/ItemInventoryFramework/Stubs/AInventoryTestCharacter.h/.cpp   (patch)
```

### Design rules this section enforces

- **The component lives on the Character** (B07 §2, Doc B19 3.3). Carry state is about a body: it ends when that body can no longer hold the item. A new pawn after a respawn starts with empty hands.
- **Server only mutates.** Every command begins with the server check. A client holds a replicated copy and nothing else.
- **Replicated to everyone** (`COND_None`, B07 §10 and Doc02 §8): other co-op players must see the carry pose and understand why this player is not fighting.
- **Messages are local** (Doc02 §19.2). The host raises a message right after it changes the state; a client raises the same message when the replicated state arrives. A late joiner gets the state from replication, never from a message.
- **Never saved** (B07 3.5, Doc00 §9). This component is not a save participant and has no save code at all. After a load every hand is empty.
- **No tick.** Everything here happens in a command or a replication notify.

### C3.1 to C3.7 — `UCarryComponent`

`UCarryComponent.h` (Section 3 version; later sections add members as marked)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CarryFramework/01Structs/S_CarryFramework.h"
#include "CarryFramework/04Delegates/D_CarryFramework.h"
#include "UCarryComponent.generated.h"

class APlayerState;
class UStaticMeshComponent;

// B07 §2: the carry slots of one character. A Character component (Doc B19 3.3). Transient: never saved (B07 3.5).
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class AWAKEN_API UCarryComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UCarryComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Query. The component on that character. A PlayerState is accepted too and means its current pawn. Null when missing.
    static UCarryComponent* findOn(const AActor* carrier);
    // Query. The component on the player's current pawn, or null.
    static UCarryComponent* findFor(const APlayerState* player);

    // ---- queries (B07 §4.1) ----
    // B07 IsCarryActive(): a carry slot is occupied. Combat, Abilities, Animation and Interaction all read this.
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") bool isCarryActive() const;
    // B07 GetCarriedItemsForCost(ItemID): how many units of that item are in the hands. Building reads it first (Doc B01 3.4).
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") int32 getCarriedItemsForCost(FName itemId) const;
    const TArray<FS_CarrySlotState>& getCarrySlots() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") TArray<FS_CarrySlotState> getCarrySlotsCopy() const;
    // The number of slots (tuning data, B07 3.1).
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") int32 getSlotCount() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") int32 getFreeSlotCount() const;
    // The primary slot of the thing picked up last, or INDEX_NONE when nothing is carried (C5).
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") int32 getActiveSlotIndex() const;
    // True when that slot holds a world actor (a corpse) and not an item. Doc B09 refuses to send one.
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") bool isActorCarry(int32 slotIndex) const;
    // For Doc R07's speed stack: the carry slowdown while carrying, otherwise 1 (C15).
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") float getSpeedMultiplier() const;
    // Why the last forced drop happened. Empty until one has happened on this character. For the debug view (Doc B22 3.5).
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") FGameplayTag getLastInterruptCause() const;

    // Raised on every machine after any change to the slots.
    UPROPERTY(BlueprintAssignable, Category = "Awaken|Carry") FD_CarryChanged onCarryChanged;

protected:
    // Replicated to everyone (B07 §10, Doc02 §8).
    UPROPERTY(ReplicatedUsing = OnRep_carrySlots) TArray<FS_CarrySlotState> carrySlots;

    UFUNCTION()
    void OnRep_carrySlots(const TArray<FS_CarrySlotState>& previousSlots);

    bool isServerSide() const;
    // The PlayerState of the carrier, or null for an NPC.
    APlayerState* getCarrierPlayerState() const;

    // ---- slot helpers ----
    // Server. Makes the array as long as the slot count. It never shrinks, so lowering the count takes nothing away.
    void ensureSlots();
    // Server. Fills `cost` free slots with one carried thing and returns its primary slot. The caller has checked the room.
    int32 fillSlots(const FGuid& carryId, FName itemId, int32 cost, AActor* actor);
    // True when no lower slot holds the same carried thing.
    static bool isPrimarySlot(const TArray<FS_CarrySlotState>& slots, int32 slotIndex);

    // ---- the state tag (B07 3.2) ----
    // Server. Adds or removes Carry.State.Active on the carrier's Ability System Component as a replicated loose tag (C11).
    void applyCarryStateTag(bool isActive) const;

    // ---- messages and visuals: run on every machine ----
    void raiseBegan(int32 slotIndex, FName itemId) const;
    void raiseEnded(int32 slotIndex, FName itemId, E_EndCarryReason reason, const FGameplayTag& interruptCause);
    // Rebuilds the meshes shown in the hands from the slots (C2).
    void refreshVisuals();
    // After any change, on the server: replicate now, redraw, notify.
    void finishChange();

    static FS_CarryBeginResult refusedBegin(const FGameplayTag& reason);
    static FS_CarryEndResult refusedEnd(const FGameplayTag& reason);

    // Server only, never replicated: the full entry of each carried item, by carryId (C2).
    // It keeps what the slot does not show: a spoil time, an instance id, a durability.
    UPROPERTY() TMap<FGuid, FS_InventoryEntry> carriedEntries;
    int32 nextSequence = 0;
    FGameplayTag lastInterruptCause;

    // Local on each machine: one mesh per slot, made on first use. Never replicated.
    UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> slotVisuals;
};
```

`UCarryComponent.cpp`

```cpp
#include "CarryFramework/UCarryComponent.h"
#include "CarryFramework/CarryLog.h"
#include "CarryFramework/CarryServices.h"
#include "CarryFramework/UCarrySettings.h"
#include "CarryFramework/02Tags/T_CarryFramework.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

#if !UE_BUILD_SHIPPING
// Tests only: -1 = use the project setting. 0 = hands that can take nothing (every heavy pickup is refused).
static TAutoConsoleVariable<int32> cvarSlotCount(TEXT("Awaken.Carry.SlotCount"), -1, TEXT("Override carrySlotCount (tests only). -1 = use the project setting."), ECVF_Cheat);
#endif

// ---------- construction and replication ----------

UCarryComponent::UCarryComponent(){
    SetIsReplicatedByDefault(true);
}

void UCarryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCarryComponent, carrySlots);   // COND_None: everyone sees who carries what (B07 §10)
}

UCarryComponent* UCarryComponent::findOn(const AActor* carrier){
    if(carrier == nullptr){
        return nullptr;
    }
    const APlayerState* player = Cast<APlayerState>(carrier);
    if(player != nullptr){
        return findFor(player);
    }
    return carrier->FindComponentByClass<UCarryComponent>();
}

UCarryComponent* UCarryComponent::findFor(const APlayerState* player){
    const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
    return pawn != nullptr ? pawn->FindComponentByClass<UCarryComponent>() : nullptr;
}

bool UCarryComponent::isServerSide() const{
    return GetOwner() != nullptr && GetOwner()->HasAuthority();
}

APlayerState* UCarryComponent::getCarrierPlayerState() const{
    const APawn* pawn = Cast<APawn>(GetOwner());
    return pawn != nullptr ? pawn->GetPlayerState() : nullptr;
}

// ---------- queries ----------

bool UCarryComponent::isCarryActive() const{
    for(const FS_CarrySlotState& slot : carrySlots){
        if(slot.isOccupied()){
            return true;
        }
    }
    return false;
}

int32 UCarryComponent::getCarriedItemsForCost(FName itemId) const{
    int32 count = 0;
    for(int32 index = 0; index < carrySlots.Num(); index++){
        const FS_CarrySlotState& slot = carrySlots[index];
        // One carried thing counts once, however many slots it fills. A carried actor is never a building material.
        if(slot.isOccupied() && !slot.holdsActor && slot.itemId == itemId && isPrimarySlot(carrySlots, index)){
            count++;
        }
    }
    return count;
}

const TArray<FS_CarrySlotState>& UCarryComponent::getCarrySlots() const{
    return carrySlots;
}

TArray<FS_CarrySlotState> UCarryComponent::getCarrySlotsCopy() const{
    return carrySlots;
}

int32 UCarryComponent::getSlotCount() const{
#if !UE_BUILD_SHIPPING
    const int32 overrideCount = cvarSlotCount.GetValueOnGameThread();
    if(overrideCount >= 0){
        return overrideCount;
    }
#endif
    return UCarrySettings::get()->carrySlotCount;
}

int32 UCarryComponent::getFreeSlotCount() const{
    int32 occupied = 0;
    for(const FS_CarrySlotState& slot : carrySlots){
        if(slot.isOccupied()){
            occupied++;
        }
    }
    return FMath::Max(0, getSlotCount() - occupied);
}

int32 UCarryComponent::getActiveSlotIndex() const{
    int32 activeIndex = INDEX_NONE;
    int32 highestSequence = -1;
    for(int32 index = 0; index < carrySlots.Num(); index++){
        // Strictly greater, so of two slots that share a carried thing the lower one (the primary) wins.
        if(carrySlots[index].isOccupied() && carrySlots[index].sequence > highestSequence){
            highestSequence = carrySlots[index].sequence;
            activeIndex = index;
        }
    }
    return activeIndex;
}

bool UCarryComponent::isActorCarry(int32 slotIndex) const{
    return carrySlots.IsValidIndex(slotIndex) && carrySlots[slotIndex].isOccupied() && carrySlots[slotIndex].holdsActor;
}

float UCarryComponent::getSpeedMultiplier() const{
    return isCarryActive() ? UCarrySettings::get()->carrySpeedMultiplier : 1.0f;
}

FGameplayTag UCarryComponent::getLastInterruptCause() const{
    return lastInterruptCause;
}

// ---------- slot helpers ----------

void UCarryComponent::ensureSlots(){
    if(carrySlots.Num() < getSlotCount()){
        carrySlots.SetNum(getSlotCount());
    }
}

int32 UCarryComponent::fillSlots(const FGuid& carryId, FName itemId, int32 cost, AActor* actor){
    ensureSlots();
    nextSequence++;
    int32 primaryIndex = INDEX_NONE;
    int32 filled = 0;
    for(int32 index = 0; index < carrySlots.Num() && filled < cost; index++){
        FS_CarrySlotState& slot = carrySlots[index];
        if(slot.isOccupied()){
            continue;
        }
        slot.itemId = itemId;
        slot.carryId = carryId;
        slot.sequence = nextSequence;
        slot.holdsActor = actor != nullptr;
        slot.carriedActor = actor;
        if(primaryIndex == INDEX_NONE){
            primaryIndex = index;
        }
        filled++;
    }
    return primaryIndex;
}

bool UCarryComponent::isPrimarySlot(const TArray<FS_CarrySlotState>& slots, int32 slotIndex){
    if(!slots.IsValidIndex(slotIndex) || !slots[slotIndex].isOccupied()){
        return false;
    }
    for(int32 index = 0; index < slotIndex; index++){
        if(slots[index].carryId == slots[slotIndex].carryId){
            return false;
        }
    }
    return true;
}

// ---------- the state tag (B07 3.2, R04-R08-IG23) ----------

void UCarryComponent::applyCarryStateTag(bool isActive) const{
    // A player's Ability System Component is on the PlayerState (decision D5); an NPC's is on the pawn.
    // The normal GAS setup lets the pawn answer for both. Until Doc R04 / R08 exist there is none: then nothing happens here,
    // and readers use isCarryActive() directly.
    UAbilitySystemComponent* abilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
    if(abilitySystem == nullptr){
        abilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(getCarrierPlayerState());
    }
    if(abilitySystem == nullptr){
        return;
    }
    if(isActive){
        abilitySystem->AddLooseGameplayTag(T_Carry_State_Active);             // the host's own gates read this one
        abilitySystem->AddReplicatedLooseGameplayTag(T_Carry_State_Active);   // and this one reaches the clients
    }else{
        abilitySystem->RemoveLooseGameplayTag(T_Carry_State_Active);
        abilitySystem->RemoveReplicatedLooseGameplayTag(T_Carry_State_Active);
    }
}

// ---------- messages (B07 §9, Doc02 §19.2: local on every machine) ----------

void UCarryComponent::raiseBegan(int32 slotIndex, FName itemId) const{
    FS_CarryBeganMessage message;
    message.carrier = GetOwner();
    message.itemId = itemId;
    message.slotIndex = slotIndex;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Carry_Began, message);
}

void UCarryComponent::raiseEnded(int32 slotIndex, FName itemId, E_EndCarryReason reason, const FGameplayTag& interruptCause){
    if(reason == E_EndCarryReason::Interrupted){
        lastInterruptCause = interruptCause;
    }
    FS_CarryEndedMessage message;
    message.carrier = GetOwner();
    message.slotIndex = slotIndex;
    message.reason = reason;
    message.itemId = itemId;
    message.interruptCause = interruptCause;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Carry_Ended, message);
}

// A client learns of a change only here. The previous value lets it tell what began and what ended.
void UCarryComponent::OnRep_carrySlots(const TArray<FS_CarrySlotState>& previousSlots){
    refreshVisuals();
    for(int32 index = 0; index < carrySlots.Num(); index++){
        const FS_CarrySlotState before = previousSlots.IsValidIndex(index) ? previousSlots[index] : FS_CarrySlotState();
        const FS_CarrySlotState& now = carrySlots[index];
        if(before.carryId == now.carryId){
            continue;
        }
        if(isPrimarySlot(previousSlots, index)){
            // The freed slot kept why it was freed, so the client can raise the same message as the host.
            raiseEnded(index, before.itemId, now.lastEndReason, now.lastInterruptCause);
        }
        if(isPrimarySlot(carrySlots, index)){
            raiseBegan(index, now.itemId);
        }
    }
    onCarryChanged.Broadcast(GetOwner());
}

// ---------- visuals (C2): each machine draws the carried items from the replicated slots ----------

void UCarryComponent::refreshVisuals(){
    const UCarrySettings* settings = UCarrySettings::get();
    const ACharacter* character = Cast<ACharacter>(GetOwner());
    USkeletalMeshComponent* body = character != nullptr ? character->GetMesh() : nullptr;
    if(!settings->showCarriedMeshes || body == nullptr){
        return;
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);

    // One mesh component per slot, made the first time the slot is seen. It is hidden while the slot shows nothing.
    while(slotVisuals.Num() < carrySlots.Num()){
        const int32 newIndex = slotVisuals.Num();
        const FName socketName = settings->carrySocketNames.IsValidIndex(newIndex) ? settings->carrySocketNames[newIndex] : NAME_None;
        UStaticMeshComponent* visual = NewObject<UStaticMeshComponent>(GetOwner());
        visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        visual->SetupAttachment(body, socketName);
        visual->RegisterComponent();
        visual->SetVisibility(false);
        slotVisuals.Add(visual);
    }

    for(int32 index = 0; index < carrySlots.Num(); index++){
        UStaticMeshComponent* visual = slotVisuals[index];
        const FS_CarrySlotState& slot = carrySlots[index];
        // A carried actor shows itself (it is attached to the body). A two-slot item is drawn once, at its primary slot.
        const UDA_ItemDefinition* definition = (catalog != nullptr && !slot.holdsActor && isPrimarySlot(carrySlots, index)) ? catalog->getDefinition(slot.itemId) : nullptr;
        if(definition == nullptr || definition->worldMesh.IsNull()){
            visual->SetVisibility(false);
            continue;
        }
        // The world mesh is a soft reference (Doc02 19.8): load it only now that something shows it.
        const TSoftObjectPtr<UStaticMesh> wanted = definition->worldMesh;
        const FGuid shownCarryId = slot.carryId;
        UAssetManager::GetStreamableManager().RequestAsyncLoad(wanted.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this, [this, index, shownCarryId, wanted](){
            // The slot may have changed while the mesh was loading.
            if(carrySlots.IsValidIndex(index) && carrySlots[index].carryId == shownCarryId && slotVisuals.IsValidIndex(index)){
                slotVisuals[index]->SetStaticMesh(wanted.Get());
                slotVisuals[index]->SetVisibility(true);
            }
        }));
    }
}

void UCarryComponent::finishChange(){
    if(isServerSide()){
        GetOwner()->ForceNetUpdate();   // the pose of other players should not wait for the next scheduled update
    }
    refreshVisuals();
    onCarryChanged.Broadcast(GetOwner());
}

// ---------- small helpers ----------

FS_CarryBeginResult UCarryComponent::refusedBegin(const FGameplayTag& reason){
    FS_CarryBeginResult result;
    result.failReason = reason;
    return result;
}

FS_CarryEndResult UCarryComponent::refusedEnd(const FGameplayTag& reason){
    FS_CarryEndResult result;
    result.failReason = reason;
    return result;
}
```

> **About the four GAS calls.** `AddLooseGameplayTag` / `AddReplicatedLooseGameplayTag` and their `Remove` twins are the long-standing way to do what B07 3.2 asks. If 5.8 marks the replicated pair deprecated, the compiler message names the replacement (one call that takes a replication argument); use it and nothing else changes. This is the one place in the framework that touches GAS.

> **Why `OnRep_carrySlots` takes the previous value.** Unreal passes the old array when the notify function has one parameter of the property's type. Comparing old and new by `carryId` turns one replicated array into the two local messages B07 §9 asks for. A late joiner arrives with an empty "previous", so it raises `Carry.Began` for everything that is already carried and the pose and meshes build themselves.

### C3.8 — Put the component on the Character

In the R01 stub `AInventoryTestCharacter` (owners: Docs B19 and R07; the real Character gets the same line):

```cpp
// header
class UCarryComponent;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCarryComponent> carryComponent;

// constructor (.cpp), with #include "CarryFramework/UCarryComponent.h"
    carryComponent = CreateDefaultSubobject<UCarryComponent>(TEXT("Carry"));
```

### C3.9 — Blueprint exposure (BP)

The queries are `BlueprintPure` and `onCarryChanged` is `BlueprintAssignable`. In a test Blueprint on the character: *Get Component by Class (CarryComponent) → bind `onCarryChanged`* and print `isCarryActive`.

### Verify (Section 3)

1. Compile. Play standalone with `AInventoryTestGameMode`: no errors.
2. `getSlotCount()` returns 2; after `Awaken.Carry.SlotCount 1` in the console it returns 1; after `-1` it returns 2 again.
3. `isCarryActive()` is false, `getActiveSlotIndex()` is `INDEX_NONE`, `getSpeedMultiplier()` is 1.
4. Nothing can be carried yet; that is Section 4.

### Handoff
- `UCarryComponent::findOn(character)` and `findFor(playerState)` are how every other framework reaches carry state.
- **B19 / R07** owners: add `UCarryComponent` to the real Character class (and to any NPC that may carry, Doc B12).

---

# SECTION 4 — Beginning a Carry, and the Seam R01 Calls

**Goal:** a heavy item goes into the hands, by a pickup, a harvest yield or a refund; a pickup the hands cannot take is refused and the item stays where it lies. Replace R01's `UStubCarryService` with the real thing.
**Spec:** B07 3.1, 3.2, §4.1 (`BeginCarry`), amendments A93 (UD9), A103 (S4-F01, S4-F18) and R04-R08-IG23/IG25; R01 3.7 step 0, 3.11; Doc B18 §10.
**Depends on:** Section 3, and the R01 guide's Sections 1 and 5.

### The rules, stated once

| Situation | What happens | Rule |
| --- | --- | --- |
| A heavy pickup and enough slots are free | the item fills its slots; the loose item on the ground is gone | B07 3.1 |
| A heavy pickup and the hands cannot take it (a third log, or a trunk with one hand full) | **refused** with `Interaction.FailReason.HandsFull`; the item stays where it lies | B07 3.1, Index A93 |
| A heavy **grant** (a harvest yield, a cut tree, a refund, a cancel) | into free slots, otherwise it lands loose beside the player. Never into the bag | R01 3.7 step 0, Index A103 |
| An item whose cost is larger than the number of slots | can never be picked up | B07 3.1 |
| An ordinary item (cost 0) | not Carry's business: it goes to the bag or a Bagless hold slot | R01 3.11 |
| The first slot becomes occupied | `Carry.State.Active` appears; a running Dodge, Sprint, Block, Reload or Weapon Charge ends | B07 3.2 |

### What R01 already does, and what it expects from us

R01 calls the seam `IBPI_CarryService` from three places (R01 guide S5.4, S5.5, S12.3). Its contract is two functions:

| Seam function | R01 calls it | What we must do |
| --- | --- | --- |
| `isCarryActive(player)` | in the request gate, for every request that needs free hands | answer from the component on the player's pawn |
| `beginCarry(player, entry)` | in `grantOrDrop`, `grantEntryOrDrop` and `pickUpLooseItem`, only for a heavy item | take the **whole** entry or nothing, and say which. On `true` R01 removes the loose item from the ground itself; on `false` R01 drops the grant loose, or refuses the pickup with `HandsFull` |

So the pickup path needs **no new request**: the player's interact key already reaches `serverPickUp` (R01 guide S12.4), which already skips the "free hands" check for a heavy item.

### C4.1 — `beginCarry`

Header additions (`UCarryComponent.h`, public, then protected):

```cpp
    // ---- commands (server only; each returns an explained result) ----
    // B07 §4.1 BeginCarry. Takes every unit of the entry or none. Refused with HandsFull when the free slots are too few.
    FS_CarryBeginResult beginCarry(const FS_InventoryEntry& entry);

protected:
    // What must be true of the carrier's body. Empty tag = allowed (C17).
    FGameplayTag checkCanBegin() const;
    // Server. After slots were filled: the state tag, the ended actions, the message, the cosmetic event.
    void afterBegin(int32 slotIndex, FName itemId, bool wasActive);
```

Implementation (append to `UCarryComponent.cpp`; add the includes at the top):

```cpp
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "GameFramework/CharacterMovementComponent.h"
```

```cpp
// ---------- Section 4: beginning a carry ----------

FGameplayTag UCarryComponent::checkCanBegin() const{
    const UInventoryServices* services = UInventoryServices::get(this);
    const APlayerState* player = getCarrierPlayerState();
    // Downed, Restrained, Arriving, dead or in a scene: the body cannot take anything (Doc02 19.6).
    if(services != nullptr && player != nullptr && services->isIncapacitated(player)){
        return T_Interaction_FailReason_NotAllowedInState;
    }
    // Swimming drops a carried item (B07 3.6), so a swimmer cannot begin a carry either.
    const ACharacter* character = Cast<ACharacter>(GetOwner());
    if(character != nullptr && character->GetCharacterMovement() != nullptr && character->GetCharacterMovement()->IsSwimming()){
        return T_Interaction_FailReason_NotAllowedInState;
    }
    return FGameplayTag();
}

FS_CarryBeginResult UCarryComponent::beginCarry(const FS_InventoryEntry& entry){
    if(!isServerSide()){
        return refusedBegin(T_Inventory_FailReason_NotServer);
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(entry.itemId) : nullptr;
    if(definition == nullptr || entry.totalQuantity < 1){
        return refusedBegin(T_Inventory_FailReason_InvalidRequest);
    }
    if(!definition->isHeavy()){
        return refusedBegin(T_Carry_FailReason_NotCarryable);   // an ordinary item goes to the bag, never to the hands (Index A93, UD9)
    }
    const FGameplayTag blocked = checkCanBegin();
    if(blocked.IsValid()){
        return refusedBegin(blocked);
    }
    // All or nothing. An item whose cost is larger than the number of slots can never pass this line (B07 3.1).
    const int32 neededSlots = definition->carryCapacityCost * entry.totalQuantity;
    if(neededSlots > getFreeSlotCount()){
        return refusedBegin(T_Interaction_FailReason_HandsFull);
    }

    // Everything checked. One carried thing per unit: a slot never holds a quantity (Index A103, S4-F18).
    FS_CarryBeginResult result;
    for(int32 unit = 0; unit < entry.totalQuantity; unit++){
        const bool wasActive = isCarryActive();
        FS_InventoryEntry unitEntry = entry;
        unitEntry.totalQuantity = 1;
        unitEntry.reservedQuantity = 0;
        const FGuid carryId = FGuid::NewGuid();
        carriedEntries.Add(carryId, unitEntry);
        result.slotIndex = fillSlots(carryId, entry.itemId, definition->carryCapacityCost, nullptr);
        afterBegin(result.slotIndex, entry.itemId, wasActive);
    }
    return result;
}

void UCarryComponent::afterBegin(int32 slotIndex, FName itemId, bool wasActive){
    if(!wasActive){
        // The first slot was filled: the lockout starts (B07 3.2).
        applyCarryStateTag(true);
        const UInventoryServices* services = UInventoryServices::get(this);
        APlayerState* player = getCarrierPlayerState();
        if(services != nullptr && player != nullptr){
            // A carry that begins ends a running Dodge, Sprint, Block, Reload or Weapon Charge (R04-R08-IG25).
            for(const FGameplayTag& actionTag : CarryTags::getActionsEndedByCarry()){
                services->cancelAction(player, actionTag);
            }
        }
    }
    finishChange();
    raiseBegan(slotIndex, itemId);
    const UCarryServices* carryServices = UCarryServices::get(this);
    if(carryServices != nullptr){
        carryServices->raiseCosmeticEvent(T_Cosmetic_Event_Carry_Picked, GetOwner()->GetActorLocation(), GetOwner());   // Doc B21 A.4
    }
    CARRY_LOG(Log, this, "%s began carrying %s in slot %d (%d of %d slots free)", *GetNameSafe(GetOwner()), *itemId.ToString(), slotIndex, getFreeSlotCount(), getSlotCount());
}
```

> **Who checks range, rate and line of sight?** Not this function. Like R01's library functions it is the *rule*; R01's `serverPickUp` is the *request* and has already checked them before the seam is reached. A harvest or a refund calls `grantOrDrop` on the server and needs no such check.

### C4.2 — `UCarryServices` becomes the real `IBPI_CarryService`

`CarryServices.h`: add the include, the base class and the two overrides.

```cpp
#include "ItemInventoryFramework/03Interfaces/BPI_CarryService.h"

class AWAKEN_API UCarryServices : public UGameInstanceSubsystem, public IBPI_CarryService{
    GENERATED_BODY()

public:
    // ---- IBPI_CarryService: the seam Doc R01 calls from grantOrDrop, pickUpLooseItem and the request gate ----
    virtual bool isCarryActive_Implementation(APlayerState* player) override;
    virtual bool beginCarry_Implementation(APlayerState* player, const FS_InventoryEntry& entry) override;
```

`CarryServices.cpp`: in `Initialize`, after the stub registration and before the log line:

```cpp
    // Replace R01's UStubCarryService with the real implementation (R01 guide, stub register S13.8).
    registry->registerSeam(UBPI_CarryService::StaticClass(), this);
```

and the two functions (add `#include "CarryFramework/UCarryComponent.h"`):

```cpp
bool UCarryServices::isCarryActive_Implementation(APlayerState* player){
    const UCarryComponent* carry = UCarryComponent::findFor(player);
    return carry != nullptr && carry->isCarryActive();
}

bool UCarryServices::beginCarry_Implementation(APlayerState* player, const FS_InventoryEntry& entry){
    UCarryComponent* carry = UCarryComponent::findFor(player);
    // No pawn or no component: the hands cannot take it, so R01 lets the item land loose. Nothing is lost.
    return carry != nullptr && carry->beginCarry(entry).succeeded();
}
```

Now delete `UStubCarryService` from R01's `InventoryStubs.h/.cpp` and its `registerSeam` line in `UInventoryServices::Initialize`, and tick it off R01's stub register (R01 guide S13.8).

Two small things in R01 stop working when the stub is gone, both on purpose:

- The dev command `Awaken.Inventory.StubFlag carrying …` casts the seam to the stub. Remove the `carrying` half of that command (it will not compile once the class is deleted) and keep the `downed` half. To make a player carry, use `Awaken.Carry.Give` (Section 10).
- The R01 Verify rows that said "carry stub refusing" are now reproduced with `Awaken.Carry.SlotCount 0`, and "accepting" with `-1`.

### Verify (Section 4)

Until Section 10 adds the Carry commands, R01's own `Awaken.Item.Give` is enough, because a grant of a heavy item goes through the seam.

| Test | Expect |
| --- | --- |
| Startup log | R01's line still counts its seams; a breakpoint shows `getSeam(UBPI_CarryService::StaticClass())` is the `UCarryServices` object, not a stub |
| `Awaken.Item.Give Log 1` | log line "stored 0, carried 1, dropped 0"; `isCarryActive()` true; slot 0 holds `Log`; the log's mesh shows at socket `carry_01`; `Carry.Began` raised once; the bag holds **no** log |
| `Awaken.Item.Give Log 2` on top of that | "carried 1, dropped 1": the second log is in slot 1, the third lies loose in front of the player |
| Walk to that loose log and press the pickup key | refused `Interaction.FailReason.HandsFull`; the log **stays on the ground** |
| Empty hands, `Awaken.Item.Give Trunk 1` (cost 2) | carried 1; **both** slots show `Trunk` with the same `carryId`; one mesh, at slot 0; `getCarriedItemsForCost(Trunk)` is **1** |
| One `Log` in the hands, pick up a loose `Trunk` | `HandsFull`: it needs two free slots |
| `Awaken.Carry.SlotCount 1`, empty hands, pick up a `Trunk` | `HandsFull`, always: its cost is larger than the slots |
| `beginCarry` with a `Stick` entry (cost 0) from a test | `Carry.FailReason.NotCarryable` |
| While carrying: eat food from the bag (R01's `serverUseConsumable`), or open a chest | refused `Interaction.FailReason.HandsFull` (this is R01's gate, now fed by the real seam) |
| While carrying, a loose `Stick` (ordinary), pickup | refused `HandsFull`; the stick stays |
| Bagless player (`Awaken.Bag.SetBagless`), `Awaken.Item.Give Log 1` | carried 1: carrying does not need the bag |
| Player marked Downed (`Awaken.Inventory.StubFlag downed 1`), `Awaken.Item.Give Log 1` | carried 0, dropped 1 (C17) |
| The stub action service (R01) after the first carry began | `cancelledActions` holds Dodge, Sprint, Block, Reload, WeaponCharge, once each. A second log adds nothing |
| Any `beginCarry` on a client | `Inventory.FailReason.NotServer`; nothing changes |

### Handoff
- **R01** needs nothing new: `grantOrDrop`, `pickUpLooseItem` and the gate now reach the real Carry.
- **B08** (harvest, tree cutting): a heavy yield is granted with `UBFL_Inventory::grantOrDrop` like any other; it lands in the hands or loose, never in the bag.
- **R04**: implement `IBPI_ActionService::cancelAction` for the five tags, and gate Dodge, Sprint, Block and Reload on `Carry.State.Active` (it is on the Ability System Component once that exists).
- **R03**: `IsAttackAllowed` reads `UBFL_Carry::isCarryActive` (Section 5) or the tag; the refusal reason is your `Combat.FailReason.CarryActive`.

---

# SECTION 5 — Ending a Carry: Drop, Placed, Sent, and Building's Cost Sourcing

**Goal:** a slot is freed in exactly one function, with a reason; a dropped item is a loose item; Building can count and consume what is in the hands.
**Spec:** B07 3.3, 3.4, §4.1 (`EndCarry`, `GetCarriedItemsForCost`), §9; Doc B01 3.4, 3.8, §5 (`DepositToBlueprint`); Doc B09 3.11; Doc02 §19.7.
**Depends on:** Section 4.

### What each reason does (C6)

| Reason | Who calls | Where the item goes | The loose-object lifetime |
| --- | --- | --- | --- |
| `Dropped` | the player's drop request (Section 9) | the component spawns a loose item a short way in front of the carrier | starts now (B07 3.6: a carried item has no lifetime until it is dropped) |
| `Interrupted` | `interruptCarry` (Section 7) | the component spawns a loose item at the carrier's feet | starts now |
| `Thrown` | `throwCarried` (Section 6) | the released entry goes into the throw pipeline | starts when it lands |
| `Placed` | Building, through `consumeCarriedForCost` | **nowhere**: it became part of a structure. The entry is returned | none |
| `Sent` | Transport (Doc B09 `RequestSendCarried`) | the returned entry becomes a Transport Packet | none until it lands (Doc B09 3.8) |

No path destroys an item by accident, and `endCarry` is the only function that frees a slot.

### C5.1 — `endCarry`

Header additions (public, then protected):

```cpp
    // B07 §4.1 EndCarry. Frees the slot(s) of the thing carried in slotIndex. Dropped and Interrupted put the item on the
    // ground here; for Placed, Sent and Thrown the released entry is in the result and the caller owns it (C6).
    FS_CarryEndResult endCarry(int32 slotIndex, E_EndCarryReason reason);
    // Server. Building's sourcing order (Doc B01 3.4): uses up to `quantity` carried units of that item, each ending
    // with the reason Placed. Returns how many were used; the caller draws any remainder from the bag (non-heavy items only).
    int32 consumeCarriedForCost(FName itemId, int32 quantity);

protected:
    FS_CarryEndResult endCarryInternal(int32 slotIndex, E_EndCarryReason reason, const FGameplayTag& interruptCause);
    // In front of the carrier for a chosen drop, at the feet for a forced one (B07 3.6).
    FVector getDropLocation(bool atFeet) const;
```

Implementation (add the includes at the top of the file):

```cpp
#include "CarryFramework/03Interfaces/BPI_CarryableActor.h"
#include "ItemInventoryFramework/UInventorySettings.h"
```

```cpp
// ---------- Section 5: ending a carry ----------

FVector UCarryComponent::getDropLocation(bool atFeet) const{
    const AActor* owner = GetOwner();
    if(atFeet){
        return owner->GetActorLocation();
    }
    return owner->GetActorLocation() + owner->GetActorForwardVector() * UInventorySettings::get()->dropForwardDistance;
}

FS_CarryEndResult UCarryComponent::endCarry(int32 slotIndex, E_EndCarryReason reason){
    return endCarryInternal(slotIndex, reason, FGameplayTag());
}

FS_CarryEndResult UCarryComponent::endCarryInternal(int32 slotIndex, E_EndCarryReason reason, const FGameplayTag& interruptCause){
    if(!isServerSide()){
        return refusedEnd(T_Inventory_FailReason_NotServer);
    }
    if(!carrySlots.IsValidIndex(slotIndex) || !carrySlots[slotIndex].isOccupied()){
        return refusedEnd(T_Carry_FailReason_SlotEmpty);
    }
    const bool isPutDown = reason == E_EndCarryReason::Dropped || reason == E_EndCarryReason::Interrupted;
    const FS_CarrySlotState released = carrySlots[slotIndex];
    if(released.holdsActor && !isPutDown){
        // A carried corpse is not an item: it cannot be thrown, built with or sent on a line (C10, Doc B09 3.11).
        return refusedEnd(reason == E_EndCarryReason::Thrown ? T_Carry_FailReason_CannotThrow.GetTag() : T_Interaction_FailReason_NotAllowedInState.GetTag());
    }

    // Everything checked. Free every slot of this carried thing; the freed slots remember why (the clients read it).
    FS_CarryEndResult result;
    int32 primaryIndex = INDEX_NONE;
    for(int32 index = 0; index < carrySlots.Num(); index++){
        if(carrySlots[index].carryId != released.carryId){
            continue;
        }
        if(primaryIndex == INDEX_NONE){
            primaryIndex = index;
        }
        carrySlots[index] = FS_CarrySlotState();
        carrySlots[index].lastEndReason = reason;
        carrySlots[index].lastInterruptCause = interruptCause;
    }
    carriedEntries.RemoveAndCopyValue(released.carryId, result.entry);

    const FVector dropLocation = getDropLocation(reason == E_EndCarryReason::Interrupted);
    if(released.holdsActor){
        AActor* actor = released.carriedActor;
        result.actor = actor;
        if(IsValid(actor)){
            actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
            actor->SetActorLocation(dropLocation);
            IBPI_CarryableActor::Execute_onCarryEnded(actor, GetOwner(), reason);   // it is an ordinary corpse where it lies (Index A93)
        }
    }else if(isPutDown){
        // A dropped item is a loose item: never saved, removed only by its own lifetime (Doc02 19.7). It is never destroyed here.
        const UInventoryServices* services = UInventoryServices::get(this);
        if(services != nullptr){
            services->spawnLooseItem(getCarrierPlayerState(), result.entry, dropLocation);
        }
    }

    if(!isCarryActive()){
        applyCarryStateTag(false);   // the last slot was freed: the lockout ends
    }
    finishChange();
    raiseEnded(primaryIndex, released.itemId, reason, interruptCause);
    if(reason != E_EndCarryReason::Placed && reason != E_EndCarryReason::Sent){
        const UCarryServices* carryServices = UCarryServices::get(this);
        if(carryServices != nullptr){
            carryServices->raiseCosmeticEvent(T_Cosmetic_Event_Carry_Dropped, dropLocation, GetOwner());   // Doc B21 A.4
        }
    }
    CARRY_LOG(Log, this, "%s stopped carrying %s from slot %d: %s%s%s", *GetNameSafe(GetOwner()), *released.itemId.ToString(), primaryIndex, *UEnum::GetValueAsString(reason), interruptCause.IsValid() ? TEXT(", cause ") : TEXT(""), interruptCause.IsValid() ? *interruptCause.ToString() : TEXT(""));
    return result;
}

int32 UCarryComponent::consumeCarriedForCost(FName itemId, int32 quantity){
    int32 consumed = 0;
    if(!isServerSide()){
        return consumed;
    }
    while(consumed < quantity){
        int32 foundIndex = INDEX_NONE;
        for(int32 index = 0; index < carrySlots.Num(); index++){
            if(carrySlots[index].isOccupied() && !carrySlots[index].holdsActor && carrySlots[index].itemId == itemId){
                foundIndex = index;
                break;
            }
        }
        if(foundIndex == INDEX_NONE || !endCarryInternal(foundIndex, E_EndCarryReason::Placed, FGameplayTag()).succeeded()){
            break;
        }
        consumed++;
    }
    return consumed;
}
```

### C5.2 — The function library (what other frameworks call)

Teammates should not need to know the component. One small library gives them character-level functions. Every function accepts the Character **or** its PlayerState.

`Public/CarryFramework/UBFL_Carry.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CarryFramework/01Structs/S_CarryFramework.h"
#include "UBFL_Carry.generated.h"

class AActor;

UCLASS()
class AWAKEN_API UBFL_Carry : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // ---- queries. An actor without the component carries nothing ----
    // B07 IsCarryActive(). Combat's attack check, Animation's carry pose, the squeeze and climb checks.
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") static bool isCarryActive(const AActor* carrier);
    // The one refusal line for "your hands are full": Interaction.FailReason.HandsFull while carrying, otherwise an empty tag.
    // Dialogue, the bag screen, crafting, climbing and the sleep registration all return this (B07 3.6).
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") static FGameplayTag getHandsBlockReason(const AActor* carrier);
    // B07 GetCarriedItemsForCost(ItemID) (Doc B01 3.4).
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") static int32 getCarriedItemsForCost(const AActor* carrier, FName itemId);
    // For Doc R07's speed stack.
    UFUNCTION(BlueprintPure, Category = "Awaken|Carry") static float getSpeedMultiplier(const AActor* carrier);

    // ---- commands, server only ----
    // Doc B01: use carried units for a cost. Each ends with the reason Placed. Returns how many were used.
    static int32 consumeCarriedForCost(AActor* carrier, FName itemId, int32 quantity);
};
```

`Private/CarryFramework/UBFL_Carry.cpp`

```cpp
#include "CarryFramework/UBFL_Carry.h"
#include "CarryFramework/UCarryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"

bool UBFL_Carry::isCarryActive(const AActor* carrier){
    const UCarryComponent* carry = UCarryComponent::findOn(carrier);
    return carry != nullptr && carry->isCarryActive();
}

FGameplayTag UBFL_Carry::getHandsBlockReason(const AActor* carrier){
    return isCarryActive(carrier) ? T_Interaction_FailReason_HandsFull.GetTag() : FGameplayTag();
}

int32 UBFL_Carry::getCarriedItemsForCost(const AActor* carrier, FName itemId){
    const UCarryComponent* carry = UCarryComponent::findOn(carrier);
    return carry != nullptr ? carry->getCarriedItemsForCost(itemId) : 0;
}

float UBFL_Carry::getSpeedMultiplier(const AActor* carrier){
    const UCarryComponent* carry = UCarryComponent::findOn(carrier);
    return carry != nullptr ? carry->getSpeedMultiplier() : 1.0f;
}

int32 UBFL_Carry::consumeCarriedForCost(AActor* carrier, FName itemId, int32 quantity){
    UCarryComponent* carry = UCarryComponent::findOn(carrier);
    return carry != nullptr ? carry->consumeCarriedForCost(itemId, quantity) : 0;
}
```

### C5.3 — The contract with Building (Doc B01 3.4) and Transport (Doc B09 3.11)

**Building.** B07 3.3 makes this framework the owner of "what am I currently carrying". Doc B01's placement-confirm and `DepositToBlueprint` do, on the server, in this order:

```cpp
    // 1. How much of the cost is in the hands?
    const int32 inHands = UBFL_Carry::getCarriedItemsForCost(player, costItemId);
    // 2. Validate EVERYTHING first (hands + bag for a non-heavy material; hands only for a heavy one, Doc B01 3.4).
    // 3. Then mutate once: hands first...
    const int32 fromHands = UBFL_Carry::consumeCarriedForCost(player, costItemId, quantityOwed);
    // 4. ...and only a non-heavy remainder from the bag, through R01.
```

A deposit is allowed while the hands are full, because it uses the carried item (Doc B18 §10, Index A103).

**Transport.** Doc B09's `RequestSendCarried(LinkID, Player)` does, on the server:

```cpp
    UCarryComponent* carry = UCarryComponent::findFor(player);
    const int32 slotIndex = carry != nullptr ? carry->getActiveSlotIndex() : INDEX_NONE;   // only the active slot, one item per press (S4-F32)
    // refuse up front when nothing is carried, when carry->isActorCarry(slotIndex) (a corpse: NotAllowedInState),
    // or when the far end's storage is full, so the item stays in the hands (Doc B09 3.11)
    const FS_CarryEndResult sent = carry->endCarry(slotIndex, E_EndCarryReason::Sent);
    // sent.entry is now the Transport Packet's one item. It is not a loose object until it lands (Doc B09 3.8).
```

### Verify (Section 5)

Use a small test or a temporary `BeginPlay` snippet on the host; the fixture's stub loose-item spawner records every drop.

| Test | Expect |
| --- | --- |
| Carry one `Log`, `endCarry(0, Dropped)` | a loose `Log` × 1 appears `dropForwardDistance` in front of the player; `isCarryActive()` false; `Carry.Ended` once with reason `Dropped`; the mesh at `carry_01` is hidden; the log has its loose lifetime from now |
| Carry two logs, `endCarry(0, Dropped)` | slot 0 free, slot 1 still `Log`; `isCarryActive()` still **true**; the state tag is still present |
| Carry a `Trunk`, `endCarry(1, Dropped)` (its second slot) | **both** slots are freed; one loose `Trunk`; `Carry.Ended` names slot **0** (the primary) |
| `endCarry(0, Dropped)` with empty hands, or `endCarry(7, …)` | `Carry.FailReason.SlotEmpty`; nothing changes |
| Carry two logs, `getCarriedItemsForCost(Log)` | 2. `getCarriedItemsForCost(Stone)` → 0 |
| Then `consumeCarriedForCost(Log, 3)` | returns **2**; hands empty; **no** loose item spawned; two `Carry.Ended` with reason `Placed`; no `Cosmetic.Event.Carry.Dropped` |
| Carry a `Stone`, `endCarry(0, Sent)` | the result's `entry` is the stone; hands empty; nothing spawned |
| A spoilable heavy item (test data), carry it, drop it | the loose item has the **same** `spoilAtTime` it was picked up with: the entry travelled through the hands unchanged |
| Any command on a client | `Inventory.FailReason.NotServer`; nothing changes |

### Handoff
- **B01** calls `getCarriedItemsForCost` then `consumeCarriedForCost` as in C5.3. A manual piece never has a heavy cost; a heavy material is paid by deposit from the hands (Doc B01 3.4).
- **B09** calls `getActiveSlotIndex`, `isActorCarry` and `endCarry(…, Sent)` as in C5.3.
- **R03, R07, R11, R02, B17, R12** read `UBFL_Carry::isCarryActive` / `getHandsBlockReason` (the full list is in Section 7).

---

# SECTION 6 — The Throw Pipeline

**Goal:** a carried item can be thrown; it flies on the server, hits something, and lands as an ordinary loose item. The same pipeline serves Combat's thrown weapons.
**Spec:** B07 3.4, §10 ("a throw is simulated on the server, and clients only show the result"); Doc R03 3.2 (Thrown), 3.5, 3.9; Doc B11 (`Interaction.Throwable`); R01 3.11; Doc02 §19.6, §19.7.
**Depends on:** Section 5.

### Files

```
Public/CarryFramework/AThrownItemActor.h
Private/CarryFramework/AThrownItemActor.cpp
Public/CarryFramework/UBFL_Carry.h / Private/…/UBFL_Carry.cpp     (additions)
```

### The rules

1. **One pipeline, two users** (C7). `UBFL_Carry::throwEntry` takes any inventory entry, a start point, a direction, a speed and a damage. Carry calls it for a log; Doc R03 calls it for a spear after `UBFL_Inventory::takeOutOfBag` (R01 guide S5.5), with the weapon's own damage.
2. **The server simulates** (B07 §10, Doc02 §19.7). The thrown item is one replicated actor; clients receive its movement and show it.
3. **In flight it is not a loose item yet.** It cannot be picked up and has no loose lifetime. When it stops it becomes an ordinary world pickup, handled by the general pickup path (R01 3.11).
4. **Damage is Combat's.** On the first thing it hits, the pipeline tells the impact sink who was hit, by whom, with what and how hard. Doc R03 runs its normal order: a thrown item is *attack* damage, so Block and Dodge apply (R03 3.9).
5. **Nothing is lost in flight.** A throw that hits nothing for `maxThrowFlightSeconds` lands where it is. Only the sink can say "do not land" (a thrown weapon that broke, R03 3.5).

### C6.1 — `AThrownItemActor`

`AThrownItemActor.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "AThrownItemActor.generated.h"

class APlayerState;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

// B07 3.4: one item in flight. The server simulates it; clients receive its movement (Doc02 19.7).
// When it stops it turns into an ordinary loose item and removes itself.
UCLASS()
class AWAKEN_API AThrownItemActor : public AActor{
    GENERATED_BODY()

public:
    AThrownItemActor();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;

    // Command, server. Called once, between the deferred spawn and its finish.
    void launch(AActor* newThrower, const FS_InventoryEntry& newEntry, const FVector& velocity, float newImpactDamage);
    // Command, server. Ends the flight where the actor is now. Used by the flight timer and by tests.
    void forceLand();

    FS_InventoryEntry getEntry() const;

protected:
    UFUNCTION()
    void onProjectileStopped(const FHitResult& impactResult);
    UFUNCTION()
    void OnRep_itemId();

    // Server. The one way a flight ends.
    void land(AActor* hitActor, const FVector& location);
    void refreshMesh();

    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> collision;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UProjectileMovementComponent> movement;

    // Replicated to everyone so each machine can show the right mesh. The rest of the entry stays on the server.
    UPROPERTY(ReplicatedUsing = OnRep_itemId) FName itemId;

    FS_InventoryEntry entry;
    TWeakObjectPtr<AActor> thrower;
    TWeakObjectPtr<APlayerState> throwerPlayer;
    float impactDamage = 0.0f;
    bool hasLanded = false;
    FTimerHandle flightTimer;
};
```

`AThrownItemActor.cpp`

```cpp
#include "CarryFramework/AThrownItemActor.h"
#include "CarryFramework/CarryLog.h"
#include "CarryFramework/CarryServices.h"
#include "CarryFramework/UCarrySettings.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

AThrownItemActor::AThrownItemActor(){
    bReplicates = true;
    SetReplicatingMovement(true);

    collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    collision->InitSphereRadius(UCarrySettings::get()->throwCollisionRadius);
    collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    SetRootComponent(collision);

    // The mesh is only the look. The sphere does the colliding, so the flight never waits for a mesh to load.
    mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    mesh->SetupAttachment(collision);
    mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
    movement->bShouldBounce = false;           // it stops at the first thing it hits
    movement->ProjectileGravityScale = 1.0f;
    movement->bRotationFollowsVelocity = true;
}

void AThrownItemActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(AThrownItemActor, itemId);
}

// A client receives the server's velocity here and hands it to its own movement component, so the flight looks smooth
// between two position updates.
void AThrownItemActor::PostNetReceiveVelocity(const FVector& NewVelocity){
    Super::PostNetReceiveVelocity(NewVelocity);
    if(movement != nullptr){
        movement->Velocity = NewVelocity;
    }
}

void AThrownItemActor::launch(AActor* newThrower, const FS_InventoryEntry& newEntry, const FVector& velocity, float newImpactDamage){
    entry = newEntry;
    entry.reservedQuantity = 0;
    itemId = entry.itemId;
    thrower = newThrower;
    const APawn* throwerPawn = Cast<APawn>(newThrower);
    throwerPlayer = throwerPawn != nullptr ? throwerPawn->GetPlayerState() : nullptr;
    impactDamage = newImpactDamage;

    if(newThrower != nullptr){
        collision->IgnoreActorWhenMoving(newThrower, true);   // it must not hit the hands it left
    }
    movement->Velocity = velocity;
    movement->OnProjectileStop.AddDynamic(this, &AThrownItemActor::onProjectileStopped);
    // The safety net of rule 5: a short timer while the item flies (Doc02 §5).
    GetWorldTimerManager().SetTimer(flightTimer, this, &AThrownItemActor::forceLand, UCarrySettings::get()->maxThrowFlightSeconds, false);
    refreshMesh();
}

void AThrownItemActor::onProjectileStopped(const FHitResult& impactResult){
    land(impactResult.GetActor(), impactResult.ImpactPoint + impactResult.ImpactNormal * collision->GetScaledSphereRadius());
}

void AThrownItemActor::forceLand(){
    land(nullptr, GetActorLocation());
}

void AThrownItemActor::land(AActor* hitActor, const FVector& location){
    if(hasLanded || !HasAuthority()){
        return;
    }
    hasLanded = true;
    GetWorldTimerManager().ClearTimer(flightTimer);

    bool shouldLand = true;
    const UCarryServices* carryServices = UCarryServices::get(this);
    if(IsValid(hitActor) && hitActor != thrower.Get() && carryServices != nullptr){
        // Doc R03 decides what the hit does. It may change the entry (a weapon loses one use) and may say "do not land".
        shouldLand = carryServices->applyThrowImpact(hitActor, thrower.Get(), entry, impactDamage, location);
    }
    if(shouldLand){
        // From here it is an ordinary world pickup with its loose lifetime (R01 3.11, Doc02 19.7).
        const UInventoryServices* services = UInventoryServices::get(this);
        if(services != nullptr){
            services->spawnLooseItem(throwerPlayer.Get(), entry, location);
        }
    }
    CARRY_LOG(Log, this, "Thrown %s landed at %s%s%s", *entry.itemId.ToString(), *location.ToCompactString(), IsValid(hitActor) ? TEXT(", it hit ") : TEXT(""), IsValid(hitActor) ? *hitActor->GetName() : TEXT(""));
    Destroy();
}

FS_InventoryEntry AThrownItemActor::getEntry() const{
    return entry;
}

void AThrownItemActor::OnRep_itemId(){
    refreshMesh();
}

void AThrownItemActor::refreshMesh(){
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(itemId) : nullptr;
    if(definition == nullptr || definition->worldMesh.IsNull()){
        return;
    }
    const TSoftObjectPtr<UStaticMesh> wanted = definition->worldMesh;
    UAssetManager::GetStreamableManager().RequestAsyncLoad(wanted.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this, [this, wanted](){
        mesh->SetStaticMesh(wanted.Get());
    }));
}
```

### C6.2 — `throwEntry`: the function Combat also calls

Additions to `UBFL_Carry.h` (add `class AThrownItemActor;` to the forward declarations and `#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"`):

```cpp
    // B07 3.4, the generic throw pipeline (C7). Server only. The entry must already be out of wherever it was held:
    // Carry passes what endCarry released, Doc R03 passes what UBFL_Inventory::takeOutOfBag returned.
    // direction is normalized here. Returns null when the actor could not be spawned; the caller then still owns the entry.
    static AThrownItemActor* throwEntry(AActor* thrower, const FS_InventoryEntry& entry, const FVector& origin, const FVector& direction, float speed, float impactDamage);
```

Additions to `UBFL_Carry.cpp`:

```cpp
#include "CarryFramework/AThrownItemActor.h"
#include "CarryFramework/CarryLog.h"
#include "Engine/World.h"
```

```cpp
AThrownItemActor* UBFL_Carry::throwEntry(AActor* thrower, const FS_InventoryEntry& entry, const FVector& origin, const FVector& direction, float speed, float impactDamage){
    UWorld* world = thrower != nullptr ? thrower->GetWorld() : nullptr;
    const FVector throwDirection = direction.GetSafeNormal();
    if(world == nullptr || !thrower->HasAuthority() || entry.itemId.IsNone() || entry.totalQuantity < 1 || throwDirection.IsNearlyZero()){
        return nullptr;
    }
    const FTransform startTransform(throwDirection.Rotation(), origin);
    // Deferred, so the velocity and the entry are set before the actor begins play and before it first replicates.
    AThrownItemActor* thrown = world->SpawnActorDeferred<AThrownItemActor>(AThrownItemActor::StaticClass(), startTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if(thrown == nullptr){
        return nullptr;
    }
    thrown->launch(thrower, entry, throwDirection * speed, impactDamage);
    thrown->FinishSpawning(startTransform);
    CARRY_LOG(Log, thrower, "%s threw %s x%d (speed %.0f, impact damage %.1f)", *GetNameSafe(thrower), *entry.itemId.ToString(), entry.totalQuantity, speed, impactDamage);
    return thrown;
}
```

### C6.3 — `throwCarried`

Header addition (`UCarryComponent.h`, public):

```cpp
    // B07 3.4. Server. Ends the carry with the reason Thrown and starts the flight from the carrier's eye.
    // A carried actor cannot be thrown (C10). slotIndex names any slot of the carried thing.
    FS_CarryEndResult throwCarried(int32 slotIndex, const FVector& direction);
```

Implementation (add `#include "CarryFramework/UBFL_Carry.h"` at the top):

```cpp
// ---------- Section 6: throwing ----------

FS_CarryEndResult UCarryComponent::throwCarried(int32 slotIndex, const FVector& direction){
    if(!isServerSide()){
        return refusedEnd(T_Inventory_FailReason_NotServer);
    }
    const FVector throwDirection = direction.GetSafeNormal();
    if(throwDirection.IsNearlyZero()){
        return refusedEnd(T_Carry_FailReason_InvalidThrow);
    }
    // Everything else (an empty slot, a carried actor) is checked by endCarry before it changes anything.
    FS_CarryEndResult result = endCarryInternal(slotIndex, E_EndCarryReason::Thrown, FGameplayTag());
    if(!result.succeeded()){
        return result;
    }
    const UCarrySettings* settings = UCarrySettings::get();
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(result.entry.itemId) : nullptr;
    const APawn* pawn = Cast<APawn>(GetOwner());
    // The origin is the HOST's idea of the eye. The client never sends one (C9).
    const FVector eye = pawn != nullptr ? pawn->GetPawnViewLocation() : GetOwner()->GetActorLocation();
    const FVector origin = eye + throwDirection * settings->throwStartDistance;
    const float damage = definition != nullptr ? definition->throwImpactDamage : 0.0f;
    if(UBFL_Carry::throwEntry(GetOwner(), result.entry, origin, throwDirection, settings->throwSpeed, damage) == nullptr){
        // The flight could not start. The item is already out of the hands, so it is put down instead: never lost.
        const UInventoryServices* services = UInventoryServices::get(this);
        if(services != nullptr){
            services->spawnLooseItem(getCarrierPlayerState(), result.entry, getDropLocation(false));
        }
    }
    return result;
}
```

### C6.4 — How Combat throws a weapon with the same pipeline (reference for the R03 owner)

```cpp
    // Doc R03, server, after its own checks (IsAttackAllowed, the weapon is of type Thrown, not a permanent item):
    FS_InventoryEntry taken;
    if(UBFL_Inventory::takeOutOfBag(player, weaponRef, 1, taken).succeeded()){
        UBFL_Carry::throwEntry(pawn, taken, eyeLocation + direction * startDistance, direction, weaponThrowSpeed, weaponDefinition->damage);
    }
    // R03's IBPI_ThrowImpactSink then applies the damage order, takes one use of durability on a hit (R03 3.5),
    // and returns false when the weapon reached zero, so a broken weapon does not land.
```

### Verify (Section 6)

| Test | Expect |
| --- | --- |
| Carry a `Log`, `throwCarried(0, forward)` | hands empty at once; `Carry.Ended` with reason `Thrown`; one `AThrownItemActor` flies from the eye, drops under gravity, and stops on the ground; a loose `Log` × 1 lies at that spot; the thrown actor is gone |
| Throw at a wall | it stops at the wall and the loose log appears just in front of it, not inside it |
| Throw a `Stone` (`throwImpactDamage 20`) at another pawn | the stub impact sink holds that pawn and `totalDamage` 20; the stone lands next to it. No health changes yet: that is R03 |
| Throw straight up into open sky on a test map without a ceiling | it comes down and lands. With gravity scale 0 forced in a test, `maxThrowFlightSeconds` later it lands where it is |
| `throwCarried(0, FVector::ZeroVector)` | `Carry.FailReason.InvalidThrow`; the log is **still in the hands** |
| `throwCarried` with empty hands | `Carry.FailReason.SlotEmpty` |
| Try to pick up the item while it is in the air | impossible: it is not a loose item yet |
| Two-player: the client throws (Section 9 adds the request; for now call it on the host for player 1) | both machines see the same flight and the same landing spot; only the host logs the landing |
| A sink that returns false (set one in a test) | nothing lands; the item is gone on purpose |

### Handoff
- **R03** implements `IBPI_ThrowImpactSink` (the damage order with the thrower as instigator; a bomb adds its area effect there) and calls `UBFL_Carry::throwEntry` for Thrown weapons as in C6.4.
- **B21 / R09 / R15**: the throw has no cosmetic event of its own in the Tag Registry; `Cosmetic.Event.Carry.Dropped` is raised when the item leaves the hands. Say if a landing sound needs its own event.
- A dropped or landed item in deep water sinks and stays on the bottom (B07 3.6, Index A87). That is the behaviour of the loose item in a water body (R01's `ALooseItemActor` and Doc B21 Part C), not of this framework; nothing here removes an item because it is under water.

---

# SECTION 7 — Interrupts, and What Is Refused While Carrying

**Goal:** when the body can no longer hold the item, it drops and is never lost; and every action that is not on the allowed list is refused by its owner with one shared reason.
**Spec:** B07 3.6 (the whole table), amendments A87, A92, A93, A103 (S4-F36, S4-F37), A106, A115, A117; Doc B20 3.6; Doc02 §19.9 (order inside one server frame).
**Depends on:** Section 6.

### C7.1 — The interrupt table, row by row

Every row that drops the item ends the carry with `E_EndCarryReason::Interrupted` and a cause tag (S4-F37). The item becomes a loose item at the player's feet, exactly as after a normal drop.

| Event (B07 3.6) | What happens | Who triggers it | Cause tag |
| --- | --- | --- | --- |
| Taking a hit | **Keep** | nobody: there is no knock-loose (designer-confirmed) | |
| Downed (co-op) | Drop at the feet. A revive does not put it back | Doc B02, when it enters Downed | `Downed` |
| Final death | Drop where the player died | Doc B02, before the pawn is removed. Safety net: the component's `EndPlay` | `Death` / `PawnRemoved` |
| Restrained | Not reachable with an item; if it ever happens, drop | Doc B02 | `Restrained` |
| Disconnect or leaving | Drop at the last position, kept by the host | Doc B20 on `Session.PlayerLeft`. Safety net: `EndPlay` | `PlayerLeft` / `PawnRemoved` |
| Scene lock (Doc R17) | **Keep** | nobody | |
| Swimming or diving | Drop at the feet when swimming begins | Doc R07 in `BeginSwim`. Safety net: the engine's movement mode | `Swim` |
| Sleeping | Drop when the sleep starts; **registering** to sleep is refused while carrying | Doc R12 | `Sleep` |
| Climbing, squeezing, dialogue, crafting, the bag | **Keep**, and the action is refused | the owner of each (C7.4) | |
| Jump or crouch | **Keep**, and the move is allowed | nobody | |
| A rope or slider | The item is **sent** and the player does not ride | Doc B09 (`endCarry(…, Sent)`, C5.3) | |

**Order inside one server frame** (Doc02 §19.9): damage, then Downed or death, then a disconnect or scene start, then completion. B02 calls `interruptCarry` as part of step 2, so a deposit hold that would complete in the same frame as a lethal hit does not complete: the log is already on the ground.

### C7.2 — `interruptCarry` and the two safety nets (C13)

Header additions (`UCarryComponent.h`; `BeginPlay` and `EndPlay` go with the other overrides):

```cpp
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // B07 3.6. Server. Drops everything that is carried at the carrier's feet with the reason Interrupted.
    // Returns how many things were dropped. Safe to call when nothing is carried.
    int32 interruptCarry(const FGameplayTag& causeTag);

protected:
    UFUNCTION()
    void onMovementModeChanged(ACharacter* character, EMovementMode previousMovementMode, uint8 previousCustomMode);
```

(add `class ACharacter;` to the forward declarations of the header, and `#include "Engine/EngineTypes.h"` for `EMovementMode`.)

Implementation:

```cpp
// ---------- Section 7: interrupts ----------

int32 UCarryComponent::interruptCarry(const FGameplayTag& causeTag){
    int32 droppedCount = 0;
    if(!isServerSide()){
        return droppedCount;
    }
    for(int32 index = 0; index < carrySlots.Num(); index++){
        // endCarry frees every slot of the carried thing, so a two-slot item is counted once.
        if(carrySlots[index].isOccupied() && endCarryInternal(index, E_EndCarryReason::Interrupted, causeTag).succeeded()){
            droppedCount++;
        }
    }
    return droppedCount;
}

void UCarryComponent::BeginPlay(){
    Super::BeginPlay();
    if(isServerSide()){
        // Safety net 1 (swimming). Event-driven, no tick. Doc R07 may also call interruptCarry from BeginSwim; the second
        // call finds nothing to drop.
        ACharacter* character = Cast<ACharacter>(GetOwner());
        if(character != nullptr){
            character->MovementModeChangedDelegate.AddDynamic(this, &UCarryComponent::onMovementModeChanged);
        }
    }
}

void UCarryComponent::onMovementModeChanged(ACharacter* character, EMovementMode previousMovementMode, uint8 previousCustomMode){
    if(character != nullptr && character->GetCharacterMovement() != nullptr && character->GetCharacterMovement()->IsSwimming()){
        interruptCarry(T_Carry_InterruptCause_Swim);   // dropped when swimming begins (B07 3.6, Index A87)
    }
}

void UCarryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason){
    if(isServerSide() && isCarryActive()){
        if(EndPlayReason == EEndPlayReason::Destroyed){
            // Safety net 2. The pawn is being removed (a death, a player who left) and nobody interrupted the carry first.
            // The item drops where the pawn was; it is never deleted with the pawn.
            interruptCarry(T_Carry_InterruptCause_PawnRemoved);
        }else{
            // The world itself is closing (a level change, quitting). Nothing is dropped and nothing is saved (B07 3.5).
            // Only the tag is cleared, because a PlayerState can outlive this pawn.
            applyCarryStateTag(false);
        }
    }
    Super::EndPlay(EndPlayReason);
}
```

### C7.3 — Library functions for the owners of each state

Additions to `UBFL_Carry.h`:

```cpp
    // B07 3.6. Server. A forced drop of everything the carrier holds. causeTag is one of Carry.InterruptCause.*.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Carry") static int32 interruptCarry(AActor* carrier, FGameplayTag causeTag);
    // Cleanup for Session.PlayerLeft (Doc B20 3.6): a carried item is dropped where the leaving player stands.
    static void handlePlayerLeft(APlayerState* player);
```

(add `class APlayerState;` to the forward declarations.)

Additions to `UBFL_Carry.cpp` (add `#include "CarryFramework/02Tags/T_CarryFramework.h"` and `#include "GameFramework/PlayerState.h"`):

```cpp
int32 UBFL_Carry::interruptCarry(AActor* carrier, FGameplayTag causeTag){
    UCarryComponent* carry = UCarryComponent::findOn(carrier);
    return carry != nullptr ? carry->interruptCarry(causeTag) : 0;
}

void UBFL_Carry::handlePlayerLeft(APlayerState* player){
    UCarryComponent* carry = UCarryComponent::findFor(player);
    if(carry != nullptr){
        carry->interruptCarry(T_Carry_InterruptCause_PlayerLeft);   // a carried corpse's despawn timer resumes in its onCarryEnded (Index A106)
    }
}
```

The one line each owner adds, on the server:

| Owner | Where | Line |
| --- | --- | --- |
| Doc B02 | entering Downed | `UBFL_Carry::interruptCarry(character, T_Carry_InterruptCause_Downed);` |
| Doc B02 | a final death, before the pawn is removed | `UBFL_Carry::interruptCarry(character, T_Carry_InterruptCause_Death);` |
| Doc B02 | becoming Restrained | `UBFL_Carry::interruptCarry(character, T_Carry_InterruptCause_Restrained);` |
| Doc R07 | `BeginSwim` | `UBFL_Carry::interruptCarry(character, T_Carry_InterruptCause_Swim);` |
| Doc R12 | when the sleep starts (the skip begins) | `UBFL_Carry::interruptCarry(character, T_Carry_InterruptCause_Sleep);` |
| Doc B20 | `Session.PlayerLeft`, next to R01's `handlePlayerLeft` | `UBFL_Carry::handlePlayerLeft(player);` |

### C7.4 — The refusal list (B07 3.6, Index A103 S4-F36)

"Everything that is not on this list of allowed things is refused while a carry slot is occupied." Carry does not police other frameworks: each owner asks one question, `UBFL_Carry::getHandsBlockReason(character)` (or reads the tag), and refuses with what it returns. **Refusing never loses an item.**

**Allowed while carrying:** move, jump, crouch, drop, throw, doors and switches, depositing the carried item into a blueprint site, placing it, sending it on a rope or slider, and picking up another heavy item while a slot is free.

| Refused while carrying | Owner | Where the check is | Status |
| --- | --- | --- | --- |
| Attack | Doc R03 | `IsAttackAllowed` (reason `Combat.FailReason.CarryActive`) | theirs to build |
| Block, Dodge, Sprint, Reload, Weapon Charge | Doc R04 | the abilities are blocked by `Carry.State.Active` on the Ability System Component | theirs; the tag is there (C3) |
| Eating, drinking, medicine | Doc R01 | `serverUseConsumable`, `serverUseHoldSlot`: `gate(true, …)` | **already works** through the seam |
| Picking up an ordinary item, opening a container, transfers | Doc R01 | `gate(true, …)` | **already works** |
| Equipping armor, changing clothes | Doc R05 | its three requests pass `needsHands = true` (R05 guide E9.3) | **already works** |
| Selecting or binding a hotbar key, holding an item from the bag | Doc R01 | the three requests of C7.5 | **patch below** |
| Opening the bag (inventory and crafting in one) | Doc B17 | `CanOpenBag(Character)` | theirs |
| Opening a crafting session, `RequestCraft` | Doc R02 | `OpenCraftingSession` | theirs |
| Starting a dialogue | Doc R11 | 3.3a | theirs |
| Climbing (a ladder or a climbing rope) | Doc R07 | the climb entry check | theirs |
| Squeezing through a gap | Doc R07 | `BeginSqueeze` checks `isCarryActive`: the squeeze simply does not begin | theirs |
| The Axe cut, harvesting | Doc B08 / B18 | the server's hands check of Doc B18 3.5 | theirs |
| Registering to sleep | Doc R12 | the sleep request (Index A106) | theirs |
| Riding a rope | Doc B09 | a carrying player sends the item instead of riding | theirs (C5.3) |

### C7.5 — Patch R01: the hotbar is refused while carrying (C18)

The R01 guide left this open on purpose (its Appendix C, point 6). In `UInventoryRequestComponent.cpp`, in the three functions `serverBindHotbarSlot_Implementation`, `serverSelectHotbarSlot_Implementation` and `serverHoldFromBag_Implementation`, change the first line:

```cpp
    const FGameplayTag blocked = gate(false, false);
```

to:

```cpp
    const FGameplayTag blocked = gate(true, false);   // refused while carrying: it is not on the allowed list (B07 3.6, S4-F36)
```

`serverPutAway`, `serverDropFromBag` and `serverDropHoldSlot` stay as they are: putting something down is always allowed.

What this guide does **not** do: it does not empty the hand when a carry begins. The held item stays what it was, so after dropping the log the player still holds the same spear. While carrying, Combat refuses the attack and Doc R07's full-body carry pose hides the held item.

### Verify (Section 7)

| Test | Expect |
| --- | --- |
| Carry two logs, `interruptCarry(Carry.InterruptCause.Downed)` | returns 2; two loose logs **at the feet** (not in front); hands empty; two `Carry.Ended` with reason `Interrupted` and the cause `Downed`; `getLastInterruptCause()` is `Downed` |
| Carry a `Trunk` (two slots), interrupt | returns **1**; one loose trunk |
| `interruptCarry` with empty hands | returns 0; no message, no log line |
| Carry a log, walk into a water volume deep enough to swim | the log drops when the movement mode becomes swimming; cause `Swim`. While swimming, `Awaken.Item.Give Log 1` → dropped, not carried |
| Carry a log, destroy the pawn on the host (call `Destroy()` on it from a test or the editor) | a loose log lies where the pawn was; cause `PawnRemoved`; the log shows in `LogAwakenCarry` |
| Carry a log, stop Play In Editor | no errors, nothing spawned during shutdown |
| Take damage while carrying (any test damage) | nothing changes: a hit never knocks an item loose |
| While carrying: press a hotbar key | refused `Interaction.FailReason.HandsFull` (the C7.5 patch); the held item is unchanged |
| While carrying: jump, crouch, open a door | all work; still carrying |
| `UBFL_Carry::getHandsBlockReason(character)` | `Interaction.FailReason.HandsFull` while carrying, an empty tag otherwise |
| `UBFL_Carry::handlePlayerLeft(player)` | the same as an interrupt with the cause `PlayerLeft`; the loose item stays on the host |

### Handoff
- **B02, R07, R12, B20**: the one line each from C7.3. Until you add yours, the two safety nets cover death, leaving and (engine) swimming; Downed, Restrained and sleep have no net.
- **Everyone in the C7.4 table marked "theirs"**: call `UBFL_Carry::getHandsBlockReason` in your server check.

---

# SECTION 8 — Carrying an Actor: the Corpse and the Wounded Ally

**Goal:** a whole world actor can fill carry slots with exactly the same consequences as a log.
**Spec:** B07 3.1 (a carried corpse is Heavy), amendment A93 (UD11), 3.6 (the disconnect row, Index A106); Doc B08 3.2, 3.4; Doc B12 3.9; Doc B09 3.11.
**Depends on:** Section 7.

### The rules (C10)

- The actor says how many slots it fills and what it is called; Carry asks through `IBPI_CarryableActor` (Section 1).
- While carried it is **attached** to the carrier on the server; an actor attachment replicates by itself, so every machine sees the body move with the carrier.
- It can only be **put down**: by a drop, or by an interrupt. It cannot be thrown, used for a building cost, or sent on a line (Doc B09 3.11 refuses that press with `Interaction.FailReason.NotAllowedInState`).
- When it is put down it is *an ordinary corpse where it lies* (Index A93). Its own code turns physics back on and lets the despawn timer run again; Carry only detaches it, moves it and tells it.
- It cannot be harvested while it is carried, and its despawn timer is paused (Doc B08 3.4). Both are the corpse's own job, in `onCarryBegan`.

### C8.1 — `beginCarryActor`

Header additions (public, then protected):

```cpp
    // Doc B08 BeginCarryCorpse / Doc B12 BeginCarryAlly delegate here. Server. The actor must implement IBPI_CarryableActor.
    FS_CarryBeginResult beginCarryActor(AActor* actor);

protected:
    // The carried actor was destroyed by something else while it was in the hands: free its slots.
    UFUNCTION()
    void onCarriedActorDestroyed(AActor* destroyedActor);
```

Implementation:

```cpp
// ---------- Section 8: carrying an actor ----------

FS_CarryBeginResult UCarryComponent::beginCarryActor(AActor* actor){
    if(!isServerSide()){
        return refusedBegin(T_Inventory_FailReason_NotServer);
    }
    if(!IsValid(actor) || !actor->GetClass()->ImplementsInterface(UBPI_CarryableActor::StaticClass())){
        return refusedBegin(T_Carry_FailReason_NotCarryable);
    }
    const int32 cost = IBPI_CarryableActor::Execute_getCarrySlotCost(actor);
    // A cost below 1 means "not now" (mid-harvest). An actor that is already attached is in somebody's hands.
    if(cost < 1 || actor->GetAttachParentActor() != nullptr){
        return refusedBegin(T_Carry_FailReason_NotCarryable);
    }
    const FGameplayTag blocked = checkCanBegin();
    if(blocked.IsValid()){
        return refusedBegin(blocked);
    }
    if(cost > getFreeSlotCount()){
        return refusedBegin(T_Interaction_FailReason_HandsFull);
    }

    // Everything checked.
    const bool wasActive = isCarryActive();
    const FName label = IBPI_CarryableActor::Execute_getCarryLabel(actor);
    FS_CarryBeginResult result;
    result.slotIndex = fillSlots(FGuid::NewGuid(), label, cost, actor);

    // The actor first makes itself carryable (physics and collision off, timer paused), then it is attached.
    IBPI_CarryableActor::Execute_onCarryBegan(actor, GetOwner());
    const ACharacter* character = Cast<ACharacter>(GetOwner());
    USceneComponent* attachParent = character != nullptr ? static_cast<USceneComponent*>(character->GetMesh()) : GetOwner()->GetRootComponent();
    const TArray<FName>& socketNames = UCarrySettings::get()->carrySocketNames;
    const FName socketName = socketNames.IsValidIndex(result.slotIndex) ? socketNames[result.slotIndex] : NAME_None;
    actor->AttachToComponent(attachParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, socketName);
    actor->OnDestroyed.AddDynamic(this, &UCarryComponent::onCarriedActorDestroyed);

    afterBegin(result.slotIndex, label, wasActive);
    return result;
}

void UCarryComponent::onCarriedActorDestroyed(AActor* destroyedActor){
    for(int32 index = 0; index < carrySlots.Num(); index++){
        if(carrySlots[index].isOccupied() && carrySlots[index].holdsActor && carrySlots[index].carriedActor == destroyedActor){
            carrySlots[index].carriedActor = nullptr;   // it is going away: endCarry must not detach, move or call it
            endCarryInternal(index, E_EndCarryReason::Interrupted, FGameplayTag());
            return;
        }
    }
}
```

And one line in `endCarryInternal` (Section 5), inside `if(IsValid(actor)){`, before the detach, so a body that was put down no longer reports to these hands:

```cpp
            actor->OnDestroyed.RemoveDynamic(this, &UCarryComponent::onCarriedActorDestroyed);
```

### C8.2 — What the corpse does (reference for the B08 owner)

```cpp
// AHarvestableCorpseActor : public ..., public IBPI_CarryableActor
int32 AHarvestableCorpseActor::getCarrySlotCost_Implementation(){
    return isBeingHarvested ? 0 : carrySlotCost;   // data on the harvest profile; 0 = refuse the carry for now
}

void AHarvestableCorpseActor::onCarryBegan_Implementation(AActor* carrier){
    // ragdoll physics and collision off; pause the despawn timer; CanInteract refuses Harvest while carried (Doc B08 3.2, 3.4)
}

void AHarvestableCorpseActor::onCarryEnded_Implementation(AActor* carrier, E_EndCarryReason reason){
    // physics and collision back on (the server simulates, Doc02 19.7); the despawn timer runs again
}

// Doc B08 §4 BeginCarryCorpse(Instigator):
    UCarryComponent* carry = UCarryComponent::findOn(instigatorCharacter);
    const FS_CarryBeginResult result = carry != nullptr ? carry->beginCarryActor(this) : FS_CarryBeginResult();
```

Doc B12's `BeginCarryAlly(WoundedAlly)` is the same call on the rescuing NPC's own `UCarryComponent`. The NPC then "inherits the exact same consequences the player faces": `isCarryActive()` is true, so its attack, Dodge and Block are refused (Index R04-R08-IG31), and it must drop the ally to fight or flee. An NPC has no PlayerState; every function used on this path works without one.

### Verify (Section 8)

Use `AStubCarryableActor` (Section 1). Section 10 adds `Awaken.Carry.CarryDummy`.

| Test | Expect |
| --- | --- |
| Spawn a dummy, `beginCarryActor(dummy)` | accepted; slot 0 shows the label `Dummy` with `holdsActor` true; the dummy is attached to the character and moves with it; its `beganCount` is 1; `isCarryActive()` true; `Carry.Began` raised |
| `getCarriedItemsForCost(Dummy)` | **0**: a carried actor is never a building material |
| `throwCarried(0, forward)` | `Carry.FailReason.CannotThrow`; still carried |
| `endCarry(0, Sent)` or `endCarry(0, Placed)` | `Interaction.FailReason.NotAllowedInState`; still carried |
| `endCarry(0, Dropped)` | the dummy is detached and stands in front of the player; `endedCount` 1, `lastReason` `Dropped`; **no** loose item is spawned; hands empty |
| Carry a dummy with `slotCost = 2` while one log is in the hands | `HandsFull` |
| Carry the dummy, `interruptCarry(Downed)` | the dummy lies at the feet, `lastReason` `Interrupted` |
| Carry the dummy, destroy the dummy | the slot is freed, `Carry.Ended` raised, no crash |
| A second player tries `beginCarryActor` on a dummy that is being carried | `Carry.FailReason.NotCarryable` |
| An actor that does not implement the interface | `Carry.FailReason.NotCarryable` |
| Two-player: the host makes player 1 carry the dummy | the client sees the dummy attached to its own character and its slot filled |

### Handoff
- **B08**: implement `IBPI_CarryableActor` on the corpse as in C8.2; `BeginCarryCorpse` calls `beginCarryActor`.
- **B12**: the same on the wounded NPC; add `UCarryComponent` to NPCs whose profile has `CanCarryWounded`.
- **B09**: a press on a line while `isActorCarry(activeSlot)` is refused with `Interaction.FailReason.NotAllowedInState`.

---

# SECTION 9 — Networking: Requests, Replication and Messages

**Goal:** a client can only *ask* to drop or throw; the host validates, changes the state, and every machine sees the result.
**Spec:** B07 §9, §10; Doc02 §6, §8, §19.2, §19.6; Doc B18 3.5.
**Depends on:** Section 8, and the R01 guide's Section 12 (`UInventoryRequestComponent`).

### C9.1 — Replication audit (B07 §10, Doc02 §19.1)

Walk this table with a listen server and two clients. Every row must be true:

| State | Replicates to | Mechanism | Late joiner gets it from |
| --- | --- | --- | --- |
| The carry slots (item id, carry id, order, actor, last end reason and cause) | **everyone** | `COND_None` on the Character component | replicated data, then `OnRep_carrySlots` raises the local messages |
| The full inventory entry of a carried item (spoil time, instance state) | nobody | server only | n/a: it travels with the item when it is dropped |
| `Carry.State.Active` on the Ability System Component | everyone | a replicated loose tag (Doc R04 3.3) | the Ability System Component |
| A carried actor's attachment | everyone | the engine's own attachment replication | the actor |
| A thrown item in flight | everyone | a replicated actor with replicated movement | the actor, if it is still flying |
| The meshes in the hands | nobody | built locally from the slots | n/a |

Carrier: none on the GameState. Carry state is per character, not shared world state, so Doc02 §19.1 asks for no carrier here.

### C9.2 — The requests

Picking up is already a request of R01 (`serverPickUp`). Carry adds two. A client never mutates anything: it sends **intent**; the server checks, runs the command, and replies to the requester only when it refuses (Doc02 §6, §19.6).

Which checks apply (Doc02 §19.6 asks each framework to say):

| Check | Drop | Throw | Reason tag |
| --- | --- | --- | --- |
| Rate limit (shared with R01's bucket) | yes | yes | `Inventory.FailReason.RateLimited` |
| Downed, Restrained, Arriving, scene lock | yes | yes | `Interaction.FailReason.NotAllowedInState` |
| Free hands | no | no | (these *are* the hands) |
| Something is carried in that slot | yes | yes | `Carry.FailReason.SlotEmpty` |
| The direction is near the host's view of the pawn (C9) | | yes | `Carry.FailReason.InvalidThrow` |
| Range, line of sight | no | no | nothing is targeted |

Header additions (`UCarryComponent.h`, public, then protected):

```cpp
    // ---- requests: input calls these on the owning client. On the host they run directly ----
    // slotIndex -1 = the active slot (the thing picked up last, C5).
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Carry") void serverDropCarried(int32 slotIndex);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Carry") void serverThrowCarried(int32 slotIndex, FVector_NetQuantizeNormal direction);

protected:
    // Rate and state, through R01's gate. False = refused and the requester has been told.
    bool passRequestGate(const TCHAR* operation) const;
    void replyIfRefused(const FGameplayTag& failReason, const TCHAR* operation) const;
```

(add `#include "Engine/NetSerialization.h"` to the header for `FVector_NetQuantizeNormal`.)

Implementation (add `#include "ItemInventoryFramework/UInventoryRequestComponent.h"`):

```cpp
// ---------- Section 9: requests ----------

bool UCarryComponent::passRequestGate(const TCHAR* operation) const{
    UInventoryRequestComponent* requests = UInventoryRequestComponent::findFor(getCarrierPlayerState());
    if(requests == nullptr){
        CARRY_LOG(Warning, this, "%s refused: the player has no request component", operation);
        return false;
    }
    const FGameplayTag blocked = requests->gate(false, false);   // hands not needed; not while Downed or Restrained
    if(blocked.IsValid()){
        requests->refuse(blocked, operation);
        return false;
    }
    return true;
}

void UCarryComponent::replyIfRefused(const FGameplayTag& failReason, const TCHAR* operation) const{
    if(!failReason.IsValid()){
        return;
    }
    UInventoryRequestComponent* requests = UInventoryRequestComponent::findFor(getCarrierPlayerState());
    if(requests != nullptr){
        requests->refuse(failReason, operation);   // logs the reason tag and tells only the requester (Doc02 §6)
    }
}

void UCarryComponent::serverDropCarried_Implementation(int32 slotIndex){
    if(!passRequestGate(TEXT("dropCarried"))){
        return;
    }
    const int32 wantedSlot = slotIndex < 0 ? getActiveSlotIndex() : slotIndex;
    replyIfRefused(endCarry(wantedSlot, E_EndCarryReason::Dropped).failReason, TEXT("dropCarried"));
}

void UCarryComponent::serverThrowCarried_Implementation(int32 slotIndex, FVector_NetQuantizeNormal direction){
    if(!passRequestGate(TEXT("throwCarried"))){
        return;
    }
    // Doc02 19.6, the throw vector: the direction must be close to where the host knows this pawn is looking.
    // The check is inclusive: a direction exactly at the limit is allowed (Doc02 §19.9).
    const APawn* pawn = Cast<APawn>(GetOwner());
    const FVector viewDirection = pawn != nullptr ? pawn->GetBaseAimRotation().Vector() : GetOwner()->GetActorForwardVector();
    const FVector requested = FVector(direction).GetSafeNormal();
    const float smallestDot = FMath::Cos(FMath::DegreesToRadians(UCarrySettings::get()->maxThrowAngleDegrees));
    if(requested.IsNearlyZero() || FVector::DotProduct(viewDirection, requested) < smallestDot){
        replyIfRefused(T_Carry_FailReason_InvalidThrow, TEXT("throwCarried"));
        return;
    }
    const int32 wantedSlot = slotIndex < 0 ? getActiveSlotIndex() : slotIndex;
    replyIfRefused(throwCarried(wantedSlot, requested).failReason, TEXT("throwCarried"));
}
```

> **Why the RPCs can live on a Character component.** A client may call a server RPC on an actor its connection owns, and a possessed pawn is owned by its PlayerController. No other client can call them. An NPC's component simply never receives one.

> **Input.** Which keys mean "drop" and "throw" is not decided here: all input goes through Enhanced Input (decision D7, Docs R04 and B17). Doc B18 §10 lists both as allowed while carrying. The input action handlers call `serverDropCarried(-1)` and `serverThrowCarried(-1, cameraForward)` on the local character's component.

### C9.3 — Message delivery (B07 §9, Doc02 §19.2)

| Message | Payload | Delivery | How it is met |
| --- | --- | --- | --- |
| `Carry.Began` | `FS_CarryBeganMessage` | **local on every machine**, raised from replicated state | the host raises it in the command; a client raises it from `OnRep_carrySlots` |
| `Carry.Ended` | `FS_CarryEndedMessage` | **local on every machine**, raised from replicated state | the same; the client reads the reason and the cause from the freed slot |
| `Cosmetic.Event.Carry.Picked` / `.Dropped` | the cosmetic event of Doc B21 | **needs the cosmetic channel** | the host raises it through `IBPI_CosmeticEventSink` |

Notes for the people who listen:

- **Animation (R07):** the carry pose follows `isCarryActive()` of that character (Doc R07 3.3), so bind `onCarryChanged` or listen to both messages and filter by `carrier`. The messages fire for **every** character on the machine.
- **UI (B17):** Doc B17's message table lists a "carry icon" for these two messages, while its HUD section says carry displays are not shown at all. Appendix C asks which is right; the messages are there either way.
- A refusal is not a message: it arrives on `onRequestRefused` of the requester's own R01 request component, with the reason tag.
- If a slot is freed and filled again between two network updates, a client sees one `Ended` and one `Began`, with the reason of the latest end. It can never end up showing the wrong item.

### C9.4 — When a player leaves, and what is never saved

- **Leaving** (Doc B20 3.6): `UBFL_Carry::handlePlayerLeft` (C7.3) drops the item where the player stands, and the host keeps it as a loose item. If B20 never calls it, the pawn's removal does the same through `EndPlay`.
- **Save** (B07 3.5, Index A93 RK2): nothing. `UCarryComponent` is not a save participant. A save made while somebody carries a log does not contain that log, and that is accepted (Index A102, ST-11). After a load every player's hands are empty, and no warning is shown when saving or quitting while carrying.
- **Host handover** (Doc B20 3.10): a normal save capture, so carried items are lost exactly as at any save.

### Verify (Section 9)

Listen server + one client (*Play As Listen Server*, 2 players), `AInventoryTestGameMode`:

| Check | Expect |
| --- | --- |
| Client picks up a loose `Log` (R01's pickup key) | the client's character shows the log on **both** machines; `Carry.Began` is raised on both, each with the client's character as `carrier` |
| Client calls `serverDropCarried(-1)` | a loose log appears in front of the client's character on both machines; both raise `Carry.Ended` with reason `Dropped` |
| Client carries two logs and drops with `-1` twice | the log picked up **last** goes first |
| Client calls `serverThrowCarried(-1, cameraForward)` | both machines see the flight and the landing; only the requester's hands change |
| Client sends a throw direction pointing behind the character | refused `Carry.FailReason.InvalidThrow`; only the requester is told; the log is still in the hands on both machines |
| Client calls `serverDropCarried(0)` with empty hands | refused `SlotEmpty`, only the requester is told |
| Client marked Downed (R01's `StubFlag downed 1 p1`) sends a drop | refused `NotAllowedInState` |
| Client sends 100 drop requests in one frame | after about `requestsPerSecond` the rest are `RateLimited` (the bucket is shared with the bag requests) |
| Host interrupts the client (`interruptCarry` on player 1's character) | both machines raise `Carry.Ended` with reason `Interrupted` **and the same cause tag** |
| A **late joiner** connects while another player carries a `Trunk` | it sees the trunk in that player's hands from replicated data; `Carry.Began` is raised once locally on the new machine |
| Client 2 calls `serverDropCarried` on client 1's component | impossible: the RPC is dropped, client 2 does not own that pawn |
| The client disconnects while carrying | the log lies where its character stood, on the host and on every remaining client |
| Offline (standalone) | the same requests work; the machine is its own host |

### Handoff
- **Input (R04 / B17)**: map the drop and throw actions and call the two requests as in C9.2.
- **B17**: add text for the four `Carry.FailReason.*` tags in `DT_ReasonText`; `SlotEmpty` should never reach a player and can share the neutral line.
- **R07** and **B17** listen as described in C9.3.

---

# SECTION 10 — Dev Tools, Tests and Final Acceptance

**Goal:** the closing "Dev Tools and Testing" section that Doc B22 §3.10 requires of every implementation guide.
**Spec:** Doc B22 §3.2 to §3.10.
**Depends on:** Sections 0–9.

### Files

```
Private/CarryFramework/DevTools/CarryDevCommands.cpp     (non-shipping only)
Private/CarryFramework/DevTools/CarryDebugView.cpp       (non-shipping only)
Private/CarryFramework/Tests/CarrySpec.cpp
Content/Awaken/Tests/TestMap_Inventory.umap              (reuse R01's map)
```

### C10.1, C10.2 — Dev commands (Doc B22 §3.2, §3.4)

The rules are the ones the R01 guide follows (B22 §3.2): names start `Awaken.<Domain>.<Verb>`; **host only**; they go **through the owner's public functions**, so the same checks and messages run as in play; each says what happened in `LogAwakenCarry`; **none saves**; compiled out of Shipping. The optional last argument `pN` picks player N (default 0), as in R01's commands.

`CarryDevCommands.cpp`

```cpp
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "CarryFramework/CarryLog.h"
#include "CarryFramework/UBFL_Carry.h"
#include "CarryFramework/UCarryComponent.h"
#include "CarryFramework/Stubs/CarryStubs.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"

namespace{
    bool isPlayerArgument(const FString& argument){
        return argument.Len() > 1 && argument.StartsWith(TEXT("p"), ESearchCase::CaseSensitive) && argument.Mid(1).IsNumeric();
    }

    // Returns null with a log line when not on the host or when the player does not exist.
    APlayerState* resolvePlayer(UWorld* world, const TArray<FString>& arguments){
        if(world == nullptr || !UBFL_Inventory::isServerWorld(world)){
            CARRY_LOG(Warning, world, "Host command: refused on a client (the Doc B22 cheat manager forwards it)");
            return nullptr;
        }
        int32 playerIndex = 0;
        for(const FString& argument : arguments){
            if(isPlayerArgument(argument)){
                playerIndex = FCString::Atoi(*argument.Mid(1));
            }
        }
        const AGameStateBase* gameState = world->GetGameState();
        if(gameState == nullptr || !gameState->PlayerArray.IsValidIndex(playerIndex)){
            CARRY_LOG(Warning, world, "No player %d", playerIndex);
            return nullptr;
        }
        return gameState->PlayerArray[playerIndex];
    }

    // The arguments that are not the player selector, in order.
    TArray<FString> plainArguments(const TArray<FString>& arguments){
        TArray<FString> plain;
        for(const FString& argument : arguments){
            if(!isPlayerArgument(argument)){
                plain.Add(argument);
            }
        }
        return plain;
    }

    // Through R01's grantOrDrop, so the real path runs: a free slot, otherwise loose beside the player.
    void giveCommand(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = resolvePlayer(world, arguments);
        const TArray<FString> plain = plainArguments(arguments);
        if(player == nullptr || plain.Num() < 1){
            CARRY_LOG(Warning, world, "Usage: Awaken.Carry.Give <itemId> [count] [pN]");
            return;
        }
        FS_ItemStack stack;
        stack.itemId = FName(*plain[0]);
        stack.quantity = plain.Num() > 1 ? FMath::Max(1, FCString::Atoi(*plain[1])) : 1;
        const FS_GrantResult result = UBFL_Inventory::grantOrDrop(player, stack);
        CARRY_LOG(Log, world, "Give %s x%d: carried %d, dropped %d, stored %d", *plain[0], stack.quantity, result.quantityCarried, result.quantityDropped, result.quantityStored);
    }

    void dropCommand(const TArray<FString>& arguments, UWorld* world){
        UCarryComponent* carry = UCarryComponent::findFor(resolvePlayer(world, arguments));
        const TArray<FString> plain = plainArguments(arguments);
        if(carry == nullptr){
            CARRY_LOG(Warning, world, "Usage: Awaken.Carry.Drop [slotIndex] [pN]");
            return;
        }
        const int32 slotIndex = plain.Num() > 0 ? FCString::Atoi(*plain[0]) : carry->getActiveSlotIndex();
        const FS_CarryEndResult result = carry->endCarry(slotIndex, E_EndCarryReason::Dropped);
        CARRY_LOG(Log, world, "Drop slot %d: %s", slotIndex, result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    void throwCommand(const TArray<FString>& arguments, UWorld* world){
        const APlayerState* player = resolvePlayer(world, arguments);
        UCarryComponent* carry = UCarryComponent::findFor(player);
        const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
        const TArray<FString> plain = plainArguments(arguments);
        if(carry == nullptr || pawn == nullptr){
            CARRY_LOG(Warning, world, "Usage: Awaken.Carry.Throw [slotIndex] [pN]");
            return;
        }
        const int32 slotIndex = plain.Num() > 0 ? FCString::Atoi(*plain[0]) : carry->getActiveSlotIndex();
        const FS_CarryEndResult result = carry->throwCarried(slotIndex, pawn->GetBaseAimRotation().Vector());
        CARRY_LOG(Log, world, "Throw slot %d: %s", slotIndex, result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    void interruptCommand(const TArray<FString>& arguments, UWorld* world){
        UCarryComponent* carry = UCarryComponent::findFor(resolvePlayer(world, arguments));
        const TArray<FString> plain = plainArguments(arguments);
        if(carry == nullptr || plain.Num() < 1){
            CARRY_LOG(Warning, world, "Usage: Awaken.Carry.Interrupt <Downed|Death|Restrained|PlayerLeft|Swim|Sleep> [pN]");
            return;
        }
        const FGameplayTag causeTag = FGameplayTag::RequestGameplayTag(FName(*(TEXT("Carry.InterruptCause.") + plain[0])), false);
        if(!causeTag.IsValid()){
            CARRY_LOG(Warning, world, "Interrupt: unknown cause '%s'", *plain[0]);
            return;
        }
        // The forced drop only. It does NOT make the player Downed or asleep: those states belong to their owners.
        const int32 dropped = carry->interruptCarry(causeTag);
        CARRY_LOG(Log, world, "Interrupt %s: %d thing(s) dropped", *plain[0], dropped);
    }

    // The situation most likely to break (Doc B22 §3.10): full hands. Adds things, so it is not safe to repeat.
    void fillHandsCommand(const TArray<FString>& arguments, UWorld* world){
        UCarryComponent* carry = UCarryComponent::findFor(resolvePlayer(world, arguments));
        const TArray<FString> plain = plainArguments(arguments);
        if(carry == nullptr || plain.Num() < 1){
            CARRY_LOG(Warning, world, "Usage: Awaken.Carry.FillHands <itemId> [pN]");
            return;
        }
        FS_InventoryEntry entry;
        entry.itemId = FName(*plain[0]);
        entry.totalQuantity = 1;
        int32 taken = 0;
        while(carry->getFreeSlotCount() > 0 && carry->beginCarry(entry).succeeded()){
            taken++;
        }
        // One more on purpose, to show the refusal. Nothing is created by a refused carry, so nothing is lost.
        const FS_CarryBeginResult extra = carry->beginCarry(entry);
        CARRY_LOG(Log, world, "FillHands %s: took %d, %d of %d slots free, one more -> %s", *plain[0], taken, carry->getFreeSlotCount(), carry->getSlotCount(), extra.succeeded() ? TEXT("accepted (unexpected)") : *extra.failReason.ToString());
    }

    // TEMPORARY — owners: Docs B08, B12. Spawns the stub carryable actor and puts it in the player's hands.
    void carryDummyCommand(const TArray<FString>& arguments, UWorld* world){
        const APlayerState* player = resolvePlayer(world, arguments);
        UCarryComponent* carry = UCarryComponent::findFor(player);
        const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
        if(carry == nullptr || pawn == nullptr){
            CARRY_LOG(Warning, world, "Usage: Awaken.Carry.CarryDummy [pN]");
            return;
        }
        AStubCarryableActor* dummy = world->SpawnActor<AStubCarryableActor>(pawn->GetActorLocation() + pawn->GetActorForwardVector() * 100.0f, FRotator::ZeroRotator);
        const FS_CarryBeginResult result = carry->beginCarryActor(dummy);
        if(!result.succeeded() && dummy != nullptr){
            dummy->Destroy();
        }
        CARRY_LOG(Log, world, "CarryDummy: %s", result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    // Read only. Runs on any machine: it prints the replicated state this machine holds.
    void dumpCommand(const TArray<FString>& arguments, UWorld* world){
        const AGameStateBase* gameState = world != nullptr ? world->GetGameState() : nullptr;
        if(gameState == nullptr){
            return;
        }
        for(const APlayerState* player : gameState->PlayerArray){
            const UCarryComponent* carry = UCarryComponent::findFor(player);
            if(carry == nullptr){
                continue;
            }
            const FGameplayTag cause = carry->getLastInterruptCause();
            CARRY_LOG(Log, world, "%s: %s, %d of %d slots free, active slot %d, speed x%.2f, last interrupt %s", *player->GetPlayerName(), carry->isCarryActive() ? TEXT("CARRYING") : TEXT("hands empty"), carry->getFreeSlotCount(), carry->getSlotCount(), carry->getActiveSlotIndex(), carry->getSpeedMultiplier(), cause.IsValid() ? *cause.ToString() : TEXT("none"));
            for(int32 index = 0; index < carry->getCarrySlots().Num(); index++){
                const FS_CarrySlotState& slot = carry->getCarrySlots()[index];
                CARRY_LOG(Log, world, "   [%d] %s%s", index, slot.isOccupied() ? *slot.itemId.ToString() : TEXT("-"), slot.holdsActor ? TEXT(" (actor)") : TEXT(""));
            }
        }
    }

    FAutoConsoleCommandWithWorldAndArgs cmdGive(TEXT("Awaken.Carry.Give"), TEXT("<itemId> [count] [pN]: grant a heavy item; it goes to the hands or lands loose"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&giveCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdDrop(TEXT("Awaken.Carry.Drop"), TEXT("[slotIndex] [pN]: drop a carried thing (default: the active slot)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&dropCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdThrow(TEXT("Awaken.Carry.Throw"), TEXT("[slotIndex] [pN]: throw a carried item where the player looks"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&throwCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdInterrupt(TEXT("Awaken.Carry.Interrupt"), TEXT("<cause> [pN]: the forced drop of B07 3.6 with that cause"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&interruptCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdFill(TEXT("Awaken.Carry.FillHands"), TEXT("<itemId> [pN]: fill every carry slot, then try one more (the full-hands situation)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&fillHandsCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdDummy(TEXT("Awaken.Carry.CarryDummy"), TEXT("[pN]: TEMPORARY, carry a stub actor (stands in for a corpse)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&carryDummyCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdDump(TEXT("Awaken.Carry.Dump"), TEXT(": print every player's carry slots as this machine sees them (read)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&dumpCommand));
}

#endif
```

| Command | What it does | Where it runs |
| --- | --- | --- |
| `Awaken.Carry.Give <itemId> [count] [pN]` | grants a heavy item through `grantOrDrop`: into free slots, the rest loose | Host with target |
| `Awaken.Carry.Drop [slotIndex] [pN]` | drops a carried thing through `endCarry` (default: the active slot) | Host with target |
| `Awaken.Carry.Throw [slotIndex] [pN]` | throws a carried item where that player looks, through `throwCarried` | Host with target |
| `Awaken.Carry.Interrupt <cause> [pN]` | the forced drop with that cause (`Downed`, `Death`, `Restrained`, `PlayerLeft`, `Swim`, `Sleep`). It does not change the player's life state | Host with target |
| `Awaken.Carry.FillHands <itemId> [pN]` | fills every slot, then tries one more and logs the refusal. **The situation most likely to break** | Host with target |
| `Awaken.Carry.CarryDummy [pN]` | TEMPORARY: spawns the stub carryable actor into the hands | Host with target |
| `Awaken.Carry.Dump` (read) | prints every player's slots as this machine sees them; run it on the client to check replication | Local |
| `Awaken.Carry.SlotCount <n>` (cvar) | overrides the slot count for tests; `0` = hands that take nothing; `-1` returns to the project setting | Local to the host |

Useful together with R01's commands: `Awaken.Item.Give Stick 5` (ordinary items go to the bag, to compare), `Awaken.Bag.SetBagless`, `Awaken.Inventory.StubFlag downed 1`.

### C10.3 — Debug view `Awaken.Debug.Carry` (Doc B22 §3.5)

Doc B22 lists this view as "the carried object, the interrupt cause and any rope or slider line with its direction". The first two are drawn here; the line part belongs to Doc B09 and is added to the same view by its guide. A view draws **locally**, only in non-shipping builds, and never replicates.

`CarryDebugView.cpp`

```cpp
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "CarryFramework/UCarryComponent.h"

namespace{
    TAutoConsoleVariable<int32> cvarDebugCarry(TEXT("Awaken.Debug.Carry"), 0, TEXT("1 = draw each player's carry slots, the active slot, the slowdown and the last interrupt cause on screen."), ECVF_Default);
    FTSTicker::FDelegateHandle carryTickerHandle;

    bool drawCarryDebug(float deltaTime){
        if(GEngine == nullptr){
            return true;
        }
        int32 messageKey = 9400;
        for(const FWorldContext& context : GEngine->GetWorldContexts()){
            const UWorld* world = context.World();
            if(world == nullptr || world->GetGameState() == nullptr){
                continue;
            }
            for(const APlayerState* player : world->GetGameState()->PlayerArray){
                const UCarryComponent* carry = UCarryComponent::findFor(player);
                if(carry == nullptr){
                    continue;
                }
                const FGameplayTag cause = carry->getLastInterruptCause();
                GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, carry->isCarryActive() ? FColor::Orange : FColor::Green, FString::Printf(TEXT("[Carry] %s: %s, %d/%d free, speed x%.2f, last interrupt %s"), *player->GetPlayerName(), carry->isCarryActive() ? TEXT("CARRYING") : TEXT("hands empty"), carry->getFreeSlotCount(), carry->getSlotCount(), carry->getSpeedMultiplier(), cause.IsValid() ? *cause.ToString() : TEXT("none")));
                const int32 activeIndex = carry->getActiveSlotIndex();
                for(int32 index = 0; index < carry->getCarrySlots().Num(); index++){
                    const FS_CarrySlotState& slot = carry->getCarrySlots()[index];
                    GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::White, FString::Printf(TEXT("   [%d] %s%s%s"), index, slot.isOccupied() ? *slot.itemId.ToString() : TEXT("-"), slot.holdsActor ? TEXT(" (actor)") : TEXT(""), index == activeIndex ? TEXT("  <- active") : TEXT("")));
                }
            }
        }
        return true;
    }

    void onCarryDebugChanged(IConsoleVariable* variable){
        if(variable->GetInt() != 0 && !carryTickerHandle.IsValid()){
            carryTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&drawCarryDebug), 0.25f);   // only while the view is on (Doc02 §5)
        }else if(variable->GetInt() == 0 && carryTickerHandle.IsValid()){
            FTSTicker::GetCoreTicker().RemoveTicker(carryTickerHandle);
            carryTickerHandle.Reset();
        }
    }

    struct FCarryDebugRegistrar{
        FCarryDebugRegistrar(){
            cvarDebugCarry->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&onCarryDebugChanged));
        }
    };
    FCarryDebugRegistrar carryDebugRegistrar;
}

#endif
```

### C10.4 — Log lines (Doc B22 §3.6)

Category `LogAwakenCarry`; every line names the machine role (done by `CARRY_LOG`). A Log-level line exists for every state change:

| State change | Where the line is |
| --- | --- |
| Framework started | `UCarryServices::Initialize` |
| A carry began (an item or an actor) | `afterBegin` |
| A carry ended, with its reason and, for a forced drop, its cause | `endCarryInternal` |
| An item was thrown | `UBFL_Carry::throwEntry` |
| A thrown item landed, and what it hit | `AThrownItemActor::land` |
| The loose item that a drop created | R01's `ULooseItemSpawner` (`LogAwakenInventory`) |
| Every refusal returned to a client | R01's `UInventoryRequestComponent::refuse` (the reason tag, in `LogAwakenInventory`) |
| Dev commands | each command logs its result |

A refused `beginCarry` from a grant is not logged here: R01's `grantOrDrop` line already says "carried 0, dropped 1".

### C10.5 — Automation specs

Run from *Session Frontend → Automation* filtered by `Awaken.Carry`, or:

```
UnrealEditor-Cmd.exe <Project>.uproject -ExecCmds="Automation RunTests Awaken.Carry; Quit" -unattended -nullrhi -log
```

`Tests/CarrySpec.cpp`

```cpp
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "EngineUtils.h"
#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestCharacter.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/Stubs/InventoryStubs.h"
#include "ItemInventoryFramework/03Interfaces/BPI_LooseItemSpawner.h"
#include "ItemInventoryFramework/03Interfaces/BPI_SaveParticipantStub.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "CarryFramework/AThrownItemActor.h"
#include "CarryFramework/UBFL_Carry.h"
#include "CarryFramework/UCarryComponent.h"
#include "CarryFramework/02Tags/T_CarryFramework.h"
#include "CarryFramework/Stubs/CarryStubs.h"

namespace{
    // One unit of a stackable heavy item, as R01 hands it to the seam.
    FS_InventoryEntry makeUnit(FName itemId){
        FS_InventoryEntry entry;
        entry.itemId = itemId;
        entry.totalQuantity = 1;
        return entry;
    }

    // A player with a character, and that character's carry component.
    UCarryComponent* makeCarrier(FInventoryTestFixture& fixture, AInventoryTestPlayerState*& outPlayer){
        outPlayer = fixture.makePlayer();
        fixture.makeCharacterFor(outPlayer);
        return UCarryComponent::findFor(outPlayer);
    }
}

BEGIN_DEFINE_SPEC(FCarrySpec, "Awaken.Carry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FCarrySpec)

void FCarrySpec::Define(){
    // ---- Spec 1 — slots and capacity (tier 1) ----
    Describe("Carry slots", [this](){
        It("fills one slot per unit of cost, refuses what the hands cannot take, and never takes an ordinary item", [this](){
            FInventoryTestFixture fixture;
            fixture.addHeavy(TEXT("Log"), 1);
            fixture.addHeavy(TEXT("Trunk"), 2);
            fixture.addStackable(TEXT("Stick"), 10);
            AInventoryTestPlayerState* player = nullptr;
            UCarryComponent* carry = makeCarrier(fixture, player);

            TestFalse(TEXT("hands start empty"), carry->isCarryActive());
            TestTrue(TEXT("an ordinary item is not carried"), carry->beginCarry(makeUnit(TEXT("Stick"))).failReason == T_Carry_FailReason_NotCarryable);

            TestEqual(TEXT("the first log goes to slot 0"), carry->beginCarry(makeUnit(TEXT("Log"))).slotIndex, 0);
            TestTrue(TEXT("carrying"), carry->isCarryActive());
            TestTrue(TEXT("a trunk needs two free slots"), carry->beginCarry(makeUnit(TEXT("Trunk"))).failReason == T_Interaction_FailReason_HandsFull);
            TestEqual(TEXT("the second log goes to slot 1"), carry->beginCarry(makeUnit(TEXT("Log"))).slotIndex, 1);
            TestTrue(TEXT("a third log is refused with HandsFull"), carry->beginCarry(makeUnit(TEXT("Log"))).failReason == T_Interaction_FailReason_HandsFull);
            TestEqual(TEXT("two logs for a cost"), carry->getCarriedItemsForCost(TEXT("Log")), 2);
            TestEqual(TEXT("the log picked up last is active"), carry->getActiveSlotIndex(), 1);
            TestTrue(TEXT("R01's seam now answers from the real component"), fixture.services()->isCarryActive(player));

            carry->interruptCarry(T_Carry_InterruptCause_Downed);
            TestEqual(TEXT("a trunk fills both slots"), carry->beginCarry(makeUnit(TEXT("Trunk"))).slotIndex, 0);
            TestEqual(TEXT("no slot is free"), carry->getFreeSlotCount(), 0);
            TestEqual(TEXT("but it is ONE trunk"), carry->getCarriedItemsForCost(TEXT("Trunk")), 1);
        });
    });

    // ---- Spec 2 — ending a carry (tier 1) ----
    Describe("Ending a carry", [this](){
        It("drops a loose item, hands a placed or sent item to the caller, and frees every slot of a two-slot item", [this](){
            FInventoryTestFixture fixture;
            fixture.addHeavy(TEXT("Log"), 1);
            fixture.addHeavy(TEXT("Trunk"), 2);
            AInventoryTestPlayerState* player = nullptr;
            UCarryComponent* carry = makeCarrier(fixture, player);
            const UStubLooseItemSpawner* spawner = fixture.stub<UStubLooseItemSpawner, UBPI_LooseItemSpawner>();

            carry->beginCarry(makeUnit(TEXT("Log")));
            TestTrue(TEXT("drop"), carry->endCarry(0, E_EndCarryReason::Dropped).succeeded());
            TestEqual(TEXT("one loose item"), spawner->spawnedEntries.Num(), 1);
            TestTrue(TEXT("it is the log"), spawner->spawnedEntries[0].itemId == FName(TEXT("Log")) && spawner->spawnedEntries[0].totalQuantity == 1);
            TestFalse(TEXT("hands empty"), carry->isCarryActive());
            TestTrue(TEXT("an empty slot is refused"), carry->endCarry(0, E_EndCarryReason::Dropped).failReason == T_Carry_FailReason_SlotEmpty);

            carry->beginCarry(makeUnit(TEXT("Log")));
            carry->beginCarry(makeUnit(TEXT("Log")));
            TestEqual(TEXT("Building uses what is carried, never more"), carry->consumeCarriedForCost(TEXT("Log"), 3), 2);
            TestEqual(TEXT("a placed item is not dropped"), spawner->spawnedEntries.Num(), 1);

            carry->beginCarry(makeUnit(TEXT("Log")));
            const FS_CarryEndResult sent = carry->endCarry(0, E_EndCarryReason::Sent);
            TestTrue(TEXT("a sent item is handed to the caller"), sent.succeeded() && sent.entry.itemId == FName(TEXT("Log")));
            TestEqual(TEXT("and is not dropped either"), spawner->spawnedEntries.Num(), 1);

            carry->beginCarry(makeUnit(TEXT("Trunk")));
            TestTrue(TEXT("ending the second slot of a trunk"), carry->endCarry(1, E_EndCarryReason::Dropped).succeeded());
            TestEqual(TEXT("frees both slots"), carry->getFreeSlotCount(), 2);
            TestEqual(TEXT("and drops one trunk"), spawner->spawnedEntries.Num(), 2);
        });
    });

    // ---- Spec 3 — interrupts and "never saved" (tier 1) ----
    Describe("Interrupts", [this](){
        It("drops everything with the cause, loses nothing, and the component is not a save participant", [this](){
            FInventoryTestFixture fixture;
            fixture.addHeavy(TEXT("Log"), 1);
            AInventoryTestPlayerState* player = nullptr;
            UCarryComponent* carry = makeCarrier(fixture, player);
            const UStubLooseItemSpawner* spawner = fixture.stub<UStubLooseItemSpawner, UBPI_LooseItemSpawner>();

            TestEqual(TEXT("nothing to interrupt"), carry->interruptCarry(T_Carry_InterruptCause_Sleep), 0);
            carry->beginCarry(makeUnit(TEXT("Log")));
            carry->beginCarry(makeUnit(TEXT("Log")));
            TestEqual(TEXT("both are dropped"), carry->interruptCarry(T_Carry_InterruptCause_Downed), 2);
            TestEqual(TEXT("as loose items"), spawner->spawnedEntries.Num(), 2);
            TestFalse(TEXT("hands empty"), carry->isCarryActive());
            TestTrue(TEXT("the cause is remembered"), carry->getLastInterruptCause() == T_Carry_InterruptCause_Downed);
            TestTrue(TEXT("the freed slot says why"), carry->getCarrySlots()[0].lastEndReason == E_EndCarryReason::Interrupted && carry->getCarrySlots()[0].lastInterruptCause == T_Carry_InterruptCause_Downed);
            TestFalse(TEXT("carry state is never saved (B07 3.5)"), carry->GetClass()->ImplementsInterface(UBPI_SaveParticipant::StaticClass()));
        });
    });

    // ---- Spec 4 — the real seam inside R01's grant (tier 2: two frameworks together) ----
    Describe("Heavy grant", [this](){
        It("goes to free slots, the surplus lands loose, and nothing enters the bag", [this](){
            FInventoryTestFixture fixture;
            fixture.addHeavy(TEXT("Log"), 1);
            AInventoryTestPlayerState* player = nullptr;
            UCarryComponent* carry = makeCarrier(fixture, player);
            const UStubLooseItemSpawner* spawner = fixture.stub<UStubLooseItemSpawner, UBPI_LooseItemSpawner>();

            FS_ItemStack stack;
            stack.itemId = TEXT("Log");
            stack.quantity = 3;
            const FS_GrantResult granted = UBFL_Inventory::grantOrDrop(player, stack);
            TestEqual(TEXT("two in the hands"), granted.quantityCarried, 2);
            TestEqual(TEXT("one on the ground"), granted.quantityDropped, 1);
            TestEqual(TEXT("none stored"), granted.quantityStored, 0);
            TestEqual(TEXT("the bag holds no log"), UPlayerInventoryComponent::findFor(player)->getPersonalInventory()->getTotalQuantity(TEXT("Log")), 0);
            TestEqual(TEXT("the component agrees"), carry->getCarriedItemsForCost(TEXT("Log")), 2);
            TestEqual(TEXT("one loose item was spawned"), spawner->spawnedEntries.Num(), 1);
        });
    });

    // ---- Spec 5 — throw (tier 2: the component, the pipeline actor and R01's loose-item seam) ----
    Describe("Throw", [this](){
        It("leaves the hands at once and lands as a loose item with the same entry", [this](){
            FInventoryTestFixture fixture;
            fixture.addHeavy(TEXT("Stone"), 1, 20.0f);
            AInventoryTestPlayerState* player = nullptr;
            UCarryComponent* carry = makeCarrier(fixture, player);
            const UStubLooseItemSpawner* spawner = fixture.stub<UStubLooseItemSpawner, UBPI_LooseItemSpawner>();

            carry->beginCarry(makeUnit(TEXT("Stone")));
            TestTrue(TEXT("a zero direction is refused"), carry->throwCarried(0, FVector::ZeroVector).failReason == T_Carry_FailReason_InvalidThrow);
            TestTrue(TEXT("and the stone is still in the hands"), carry->isCarryActive());

            TestTrue(TEXT("throw"), carry->throwCarried(0, FVector::ForwardVector).succeeded());
            TestFalse(TEXT("hands empty at once"), carry->isCarryActive());
            TestEqual(TEXT("not a loose item while it flies"), spawner->spawnedEntries.Num(), 0);

            AThrownItemActor* thrown = nullptr;
            for(TActorIterator<AThrownItemActor> iterator(fixture.world()); iterator; ++iterator){
                thrown = *iterator;
            }
            TestNotNull(TEXT("one item is in flight"), thrown);
            if(thrown != nullptr){
                TestTrue(TEXT("it carries the entry"), thrown->getEntry().itemId == FName(TEXT("Stone")));
                thrown->forceLand();
                TestEqual(TEXT("it landed as a loose item"), spawner->spawnedEntries.Num(), 1);
            }
        });
    });

    // ---- Spec 6 — carrying an actor (tier 2) ----
    Describe("Actor carry", [this](){
        It("fills slots like an item, can only be put down, and is told when it is", [this](){
            FInventoryTestFixture fixture;
            AInventoryTestPlayerState* player = nullptr;
            UCarryComponent* carry = makeCarrier(fixture, player);
            const UStubLooseItemSpawner* spawner = fixture.stub<UStubLooseItemSpawner, UBPI_LooseItemSpawner>();
            AStubCarryableActor* dummy = fixture.world()->SpawnActor<AStubCarryableActor>();

            TestTrue(TEXT("carry the dummy"), carry->beginCarryActor(dummy).succeeded());
            TestTrue(TEXT("carrying"), carry->isCarryActive() && carry->isActorCarry(0));
            TestEqual(TEXT("the actor was told"), dummy->beganCount, 1);
            TestTrue(TEXT("it is attached to the carrier"), dummy->GetAttachParentActor() == carry->GetOwner());
            TestEqual(TEXT("it is never a building material"), carry->getCarriedItemsForCost(TEXT("Dummy")), 0);
            TestTrue(TEXT("it cannot be thrown"), carry->throwCarried(0, FVector::ForwardVector).failReason == T_Carry_FailReason_CannotThrow);
            TestTrue(TEXT("it cannot be sent"), carry->endCarry(0, E_EndCarryReason::Sent).failReason == T_Interaction_FailReason_NotAllowedInState);
            TestTrue(TEXT("still carried after the refusals"), carry->isCarryActive());

            TestTrue(TEXT("put it down"), carry->endCarry(0, E_EndCarryReason::Dropped).succeeded());
            TestEqual(TEXT("the actor was told why"), dummy->endedCount, 1);
            TestTrue(TEXT("with the reason"), dummy->lastReason == E_EndCarryReason::Dropped);
            TestTrue(TEXT("it is detached"), dummy->GetAttachParentActor() == nullptr);
            TestEqual(TEXT("no loose item: it is itself the object"), spawner->spawnedEntries.Num(), 0);
        });
    });
}

#endif
```

| Test | Tier | Sets up | Checks |
| --- | --- | --- | --- |
| Carry slots | 1 | a log (cost 1), a trunk (cost 2), a stick (cost 0) | slot filling, `HandsFull`, `NotCarryable`, the active slot, cost counting, R01's seam answers from the component |
| Ending a carry | 1 | logs and a trunk | a drop spawns a loose item; Placed and Sent do not; a two-slot item frees both slots; an empty slot is refused |
| Interrupts | 1 | two logs | both dropped with the cause; nothing lost; the freed slot keeps reason and cause; not a save participant |
| Heavy grant | 2 | R01's `grantOrDrop` with 3 logs | 2 carried, 1 dropped, 0 in the bag |
| Throw | 2 | a stone, the thrown actor, R01's loose-item seam | the hands empty at once; not loose in flight; lands as a loose item |
| Actor carry | 2 | the stub carryable actor | attach, the refusals of throw and send, detach, the callbacks |
| Replication and requests | 3 (manual) | C10.7 | what the client must see |

Doc B22's milestone M2 lists "carry interrupts" among its tests: Spec 3 here is that test. Its starter test "R13 Save round trip" lists carry state among the things that are **not** restored; the last line of Spec 3 is Carry's half of it.

### C10.6 — Test map and scenario

Reuse R01's `TestMap_Inventory` (GameMode override `AInventoryTestGameMode`, two PlayerStarts). Add one water volume deep enough to swim in. Loose items are never saved with a map, so the logs for a test come from the commands (`Awaken.Carry.Give Log 3` leaves the surplus on the ground). The **"full hands" scenario** is `Awaken.Carry.FillHands Log`, which ends by logging `Interaction.FailReason.HandsFull`.

### C10.7 — The two-player check (Doc B22 §3.8, §3.10)

Editor *Play As Listen Server*, 2 players (host = player 0, client = player 1), then once offline. Turn on `Awaken.Debug.Carry 1` on **both** machines.

1. Host: `Awaken.Carry.Give Log 1 p1`. **The client sees** a log in its own hands and `CARRYING` in its debug view; **the host sees the same log** on player 1's character. The host's own hands are empty.
2. Host: `Awaken.Carry.Give Log 2 p1`. Both machines show two logs on player 1; the third lies loose in front of player 1 on both machines.
3. Client: walk to the loose log and press pickup. Refused `HandsFull`, **only the client** is told, and the log stays on the ground on both machines.
4. Client: try to eat, open a chest, and press a hotbar key. All refused `HandsFull`. Jump and crouch work.
5. Client: drop (`serverDropCarried(-1)` from the drop key or a test widget). The log picked up **last** lands in front of the client on both machines; the client is still `CARRYING` (one log left).
6. Client: throw (`serverThrowCarried(-1, cameraForward)`). Both machines see the flight and the same landing spot; the client now shows `hands empty` on both.
7. Host: `Awaken.Carry.FillHands Log p1`, then `Awaken.Carry.Interrupt Downed p1`. Both logs drop **at player 1's feet** on both machines; both debug views show `last interrupt Carry.InterruptCause.Downed`.
8. Host: `Awaken.Carry.Give Trunk 1 p1`. Both machines show both slots as `Trunk`, drawn once. Client walks into the deep water: the trunk drops when swimming begins, cause `Swim`.
9. Host: `Awaken.Carry.CarryDummy p1`. The dummy is attached to player 1 on both machines. `Awaken.Carry.Throw p1` → `CannotThrow`. `Awaken.Carry.Drop p1` → it stands on the ground on both machines.
10. Host: `Awaken.Carry.Give Log 1 p1`. A **late joiner** connects now: it sees the log in player 1's hands from replicated data.
11. The client disconnects while carrying that log: the log lies where its character stood, on the host and on the late joiner.
12. Repeat steps 1, 2, 5, 6 and 7 **offline** (standalone): same results on the one machine.

### C10.8 — Stub register (what to delete, and when)

| Stub / temporary item | File | Owner | Remove when |
| --- | --- | --- | --- |
| `UStubThrowImpactSink` | `Stubs/CarryStubs.*` | Doc R03 Combat | R03 registers an `IBPI_ThrowImpactSink` with `registerSeam` |
| `AStubCarryableActor`, the command `Awaken.Carry.CarryDummy` | `Stubs/CarryStubs.*`, `DevTools/` | Docs B08, B12 | the corpse implements `IBPI_CarryableActor`. Keep the stub for Spec 6 if you like |
| Duplicate tag definitions `Action.Dodge`, `.Sprint`, `.Block`, `.WeaponCharge` | `T_CarryFramework.*` | Doc R04 | R04's tag header exists |
| `carryComponent` on `AInventoryTestCharacter` | R01 `Stubs/` | Docs B19, R07 | the real Character carries the component |
| The local carried-item meshes (`refreshVisuals`, `showCarriedMeshes`, `carrySocketNames`) | `UCarryComponent.*`, `UCarrySettings.*` | Doc R07 | R07 draws the carried item with its carry pose; then set `showCarriedMeshes = false` or delete the function |
| The engine swim hook (`onMovementModeChanged`) | `UCarryComponent.cpp` | Doc R07 | R07's `BeginSwim` calls `interruptCarry` and its Swim state does not use the engine's swimming mode. Harmless if left |
| **Removed by this guide:** `UStubCarryService` and the `carrying` half of `Awaken.Inventory.StubFlag` | R01 `Stubs/InventoryStubs.*`, R01 dev commands | Doc B07 (us) | done in C4.2 |

### Handoff notes for teammates

| Doc | You must |
| --- | --- |
| **R01** (Item & Inventory) | nothing new: the seam is real now. Patches applied by this guide: C2.1, C2.2 (item data), C2.4 (fixture), C3.8 (stub Character), C4.2 (stub removed), C7.5 (hotbar refused while carrying) |
| **R03** (Combat) | `IsAttackAllowed` reads `UBFL_Carry::isCarryActive`; implement `IBPI_ThrowImpactSink`; throw weapons with `UBFL_Carry::throwEntry` (C6.4) |
| **R04** (Abilities) | block Dodge, Sprint, Block, Reload on `Carry.State.Active`; implement `cancelAction` for the five tags; take over the four `Action.*` tags |
| **R07** (Animation) | carry pose from `isCarryActive` (bind `onCarryChanged`); `getSpeedMultiplier` in the speed stack; refuse climb and squeeze with `getHandsBlockReason`; call `interruptCarry(…, Swim)` in `BeginSwim`; hide the held item under the pose; decide whether you draw the carried item (stub register) |
| **B01** (Building) | cost sourcing with `getCarriedItemsForCost` then `consumeCarriedForCost` (C5.3); data check: no heavy item in a manual cost |
| **B02** (Checkpoint & Respawn) | `interruptCarry` on Downed, on a final death (before the pawn is removed) and on Restrained (C7.3) |
| **B08** (Harvest & Corpse) | grant heavy yields with `grantOrDrop`; the corpse implements `IBPI_CarryableActor`; `BeginCarryCorpse` calls `beginCarryActor`; refuse the Axe cut and a harvest while the harvester carries |
| **B09** (Transport) | `RequestSendCarried`: `getActiveSlotIndex`, refuse an actor carry, `endCarry(…, Sent)` (C5.3); add the line part to `Awaken.Debug.Carry` |
| **B12** (AI) | add `UCarryComponent` to NPCs that may rescue; the wounded NPC implements `IBPI_CarryableActor`; `BeginCarryAlly` calls `beginCarryActor` |
| **B17** (UI) | `CanOpenBag` refuses with `getHandsBlockReason`; text for the four `Carry.FailReason.*` tags; decide the carry icon (Appendix C) |
| **B18** (Interaction) | the server's hands check of 3.5 uses `getHandsBlockReason`; an interaction that uses the carried item (deposit, place, send) skips it; drop and throw input reach `serverDropCarried` / `serverThrowCarried` |
| **B20** (Session) | call `UBFL_Carry::handlePlayerLeft(player)` on `Session.PlayerLeft` |
| **B21 / R09 / R15** | play the pick-up and drop effect and sound for `Cosmetic.Event.Carry.Picked` / `.Dropped` |
| **R02, R11, R12** | refuse a crafting session, a dialogue start and a sleep registration with `getHandsBlockReason`; R12 calls `interruptCarry(…, Sleep)` when the sleep starts; R02's data check: no heavy item as a recipe ingredient |
| **B19** | add `UCarryComponent` to the real Character class |

### C10.9 — Final review: every confirmed B07 rule maps to a task

| B07 rule | Where |
| --- | --- |
| `UCarryComponent`, a Character component; transient (§2) | C3 |
| A small fixed number of slots, tuning data (3.1) | C0.4, C3 `getSlotCount` |
| A cost consumes that many slots; each slot holds exactly one heavy item, no quantity (3.1, A103) | C3 `fillSlots`, C4 (C4 decision) |
| Heavy items go to the hands only and never enter the bag; everything else goes to the bag (A93 UD9, A103 S4-F01) | C4 (and R01 3.7 step 0, already built) |
| A heavy pickup that does not fit is refused with `HandsFull` and stays where it lies (3.1) | C4 |
| A heavy harvest yield goes into a free slot or lands loose (A103) | C4.2 through `grantOrDrop` |
| A cost larger than the slots can never be picked up (3.1) | C4, data warning C2.2 |
| The Axe and the map are never carry items (A92) | C2.2 |
| `Carry.State.Active` while any slot is occupied; also on the Ability System Component (3.2, IG23) | C3 `applyCarryStateTag`, C4, C5 |
| A carry that begins ends a running Dodge, Sprint, Block, Reload or Weapon Charge (3.2, IG25) | C4 `afterBegin` |
| `GetCarriedItemsForCost` is what Building calls first (3.3) | C3, C5.2, C5.3 |
| Throw reuses one generic pipeline with a blunt-impact damage (3.4) | C6 |
| `EndCarry(SlotIndex, reason)` frees the slot whichever path ends it (3.4) | C5 |
| Not saved; hands empty on load; no warning (3.5, A93 RK2) | C3 design rule, C9.4, Spec 3 |
| Carry applies on Peaceful as on every difficulty (3.5) | no code: nothing here reads the difficulty |
| A hit never knocks an item loose (round 4) | no code, on purpose; Verify of Section 7 |
| The interrupt table; a forced drop ends with `Interrupted`; no row destroys the item (3.6, S4-F37) | C7 |
| Everything not on the allowed list is refused; refusing never loses an item (3.6, S4-F36) | C7.4, C7.5 |
| A carried item has no lifetime until it is dropped (3.6, S4-F19) | C5 (it is data, not a loose actor, while carried) |
| A rope press while carrying sends the item, reason `Sent` (A92) | C5.3 |
| A carried corpse is Heavy; dropped it is an ordinary corpse; a corpse is dropped on disconnect and its timer resumes (A93 UD11, A106) | C8, C7.3 |
| `Carry.Began` / `Carry.Ended`, local on every machine from replicated state (§9) | C3, C9.3 |
| Cosmetic events `Carry.Picked` / `Carry.Dropped` (A89) | C4, C5 |
| Server-authoritative commands; the state replicates to everyone (§10) | C3, C9 |
| A throw is simulated on the server; clients show the result (§10) | C6 |
| `Session.PlayerLeft`: the carried item is dropped where the leaving player stands (A89) | C7.3, safety net C7.2 |

### The closing section every guide must carry (Doc B22 §3.10)

| Part | B07 |
| --- | --- |
| **Commands** | `Awaken.Carry.Give`, `.Drop`, `.Throw`, `.Interrupt`, `.CarryDummy`, `.Dump` (read), `.FillHands` (the situation most likely to break: full hands), `.SlotCount` (cvar) |
| **Debug view** | `Awaken.Debug.Carry` (each player's slots, the active slot, the slowdown, the last interrupt cause; Doc B09 adds its line) |
| **Log category** | `LogAwakenCarry`, role-prefixed, one Log line per state change; refusals log their reason tag through R01's request component |
| **Automated tests** | Carry slots (tier 1), Ending a carry (tier 1), Interrupts (tier 1), Heavy grant (tier 2), Throw (tier 2), Actor carry (tier 2), Replication and requests (tier 3, manual two-player) |
| **Two-player check** | C10.7 |
| **Done when** | the commands run, the tests pass, C10.7 has been done on a listen server plus one client and once offline |

---

# Appendix A — Files To Create (Doc02 §20.3 layout)

```
Public/CarryFramework/
    00Enums/E_CarryFramework.h
    01Structs/S_CarryFramework.h
    02Tags/T_CarryFramework.h
    03Interfaces/BPI_CarryableActor.h  (incoming)   BPI_ThrowImpactSink.h  (outgoing seam)
    04Delegates/D_CarryFramework.h
    CarryLog.h   CarryServices.h   UCarrySettings.h
    UCarryComponent.h   UBFL_Carry.h   AThrownItemActor.h
Private/CarryFramework/
    02Tags/T_CarryFramework.cpp
    CarryLog.cpp   CarryServices.cpp   UCarrySettings.cpp
    UCarryComponent.cpp   UBFL_Carry.cpp   AThrownItemActor.cpp
    Stubs/CarryStubs.h/.cpp
    DevTools/CarryDevCommands.cpp   CarryDebugView.cpp
    Tests/CarrySpec.cpp
Content/Awaken/Items/*            (Log, Stone, Trunk; Rock as the ordinary ingredient)
```

Files of the R01 guide that this guide **patches**: `Awaken.Build.cs` (C0.2), `UDA_ItemDefinition.h/.cpp` (C2.1, C2.2), `InventoryTestFixture.h/.cpp` (C2.4), `AInventoryTestCharacter.h/.cpp` (C3.8), `InventoryStubs.h/.cpp`, `InventoryServices.cpp` and the `StubFlag` dev command (C4.2, stub removed), `UInventoryRequestComponent.cpp` (C7.5).

> B07 §13 lists `S_CarrySlotState.h` as its own file and "Carry GameplayTags registration". That list is older than Doc02 §20.3, which asks for one struct file and one tag file per framework; this guide follows §20.3 and puts the struct in `S_CarryFramework.h` and the tags in `T_CarryFramework.h/.cpp`.

# Appendix B — Tags and data fields to register

- **Already in the Tag Registry (owner B07):** `Carry.Began`, `Carry.Ended`, `Carry.State.Active`, `Carry.WeightClass.*`, `Cosmetic.Event.Carry.Picked`, `Cosmetic.Event.Carry.Dropped`.
- **New leaves:** `Carry.WeightClass.Light`, `Carry.WeightClass.Heavy`.
- **New, PROPOSED:** `Carry.FailReason.NotCarryable`, `.SlotEmpty`, `.CannotThrow`, `.InvalidThrow`; `Carry.InterruptCause.Downed`, `.Death`, `.Restrained`, `.PlayerLeft`, `.Swim`, `.Sleep`, `.PawnRemoved`.
- **Still waiting in the Tag Registry:** the end reason `Interrupted` of `E_EndCarryReason` is listed there as PROPOSED; B07 itself marks it designer-approved (Index A103). This guide builds it.
- **Reused from other owners:** `Interaction.FailReason.HandsFull`, `.NotAllowedInState` (B18); `Inventory.FailReason.NotServer`, `.NotFound`, `.InvalidRequest`, `.RateLimited` (R01); `Action.Dodge`, `.Sprint`, `.Block`, `.Reload`, `.WeaponCharge` (R04); `Combat.FailReason.CarryActive` (R03, theirs to use).
- **New data to log in the Index:** `throwImpactDamage` on `DA_ItemDefinition`; the settings `carrySlotCount`, `carrySpeedMultiplier`, `carrySocketNames`, `showCarriedMeshes`, `throwSpeed`, `throwStartDistance`, `throwCollisionRadius`, `maxThrowFlightSeconds`, `maxThrowAngleDegrees`; the six added fields of `S_CarrySlotState` (C3); the added payload fields of `Carry.Began` (`carrier`) and `Carry.Ended` (`carrier`, `itemId`, `interruptCause`).
- **New names chosen here:** `IBPI_CarryableActor`, `IBPI_ThrowImpactSink`, `AThrownItemActor`, `UCarryServices`, `UBFL_Carry` (`getHandsBlockReason`, `consumeCarriedForCost`, `interruptCarry`, `handlePlayerLeft`, `throwEntry`), and on the component `beginCarryActor`, `throwCarried`, `getActiveSlotIndex`, `isActorCarry`, `getSpeedMultiplier`, `serverDropCarried`, `serverThrowCarried`.

# Appendix C — Open points to confirm with the designer or teammates

| # | Point | Who |
| --- | --- | --- |
| 1 | **C2.** A carried item is data while it is in the hands, and each machine draws its mesh from the replicated slot. B07 §10 PROPOSES an attached, replicated actor instead. The result on screen is the same; the data version is cheaper and is what R01's seam already hands over | You (B07 owner), R07 owner |
| 2 | **C5.** The active slot is the one filled last. So with two logs, a drop, a throw or a rope press takes the newer one | Designer |
| 3 | **C7.** The throw pipeline is built here, because Doc R03 and Doc B11 both say it is B07's while B07 3.4 says it only reuses one. One of the three texts should be reworded | You, R03 owner |
| 4 | **C8.** `throwImpactDamage` is one flat number per item, and `throwSpeed` is one project setting. A per-item speed or an arc that depends on the weight is one more field | Designer, R03 owner |
| 5 | **C9.** A throw direction more than 60 degrees (data) from the host's view of the pawn is refused. Is that tolerance right for a fast camera turn? | You, after the first network test |
| 6 | **C10.** A carried corpse or wounded ally can only be put down, never thrown. The spec does not say | Designer, B08 and B12 owners |
| 7 | **C17.** A heavy harvest yield granted to a player who is Downed or swimming lands loose instead of going to the hands | Designer |
| 8 | **C18, hotbar.** Pressing a hotbar key while carrying is refused with `HandsFull`, and the held item is neither put away nor changed when a carry begins. The alternative is to empty the hand when a carry begins | Designer, R01 and R07 owners |
| 9 | **Drop position.** A chosen drop lands a short way in front of the player (R01's `dropForwardDistance`); a forced drop lands at the feet. B07 3.6 says "at the player's feet" for forced drops only | Designer |
| 10 | **Sleeping.** B07 3.6 has both "drop at the feet when the sleep starts" and "registering to sleep is refused while carrying" (Index A106). With the refusal in place the drop can only happen to a player who picked something up after registering. Both are built; confirm that is the intent | Designer, R12 owner |
| 11 | **Downed, Restrained and sleep have no safety net.** Until Docs B02 and R12 add their one line (C7.3), a Downed player keeps the log in their hands. Death, leaving and engine swimming are covered | B02, R12 owners |
| 12 | **The carry icon.** Doc B17's message table lists a carry icon for `Carry.Began` / `Carry.Ended`; its HUD section (designer-confirmed) says carry displays are not shown at all | B17 owner |
| 13 | **Prediction (C19).** The owning client does not show the carry pose before the server confirms. If the delay is visible on a slow connection, the pose can be predicted in R01's pickup request | You, R07 owner |
| 14 | **NPC item carries.** NPCs use the component for a wounded ally (an actor). An NPC carrying a *log* works through `beginCarry`, but a drop then spawns the loose item with no player as instigator; that path is untested | B12 owner |
| 15 | **Deep water.** "A dropped item in deep water sinks and stays on the bottom" (Index A87) is behaviour of the loose item in a water body. Nothing in this framework implements or prevents it; it needs an owner (R01's loose actor or Doc B21 Part C) | R01 / B21 owners |
| 16 | **A throw's cosmetic event.** The registry has `Carry.Picked` and `Carry.Dropped` only. A throw raises `Dropped` when the item leaves the hands, and nothing when it lands | B21 owner |
| 17 | **Stale text in Doc B07** to tidy at the next edit: 3.1 still says "1-2 slots" and "consumes that many slots" next to "each slot holds exactly one heavy item" (C4 reconciles them); §2 compares the component to Stats and Survival Tick, which now sit on the PlayerState (decision D5); §7 lists one field for `S_CarrySlotState`; §9's payloads lack the added fields; §13's file list (see Appendix A); the header notice still calls 3.6 PROPOSED although most rows are designer-approved | You (B07 owner) |

# Appendix D — Definition of done for the framework

Eleven sections (0 to 10) delivered and reviewed with every task ticked; `Awaken.Item.Validate` reports zero errors on the heavy items; every test in the Verify tables passes; the six automation specs pass; C10.7 done on a listen server plus a client and once offline; R01's `UStubCarryService` is deleted and R01's own tests still pass with the real seam; every stub in the register has an owner and a removal condition; the closing Dev Tools section is in place.

> **Working method from here:** tell me **"start Section 0"** (or paste the first compiler error of whichever section you built) and we go section by section, fixing as we compile, the same way as R01 and R05. If a name in 0.4 differs in your real code, send it first and I will adjust the guide before you begin.
