# TheAwaken — Doc R01 Step-by-Step Implementation Guide
## Item & Inventory Framework (items, the bag, containers, hotbar, held item, staging, spoilage, the Bagless state)

Prepared for Yasir · Target: Unreal Engine 5.8 · C++ core with a thin layer of Blueprints
Companion to: *TheAwaken_DocR01_ItemInventoryFramework_ReworkSpec_v2* (the spec wins on any conflict). The *TheAwaken_DocR05_StepByStep_Implementation_Guide* (Equipment) builds on the classes this guide creates; section 0.4 lists the exact names it relies on.

---

## How to use this guide

1. Work top to bottom. Each section ends with **Verify** and **Handoff**. Do not start a section until the previous one compiles and its checks pass.
2. Task ids read `S3.4` = Section 3, task 4. Each task cites the R01 clause (or related doc) it implements. Other guides refer to these ids (for example the R05 guide cites S1.10, S2.14, S9, S11, S12 and S13), so do not renumber them.
3. **Honesty note:** this code was written against UE 5.8 conventions and was **not compiled**. Expect a few small API or include fixes. After each section, paste the first compiler error (if any) and we fix it together; do not "fix forward" across sections.
4. Everything marked **PROPOSED** is implemented as written but kept easy to change: every number and every content choice lives in data (Doc00 §2.7).
5. Placeholders: `AWAKEN_API` is your module's export macro and `Awaken` the module name. Replace both if your module is named differently.
6. A later section sometimes adds members to a class an earlier section created. Those places say **Header additions** and show only the new lines; the earlier code stays as it is.

**What this framework is, in six sentences.** Every item is a data asset; an item is either *stackable* (an id and a quantity) or *instanced* (one physical thing with its own state). An *inventory* is a plain data object on the server that lists entries; the player's bag, a chest and the inside of a pouch are all the same kind of object. The bag limits the amount of each item type, not a slot count. The hotbar stores *which item type* a key uses, and a small component on the Character says what is in the hand right now. Crafting does not take items out of the bag: it *reserves* them, and everything else sees only what is not reserved. A player who dies in co-op leaves the whole bag at the death spot and is *Bagless* (three hold slots, no inventory) until they walk back to it.

---

## 0.1 Code style cheat sheet (Doc02 §20, designer-confirmed)

Every block in this guide follows these rules. The R05 guide points here for the full version.

| Rule | Right | Wrong |
| --- | --- | --- |
| Variables, parameters, members, functions: camelCase | `saveSlot`, `grantOrDrop()` | `SaveSlot`, `GrantOrDrop()`, `m_saveSlot` |
| Booleans: no `b` prefix | `isLoaded`, `hasBag`, `canSpoil` | `bLoaded`, `bHasBag`, `bCanSpoil` |
| Enum type and values | `E_ItemKind::Instanced` (values are PascalCase) | `EItemKind`, `E_ItemKind::instanced` |
| Struct | `FS_InventoryEntry` (shown as `S_InventoryEntry` in the editor) | `FInventoryEntry` |
| Interface | `UBPI_EquipmentGear` + `IBPI_EquipmentGear` | `IEquipmentGear` |
| Delegate type / variable | `FD_HeldItemChanged` / `onHeldItemChanged` | `FOnHeldItemChanged` |
| Tag variable / tag string | `T_Inventory_Changed` / `"Inventory.Changed"` | `TAG_Inventory_Changed` |
| Keyword and brace spacing | `if(x){` … `}else if(y){` … `}else{` | `if (x) {`, `} else {` |
| Closing brace | alone on its own line, always, also after `return x;` | `return x; }` |
| Braces | always written, also around one statement | `if(x) return;` |
| Indent | 4 spaces | tabs |

Names Unreal forces on us (Doc02 §20.5), used exactly as the engine defines them: `BeginPlay`, `EndPlay`, `Initialize`, `Deinitialize`, `DoesSupportWorldType`, `GetLifetimeReplicatedProps`, `GetPrimaryAssetId`, `IsDataValid`, `PostLogin`, `NetDeltaSerialize`, `PostReplicatedReceive`, the `_Implementation` suffix of RPCs and interface events, every engine type, member and macro, and replication-notify functions, written `OnRep_` plus the camelCase variable name (`OnRep_hotbar`).

> The spec writes names the old way (`GrantOrDrop`, `S_InventoryEntry`, `bCanSpoil`). In code they become `grantOrDrop`, `FS_InventoryEntry`, `canSpoil`. Same thing, Doc02 §20 spelling. The one approved name that changes more than its first letter is `bLoaded`, which becomes `isLoaded`.

**Folder layout (Doc02 §20.3).** One file per category (`E_`, `S_`, `T_`, `D_` plus the framework name) holding every type of that category, one file per interface, and everything else in the framework folder itself.

---

## 0.2 Decisions this guide makes where the spec is silent

R01 marks much of its data model PROPOSED and leaves some mechanics open. These are the choices made so you can build without waiting. Each is small and easy to reverse, and each is repeated in Appendix C as a question. **Confirm or override any of them**; none blocks Section 0.

| # | Open point | Choice in this guide | Why |
| --- | --- | --- | --- |
| I1 | What an ItemID is in code | An `FName itemId` field on the definition; the `PrimaryAssetId` is `Item:<itemId>` | Saves and replication carry a small id (Doc02 §19.8), and renaming the asset file never changes the id (Doc02 §19.8 extension rule 5) |
| I2 | How an inventory replicates (R01 3.3, 7) | `UInventoryData` is a plain server object kept in the inventory registry and never replicates itself. The owner's component holds a fast-array **mirror** of the bag (owner only); a view component holds a mirror of the one container a player has open | The data object must move from a player to a bag in the world in one step (R01 3.3); a mirror makes that a pointer swap. This is the per-viewer mirror of the R01 amendment |
| I3 | Stacks per inventory | One entry per stackable `itemId` per inventory, so one spoil time per stack (R01 3.2a) | Fresh and spoiled food are two ids, so they are two entries already |
| I4 | The unbreakable sentinel (R01 3.1) | `durabilityMax = -1` means unbreakable, `0` means the item has no durability at all, above `0` it wears | A sentinel, not a very large number, as the spec asks |
| I5 | `WeaponType` of an item that is not a weapon | `E_WeaponType::None`, a sixth value | The field exists on every definition; the five spec values stay as they are |
| I6 | Frameworks that do not exist yet | Each is reached through a small interface (a *seam*) with a stub, registered in `UInventoryServices` | Doc00 §4.2: no hard dependency on a framework that is not guaranteed to exist |
| I7 | Who owns the loose item actor (Doc02 §19.7 names a shared base class but no owner) | `ALooseItemActor` lives here and is spawned through a seam. Re-parent it to the shared base when that exists | `grantOrDrop` and the pickup path are R01's and cannot be tested without it |
| I8 | The reason shown when all three Bagless hold slots are full (R01 3.11 says "a short reason" and names none) | `Interaction.FailReason.HandsFull` | It is the existing reason that says exactly this; no new tag is invented |
| I9 | "The last used instance first" after a load (R01 3.5) | The use history is not saved. With no history the first copy in the bag is used | The held item is not saved either (R01 3.5), so nothing is "last used" after a load |
| I10 | What Combat reads when the hand holds food | The Unarmed numbers. Only a weapon held by a player who has the bag is *wielded* | R01 3.8 already says a Bagless player's weapon is "shown in the hand but not wielded"; the same idea covers non-weapons |
| I11 | Who clears a crossbow's loaded flag or spends a round | `spendLoadedShot` (name PROPOSED), called by Doc R03 when a shot is fired | R01 3.2 names the writer of a reload (`applyReload`) but no writer for the shot |
| I12 | How Crafting consumes what it reserved (R01 3.6) | `consumeStaged` (name PROPOSED) | The spec's function table has reserve and release but no consume |
| I13 | How a page of the bag screen is named in data (R01 3.1a) | Tags `Inventory.Page.Weapons`, `.Tools`, `.Consumables`, `.Materials`, `.Others`, mapped from categories in Project Settings | The page list is data; a tag is the open-ended choice of Doc02 §3 |
| I14 | "A full pile fits inside its pocket" (R01 3.1a) | The stack layout declares its `pileFootprint`; the check compares it with the slot size | Measuring real mesh bounds needs the meshes loaded; a declared size keeps the check cheap |
| I15 | Refusal reasons the spec does not name | Twenty-one new `Inventory.FailReason.*` leaves, listed in S1.8 and Appendix B | Doc02 §6: a refusal is always a tag, never a bare `false` |
| I16 | A heavy item granted into a carry slot (R01 3.7 step 0) | `FS_GrantResult` gains `quantityCarried` next to stored and dropped | Otherwise the caller cannot tell where the item went |
| I17 | The shape of the bag record calls (Doc B02 owns the actor and the registry, Doc R13 the record) | Seam `IBPI_BagActorService`: `createBag`, `setBagContents`, `findBag`, `findBagOwnedBy`, `deleteBag`, `removeBagsOwnedBy` | The order of R01 3.8: the record and actor first, then the atomic move, and both deleted on failure |
| I18 | "A wielded weapon is one entry of the bag … an equipped entry that points to the same item instance" (R01 3.5) | The bag contents record which instance was in the hand; recovery puts it back in the hand | Recovered gear "is re-equipped automatically" (R01 3.8) and a weapon is equipped by holding it |
| I19 | Can a Bagless player put a held item down | Yes: `dropHoldSlot` drops it as a loose item | Otherwise three unwanted pickups lock the hands until death |
| I20 | The cosmetic event `Cosmetic.Event.Interaction.ItemDropped` (R01 amendment A89) | One function, `raiseItemDroppedCosmetic`, with a temporary body. The seam interface for Doc B21 is created by the R05 guide (or by B21); S5 gives the two lines to paste once it exists | Two guides must not declare the same interface |
| I21 | Container limits and "one kind of water at a time" (R01 3.4) | `FS_ContainerRules`: a default limit, per-item limits, allowed tags, and `singleItemTag` (of the items that carry this tag, the container holds one `itemId` at a time) | The same model as the bag (PROPOSED in the spec), and the water rule becomes data |
| I22 | Item names on screen (Doc R10) | The text key is derived from the id: `Item.<itemId>.Name`, `Item.<itemId>.Description` | Doc R10 3.3: a data asset holds no typed key when the key follows the convention |
| I23 | Where the CharacterID lives | The inventory component holds a copy for bag ownership; Doc R13 owns and saves the real one and sets ours after a load | Single owner (Doc00 §2.2) |
| I24 | The game clock before Doc R12 exists | A stub clock that moves only when a test or a dev command moves it | Spoilage tests need exact, repeatable times |
| I25 | Closing a container view when the player walks away | A half-second timer that runs only while a view is open | Doc02 §5 allows a short-lived timer while something is active |

---

## 0.3 Section map and dependency order

```
S0  scaffolding ─ S1 types/tags/seams ─ S2 item data ─ S3 inventory data
S4  registry, the bag on the PlayerState, containers
S5  loose items, grantOrDrop, pickup, drop, transfer
S6  hotbar and the held item ─ S7 craft-staging reservations
S8  spoilage, consumables, medicine
S9  the bag lifecycle: detach, Bagless, recover, starting kit, fresh character
S10 bag-screen data: pages, fixed slots, piles, the catalog checks
S11 save ─ S12 networking ─ S13 dev tools, tests, acceptance
```

Each section depends only on earlier ones. The Equipment guide (R05) needs Sections 0 to 9, 11 and 12.

## 0.4 Names the R05 guide relies on

The R05 guide was written against these names. If you rename one while building, apply the same rename there.

| Piece | Where it is built |
| --- | --- |
| `UInventoryServices` (`get`, `registerSeam`, `getSeam`, `isIncapacitated`, `isCarryActive`, `spawnLooseItem`, `releaseEquippedGear`) | S1.12 |
| `IBPI_EquipmentGear`, `FS_GearReleaseResult`, `FS_GearSlotRecord`, `UStubEquipmentGear` | S1.5, S1.10, S1.11 |
| `UDA_ItemDefinition` (`itemId`, `categoryTag`, `itemKind`, `durabilityMax`, `perTypeLimit`, `inventorySlot`, `isWeapon()`, the empty Equipment block, `IsDataValid` with its `fail` helper) | S2 |
| `UItemCatalogSubsystem::get` / `getDefinition` | S2.11 |
| `UInventoryData` (`findInstance`, `addInstance`, `addInstanceForced`, `addStack`, `removeInstance`, `removeStack`, `getAvailableQuantity`, `getTotalQuantity`, `getInstanceCount`, `canFit`, `getEntries`, `isDeathBag`) | S3 |
| `UInventoryRegistrySubsystem::get` / `findInventory` | S4 |
| `UPlayerInventoryComponent` (`findFor`, `hasBag`, `getPersonalInventory`) | S4 |
| `UBFL_Inventory` (`isServerWorld`, `grantOrDrop`, `getDropLocation`, `makeFreshInstance`) | S5 |
| `UBFL_InventoryBag` (`detachBag`, `reattachBag`, `startNewCharacter`), `FS_BagDetachResult`, `FS_BagContents.gear`, `IBPI_BagActorService`, the cvar `Awaken.Inventory.FailBagStep` | S9 |
| `IBPI_SaveParticipant`, `E_SaveScope`, `UStubSaveRegistry`, `UBFL_InventorySave::toBytes` / `fromBytes` | S11 |
| `UInventoryRequestComponent` (`findFor`, `gate`, `refuse`, `isContainerReachable`, `clientRefused`), `UContainerViewComponent` (`findFor`, `getViewedContainerId`) | S12 |
| `InventoryLog::roleName`, `FInventoryTestFixture` (`addStackable`, `addInstanced`, `makePlayer`, `services`), `AInventoryTestPlayerState`, `AInventoryTestGameMode`, `TestMap_Inventory` | S0, S4, S13 |
| Commands `Awaken.Item.Give`, `Awaken.Item.Validate`, `Awaken.Bag.DropHere`, `Awaken.Bag.Recover`, `Awaken.Bag.SetBagless` | S13 |
| Tags `T_Item_Category_Armor`, `T_Item_Category_Clothing`, `T_Inventory_FailReason_*`, `T_Interaction_FailReason_BagFull` / `_HandsFull` / `_NotAllowedInState` | S1.8 |

> **Two R05 patches are already applied here.** The R05 guide asks you to patch this framework in two places: E7.3 (the injected failure of `detachBag` step 2 must happen before the gear is released) and E9.2 (`gate`, `isContainerReachable` and `refuse` become public). This guide is written with both in place, so when you reach those two tasks there is nothing to change. Every other R05 patch (the Equipment block, the fixture helpers, the PlayerState component, the stub removal) still applies as written.

---

# SECTION 0 — Scaffolding

**Goal:** an empty Item & Inventory framework that compiles, logs, and shows its settings.
**Spec:** Doc02 §14, §20.3; Doc B22 §3.6.
**Depends on:** nothing.

### Files

```
Source/Awaken/Public/ItemInventoryFramework/
    00Enums/   01Structs/   02Tags/   03Interfaces/   04Delegates/
    InventoryLog.h
    InventoryServices.h
    UInventorySettings.h
Source/Awaken/Private/ItemInventoryFramework/
    02Tags/   Stubs/   DevTools/   Tests/
    InventoryLog.cpp
    InventoryServices.cpp
    UInventorySettings.cpp
```

### S0.1 — Create the folder tree (Doc02 §20.3)

From `Source/<Module>`:

```bash
for dir in 00Enums 01Structs 02Tags 03Interfaces 04Delegates; do
  mkdir -p "Public/ItemInventoryFramework/$dir"
done
mkdir -p Private/ItemInventoryFramework/02Tags Private/ItemInventoryFramework/Stubs
mkdir -p Private/ItemInventoryFramework/DevTools Private/ItemInventoryFramework/Tests
```

### S0.2 — Module dependencies and plugins

In `Awaken.Build.cs`, make sure these are in `PublicDependencyModuleNames`:

```csharp
PublicDependencyModuleNames.AddRange(new string[]{
    "Core", "CoreUObject", "Engine",
    "GameplayTags",              // item categories, reasons, messages
    "GameplayMessageRuntime",    // the Gameplay Message Subsystem (Doc02 §11)
    "NetCore",                   // FFastArraySerializer (the inventory mirror)
    "DeveloperSettings"          // UInventorySettings
});
```

In the editor, *Edit → Plugins*: enable **Gameplay Message Router** (the plugin that provides `GameplayMessageRuntime`). Restart the editor when it asks.

### S0.3 — Log category (Doc B22 §3.6 names it `LogAwakenInventory`)

`Public/ItemInventoryFramework/InventoryLog.h`

```cpp
#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAwakenInventory, Log, All);

namespace InventoryLog{
    // "Host", "Client" or "Offline" (Doc B22 §3.6: every log line names the role of the machine).
    AWAKEN_API FString roleName(const UObject* worldContext);
}

// Use: INVENTORY_LOG(Log, this, "text %d", value);
#define INVENTORY_LOG(verbosity, worldContext, format, ...) \
    UE_LOG(LogAwakenInventory, verbosity, TEXT("[%s] " format), *InventoryLog::roleName(worldContext), ##__VA_ARGS__)
```

`Private/ItemInventoryFramework/InventoryLog.cpp`

```cpp
#include "ItemInventoryFramework/InventoryLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY(LogAwakenInventory);

FString InventoryLog::roleName(const UObject* worldContext){
    const UWorld* world = (GEngine != nullptr && worldContext != nullptr) ? GEngine->GetWorldFromContextObject(worldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    if(world == nullptr){
        return TEXT("NoWorld");
    }
    switch(world->GetNetMode()){
        case NM_Standalone:{
            return TEXT("Offline");
        }
        case NM_Client:{
            return TEXT("Client");
        }
        default:{
            return TEXT("Host");
        }
    }
}
```

### S0.4 — Project settings (the tuning numbers in one place)

`Public/ItemInventoryFramework/UInventorySettings.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UInventorySettings.generated.h"

// Project Settings -> Game -> Awaken Inventory. Every value is tuning data (Doc00 2.7).
// Sections 9 and 10 add the starting kit and the page mapping to this class.
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Awaken Inventory"))
class AWAKEN_API UInventorySettings : public UDeveloperSettings{
    GENERATED_BODY()

public:
    UInventorySettings();

    static const UInventorySettings* get(){
        return GetDefault<UInventorySettings>();
    }

    // ---- hotbar (R01 3.5, 3.8) ----
    // Slots 0 to 9.
    UPROPERTY(Config, EditAnywhere, Category = "Hotbar", meta = (ClampMin = "1")) int32 hotbarSlotCount = 10;
    // While Bagless only keys 1, 2 and 3 work.
    UPROPERTY(Config, EditAnywhere, Category = "Hotbar", meta = (ClampMin = "0")) int32 baglessSlotCount = 3;
    // The always-available Unarmed entry (R01 3.10).
    UPROPERTY(Config, EditAnywhere, Category = "Hotbar") FName unarmedItemId = TEXT("Unarmed");

    // ---- ranges, in centimetres, measured from the pawn's eye. Inclusive (Doc02 19.9) ----
    UPROPERTY(Config, EditAnywhere, Category = "Requests", meta = (ClampMin = "0")) float pickupRange = 250.0f;
    UPROPERTY(Config, EditAnywhere, Category = "Requests", meta = (ClampMin = "0")) float containerRange = 300.0f;
    // Rate limit of one player's requests (Doc02 19.6). A player may also spend up to one second's worth at once.
    UPROPERTY(Config, EditAnywhere, Category = "Requests", meta = (ClampMin = "1")) float requestsPerSecond = 10.0f;

    // ---- loose items (Doc02 19.7) ----
    // Life of a loose item whose definition says 0. There is no global cap, only lifetimes.
    UPROPERTY(Config, EditAnywhere, Category = "Loose Items", meta = (ClampMin = "0")) float defaultLooseLifetimeSeconds = 600.0f;
    // How far in front of the player a dropped item appears.
    UPROPERTY(Config, EditAnywhere, Category = "Loose Items", meta = (ClampMin = "0")) float dropForwardDistance = 80.0f;
};
```

`Private/ItemInventoryFramework/UInventorySettings.cpp`

```cpp
#include "ItemInventoryFramework/UInventorySettings.h"

UInventorySettings::UInventorySettings(){
    CategoryName = TEXT("Game");
}
```

> The numbers under *Requests* and *Loose Items* are placeholders to tune in testing. None of them is a designer decision.

### S0.5 — Services subsystem skeleton (startup line)

`Public/ItemInventoryFramework/InventoryServices.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InventoryServices.generated.h"

UCLASS()
class AWAKEN_API UInventoryServices : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static UInventoryServices* get(const UObject* worldContext);
};
```

`Private/ItemInventoryFramework/InventoryServices.cpp`

```cpp
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

void UInventoryServices::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);
    INVENTORY_LOG(Log, this, "Item & Inventory framework started");
}

UInventoryServices* UInventoryServices::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    if(gameInstance == nullptr){
        return nullptr;
    }
    return gameInstance->GetSubsystem<UInventoryServices>();
}
```

### Verify

Clean Editor + Development build, zero warnings. When a game instance starts (press Play), the log shows `LogAwakenInventory: [...] Item & Inventory framework started`. *Project Settings → Game → Awaken Inventory* shows `hotbarSlotCount = 10` and `baglessSlotCount = 3`.

### Handoff

The folder tree, the log helper, the settings and the services skeleton exist. Nothing is exported to other frameworks yet.

---
# SECTION 1 — Types, Tags, Delegates and Seams

**Goal:** every shared type in one place, plus the interfaces to frameworks that do not exist yet, each with a stub.
**Spec:** R01 §4, §6; Doc02 §2, §3, §20.2, §20.3; Doc00 §4.2.
**Depends on:** Section 0.

### S1.1 — Enums

`Public/ItemInventoryFramework/00Enums/E_ItemInventoryFramework.h`

```cpp
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
```

> These are enums, not tags, because code branches on them and adding a value is a deliberate code change (Doc02 §3). Item *categories* are tags, because content extends them.

### S1.2 to S1.6 — Structs

All structs live in one file (Doc02 §20.3). Section 2 appends the bag-screen structs and Section 11 the save records.

`Public/ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "ItemInventoryFramework/00Enums/E_ItemInventoryFramework.h"
#include "S_ItemInventoryFramework.generated.h"

class APlayerState;

// =====================================================================
// S1.2  Items and entries (R01 3.2, §4)
// =====================================================================

// The state of one instanced item. It travels with the item wherever it goes.
USTRUCT(BlueprintType)
struct FS_ItemInstanceState{
    GENERATED_BODY()

    // -1 = unbreakable (the starter Axe only), 0 = the item has no durability (I4).
    UPROPERTY(BlueprintReadOnly) float durability = 0.0f;
    // Portable Containers only: the id of the inventory that holds the contents (R01 3.4).
    UPROPERTY(BlueprintReadOnly) FGuid containedInventoryId;
    // Crossbow only (the spec's bLoaded, Index A111).
    UPROPERTY(BlueprintReadOnly) bool isLoaded = false;
    // Firearm only (Index A116).
    UPROPERTY(BlueprintReadOnly) int32 loadedRounds = 0;

    bool operator==(const FS_ItemInstanceState& other) const{
        return durability == other.durability && containedInventoryId == other.containedInventoryId && isLoaded == other.isLoaded && loadedRounds == other.loadedRounds;
    }
};

// One line of an inventory. Stackable: itemId + quantities (+ spoil time). Instanced: itemId + instanceId + state.
USTRUCT(BlueprintType)
struct FS_InventoryEntry{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName itemId;
    // Stackable: how many. Instanced: always 1.
    UPROPERTY(BlueprintReadOnly) int32 totalQuantity = 0;
    // Staged for a craft (R01 3.6). Instanced: 1 while reserved. Never saved.
    UPROPERTY(BlueprintReadOnly) int32 reservedQuantity = 0;
    // Stackable items that can spoil: the absolute GameTime at which the stack spoils. 0 = no spoil clock (R01 3.2a).
    UPROPERTY(BlueprintReadOnly) int64 spoilAtTime = 0;
    // Instanced only. Created with the instance and never changed (Doc02 19.3, player-created kind).
    UPROPERTY(BlueprintReadOnly) FGuid instanceId;
    UPROPERTY(BlueprintReadOnly) FS_ItemInstanceState state;

    bool isInstanced() const{
        return instanceId.IsValid();
    }

    int32 getAvailableQuantity() const{
        return totalQuantity - reservedQuantity;
    }

    // The same line of an inventory: the same stack, or the same physical item.
    bool isSameLineAs(const FS_InventoryEntry& other) const{
        return itemId == other.itemId && instanceId == other.instanceId;
    }

    bool operator==(const FS_InventoryEntry& other) const{
        return isSameLineAs(other) && totalQuantity == other.totalQuantity && reservedQuantity == other.reservedQuantity && spoilAtTime == other.spoilAtTime && state == other.state;
    }
};

// A request to grant or remove (R01 §4).
USTRUCT(BlueprintType)
struct FS_ItemStack{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName itemId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1")) int32 quantity = 1;
};

// "ItemID or InstanceId" (R01 §5): names a stack by its item id, or one physical item by its instance id.
USTRUCT(BlueprintType)
struct FS_ItemRef{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FName itemId;
    UPROPERTY(BlueprintReadWrite) FGuid instanceId;

    bool isInstance() const{
        return instanceId.IsValid();
    }
};

// One entry of an item's ConsumeEffects (R01 3.1, 3.2a). Every value is data.
USTRUCT(BlueprintType)
struct FS_ConsumeEffect{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) E_ConsumeEffectKind kind = E_ConsumeEffectKind::ModifyMeter;
    // ModifyMeter: the meter (Survival.Meter.*). IllnessChance: the illness source (Survival.IllnessSource.*).
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag targetTag;
    // ModifyMeter: the change. HealthLoss: the HP lost (placeholder 10).
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float amount = 0.0f;
    // IllnessChance only: 0 to 1 (placeholder 0.3).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0", ClampMax = "1")) float chance = 1.0f;
    // IllnessChance only: how long the source lasts, in in-game hours (Doc B03 3.4, SourceDuration).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0")) float sourceDurationHours = 0.0f;
};

// What a Container or a Portable Container accepts (R01 3.1 CapacityRules and AllowedTags, 3.4; I21).
USTRUCT(BlueprintType)
struct FS_ContainerRules{
    GENERATED_BODY()

    // 0 = use each item's own perTypeLimit, the same model as the bag (PROPOSED in the spec).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0")) int32 defaultPerTypeLimit = 0;
    // A limit for one item id; wins over the default.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TMap<FName, int32> perItemLimits;
    // Empty = accepts everything. Otherwise the item's category tag or one of its item tags must match.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTagContainer allowedTags;
    // Of the items that carry this tag, the container holds only one item id at a time (clean OR unclean water, R01 3.4).
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag singleItemTag;
};

// =====================================================================
// S1.3  Results (Doc02 §6: a refusal is always explained)
// =====================================================================

// The result of every inventory command. Success means the failReason tag is empty.
USTRUCT(BlueprintType)
struct FS_InventoryResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FGameplayTag failReason;
    // How many units the command really moved, added or removed.
    UPROPERTY(BlueprintReadOnly) int32 quantity = 0;
    // Set when the command chose or created one instance.
    UPROPERTY(BlueprintReadOnly) FGuid instanceId;

    bool succeeded() const{
        return !failReason.IsValid();
    }
};

// R01 3.7 step 3. quantityCarried is the I16 addition (a heavy item that went into a carry slot).
USTRUCT(BlueprintType)
struct FS_GrantResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 quantityStored = 0;
    UPROPERTY(BlueprintReadOnly) int32 quantityDropped = 0;
    UPROPERTY(BlueprintReadOnly) int32 quantityCarried = 0;
};

// =====================================================================
// S1.4  Hotbar, held item, staging (R01 3.5, 3.6, 3.8)
// =====================================================================

// A hotbar slot points at an item TYPE, never at one physical stack (R01 3.5).
USTRUCT(BlueprintType)
struct FS_HotbarBinding{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 slotIndex = 0;
    UPROPERTY(BlueprintReadOnly) FName boundItemId;
};

// resolveHotbarSlot(): what the slot would use right now.
USTRUCT(BlueprintType)
struct FS_ResolvedHotbarSlot{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 slotIndex = 0;
    UPROPERTY(BlueprintReadOnly) FName itemId;
    // Instanced items: the copy that will be used (the last used one, decision D4).
    UPROPERTY(BlueprintReadOnly) FGuid instanceId;
    UPROPERTY(BlueprintReadOnly) int32 availableQuantity = 0;
    // False for an empty slot, a slot whose pool is gone, and a locked slot of a Bagless player.
    UPROPERTY(BlueprintReadOnly) bool isUsable = false;
};

// What is in the hand. Replicated to everyone so other players see it. Never saved (R01 3.5).
USTRUCT(BlueprintType)
struct FS_HeldItem{
    GENERATED_BODY()

    // Empty = nothing in the hand (Combat then reads the Unarmed entry).
    UPROPERTY(BlueprintReadOnly) FName itemId;
    UPROPERTY(BlueprintReadOnly) FGuid instanceId;
    // True only for a weapon held by a player who has the bag (I10). A Bagless player's weapon is shown, not wielded.
    UPROPERTY(BlueprintReadOnly) bool isWielded = false;
    // The hotbar slot it was taken from, or -1 for a direct pick from the bag.
    UPROPERTY(BlueprintReadOnly) int32 sourceSlot = -1;

    bool isEmpty() const{
        return itemId.IsNone();
    }

    bool operator==(const FS_HeldItem& other) const{
        return itemId == other.itemId && instanceId == other.instanceId && isWielded == other.isWielded && sourceSlot == other.sourceSlot;
    }
};

// One of the three hold slots of a Bagless player: exactly one item, no stacking (R01 3.8).
USTRUCT(BlueprintType)
struct FS_BaglessHoldSlot{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool isOccupied = false;
    UPROPERTY(BlueprintReadOnly) FS_InventoryEntry entry;
    // Food does not spoil in the hand: the time left is kept here and the spoil time is rebuilt when the item leaves.
    UPROPERTY(BlueprintReadOnly) bool hasSpoilClock = false;
    UPROPERTY(BlueprintReadOnly) int64 timeToSpoil = 0;
};

// One staging reservation (R01 §4). Server only: never saved, never replicated.
USTRUCT()
struct FS_StagedReservation{
    GENERATED_BODY()

    UPROPERTY() FName itemId;
    UPROPERTY() FGuid instanceId;
    UPROPERTY() int32 reservedQuantity = 0;
    // The station of the crafting session. An invalid id is a hand-craft session (no station, Doc R02 3.8).
    UPROPERTY() FGuid owningStationId;
};

// =====================================================================
// S1.5  The bag and worn gear (R01 3.8; Doc B02 3.2, 3.6; Doc R05 3.3)
// =====================================================================

// Where one worn armor piece sat, so recovery puts it back in the same place (Doc B02).
USTRUCT(BlueprintType)
struct FS_GearSlotRecord{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FGuid instanceId;
    UPROPERTY(BlueprintReadOnly) int32 stackIndex = 0;
};

// What Equipment hands over when the bag is detached (Doc R05 ReleaseEquippedGear).
USTRUCT(BlueprintType)
struct FS_GearReleaseResult{
    GENERATED_BODY()

    // "Nothing was worn" is a success.
    UPROPERTY(BlueprintReadOnly) bool succeeded = true;
    // Every worn piece as a full bag entry: same instance id, current durability.
    UPROPERTY(BlueprintReadOnly) TArray<FS_InventoryEntry> gearEntries;
    UPROPERTY(BlueprintReadOnly) TArray<FS_GearSlotRecord> gearSlots;
};

// What a bag lying in the world holds besides its items (Doc R13 3.6: contents, hotbar snapshot, equipped gear).
USTRUCT(BlueprintType)
struct FS_BagContents{
    GENERATED_BODY()

    // The inventory in the registry that IS the bag. Not a copy.
    UPROPERTY(BlueprintReadOnly) FGuid inventoryId;
    UPROPERTY(BlueprintReadOnly) TArray<FS_HotbarBinding> hotbar;
    // The armor that was worn. The pieces themselves are entries of the bag's inventory, listed once.
    UPROPERTY(BlueprintReadOnly) TArray<FS_GearSlotRecord> gear;
    // The item that was in the hand: a pointer to an entry of the bag, never a copy (I18).
    UPROPERTY(BlueprintReadOnly) FS_ItemRef wielded;
};

// A bag record as the bag service answers it (the record itself is S_BagRecord of Doc R13).
USTRUCT(BlueprintType)
struct FS_BagInfo{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) bool isValid = false;
    UPROPERTY(BlueprintReadOnly) FGuid bagId;
    // A bag belongs to a character, not to a player (Index A93).
    UPROPERTY(BlueprintReadOnly) FGuid ownerCharacterId;
    UPROPERTY(BlueprintReadOnly) FVector location = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FS_BagContents contents;
};

USTRUCT(BlueprintType)
struct FS_BagDetachResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FGameplayTag failReason;
    UPROPERTY(BlueprintReadOnly) FGuid bagId;

    bool succeeded() const{
        return !failReason.IsValid();
    }
};

// =====================================================================
// S1.6  Message payloads (R01 §6, Doc02 §11)
// =====================================================================

// Inventory.Changed
USTRUCT(BlueprintType)
struct FS_InventoryChangedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FGuid inventoryId;
};

// Inventory.GrantDropped
USTRUCT(BlueprintType)
struct FS_GrantDroppedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<APlayerState> player = nullptr;
    UPROPERTY(BlueprintReadOnly) FName itemId;
    UPROPERTY(BlueprintReadOnly) int32 quantity = 0;
};

// Hotbar.Changed
USTRUCT(BlueprintType)
struct FS_HotbarChangedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<APlayerState> player = nullptr;
    UPROPERTY(BlueprintReadOnly) int32 slotIndex = 0;
};

// =====================================================================
// S1.7  The replication mirror (R01 3.3 "replicates as a fast array"; I2)
// =====================================================================

USTRUCT()
struct FS_ReplicatedInventoryEntry : public FFastArraySerializerItem{
    GENERATED_BODY()

    UPROPERTY() FS_InventoryEntry entry;
};

// A copy of one inventory's entries that replicates only what changed.
// It is a member of a component, which decides who receives it (owner only).
USTRUCT()
struct FS_InventoryEntryList : public FFastArraySerializer{
    GENERATED_BODY()

    UPROPERTY() TArray<FS_ReplicatedInventoryEntry> items;

    // Client: runs once after each batch of changes has arrived. Not a UPROPERTY, so it never replicates.
    FSimpleDelegate onReceived;

    // Server. Makes the mirror equal to the real entries and marks only the lines that changed.
    void syncFrom(const TArray<FS_InventoryEntry>& source);
    // The mirror as plain entries.
    TArray<FS_InventoryEntry> toEntries() const;

    bool NetDeltaSerialize(FNetDeltaSerializeInfo& deltaParameters){
        return FFastArraySerializer::FastArrayDeltaSerialize<FS_ReplicatedInventoryEntry, FS_InventoryEntryList>(items, deltaParameters, *this);
    }

    void PostReplicatedReceive(const FFastArraySerializer::FPostReplicatedReceiveParameters& parameters){
        onReceived.ExecuteIfBound();
    }
};

template<>
struct TStructOpsTypeTraits<FS_InventoryEntryList> : public TStructOpsTypeTraitsBase2<FS_InventoryEntryList>{
    enum{
        WithNetDeltaSerializer = true
    };
};
```

`Private/ItemInventoryFramework/S_ItemInventoryFramework.cpp`

```cpp
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"

void FS_InventoryEntryList::syncFrom(const TArray<FS_InventoryEntry>& source){
    // 1. Lines that left the inventory.
    for(int32 index = items.Num() - 1; index >= 0; index--){
        const FS_InventoryEntry& mirrored = items[index].entry;
        const bool stillThere = source.ContainsByPredicate([&mirrored](const FS_InventoryEntry& wanted){
            return wanted.isSameLineAs(mirrored);
        });
        if(!stillThere){
            items.RemoveAt(index);
            MarkArrayDirty();
        }
    }
    // 2. New lines and changed lines.
    for(const FS_InventoryEntry& wanted : source){
        FS_ReplicatedInventoryEntry* existing = items.FindByPredicate([&wanted](const FS_ReplicatedInventoryEntry& item){
            return item.entry.isSameLineAs(wanted);
        });
        if(existing == nullptr){
            FS_ReplicatedInventoryEntry& added = items.AddDefaulted_GetRef();
            added.entry = wanted;
            MarkItemDirty(added);
        }else if(!(existing->entry == wanted)){
            existing->entry = wanted;
            MarkItemDirty(*existing);
        }
    }
}

TArray<FS_InventoryEntry> FS_InventoryEntryList::toEntries() const{
    TArray<FS_InventoryEntry> entries;
    entries.Reserve(items.Num());
    for(const FS_ReplicatedInventoryEntry& item : items){
        entries.Add(item.entry);
    }
    return entries;
}
```

> **Why a mirror and not a replicated inventory object.** The bag must move from a player to a bag in the world in one server step (R01 3.3). A replicated sub-object belongs to one actor and cannot change owner cleanly. A plain object in the registry can: the player's component simply stops pointing at it. What the owner sees is this mirror, refreshed whenever the real data changes. `syncFrom` compares line by line; an inventory holds tens of lines, not thousands, so the simple loop is enough.

### S1.8 — Tags

`Public/ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

// ---- Messages (R01 §6; all three are in the Tag Registry) ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_Changed);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_GrantDropped);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Hotbar_Changed);

// ---- Item categories (R01 3.1). The list is data: content may add leaves under Item.Category ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Item_Category);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Item_Category_Weapon);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Item_Category_Armor);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Item_Category_Clothing);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Item_Category_Medicine);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Item_Category_Food);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Item_Category_Water);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Item_Category_Material);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Item_Category_Tool);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Item_Category_Container);

// ---- Item flags, held in an item's itemTags ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Item_Flag_Permanent);            // the starter Axe and the map (R01 3.10)

// ---- Pages of the bag screen (R01 3.1a; names PROPOSED, I13) ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_Page_Weapons);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_Page_Tools);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_Page_Consumables);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_Page_Materials);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_Page_Others);

// ---- Refusal reasons already in the Tag Registry ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_ContainerNotEmpty);   // R01 3.4a (approved, A101)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_Permanent);           // R01 3.10 (approved, A99)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_CannotOpenHere);      // Doc B17 3.6 raises it; the root is ours

// ---- Refusal reasons that are NEW and PROPOSED (I15): add them to the Tag Registry ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_NotServer);            // a command was called on a client
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_InvalidRequest);       // malformed: zero quantity, wrong item kind, null target
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_NotFound);             // no such item, instance, inventory or bag
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_NoBag);                // the player is Bagless (R01 3.8)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_AlreadyHasBag);        // recovery asked while the player has a bag
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_NotBagOwner);          // the bag belongs to another character
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_InsufficientQuantity); // fewer available than asked
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_ReservedQuantity);     // the item is staged for a craft (R01 3.6)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_ItemWielded);          // an equipped weapon cannot be staged (Doc R02 3.10)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_ContainerFull);        // a container is at its limit for that type
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_ItemNotAllowed);       // the container's rules do not accept the item
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_NestedContainer);      // a Portable Container into a Portable Container
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_DeathBagLocked);       // nobody takes from or stores into a death bag
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_InvalidSlot);          // no such hotbar slot, or a key locked while Bagless
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_NotConsumable);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_CannotReload);         // not a crossbow or firearm, or already full
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_NoAmmo);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_OutOfRange);           // too far, or no clear line (Doc02 19.6)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_RateLimited);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_BagCreateFailed);      // DetachBag step 1
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_GearReleaseFailed);    // DetachBag step 2
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Inventory_FailReason_KitAlreadyGranted);    // R01 amendment (3.8)

// ---- Tags OWNED BY OTHER FRAMEWORKS, used here (see the stub register, S13.8) ----
// TEMPORARY duplicates: remove each definition when its owner's tag header exists, and include that header instead.
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FailReason_BagFull);            // Doc B18 (approved, A94)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FailReason_HandsFull);          // Doc B18
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FailReason_NotAllowedInState);  // Doc B18
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Action_Reload);                             // Doc R04
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_DamageType_Illness);                        // Doc R03
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Survival_Meter_Illness);                    // Doc B03
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Stats_Source_Medicine);                     // Doc R08 (PROPOSED there)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Interaction_ItemDropped);    // Doc B21 A.4 (Index A89)
```

`Private/ItemInventoryFramework/02Tags/T_ItemInventoryFramework.cpp`

```cpp
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"

UE_DEFINE_GAMEPLAY_TAG(T_Inventory_Changed, "Inventory.Changed");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_GrantDropped, "Inventory.GrantDropped");
UE_DEFINE_GAMEPLAY_TAG(T_Hotbar_Changed, "Hotbar.Changed");

UE_DEFINE_GAMEPLAY_TAG(T_Item_Category, "Item.Category");
UE_DEFINE_GAMEPLAY_TAG(T_Item_Category_Weapon, "Item.Category.Weapon");
UE_DEFINE_GAMEPLAY_TAG(T_Item_Category_Armor, "Item.Category.Armor");
UE_DEFINE_GAMEPLAY_TAG(T_Item_Category_Clothing, "Item.Category.Clothing");
UE_DEFINE_GAMEPLAY_TAG(T_Item_Category_Medicine, "Item.Category.Medicine");
UE_DEFINE_GAMEPLAY_TAG(T_Item_Category_Food, "Item.Category.Food");
UE_DEFINE_GAMEPLAY_TAG(T_Item_Category_Water, "Item.Category.Water");
UE_DEFINE_GAMEPLAY_TAG(T_Item_Category_Material, "Item.Category.Material");
UE_DEFINE_GAMEPLAY_TAG(T_Item_Category_Tool, "Item.Category.Tool");
UE_DEFINE_GAMEPLAY_TAG(T_Item_Category_Container, "Item.Category.Container");

UE_DEFINE_GAMEPLAY_TAG(T_Item_Flag_Permanent, "Item.Flag.Permanent");

UE_DEFINE_GAMEPLAY_TAG(T_Inventory_Page_Weapons, "Inventory.Page.Weapons");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_Page_Tools, "Inventory.Page.Tools");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_Page_Consumables, "Inventory.Page.Consumables");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_Page_Materials, "Inventory.Page.Materials");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_Page_Others, "Inventory.Page.Others");

UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_ContainerNotEmpty, "Inventory.FailReason.ContainerNotEmpty");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_Permanent, "Inventory.FailReason.Permanent");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_CannotOpenHere, "Inventory.FailReason.CannotOpenHere");

UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_NotServer, "Inventory.FailReason.NotServer");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_InvalidRequest, "Inventory.FailReason.InvalidRequest");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_NotFound, "Inventory.FailReason.NotFound");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_NoBag, "Inventory.FailReason.NoBag");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_AlreadyHasBag, "Inventory.FailReason.AlreadyHasBag");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_NotBagOwner, "Inventory.FailReason.NotBagOwner");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_InsufficientQuantity, "Inventory.FailReason.InsufficientQuantity");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_ReservedQuantity, "Inventory.FailReason.ReservedQuantity");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_ItemWielded, "Inventory.FailReason.ItemWielded");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_ContainerFull, "Inventory.FailReason.ContainerFull");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_ItemNotAllowed, "Inventory.FailReason.ItemNotAllowed");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_NestedContainer, "Inventory.FailReason.NestedContainer");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_DeathBagLocked, "Inventory.FailReason.DeathBagLocked");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_InvalidSlot, "Inventory.FailReason.InvalidSlot");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_NotConsumable, "Inventory.FailReason.NotConsumable");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_CannotReload, "Inventory.FailReason.CannotReload");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_NoAmmo, "Inventory.FailReason.NoAmmo");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_OutOfRange, "Inventory.FailReason.OutOfRange");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_RateLimited, "Inventory.FailReason.RateLimited");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_BagCreateFailed, "Inventory.FailReason.BagCreateFailed");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_GearReleaseFailed, "Inventory.FailReason.GearReleaseFailed");
UE_DEFINE_GAMEPLAY_TAG(T_Inventory_FailReason_KitAlreadyGranted, "Inventory.FailReason.KitAlreadyGranted");

UE_DEFINE_GAMEPLAY_TAG(T_Interaction_FailReason_BagFull, "Interaction.FailReason.BagFull");
UE_DEFINE_GAMEPLAY_TAG(T_Interaction_FailReason_HandsFull, "Interaction.FailReason.HandsFull");
UE_DEFINE_GAMEPLAY_TAG(T_Interaction_FailReason_NotAllowedInState, "Interaction.FailReason.NotAllowedInState");
UE_DEFINE_GAMEPLAY_TAG(T_Action_Reload, "Action.Reload");
UE_DEFINE_GAMEPLAY_TAG(T_DamageType_Illness, "DamageType.Illness");
UE_DEFINE_GAMEPLAY_TAG(T_Survival_Meter_Illness, "Survival.Meter.Illness");
UE_DEFINE_GAMEPLAY_TAG(T_Stats_Source_Medicine, "Stats.Source.Medicine");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Interaction_ItemDropped, "Cosmetic.Event.Interaction.ItemDropped");
```

> Content also uses tags of Doc B03 that C++ never names: `Survival.Meter.Hunger`, `Survival.Meter.Thirst` and the illness sources `Survival.IllnessSource.SpoiledFood` and `.UntreatedWater`. Until Doc B03's tags exist, add those four in *Project Settings → Gameplay Tags* (source: the default ini) so the food and water assets of S2.14 can pick them.

### S1.9 — Delegates

`Public/ItemInventoryFramework/04Delegates/D_ItemInventoryFramework.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "D_ItemInventoryFramework.generated.h"

class APlayerState;
class UInventoryData;

// ---- Blueprint-visible. Each is raised locally on the machine that holds the state ----
// The bag, the hotbar or the hold slots of this player changed.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FD_PlayerInventoryChanged, APlayerState*, player);
// What is in the hand changed.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FD_HeldItemChanged);
// The container this player has open changed, opened (a valid id) or closed (an invalid id).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FD_ContainerViewChanged, FGuid, containerId);
// The server refused a request of this player. The reason is the tag the UI turns into text (Doc B17 3.14).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FD_InventoryRequestRefused, FGameplayTag, reason);

// ---- C++ only ----
DECLARE_MULTICAST_DELEGATE_OneParam(FD_InventoryDataChanged, UInventoryData*);
// A staged item was lost because its stack spoiled (R01 3.2a). Doc R02 listens and refuses the craft.
DECLARE_MULTICAST_DELEGATE_TwoParams(FD_StagedItemLost, const FGuid& /*stationId*/, FName /*itemId*/);
```

> If Unreal Header Tool complains about a header that holds only delegates, move the four dynamic delegates to the top of `S_ItemInventoryFramework.h` (above the first struct) and keep this file for the two C++ ones without the `.generated.h` include. Remember which fix you used: the R05 guide's `D_EquipmentFramework.h` needs the same one.

### S1.10 — Seam interfaces

R01 calls nine frameworks that belong to teammates or do not exist yet. Each is reached only through an interface (Doc00 §4.2). One file per interface in `Public/ItemInventoryFramework/03Interfaces/`. Every one follows the same pattern, so only the functions differ.

`BPI_GameClock.h` (owner: Doc R12 Time)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BPI_GameClock.generated.h"

UINTERFACE(BlueprintType)
class UBPI_GameClock : public UInterface{
    GENERATED_BODY()
};

class IBPI_GameClock{
    GENERATED_BODY()

public:
    // Doc R12 GetGameTime(): game seconds since the world was created, a 64-bit whole number (Doc02 19.4).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    int64 getGameTime();
};
```

`BPI_PlayerStateQuery.h` (owners: Docs B02, R03, R11, R17)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BPI_PlayerStateQuery.generated.h"

class APlayerState;

UINTERFACE(BlueprintType)
class UBPI_PlayerStateQuery : public UInterface{
    GENERATED_BODY()
};

class IBPI_PlayerStateQuery{
    GENERATED_BODY()

public:
    // True while the player may not act: Downed, Restrained, Arriving, dead, or locked in a scene (Doc02 19.6).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool isIncapacitated(APlayerState* player);
};
```

`BPI_CarryService.h` (owner: Doc B07 Carry)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "BPI_CarryService.generated.h"

class APlayerState;

UINTERFACE(BlueprintType)
class UBPI_CarryService : public UInterface{
    GENERATED_BODY()
};

class IBPI_CarryService{
    GENERATED_BODY()

public:
    // Doc B07 IsCarryActive(): a carry slot is occupied.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool isCarryActive(APlayerState* player);

    // Doc B07 BeginCarry(): puts a heavy item into a free carry slot. False when the hands cannot take it.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool beginCarry(APlayerState* player, const FS_InventoryEntry& entry);
};
```

`BPI_SurvivalService.h` (owner: Doc B03 Survival Tick)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "BPI_SurvivalService.generated.h"

class APlayerState;

UINTERFACE(BlueprintType)
class UBPI_SurvivalService : public UInterface{
    GENERATED_BODY()
};

class IBPI_SurvivalService{
    GENERATED_BODY()

public:
    // Doc B03 ModifyMeter(MeterType, Delta, SourceTag).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void modifyMeter(APlayerState* player, FGameplayTag meterTag, float delta, FGameplayTag sourceTag);

    // Doc B03 RegisterIllnessSource, for a source that comes from eating or drinking (it ends by itself).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void registerIllnessSource(APlayerState* player, FGameplayTag sourceTag, float durationHours);

    // False when the Illness meter is switched off (Doc B13): then neither the illness chance nor the HP loss applies.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool isIllnessActive(APlayerState* player);
};
```

`BPI_HealthService.h` (owners: Doc R08 Stats for healing, Doc R03 Combat for damage)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "BPI_HealthService.generated.h"

class APlayerState;

UINTERFACE(BlueprintType)
class UBPI_HealthService : public UInterface{
    GENERATED_BODY()
};

class IBPI_HealthService{
    GENERATED_BODY()

public:
    // Doc R08 ModifyHP with a positive amount (medicine, R01 3.9).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void healPlayer(APlayerState* player, float amount, FGameplayTag sourceTag);

    // The normal damage path of Doc R03 3.7. That path applies the Peaceful floor and the dialogue refusal (R01 3.2a).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void damagePlayer(APlayerState* player, float amount, FGameplayTag damageTypeTag);
};
```

`BPI_ActionService.h` (owner: Doc R04 Ability & Action)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "BPI_ActionService.generated.h"

class APlayerState;

UINTERFACE(BlueprintType)
class UBPI_ActionService : public UInterface{
    GENERATED_BODY()
};

class IBPI_ActionService{
    GENERATED_BODY()

public:
    // Doc R04 cancelAction(ActionTag). R01 calls it with Action.Reload when the held item changes (R04-R08-IG17).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void cancelAction(APlayerState* player, FGameplayTag actionTag);
};
```

`BPI_LooseItemSpawner.h` (owner: this framework until the shared base of Doc02 §19.7 exists, I7)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "BPI_LooseItemSpawner.generated.h"

class APlayerState;

UINTERFACE(BlueprintType)
class UBPI_LooseItemSpawner : public UInterface{
    GENERATED_BODY()
};

class IBPI_LooseItemSpawner{
    GENERATED_BODY()

public:
    // Puts one entry on the ground as a loose item (Doc02 19.7). instigatorPlayer may be null.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    AActor* spawnLooseItem(APlayerState* instigatorPlayer, const FS_InventoryEntry& entry, FVector location);
};
```

`BPI_EquipmentGear.h` (owner: Doc R05 Equipment)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "BPI_EquipmentGear.generated.h"

class APlayerState;

UINTERFACE(BlueprintType)
class UBPI_EquipmentGear : public UInterface{
    GENERATED_BODY()
};

class IBPI_EquipmentGear{
    GENERATED_BODY()

public:
    // Doc R05 ReleaseEquippedGear. Empties the worn armor and RETURNS the pieces; R01 stores them in the bag.
    // Must change nothing when it reports failure.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FS_GearReleaseResult releaseEquippedGear(APlayerState* player);

    // Takes each listed instance out of the recovered bag and puts it back on, in the place it occupied (Doc B02).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool reEquipGear(APlayerState* player, const TArray<FS_GearSlotRecord>& gear);
};
```

`BPI_BagActorService.h` (owners: Doc B02 for the bag actor and its registry, Doc R13 for the record; I17)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "BPI_BagActorService.generated.h"

UINTERFACE(BlueprintType)
class UBPI_BagActorService : public UInterface{
    GENERATED_BODY()
};

class IBPI_BagActorService{
    GENERATED_BODY()

public:
    // Step 1 of DetachBag: an empty record and its actor at the location (Doc B02 3.6). False on failure.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool createBag(FGuid bagId, FGuid ownerCharacterId, FVector location);

    // The last step of DetachBag: what the bag holds. Cannot fail.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void setBagContents(FGuid bagId, const FS_BagContents& contents);

    // isValid is false when there is no such bag.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FS_BagInfo findBag(FGuid bagId);

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FS_BagInfo findBagOwnedBy(FGuid ownerCharacterId);

    // Removes the record and the actor. The inventory is NOT touched: R01 owns it.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void deleteBag(FGuid bagId);

    // Fresh character (R01 3.8): removes every bag of that character and returns the inventory id of each,
    // so that R01 can destroy those inventories (a request of the R01 spec to Doc B02).
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    TArray<FGuid> removeBagsOwnedBy(FGuid ownerCharacterId);
};
```

> The tenth seam, the save participant of Doc R13, is created in Section 11 with the rest of the save code.

### S1.11 — Stub implementations

Each stub does the least that lets R01 run and be tested, and remembers what it was asked so a test can read it.

`Private/ItemInventoryFramework/Stubs/InventoryStubs.h`

```cpp
// TEMPORARY STUBS — the owner of each is named on its class. Delete a class when the real framework registers its seam.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameFramework/PlayerState.h"
#include "ItemInventoryFramework/03Interfaces/BPI_GameClock.h"
#include "ItemInventoryFramework/03Interfaces/BPI_PlayerStateQuery.h"
#include "ItemInventoryFramework/03Interfaces/BPI_CarryService.h"
#include "ItemInventoryFramework/03Interfaces/BPI_SurvivalService.h"
#include "ItemInventoryFramework/03Interfaces/BPI_HealthService.h"
#include "ItemInventoryFramework/03Interfaces/BPI_ActionService.h"
#include "ItemInventoryFramework/03Interfaces/BPI_LooseItemSpawner.h"
#include "ItemInventoryFramework/03Interfaces/BPI_EquipmentGear.h"
#include "ItemInventoryFramework/03Interfaces/BPI_BagActorService.h"
#include "InventoryStubs.generated.h"

// TEMPORARY STUB — owner: Doc R12 Time. The clock moves only when a test or a dev command moves it (I24).
UCLASS()
class UStubGameClock : public UObject, public IBPI_GameClock{
    GENERATED_BODY()

public:
    UPROPERTY() int64 gameTime = 0;

    virtual int64 getGameTime_Implementation() override;
};

// TEMPORARY STUB — owners: Docs B02, R03, R11, R17. A set of players marked as unable to act.
UCLASS()
class UStubPlayerStateQuery : public UObject, public IBPI_PlayerStateQuery{
    GENERATED_BODY()

public:
    TSet<TWeakObjectPtr<APlayerState>> incapacitatedPlayers;

    virtual bool isIncapacitated_Implementation(APlayerState* player) override;
};

// TEMPORARY STUB — owner: Doc B07 Carry. A set of players marked as carrying; it accepts a heavy item only when told to.
UCLASS()
class UStubCarryService : public UObject, public IBPI_CarryService{
    GENERATED_BODY()

public:
    TSet<TWeakObjectPtr<APlayerState>> carryingPlayers;
    UPROPERTY() bool acceptsNewCarry = false;
    UPROPERTY() TArray<FS_InventoryEntry> carriedEntries;

    virtual bool isCarryActive_Implementation(APlayerState* player) override;
    virtual bool beginCarry_Implementation(APlayerState* player, const FS_InventoryEntry& entry) override;
};

// TEMPORARY STUB — owner: Doc B03 Survival Tick. Adds up what each meter received.
UCLASS()
class UStubSurvivalService : public UObject, public IBPI_SurvivalService{
    GENERATED_BODY()

public:
    UPROPERTY() TMap<FGameplayTag, float> meterTotals;
    UPROPERTY() TArray<FGameplayTag> registeredSources;
    UPROPERTY() bool illnessActive = true;

    virtual void modifyMeter_Implementation(APlayerState* player, FGameplayTag meterTag, float delta, FGameplayTag sourceTag) override;
    virtual void registerIllnessSource_Implementation(APlayerState* player, FGameplayTag sourceTag, float durationHours) override;
    virtual bool isIllnessActive_Implementation(APlayerState* player) override;
};

// TEMPORARY STUB — owners: Doc R08 Stats (heal) and Doc R03 Combat (damage).
UCLASS()
class UStubHealthService : public UObject, public IBPI_HealthService{
    GENERATED_BODY()

public:
    UPROPERTY() float totalHealed = 0.0f;
    UPROPERTY() float totalDamaged = 0.0f;
    UPROPERTY() FGameplayTag lastDamageType;

    virtual void healPlayer_Implementation(APlayerState* player, float amount, FGameplayTag sourceTag) override;
    virtual void damagePlayer_Implementation(APlayerState* player, float amount, FGameplayTag damageTypeTag) override;
};

// TEMPORARY STUB — owner: Doc R04 Ability & Action.
UCLASS()
class UStubActionService : public UObject, public IBPI_ActionService{
    GENERATED_BODY()

public:
    UPROPERTY() TArray<FGameplayTag> cancelledActions;

    virtual void cancelAction_Implementation(APlayerState* player, FGameplayTag actionTag) override;
};

// A spawner that only remembers. Tests use it; Section 5 registers the real spawner for play.
UCLASS()
class UStubLooseItemSpawner : public UObject, public IBPI_LooseItemSpawner{
    GENERATED_BODY()

public:
    UPROPERTY() TArray<FS_InventoryEntry> spawnedEntries;
    UPROPERTY() TArray<FVector> spawnLocations;

    virtual AActor* spawnLooseItem_Implementation(APlayerState* instigatorPlayer, const FS_InventoryEntry& entry, FVector location) override;
};

// TEMPORARY STUB — owner: Doc R05 Equipment. Nothing is worn, so releasing and re-equipping do nothing.
UCLASS()
class UStubEquipmentGear : public UObject, public IBPI_EquipmentGear{
    GENERATED_BODY()

public:
    virtual FS_GearReleaseResult releaseEquippedGear_Implementation(APlayerState* player) override;
    virtual bool reEquipGear_Implementation(APlayerState* player, const TArray<FS_GearSlotRecord>& gear) override;
};

// TEMPORARY STUB — owners: Doc B02 (actor, registry) and Doc R13 (record). Records only: no actor, nothing saved.
UCLASS()
class UStubBagActorService : public UObject, public IBPI_BagActorService{
    GENERATED_BODY()

public:
    UPROPERTY() TMap<FGuid, FS_BagInfo> bags;

    virtual bool createBag_Implementation(FGuid bagId, FGuid ownerCharacterId, FVector location) override;
    virtual void setBagContents_Implementation(FGuid bagId, const FS_BagContents& contents) override;
    virtual FS_BagInfo findBag_Implementation(FGuid bagId) override;
    virtual FS_BagInfo findBagOwnedBy_Implementation(FGuid ownerCharacterId) override;
    virtual void deleteBag_Implementation(FGuid bagId) override;
    virtual TArray<FGuid> removeBagsOwnedBy_Implementation(FGuid ownerCharacterId) override;
};
```

`Private/ItemInventoryFramework/Stubs/InventoryStubs.cpp`

```cpp
// TEMPORARY STUBS — see InventoryStubs.h for the owner of each.
#include "ItemInventoryFramework/Stubs/InventoryStubs.h"

int64 UStubGameClock::getGameTime_Implementation(){
    return gameTime;
}

bool UStubPlayerStateQuery::isIncapacitated_Implementation(APlayerState* player){
    return incapacitatedPlayers.Contains(player);
}

bool UStubCarryService::isCarryActive_Implementation(APlayerState* player){
    return carryingPlayers.Contains(player);
}

bool UStubCarryService::beginCarry_Implementation(APlayerState* player, const FS_InventoryEntry& entry){
    if(!acceptsNewCarry){
        return false;
    }
    carriedEntries.Add(entry);
    return true;
}

void UStubSurvivalService::modifyMeter_Implementation(APlayerState* player, FGameplayTag meterTag, float delta, FGameplayTag sourceTag){
    meterTotals.FindOrAdd(meterTag) += delta;
}

void UStubSurvivalService::registerIllnessSource_Implementation(APlayerState* player, FGameplayTag sourceTag, float durationHours){
    registeredSources.Add(sourceTag);
}

bool UStubSurvivalService::isIllnessActive_Implementation(APlayerState* player){
    return illnessActive;
}

void UStubHealthService::healPlayer_Implementation(APlayerState* player, float amount, FGameplayTag sourceTag){
    totalHealed += amount;
}

void UStubHealthService::damagePlayer_Implementation(APlayerState* player, float amount, FGameplayTag damageTypeTag){
    totalDamaged += amount;
    lastDamageType = damageTypeTag;
}

void UStubActionService::cancelAction_Implementation(APlayerState* player, FGameplayTag actionTag){
    cancelledActions.Add(actionTag);
}

AActor* UStubLooseItemSpawner::spawnLooseItem_Implementation(APlayerState* instigatorPlayer, const FS_InventoryEntry& entry, FVector location){
    spawnedEntries.Add(entry);
    spawnLocations.Add(location);
    return nullptr;
}

FS_GearReleaseResult UStubEquipmentGear::releaseEquippedGear_Implementation(APlayerState* player){
    return FS_GearReleaseResult();
}

bool UStubEquipmentGear::reEquipGear_Implementation(APlayerState* player, const TArray<FS_GearSlotRecord>& gear){
    return true;
}

bool UStubBagActorService::createBag_Implementation(FGuid bagId, FGuid ownerCharacterId, FVector location){
    FS_BagInfo info;
    info.isValid = true;
    info.bagId = bagId;
    info.ownerCharacterId = ownerCharacterId;
    info.location = location;
    bags.Add(bagId, info);
    return true;
}

void UStubBagActorService::setBagContents_Implementation(FGuid bagId, const FS_BagContents& contents){
    FS_BagInfo* info = bags.Find(bagId);
    if(info != nullptr){
        info->contents = contents;
    }
}

FS_BagInfo UStubBagActorService::findBag_Implementation(FGuid bagId){
    const FS_BagInfo* info = bags.Find(bagId);
    return info != nullptr ? *info : FS_BagInfo();
}

FS_BagInfo UStubBagActorService::findBagOwnedBy_Implementation(FGuid ownerCharacterId){
    for(const TPair<FGuid, FS_BagInfo>& pair : bags){
        if(pair.Value.ownerCharacterId == ownerCharacterId){
            return pair.Value;
        }
    }
    return FS_BagInfo();
}

void UStubBagActorService::deleteBag_Implementation(FGuid bagId){
    bags.Remove(bagId);
}

TArray<FGuid> UStubBagActorService::removeBagsOwnedBy_Implementation(FGuid ownerCharacterId){
    TArray<FGuid> inventoryIds;
    for(auto iterator = bags.CreateIterator(); iterator; ++iterator){
        if(iterator.Value().ownerCharacterId == ownerCharacterId){
            inventoryIds.Add(iterator.Value().contents.inventoryId);
            iterator.RemoveCurrent();
        }
    }
    return inventoryIds;
}
```

### S1.12 — Fill `UInventoryServices`

Replace the Section 0 skeleton. This subsystem is the **one seam registry of the project**: a map from an interface class to the object that implements it. The R05 guide registers its own seams in it too. A teammate replaces a stub with one line: `UInventoryServices::get(world)->registerSeam(UBPI_GameClock::StaticClass(), myTimeSubsystem);`

`InventoryServices.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Math/RandomStream.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "InventoryServices.generated.h"

class APlayerState;

UCLASS()
class AWAKEN_API UInventoryServices : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static UInventoryServices* get(const UObject* worldContext);

    // ---- the seam registry ----
    // Command. Replaces whatever was registered for that interface.
    void registerSeam(UClass* interfaceClass, UObject* implementation);
    // Query. Null when nothing is registered.
    UObject* getSeam(const UClass* interfaceClass) const;

    // ---- facade: one short call per outside framework ----
    int64 getGameTime() const;                                                                                      // Doc R12
    bool isIncapacitated(const APlayerState* player) const;                                                         // Docs B02, R03, R11, R17
    bool isCarryActive(const APlayerState* player) const;                                                           // Doc B07
    bool beginCarry(APlayerState* player, const FS_InventoryEntry& entry) const;                                    // Doc B07
    void modifyMeter(APlayerState* player, const FGameplayTag& meterTag, float delta, const FGameplayTag& sourceTag) const;   // Doc B03
    void registerIllnessSource(APlayerState* player, const FGameplayTag& sourceTag, float durationHours) const;     // Doc B03
    bool isIllnessActive(const APlayerState* player) const;                                                         // Docs B03, B13
    void healPlayer(APlayerState* player, float amount, const FGameplayTag& sourceTag) const;                       // Doc R08
    void damagePlayer(APlayerState* player, float amount, const FGameplayTag& damageTypeTag) const;                 // Doc R03
    void cancelAction(APlayerState* player, const FGameplayTag& actionTag) const;                                   // Doc R04
    AActor* spawnLooseItem(APlayerState* instigatorPlayer, const FS_InventoryEntry& entry, const FVector& location) const;   // Doc02 19.7
    FS_GearReleaseResult releaseEquippedGear(APlayerState* player) const;                                           // Doc R05
    bool reEquipGear(APlayerState* player, const TArray<FS_GearSlotRecord>& gear) const;                            // Doc R05
    UObject* getBagService() const;                                                                                 // Docs B02, R13
    void raiseItemDroppedCosmetic(APlayerState* player, const FVector& location) const;                             // Doc B21 (I20)

    // True with the given probability. Uses one stream so a fixed seed repeats a test (Doc B22 3.7).
    bool rollChance(float chance) const;

private:
    template<typename InterfaceClass>
    UObject* find() const{
        return getSeam(InterfaceClass::StaticClass());
    }

    UPROPERTY() TMap<TObjectPtr<UClass>, TObjectPtr<UObject>> seams;
    mutable FRandomStream randomStream;
};
```

`InventoryServices.cpp`

```cpp
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/Stubs/InventoryStubs.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

#if !UE_BUILD_SHIPPING
// Tests only. -1 = a different sequence every run.
static TAutoConsoleVariable<int32> cvarRandomSeed(TEXT("Awaken.Inventory.RandomSeed"), -1, TEXT("Fixed seed for the illness-chance roll (tests only). -1 = not fixed. Read when the game instance starts."), ECVF_Cheat);
#endif

void UInventoryServices::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);

    // One stub per seam. A real framework replaces its stub with registerSeam.
    registerSeam(UBPI_GameClock::StaticClass(), NewObject<UStubGameClock>(this));                  // Doc R12
    registerSeam(UBPI_PlayerStateQuery::StaticClass(), NewObject<UStubPlayerStateQuery>(this));    // Docs B02, R03, R11, R17
    registerSeam(UBPI_CarryService::StaticClass(), NewObject<UStubCarryService>(this));            // Doc B07
    registerSeam(UBPI_SurvivalService::StaticClass(), NewObject<UStubSurvivalService>(this));      // Doc B03
    registerSeam(UBPI_HealthService::StaticClass(), NewObject<UStubHealthService>(this));          // Docs R08, R03
    registerSeam(UBPI_ActionService::StaticClass(), NewObject<UStubActionService>(this));          // Doc R04
    registerSeam(UBPI_EquipmentGear::StaticClass(), NewObject<UStubEquipmentGear>(this));          // Doc R05
    registerSeam(UBPI_BagActorService::StaticClass(), NewObject<UStubBagActorService>(this));      // Docs B02, R13
    registerSeam(UBPI_LooseItemSpawner::StaticClass(), NewObject<UStubLooseItemSpawner>(this));    // replaced by the real spawner in S5.3

    int32 seed = FMath::Rand();
#if !UE_BUILD_SHIPPING
    if(cvarRandomSeed.GetValueOnGameThread() >= 0){
        seed = cvarRandomSeed.GetValueOnGameThread();
    }
#endif
    randomStream.Initialize(seed);

    INVENTORY_LOG(Log, this, "Item & Inventory framework started, %d seams registered", seams.Num());
}

UInventoryServices* UInventoryServices::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    if(gameInstance == nullptr){
        return nullptr;
    }
    return gameInstance->GetSubsystem<UInventoryServices>();
}

void UInventoryServices::registerSeam(UClass* interfaceClass, UObject* implementation){
    if(interfaceClass == nullptr || implementation == nullptr || !implementation->GetClass()->ImplementsInterface(interfaceClass)){
        INVENTORY_LOG(Error, this, "registerSeam refused: %s does not implement %s", *GetNameSafe(implementation), *GetNameSafe(interfaceClass));
        return;
    }
    seams.Add(interfaceClass, implementation);
}

UObject* UInventoryServices::getSeam(const UClass* interfaceClass) const{
    const TObjectPtr<UObject>* found = seams.Find(const_cast<UClass*>(interfaceClass));
    return found != nullptr ? found->Get() : nullptr;
}

int64 UInventoryServices::getGameTime() const{
    UObject* seam = find<UBPI_GameClock>();
    return seam != nullptr ? IBPI_GameClock::Execute_getGameTime(seam) : 0;
}

bool UInventoryServices::isIncapacitated(const APlayerState* player) const{
    UObject* seam = find<UBPI_PlayerStateQuery>();
    return seam != nullptr && IBPI_PlayerStateQuery::Execute_isIncapacitated(seam, const_cast<APlayerState*>(player));
}

bool UInventoryServices::isCarryActive(const APlayerState* player) const{
    UObject* seam = find<UBPI_CarryService>();
    return seam != nullptr && IBPI_CarryService::Execute_isCarryActive(seam, const_cast<APlayerState*>(player));
}

bool UInventoryServices::beginCarry(APlayerState* player, const FS_InventoryEntry& entry) const{
    UObject* seam = find<UBPI_CarryService>();
    return seam != nullptr && IBPI_CarryService::Execute_beginCarry(seam, player, entry);
}

void UInventoryServices::modifyMeter(APlayerState* player, const FGameplayTag& meterTag, float delta, const FGameplayTag& sourceTag) const{
    UObject* seam = find<UBPI_SurvivalService>();
    if(seam != nullptr){
        IBPI_SurvivalService::Execute_modifyMeter(seam, player, meterTag, delta, sourceTag);
    }
}

void UInventoryServices::registerIllnessSource(APlayerState* player, const FGameplayTag& sourceTag, float durationHours) const{
    UObject* seam = find<UBPI_SurvivalService>();
    if(seam != nullptr){
        IBPI_SurvivalService::Execute_registerIllnessSource(seam, player, sourceTag, durationHours);
    }
}

bool UInventoryServices::isIllnessActive(const APlayerState* player) const{
    UObject* seam = find<UBPI_SurvivalService>();
    return seam != nullptr && IBPI_SurvivalService::Execute_isIllnessActive(seam, const_cast<APlayerState*>(player));
}

void UInventoryServices::healPlayer(APlayerState* player, float amount, const FGameplayTag& sourceTag) const{
    UObject* seam = find<UBPI_HealthService>();
    if(seam != nullptr){
        IBPI_HealthService::Execute_healPlayer(seam, player, amount, sourceTag);
    }
}

void UInventoryServices::damagePlayer(APlayerState* player, float amount, const FGameplayTag& damageTypeTag) const{
    UObject* seam = find<UBPI_HealthService>();
    if(seam != nullptr){
        IBPI_HealthService::Execute_damagePlayer(seam, player, amount, damageTypeTag);
    }
}

void UInventoryServices::cancelAction(APlayerState* player, const FGameplayTag& actionTag) const{
    UObject* seam = find<UBPI_ActionService>();
    if(seam != nullptr){
        IBPI_ActionService::Execute_cancelAction(seam, player, actionTag);
    }
}

AActor* UInventoryServices::spawnLooseItem(APlayerState* instigatorPlayer, const FS_InventoryEntry& entry, const FVector& location) const{
    UObject* seam = find<UBPI_LooseItemSpawner>();
    return seam != nullptr ? IBPI_LooseItemSpawner::Execute_spawnLooseItem(seam, instigatorPlayer, entry, location) : nullptr;
}

FS_GearReleaseResult UInventoryServices::releaseEquippedGear(APlayerState* player) const{
    UObject* seam = find<UBPI_EquipmentGear>();
    return seam != nullptr ? IBPI_EquipmentGear::Execute_releaseEquippedGear(seam, player) : FS_GearReleaseResult();
}

bool UInventoryServices::reEquipGear(APlayerState* player, const TArray<FS_GearSlotRecord>& gear) const{
    UObject* seam = find<UBPI_EquipmentGear>();
    return seam != nullptr && IBPI_EquipmentGear::Execute_reEquipGear(seam, player, gear);
}

UObject* UInventoryServices::getBagService() const{
    return find<UBPI_BagActorService>();
}

void UInventoryServices::raiseItemDroppedCosmetic(APlayerState* player, const FVector& location) const{
    // TEMPORARY body — owner: Doc B21. The cosmetic channel does not exist yet (I20). S5.7 gives the replacement.
    INVENTORY_LOG(Verbose, this, "Cosmetic event %s at %s (not sent: no cosmetic channel yet)", *T_Cosmetic_Event_Interaction_ItemDropped.GetTag().ToString(), *location.ToString());
}

bool UInventoryServices::rollChance(float chance) const{
    return randomStream.FRand() < chance;
}
```

### Verify (Section 1)

1. **Compile** Editor + Development. Zero warnings.
2. **Tags:** *Project Settings → Gameplay Tags*, search `Inventory.` and `Item.`: the messages, categories, pages and fail reasons show with *Native* source.
3. **Startup log:** `Item & Inventory framework started, 9 seams registered`.
4. **Default-value test (tier 1):** a fresh `FS_InventoryEntry` answers `isInstanced() == false` and `getAvailableQuantity() == 0`; a fresh `FS_InventoryResult` answers `succeeded() == true`; a fresh `FS_GearReleaseResult` has `succeeded == true`; two entries with the same `itemId` and no instance id answer `isSameLineAs == true`.
5. **Mirror test (tier 1):** fill an `FS_InventoryEntryList` with `syncFrom` from three entries, change one quantity and remove one, call `syncFrom` again: `toEntries()` equals the new array.

### Handoff

- Every type, tag and seam interface exists. `UInventoryServices::get(world)->registerSeam(...)` is how every teammate plugs a real framework in.
- Tell the owners: R12 implements `IBPI_GameClock`; B07 `IBPI_CarryService`; B03 `IBPI_SurvivalService`; R08 and R03 `IBPI_HealthService`; R04 `IBPI_ActionService`; R05 `IBPI_EquipmentGear`; B02 and R13 `IBPI_BagActorService`; B02, R03, R11 and R17 agree on one `IBPI_PlayerStateQuery`.
- New tags to log in the Tag Registry: the five `Inventory.Page.*` leaves, `Item.Flag.Permanent`, and the PROPOSED `Inventory.FailReason.*` leaves of S1.8.

---
# SECTION 2 — The Item Definition, the Catalog and the First Items

**Goal:** an item is a data asset, never code; the game can look any item up by its id; bad item data is caught in the editor.
**Spec:** R01 3.1, 3.1a, 3.1b, 3.2a, 3.9, 3.10, §11; Doc02 §12, §19.8; Doc R10 3.3.
**Depends on:** Section 1.

### Files

```
Public/ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h   (append)
Public/ItemInventoryFramework/
    UDA_InventoryPageLayout.h   UDA_StackLayout.h
    UDA_ItemDefinition.h        UItemCatalogSubsystem.h   InventoryTime.h
Private/ItemInventoryFramework/
    UDA_InventoryPageLayout.cpp UDA_StackLayout.cpp
    UDA_ItemDefinition.cpp      UItemCatalogSubsystem.cpp
Config/DefaultGame.ini          (the Asset Manager entry)
Content/Awaken/Items/           Content/Awaken/Inventory/
```

### S2.1 — Bag-screen structs (R01 3.1a, 3.1b)

Append to the end of `S_ItemInventoryFramework.h`, and add `class UDA_InventoryPageLayout;` to the forward declarations at the top:

```cpp
// =====================================================================
// S2.1  Bag-screen data (R01 3.1a, 3.1b). Presentation only: never saved, never replicated, never read by gameplay.
// =====================================================================

// One fixed slot of a page. slotId is unique inside its layout and is never renumbered or reused (R01 3.1b).
USTRUCT(BlueprintType)
struct FS_InventorySlotDef{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName slotId;
    // Position and size on the page, chosen when the layout is authored. A long item gets a wide slot.
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D position = FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector2D size = FVector2D(100.0, 100.0);
};

// How an item points at its slot: the page layout and the slot id, so moving or resizing the slot breaks nothing.
USTRUCT(BlueprintType)
struct FS_InventorySlotRef{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UDA_InventoryPageLayout> pageLayout;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName slotId;

    bool isSet() const{
        return !pageLayout.IsNull() && !slotId.IsNone();
    }
};

// Where unit n of a pile sits inside the slot's pocket and how it is rotated (R01 3.1a).
USTRUCT(BlueprintType)
struct FS_StackUnit{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FVector position = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FRotator rotation = FRotator::ZeroRotator;
};

// One row of the page mapping (R01 3.1a): which categories land on which page, and that page's layout.
USTRUCT(BlueprintType)
struct FS_InventoryPageMapping{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Inventory.Page")) FGameplayTag pageTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Item.Category")) FGameplayTagContainer categories;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UDA_InventoryPageLayout> layout;
};

// The answer of the slot lookup (Section 10): where an item is drawn.
USTRUCT(BlueprintType)
struct FS_ItemSlotInfo{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FGameplayTag pageTag;
    UPROPERTY(BlueprintReadOnly) FS_InventorySlotRef slot;
    // A declared variant pair (fresh and spoiled, clean and unclean) shares the slot; each id is its own pile.
    UPROPERTY(BlueprintReadOnly) TArray<FName> itemsInSlot;
};
```

### S2.2 — `UDA_InventoryPageLayout`

`Public/ItemInventoryFramework/UDA_InventoryPageLayout.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UDA_InventoryPageLayout.generated.h"

// The fixed slots of one page of the bag screen (R01 3.1a). Adding a slot is an edit of this asset, never new code.
UCLASS(BlueprintType)
class AWAKEN_API UDA_InventoryPageLayout : public UDataAsset{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout", meta = (Categories = "Inventory.Page")) FGameplayTag pageTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout") TArray<FS_InventorySlotDef> slots;

    // Query. Null when the layout has no slot with that id.
    const FS_InventorySlotDef* findSlot(FName slotId) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
```

`Private/ItemInventoryFramework/UDA_InventoryPageLayout.cpp`

```cpp
#include "ItemInventoryFramework/UDA_InventoryPageLayout.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const FS_InventorySlotDef* UDA_InventoryPageLayout::findSlot(FName slotId) const{
    return slots.FindByPredicate([slotId](const FS_InventorySlotDef& slot){
        return slot.slotId == slotId;
    });
}

#if WITH_EDITOR
EDataValidationResult UDA_InventoryPageLayout::IsDataValid(FDataValidationContext& Context) const{
    EDataValidationResult result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
    if(!pageTag.IsValid()){
        Context.AddError(FText::FromString(FString::Printf(TEXT("%s: the layout has no page tag."), *GetName())));
        result = EDataValidationResult::Invalid;
    }
    TSet<FName> seenIds;
    for(const FS_InventorySlotDef& slot : slots){
        if(slot.slotId.IsNone() || seenIds.Contains(slot.slotId)){
            Context.AddError(FText::FromString(FString::Printf(TEXT("%s: slot id '%s' is empty or used twice."), *GetName(), *slot.slotId.ToString())));
            result = EDataValidationResult::Invalid;
        }
        seenIds.Add(slot.slotId);
        if(slot.size.X <= 0.0 || slot.size.Y <= 0.0){
            Context.AddError(FText::FromString(FString::Printf(TEXT("%s: slot '%s' has no size."), *GetName(), *slot.slotId.ToString())));
            result = EDataValidationResult::Invalid;
        }
    }
    return result;
}
#endif
```

### S2.3 — `UDA_StackLayout`

`Public/ItemInventoryFramework/UDA_StackLayout.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UDA_StackLayout.generated.h"

// How a pile is built (R01 3.1a): unit n always sits at the same place with the same rotation,
// so adding a unit moves no other unit, and nothing is random or saved. A few layouts are shared
// (a heap, a stack, a row, a bundle) and each item picks one.
UCLASS(BlueprintType)
class AWAKEN_API UDA_StackLayout : public UDataAsset{
    GENERATED_BODY()

public:
    // Entry 0 is unit 1. The visible capacity of the pile is the length of this list: there is no separate cap.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout") TArray<FS_StackUnit> units;
    // The size a full pile takes in the slot, in the same units as the slot size (I14).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout") FVector2D pileFootprint = FVector2D(100.0, 100.0);

    // Query. How many units the UI draws for a quantity: beyond the end of the list the pile stays as full as it can look.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory")
    int32 getVisibleUnitCount(int32 quantity) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
```

`Private/ItemInventoryFramework/UDA_StackLayout.cpp`

```cpp
#include "ItemInventoryFramework/UDA_StackLayout.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

int32 UDA_StackLayout::getVisibleUnitCount(int32 quantity) const{
    return FMath::Clamp(quantity, 0, units.Num());
}

#if WITH_EDITOR
EDataValidationResult UDA_StackLayout::IsDataValid(FDataValidationContext& Context) const{
    EDataValidationResult result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
    if(units.Num() == 0){
        Context.AddError(FText::FromString(FString::Printf(TEXT("%s: a stack layout needs at least one unit."), *GetName())));
        result = EDataValidationResult::Invalid;
    }
    return result;
}
#endif
```

### S2.4 to S2.9 — `UDA_ItemDefinition`

One class, built block by block. The definition holds data that never changes; state that changes belongs to an instance (R01 3.1).

`Public/ItemInventoryFramework/UDA_ItemDefinition.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ItemInventoryFramework/00Enums/E_ItemInventoryFramework.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "UDA_ItemDefinition.generated.h"

class UStaticMesh;
class UTexture2D;
class UDA_StackLayout;

// R01 3.1: every item is one of these. A new item is a new data asset, never new code.
UCLASS(BlueprintType)
class AWAKEN_API UDA_ItemDefinition : public UPrimaryDataAsset{
    GENERATED_BODY()

public:
    // The unbreakable sentinel (R01 3.1, I4). Used by the starter Axe only.
    inline static constexpr float unbreakableDurability = -1.0f;

    static FPrimaryAssetType getPrimaryAssetType(){
        return FPrimaryAssetType(TEXT("Item"));
    }

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    // ---------- S2.4 Identity ----------
    // The stable id (I1). Saves, replication, recipes and hotbar keys all carry this name. Never rename it after release.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") FName itemId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") E_ItemKind itemKind = E_ItemKind::Stackable;
    // What kind of item it is. The page of the bag screen is derived from this (R01 3.1a).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (Categories = "Item.Category")) FGameplayTag categoryTag;
    // Tags used for restrictions and flags: Item.Flag.Permanent, clean or unclean water, and so on.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") FGameplayTagContainer itemTags;
    // The most of this item the bag can hold (designer-confirmed: the bag limits each item type). The value is data.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (ClampMin = "0")) int32 perTypeLimit = 1;
    // Soft references (Doc02 19.8). The icon stays for places that cannot show 3D (hover cards, popups).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") TSoftObjectPtr<UTexture2D> icon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") TSoftObjectPtr<UStaticMesh> worldMesh;

    // ---------- S2.5 Carry (Doc B07) and processing (Doc B10). This document only holds the fields ----------
    // 0 = an ordinary item. Above 0 = a heavy item: it fills that many carry slots and NEVER enters the bag (R01 3.7 step 0).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry", meta = (ClampMin = "0")) int32 carryCapacityCost = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Carry", meta = (Categories = "Carry.WeightClass")) FGameplayTag weightClassTag;
    // 'Use directly' versus 'must be cooked, dried or boiled first'.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing") bool requiresProcessing = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing") FName linkedProcessingRecipe;

    // ---------- S2.6 The weapon block (Doc R03 reads these numbers; it does not own them) and durability ----------
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") E_WeaponType weaponType = E_WeaponType::None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0")) float attack = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0")) float attackSpeed = 1.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0")) float damage = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0", ClampMax = "1")) float armorPenetrationPercent = 0.0f;
    // Crossbow and Firearm: the stackable ammunition item that a reload spends (name PROPOSED; to confirm with Doc R03).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon") FName ammoItemId;
    // Firearm only (names PROPOSED, all values data). 1 for a single-shot gun.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Firearm", meta = (ClampMin = "0")) int32 magazineSize = 0;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Firearm", meta = (ClampMin = "0")) float bulletSpeed = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Firearm", meta = (ClampMin = "0")) float bulletDrop = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Firearm", meta = (ClampMin = "0")) float bulletLongestRange = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Firearm") float recoilPitch = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Firearm") float recoilYaw = 0.0f;
    // -1 = unbreakable (the starter Axe only), 0 = no durability, above 0 = how much the item has when new.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Durability") float durabilityMax = 0.0f;
    // Weapon wear is a flat amount per use (Index A112). Doc R03 applies it.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Durability", meta = (ClampMin = "0")) float durabilityLossPerUse = 0.0f;

    // ---------- S2.7 Consumables, spoilage, medicine (R01 3.2a, 3.9) ----------
    // Food, drink and medicine: meter changes, plus the illness chance and HP loss of spoiled food and unclean water.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume") TArray<FS_ConsumeEffect> consumeEffects;
    // The spec's bCanSpoil. Off = never spoils.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume|Spoilage") bool canSpoil = false;
    // In in-game hours. Only read when canSpoil is on.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume|Spoilage", meta = (ClampMin = "0", EditCondition = "canSpoil")) float spoilTimeHours = 0.0f;
    // The item this one becomes when its spoil time is reached.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume|Spoilage", meta = (EditCondition = "canSpoil")) FName spoiledItemId;
    // The share of this item's fresh meter values that its spoiled variant gives (designer: 0.5).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume|Spoilage", meta = (ClampMin = "0", ClampMax = "1")) float spoiledEffectiveness = 0.5f;
    // Variant items only (unclean water): the item whose meter values this one derives from...
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume|Variant") FName effectBaseItemId;
    // ...scaled by this share (designer: 0.5). A separate field from spoiledEffectiveness so they tune apart.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume|Variant", meta = (ClampMin = "0", ClampMax = "1")) float uncleanWaterEffectiveness = 0.5f;
    // Medicine only (R01 3.9, decision D10).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume|Medicine", meta = (ClampMin = "0")) float healthRestoreAmount = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consume|Medicine", meta = (ClampMin = "0")) float illnessReductionAmount = 0.0f;

    // ---------- S2.8 Portable Container, loose life, and the Equipment block ----------
    // Portable Container items only: the limit per item type and which tags it accepts (R01 3.1, 3.4).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container") FS_ContainerRules capacityRules;
    // Life of this item as a loose object, in seconds. 0 = the project default (Doc02 19.7).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loose", meta = (ClampMin = "0")) float looseLifetimeSeconds = 0.0f;

    // Equipment block: Doc R05 fills this in later. Nothing here on purpose.

    // ---------- S2.9 Bag screen (R01 3.1a; names PROPOSED) ----------
    // The one fixed slot of this item on its page. The page itself is not stored: it is derived from categoryTag.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bag Screen") FS_InventorySlotRef inventorySlot;
    // The low-poly model drawn in the slot, one per unit. Separate from worldMesh so its detail is its own data.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bag Screen") TSoftObjectPtr<UStaticMesh> inventoryMesh;
    // Optional. The other item id that shares this item's slot: spoiled and fresh food, clean and unclean water.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bag Screen") FName slotSharedWith;
    // Where unit 1, 2, 3 and so on of the pile sit.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bag Screen") TSoftObjectPtr<UDA_StackLayout> stackLayout;
    // Optional second slot for the smaller staging widget (Doc R02 3.3). Unset = the inventory slot, scaled.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bag Screen") FS_InventorySlotRef stagingSlot;

    // ---------- Queries ----------
    bool isStackable() const{
        return itemKind == E_ItemKind::Stackable;
    }

    bool isInstanced() const{
        return itemKind == E_ItemKind::Instanced;
    }

    bool isWeapon() const{
        return categoryTag == T_Item_Category_Weapon;
    }

    // Permanent items cannot be thrown, dropped, moved into a container, staged or destroyed (R01 3.10).
    bool isPermanent() const{
        return itemTags.HasTagExact(T_Item_Flag_Permanent);
    }

    bool isUnbreakable() const{
        return durabilityMax == unbreakableDurability;
    }

    bool hasDurability() const{
        return durabilityMax > 0.0f;
    }

    // A heavy item goes to the hands only (R01 3.7 step 0, 3.11).
    bool isHeavy() const{
        return carryCapacityCost > 0;
    }

    bool isPortableContainer() const{
        return categoryTag == T_Item_Category_Container && isInstanced();
    }

    bool isConsumable() const{
        return consumeEffects.Num() > 0 || !effectBaseItemId.IsNone() || healthRestoreAmount > 0.0f || illnessReductionAmount > 0.0f;
    }

    // True when the item's category tag or one of its item tags matches.
    bool carriesTag(const FGameplayTag& tag) const{
        return tag.IsValid() && (categoryTag.MatchesTag(tag) || itemTags.HasTag(tag));
    }

    // Text keys, derived from the id (Doc R10 3.3, I22). The UI turns a key into text; nothing here holds a sentence.
    UFUNCTION(BlueprintPure, Category = "Awaken|Item") FString getDisplayNameKey() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Item") FString getDescriptionKey() const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
```

> **Do not reword the Equipment block comment.** The R05 guide (E2.1) tells you to replace exactly that line with the armor and clothes fields.

`Private/ItemInventoryFramework/UDA_ItemDefinition.cpp` (the data check is added in S2.12)

```cpp
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FPrimaryAssetId UDA_ItemDefinition::GetPrimaryAssetId() const{
    // Item:<itemId>. Before the id is typed in, the asset name stands in so the Asset Manager can still list it.
    return FPrimaryAssetId(getPrimaryAssetType(), itemId.IsNone() ? GetFName() : itemId);
}

FString UDA_ItemDefinition::getDisplayNameKey() const{
    return FString::Printf(TEXT("Item.%s.Name"), *itemId.ToString());
}

FString UDA_ItemDefinition::getDescriptionKey() const{
    return FString::Printf(TEXT("Item.%s.Description"), *itemId.ToString());
}
```

### S2.10 — Register items with the Asset Manager (Doc02 §19.8)

*Project Settings → Game → Asset Manager → Primary Asset Types to Scan*, add one entry:

| Field | Value |
| --- | --- |
| Primary Asset Type | `Item` |
| Asset Base Class | `DA_ItemDefinition` (`UDA_ItemDefinition`) |
| Has Blueprint Classes | off |
| Directories | `/Game/Awaken/Items` |
| Rules → Cook Rule | Always Cook |

This writes the entry into `Config/DefaultGame.ini`. Chunking and loading bundles are still open in Doc02 §19.8, so nothing more is set here.

### S2.11 — `UItemCatalogSubsystem`

The catalog loads every item *definition* once, when the game instance starts. Definitions are small: the heavy assets (meshes, icons) are soft references and are loaded only by whoever shows them, so this does not load the art of the whole catalog.

`Public/ItemInventoryFramework/UItemCatalogSubsystem.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UItemCatalogSubsystem.generated.h"

class UDA_ItemDefinition;

// Looks an item definition up by its id. Exists on every machine; the data is static content (Doc00 §3).
UCLASS()
class AWAKEN_API UItemCatalogSubsystem : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static UItemCatalogSubsystem* get(const UObject* worldContext);

    // ---- queries ----
    // Null when no item has that id (a removed item in an old save, a typo in a command). Never crashes.
    const UDA_ItemDefinition* getDefinition(FName itemId) const;
    // The always-available Unarmed entry (R01 3.10).
    const UDA_ItemDefinition* getUnarmedDefinition() const;
    // The fresh item whose spoiledItemId is this id, or null (R01 3.2a).
    const UDA_ItemDefinition* findFreshItemFor(FName spoiledItemId) const;
    const TMap<FName, TObjectPtr<UDA_ItemDefinition>>& getAllDefinitions() const;

    // ---- commands ----
    // Adds one definition. Used by the loader and by tests.
    void registerDefinition(UDA_ItemDefinition* definition);
    // Tests only: forget every definition.
    void resetForTest();

private:
    void loadAllDefinitions();

    UPROPERTY() TMap<FName, TObjectPtr<UDA_ItemDefinition>> definitions;
};
```

`Private/ItemInventoryFramework/UItemCatalogSubsystem.cpp`

```cpp
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

void UItemCatalogSubsystem::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);
    loadAllDefinitions();
    INVENTORY_LOG(Log, this, "Item catalog loaded: %d definitions", definitions.Num());
}

UItemCatalogSubsystem* UItemCatalogSubsystem::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    if(gameInstance == nullptr){
        return nullptr;
    }
    return gameInstance->GetSubsystem<UItemCatalogSubsystem>();
}

void UItemCatalogSubsystem::loadAllDefinitions(){
    UAssetManager* manager = UAssetManager::GetIfInitialized();
    if(manager == nullptr){
        return;
    }
    TArray<FPrimaryAssetId> ids;
    manager->GetPrimaryAssetIdList(UDA_ItemDefinition::getPrimaryAssetType(), ids);
    for(const FPrimaryAssetId& id : ids){
        UDA_ItemDefinition* definition = Cast<UDA_ItemDefinition>(manager->GetPrimaryAssetPath(id).TryLoad());
        if(definition != nullptr){
            registerDefinition(definition);
        }
    }
}

void UItemCatalogSubsystem::registerDefinition(UDA_ItemDefinition* definition){
    if(definition == nullptr || definition->itemId.IsNone()){
        INVENTORY_LOG(Error, this, "Item definition %s has no itemId and was not registered", *GetNameSafe(definition));
        return;
    }
    if(definitions.Contains(definition->itemId)){
        INVENTORY_LOG(Error, this, "Duplicate itemId %s: %s was not registered", *definition->itemId.ToString(), *definition->GetName());
        return;
    }
    definitions.Add(definition->itemId, definition);
}

void UItemCatalogSubsystem::resetForTest(){
    definitions.Reset();
}

const UDA_ItemDefinition* UItemCatalogSubsystem::getDefinition(FName itemId) const{
    const TObjectPtr<UDA_ItemDefinition>* found = definitions.Find(itemId);
    return found != nullptr ? found->Get() : nullptr;
}

const UDA_ItemDefinition* UItemCatalogSubsystem::getUnarmedDefinition() const{
    return getDefinition(UInventorySettings::get()->unarmedItemId);
}

const UDA_ItemDefinition* UItemCatalogSubsystem::findFreshItemFor(FName spoiledItemId) const{
    if(spoiledItemId.IsNone()){
        return nullptr;
    }
    // Asked only when something is eaten or validated, so a plain search over the catalog is enough.
    for(const TPair<FName, TObjectPtr<UDA_ItemDefinition>>& pair : definitions){
        if(pair.Value->canSpoil && pair.Value->spoiledItemId == spoiledItemId){
            return pair.Value;
        }
    }
    return nullptr;
}

const TMap<FName, TObjectPtr<UDA_ItemDefinition>>& UItemCatalogSubsystem::getAllDefinitions() const{
    return definitions;
}
```

### S2.12 — The per-asset data check (Doc02 §19.8, R01 3.1a)

This check runs when an item asset is saved or validated. It covers what one asset can know about itself. What needs the whole catalog (two items in one slot, a missing spoiled item) is checked in Section 10.

Add to `UDA_ItemDefinition.cpp`:

```cpp
#if WITH_EDITOR
EDataValidationResult UDA_ItemDefinition::IsDataValid(FDataValidationContext& Context) const{
    EDataValidationResult result = CombineDataValidationResults(Super::IsDataValid(Context), EDataValidationResult::Valid);
    auto fail = [this, &Context, &result](const TCHAR* text){
        Context.AddError(FText::FromString(FString::Printf(TEXT("%s: %s"), *GetName(), text)));
        result = EDataValidationResult::Invalid;
    };
    // The Unarmed entry is never stored anywhere and a heavy item never enters the bag: neither needs a limit or a slot.
    const bool isNeverStored = itemId == UInventorySettings::get()->unarmedItemId;
    const bool isBagItem = !isNeverStored && !isHeavy();

    // ---- identity ----
    if(itemId.IsNone()){
        fail(TEXT("itemId is empty (Doc02 19.8: a missing id fails the check)."));
    }
    if(!categoryTag.IsValid() || !categoryTag.MatchesTag(T_Item_Category)){
        fail(TEXT("categoryTag must be a leaf of Item.Category."));
    }
    if(isBagItem && perTypeLimit < 1){
        fail(TEXT("perTypeLimit must be at least 1."));
    }

    // ---- the two kinds (R01 3.2) ----
    if(isStackable() && durabilityMax != 0.0f){
        fail(TEXT("A stackable item has no state of its own: durabilityMax must be 0. Make it Instanced if it wears out."));
    }
    if(isInstanced() && canSpoil){
        fail(TEXT("Only a stackable item can spoil: the spoil time belongs to the stack."));
    }
    if((categoryTag == T_Item_Category_Container) != isPortableContainer()){
        fail(TEXT("A Portable Container item must be Instanced: each copy keeps its own contents."));
    }

    // ---- weapon and durability ----
    if(isWeapon() != (weaponType != E_WeaponType::None)){
        fail(TEXT("A weapon needs a weaponType, and only a weapon may have one."));
    }
    if(isWeapon() && attackSpeed <= 0.0f){
        fail(TEXT("A weapon needs attackSpeed above zero."));
    }
    if((weaponType == E_WeaponType::Crossbow || weaponType == E_WeaponType::Firearm) && ammoItemId.IsNone()){
        fail(TEXT("A crossbow or a firearm needs ammoItemId."));
    }
    if(weaponType == E_WeaponType::Firearm && magazineSize < 1){
        fail(TEXT("A firearm needs magazineSize of at least 1."));
    }
    if(durabilityMax < 0.0f && !isUnbreakable()){
        fail(TEXT("durabilityMax below zero is only allowed as the unbreakable sentinel, -1."));
    }
    if(isUnbreakable() && !isPermanent()){
        fail(TEXT("The unbreakable sentinel is for the starter Axe only, which also carries Item.Flag.Permanent (R01 3.1, 3.10)."));
    }

    // ---- consumables ----
    if(canSpoil && (spoilTimeHours <= 0.0f || spoiledItemId.IsNone() || spoiledItemId == itemId)){
        fail(TEXT("An item that can spoil needs spoilTimeHours above zero and a spoiledItemId that is another item."));
    }
    if(effectBaseItemId == itemId && !itemId.IsNone()){
        fail(TEXT("effectBaseItemId cannot point at the item itself."));
    }
    for(const FS_ConsumeEffect& effect : consumeEffects){
        if(effect.kind != E_ConsumeEffectKind::HealthLoss && !effect.targetTag.IsValid()){
            fail(TEXT("A ModifyMeter or IllnessChance entry needs its targetTag."));
        }
    }

    // ---- bag screen (R01 3.1a: every item has a slot, a mesh and a stack layout) ----
    if(isBagItem){
        if(!inventorySlot.isSet()){
            fail(TEXT("inventorySlot is not set. Pick a free slot of the page layout of this item's category."));
        }
        if(inventoryMesh.IsNull()){
            fail(TEXT("inventoryMesh is not set."));
        }
        if(stackLayout.IsNull()){
            fail(TEXT("stackLayout is not set."));
        }
    }
    return result;
}
#endif
```

> The R05 guide (E2.2) adds its armor and clothes checks just before `return result;` and uses the `fail` helper and `Context` exactly as named here.

### S2.13 — Time units

Spoil times are in in-game hours in data and in game seconds at runtime (Doc02 §19.4: GameTime is game seconds as a 64-bit whole number). One helper keeps the conversion in one place.

`Public/ItemInventoryFramework/InventoryTime.h`

```cpp
#pragma once

#include "CoreMinimal.h"

namespace InventoryTime{
    inline int64 hoursToGameSeconds(float hours){
        return FMath::RoundToInt64(static_cast<double>(hours) * 3600.0);
    }
}
```

### S2.14 — Author the starter content (editor)

**Layouts first.** In `Content/Awaken/Inventory/` create:

| Asset | Class | Content |
| --- | --- | --- |
| `DA_Page_Weapons`, `DA_Page_Tools`, `DA_Page_Consumables`, `DA_Page_Materials`, `DA_Page_Others` | `DA_InventoryPageLayout` | `pageTag` = the matching `Inventory.Page.*`; about 8 slots each with ids `W1…`, `T1…`, `C1…`, `M1…`, `O1…`; make `W3` wide for the spear |
| `DA_Stack_Heap`, `DA_Stack_Row`, `DA_Stack_Bundle`, `DA_Stack_Single` | `DA_StackLayout` | 10, 4, 10 and 1 units; any positions for now |

How many slots a page has and which item sits in which slot are content, authored with the items (R01 §10). The numbers above are only enough to test with.

**Then the items.** In `Content/Awaken/Items/`, one `DA_ItemDefinition` per row. Name the asset `DA_Item_<itemId>`; the `itemId` field is what the game uses. Every number is a placeholder for testing, not design. Use any small mesh as `inventoryMesh` until the real models exist.

| itemId | Kind | Category | Other fields |
| --- | --- | --- | --- |
| `Unarmed` | Instanced | Weapon | `weaponType Melee`, low `attack`, `attackSpeed 1`, `durabilityMax 0`, `perTypeLimit 0`. No slot: it is never stored (R01 3.10) |
| `StarterAxe` | Instanced | Weapon | `weaponType Melee`, `durabilityMax -1`, `itemTags Item.Flag.Permanent`, `perTypeLimit 1`, slot `W1` |
| `Map` | Stackable | Tool | `itemTags Item.Flag.Permanent`, `perTypeLimit 1`, slot `T1` |
| `Spear` | Instanced | Weapon | `weaponType Thrown`, `durabilityMax 50`, `durabilityLossPerUse 5`, `perTypeLimit 4`, slot `W3`, `DA_Stack_Row` |
| `Crossbow` | Instanced | Weapon | `weaponType Crossbow`, `ammoItemId Arrow`, `durabilityMax 80`, `perTypeLimit 2`, slot `W2` |
| `Arrow` | Stackable | Material | `perTypeLimit 40`, slot `M3`, `DA_Stack_Bundle` |
| `Stick` | Stackable | Material | `perTypeLimit 20`, slot `M1`, `DA_Stack_Bundle` |
| `Stone` | Stackable | Material | `perTypeLimit 20`, slot `M2`, `DA_Stack_Heap` |
| `Log` | Stackable | Material | `carryCapacityCost 1`. A heavy item: no slot, it never enters the bag |
| `RawMeat` | Stackable | Food | `canSpoil`, `spoilTimeHours 24`, `spoiledItemId SpoiledMeat`, `spoiledEffectiveness 0.5`, one effect `ModifyMeter / Survival.Meter.Hunger / 100`, `perTypeLimit 10`, slot `C1` |
| `SpoiledMeat` | Stackable | Food | `slotSharedWith RawMeat`, slot `C1`, two effects: `IllnessChance / Survival.IllnessSource.SpoiledFood / chance 0.3 / sourceDurationHours 24` and `HealthLoss / amount 10`. **No** `ModifyMeter` entry: its hunger value is derived from `RawMeat` |
| `CleanWater` | Stackable | Water | one effect `ModifyMeter / Survival.Meter.Thirst / 100`, `perTypeLimit 10`, slot `C2` |
| `UncleanWater` | Stackable | Water | `effectBaseItemId CleanWater`, `uncleanWaterEffectiveness 0.5`, `slotSharedWith CleanWater`, slot `C2`, effects `IllnessChance / Survival.IllnessSource.UntreatedWater / 0.3 / 24` and `HealthLoss / 10` |
| `Herb` | Stackable | Medicine | `healthRestoreAmount 20`, `illnessReductionAmount 10`, `perTypeLimit 10`, slot `C3` |
| `Pouch` | Instanced | Container | `capacityRules.defaultPerTypeLimit 5`, `perTypeLimit 2`, slot `O1` |
| `IronVest` | Instanced | Armor | `durabilityMax 100`, `perTypeLimit 14`, slot `O2` (the R05 guide adds its armor value) |

> **Signs and amounts of meter changes belong to Doc B03.** Whether "eating gives 100 hunger" is `+100` or `−100` depends on which way the Hunger meter runs. The assets only hold the number that is handed to `modifyMeter`; fix the sign when B03 exists.

> `canSpoil` is set on `RawMeat` only because the tests need one spoilable item. Which foods spoil is deferred by the designer (R01 3.2a).

### Verify (Section 2)

1. **Compile.** Open `DA_Item_Spear`: the categories *Identity, Carry, Processing, Weapon, Durability, Consume, Container, Loose, Bag Screen* show their fields.
2. **Startup log:** `Item catalog loaded: 16 definitions`.
3. **Asset Manager:** *Tools → Audit → Asset Audit → Add Primary Asset → Item* lists the sixteen items as `Item:Spear` and so on.
4. **Break an asset on purpose** (right-click → *Asset Actions → Validate Assets*), then undo each:

| Change | The validator says |
| --- | --- |
| Clear `Spear.itemId` | itemId is empty |
| Set `Stick` to `durabilityMax 5` | A stackable item has no state of its own |
| Set `Spear.durabilityMax -1` | The unbreakable sentinel is for the starter Axe only |
| Set `Spear.weaponType None` | A weapon needs a weaponType |
| Clear `Stone.stackLayout` | stackLayout is not set |
| Clear `Crossbow.ammoItemId` | A crossbow or a firearm needs ammoItemId |
| Set `Pouch` to Stackable | A Portable Container item must be Instanced |
| Give two slots of `DA_Page_Weapons` the id `W1` | slot id 'W1' is empty or used twice |

5. **Duplicate id:** give `Stone` the itemId `Stick` and press Play: the log shows the *Duplicate itemId* error and the catalog holds 15 definitions. Undo.

### Handoff

- `UItemCatalogSubsystem::get(context)->getDefinition(itemId)` is how every framework reads item data. It answers null for an unknown id; callers skip the entry and never crash (Doc R13 3.11).
- **R03** reads the weapon block; confirm the name `ammoItemId` and the firearm field names with its owner.
- **B07** reads `carryCapacityCost` and `weightClassTag`; **B10** reads `requiresProcessing` and `linkedProcessingRecipe`.
- **R10** adds two rows per item to `ST_Items`: `Item.<itemId>.Name` and `Item.<itemId>.Description`.
- **R05** fills the Equipment block (its guide, E2).

---
# SECTION 3 — Inventory Data

**Goal:** one data object that holds entries, enforces the per-type limit, and never loses or duplicates an item.
**Spec:** R01 3.2, 3.3, 3.4, §4, §5 (`GetAvailableQuantity`, `CanFit`, `RemoveStack`, `RemoveInstance`); Doc02 §4, §6, §7.
**Depends on:** Section 2.

### Files

```
Public/ItemInventoryFramework/UInventoryData.h
Private/ItemInventoryFramework/UInventoryData.cpp
```

### Design rules this section enforces

- **An inventory is a data object, not a component** (R01 3.3). It can be handed from a player to a bag in the world in one server step. It never replicates itself (I2).
- **No slot count.** The inventory accepts every item type and limits the *amount of each type* (designer-confirmed). The limit of an instanced item counts its copies.
- **One entry per stackable item id** (I3); one entry per physical instanced item.
- **Server only mutates.** These functions are the *rules*: they are called by the library functions of later sections, which check who is asking. A client holds a read-only copy and never calls a command on it.
- **Validate everything, then change once** (Doc02 §6). A refusal changes nothing and names its reason with a tag.
- **Every reader sees the available quantity** (held minus reserved, R01 3.6). `removeStack` and `removeInstance` only ever take from the available part.

### S3.1 to S3.6 — `UInventoryData`

`UInventoryData.h` (Sections 7 and 8 add the reservation members and fill in `evaluateSpoilage`)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ItemInventoryFramework/04Delegates/D_ItemInventoryFramework.h"
#include "UInventoryData.generated.h"

class UDA_ItemDefinition;

// R01 3.3: the bag, a chest and the inside of a pouch are all one of these.
UCLASS()
class AWAKEN_API UInventoryData : public UObject{
    GENERATED_BODY()

public:
    virtual UWorld* GetWorld() const override;

    // Command. Called once, right after the object is created.
    void setup(const FGuid& newInventoryId, E_InventoryType newType, const FS_ContainerRules& newRules);

    // ---- S3.2 queries ----
    const FGuid& getInventoryId() const;
    E_InventoryType getInventoryType() const;
    // True while this inventory is a bag lying in the world: nobody may take from it or store into it (R01 3.4).
    bool isDeathBag() const;
    bool isEmpty() const;
    const TArray<FS_InventoryEntry>& getEntries() const;
    const FS_InventoryEntry* findStack(FName itemId) const;
    const FS_InventoryEntry* findInstance(const FGuid& instanceId) const;
    // Stackable: the quantity of the stack. Instanced: the number of copies.
    int32 getTotalQuantity(FName itemId) const;
    int32 getInstanceCount(FName itemId) const;
    // R01 §5 GetAvailableQuantity: total minus reserved. The number every consumption check reads.
    int32 getAvailableQuantity(FName itemId) const;
    // The most of this item type the inventory may hold.
    int32 getLimit(const UDA_ItemDefinition& definition) const;
    // An empty tag when this inventory may hold that item type at all (rules, not room).
    FGameplayTag checkAccepts(const UDA_ItemDefinition& definition) const;
    // R01 §5 CanFit: how many units of the stack fit right now (0 to stack.quantity).
    int32 canFit(const FS_ItemStack& stack) const;
    // The reason to give when nothing fits: BagFull for a bag, ContainerFull for a container.
    FGameplayTag getFullReason() const;

    // ---- S3.3, S3.4 commands (server) ----
    // Adds as much as the limit allows and reports how much. Refused only when nothing fits.
    FS_InventoryResult addStack(FName itemId, int32 quantity, int64 spoilAtTime = 0);
    FS_InventoryResult addInstance(const FS_InventoryEntry& entry);
    // Ignore the limit and the rules. Only for moves that must never lose an item:
    // worn armor going into the detached bag, and a spoiled stack replacing a fresh one.
    void addStackForced(FName itemId, int32 quantity, int64 spoilAtTime);
    void addInstanceForced(const FS_InventoryEntry& entry);
    // R01 §5 RemoveStack: removes from the AVAILABLE quantity only.
    FS_InventoryResult removeStack(FName itemId, int32 quantity);
    // R01 §5 RemoveInstance. Refused while the instance is reserved.
    FS_InventoryResult removeInstance(const FGuid& instanceId);
    // Durability, the loaded flag, loaded rounds. False when the instance is not here.
    bool setInstanceState(const FGuid& instanceId, const FS_ItemInstanceState& newState);
    void setDeathBag(bool newValue);
    // Replaces everything: a restore from a save, or a client copy being refreshed.
    void replaceEntries(const TArray<FS_InventoryEntry>& newEntries);
    void clearAll();

    // R01 3.2a: turns every stack that has reached its spoil time into its spoiled item. Section 8 fills this in.
    void evaluateSpoilage(int64 now);

    // C++ only. Raised after every change, on the machine that holds this object.
    FD_InventoryDataChanged onChanged;

protected:
    const UDA_ItemDefinition* findDefinition(FName itemId) const;
    FS_InventoryEntry* findStackMutable(FName itemId);
    FS_InventoryEntry* findInstanceMutable(const FGuid& instanceId);
    void notifyChanged();
    static FS_InventoryResult refusal(const FGameplayTag& reason);

    UPROPERTY() FGuid inventoryId;
    UPROPERTY() E_InventoryType inventoryType = E_InventoryType::Personal;
    UPROPERTY() FS_ContainerRules rules;
    UPROPERTY() bool deathBag = false;
    // Screen order and position come from data (R01 3.1a), never from the order of this array.
    UPROPERTY() TArray<FS_InventoryEntry> entries;
};
```

`UInventoryData.cpp`

```cpp
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "GameFramework/GameplayMessageSubsystem.h"

// ---------- setup ----------

UWorld* UInventoryData::GetWorld() const{
    if(HasAnyFlags(RF_ClassDefaultObject) || GetOuter() == nullptr){
        return nullptr;
    }
    return GetOuter()->GetWorld();
}

void UInventoryData::setup(const FGuid& newInventoryId, E_InventoryType newType, const FS_ContainerRules& newRules){
    inventoryId = newInventoryId;
    inventoryType = newType;
    rules = newRules;
}

// ---------- queries ----------

const FGuid& UInventoryData::getInventoryId() const{
    return inventoryId;
}

E_InventoryType UInventoryData::getInventoryType() const{
    return inventoryType;
}

bool UInventoryData::isDeathBag() const{
    return deathBag;
}

bool UInventoryData::isEmpty() const{
    return entries.Num() == 0;
}

const TArray<FS_InventoryEntry>& UInventoryData::getEntries() const{
    return entries;
}

const FS_InventoryEntry* UInventoryData::findStack(FName itemId) const{
    return entries.FindByPredicate([itemId](const FS_InventoryEntry& entry){
        return !entry.isInstanced() && entry.itemId == itemId;
    });
}

const FS_InventoryEntry* UInventoryData::findInstance(const FGuid& instanceId) const{
    if(!instanceId.IsValid()){
        return nullptr;
    }
    return entries.FindByPredicate([&instanceId](const FS_InventoryEntry& entry){
        return entry.instanceId == instanceId;
    });
}

int32 UInventoryData::getTotalQuantity(FName itemId) const{
    const FS_InventoryEntry* stack = findStack(itemId);
    return stack != nullptr ? stack->totalQuantity : getInstanceCount(itemId);
}

int32 UInventoryData::getInstanceCount(FName itemId) const{
    int32 count = 0;
    for(const FS_InventoryEntry& entry : entries){
        if(entry.isInstanced() && entry.itemId == itemId){
            count++;
        }
    }
    return count;
}

int32 UInventoryData::getAvailableQuantity(FName itemId) const{
    const FS_InventoryEntry* stack = findStack(itemId);
    if(stack != nullptr){
        return stack->getAvailableQuantity();
    }
    int32 count = 0;
    for(const FS_InventoryEntry& entry : entries){
        if(entry.isInstanced() && entry.itemId == itemId && entry.reservedQuantity == 0){
            count++;
        }
    }
    return count;
}

int32 UInventoryData::getLimit(const UDA_ItemDefinition& definition) const{
    if(inventoryType == E_InventoryType::Personal){
        return definition.perTypeLimit;   // the bag limits each item type (designer-confirmed)
    }
    const int32* itemLimit = rules.perItemLimits.Find(definition.itemId);
    if(itemLimit != nullptr){
        return *itemLimit;
    }
    return rules.defaultPerTypeLimit > 0 ? rules.defaultPerTypeLimit : definition.perTypeLimit;
}

FGameplayTag UInventoryData::checkAccepts(const UDA_ItemDefinition& definition) const{
    if(inventoryType == E_InventoryType::Personal){
        // The bag accepts every item type, except that a heavy item never enters it (R01 3.7 step 0).
        return definition.isHeavy() ? T_Inventory_FailReason_ItemNotAllowed.GetTag() : FGameplayTag();
    }
    if(!rules.allowedTags.IsEmpty() && !definition.categoryTag.MatchesAny(rules.allowedTags) && !definition.itemTags.HasAny(rules.allowedTags)){
        return T_Inventory_FailReason_ItemNotAllowed;
    }
    if(definition.carriesTag(rules.singleItemTag)){
        // Only one item id of that group at a time: a container holds clean OR unclean water (R01 3.4).
        for(const FS_InventoryEntry& entry : entries){
            const UDA_ItemDefinition* other = entry.itemId != definition.itemId ? findDefinition(entry.itemId) : nullptr;
            if(other != nullptr && other->carriesTag(rules.singleItemTag)){
                return T_Inventory_FailReason_ItemNotAllowed;
            }
        }
    }
    return FGameplayTag();
}

int32 UInventoryData::canFit(const FS_ItemStack& stack) const{
    const UDA_ItemDefinition* definition = findDefinition(stack.itemId);
    if(definition == nullptr || stack.quantity < 1 || checkAccepts(*definition).IsValid()){
        return 0;
    }
    const int32 room = getLimit(*definition) - getTotalQuantity(stack.itemId);
    return FMath::Clamp(room, 0, stack.quantity);
}

FGameplayTag UInventoryData::getFullReason() const{
    return inventoryType == E_InventoryType::Personal ? T_Interaction_FailReason_BagFull.GetTag() : T_Inventory_FailReason_ContainerFull.GetTag();
}

// ---------- commands ----------

FS_InventoryResult UInventoryData::addStack(FName itemId, int32 quantity, int64 spoilAtTime){
    const UDA_ItemDefinition* definition = findDefinition(itemId);
    if(definition == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(quantity < 1 || !definition->isStackable()){
        return refusal(T_Inventory_FailReason_InvalidRequest);
    }
    const FGameplayTag blocked = checkAccepts(*definition);
    if(blocked.IsValid()){
        return refusal(blocked);
    }
    FS_ItemStack wanted;
    wanted.itemId = itemId;
    wanted.quantity = quantity;
    const int32 fits = canFit(wanted);
    if(fits < 1){
        return refusal(getFullReason());
    }
    addStackForced(itemId, fits, definition->canSpoil ? spoilAtTime : 0);
    FS_InventoryResult result;
    result.quantity = fits;
    return result;
}

FS_InventoryResult UInventoryData::addInstance(const FS_InventoryEntry& entry){
    const UDA_ItemDefinition* definition = findDefinition(entry.itemId);
    if(definition == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(!entry.isInstanced() || !definition->isInstanced() || findInstance(entry.instanceId) != nullptr){
        return refusal(T_Inventory_FailReason_InvalidRequest);
    }
    const FGameplayTag blocked = checkAccepts(*definition);
    if(blocked.IsValid()){
        return refusal(blocked);
    }
    FS_ItemStack single;
    single.itemId = entry.itemId;
    single.quantity = 1;
    if(canFit(single) < 1){
        return refusal(getFullReason());
    }
    addInstanceForced(entry);
    FS_InventoryResult result;
    result.quantity = 1;
    result.instanceId = entry.instanceId;
    return result;
}

void UInventoryData::addStackForced(FName itemId, int32 quantity, int64 spoilAtTime){
    if(quantity < 1){
        return;
    }
    FS_InventoryEntry* stack = findStackMutable(itemId);
    if(stack == nullptr){
        FS_InventoryEntry& added = entries.AddDefaulted_GetRef();
        added.itemId = itemId;
        added.totalQuantity = quantity;
        added.spoilAtTime = spoilAtTime;
    }else{
        stack->totalQuantity += quantity;
        // Merging keeps the earliest time, so a mixed stack is as old as its oldest unit (R01 3.2a).
        if(spoilAtTime > 0 && (stack->spoilAtTime == 0 || spoilAtTime < stack->spoilAtTime)){
            stack->spoilAtTime = spoilAtTime;
        }
    }
    notifyChanged();
}

void UInventoryData::addInstanceForced(const FS_InventoryEntry& entry){
    FS_InventoryEntry& added = entries.Add_GetRef(entry);
    added.totalQuantity = 1;
    added.reservedQuantity = 0;
    added.spoilAtTime = 0;
    notifyChanged();
}

FS_InventoryResult UInventoryData::removeStack(FName itemId, int32 quantity){
    FS_InventoryEntry* stack = findStackMutable(itemId);
    if(stack == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(quantity < 1){
        return refusal(T_Inventory_FailReason_InvalidRequest);
    }
    if(stack->getAvailableQuantity() < quantity){
        // Enough is held but part of it is staged, or there simply is not enough.
        return refusal(stack->totalQuantity >= quantity ? T_Inventory_FailReason_ReservedQuantity : T_Inventory_FailReason_InsufficientQuantity);
    }
    stack->totalQuantity -= quantity;
    if(stack->totalQuantity == 0){
        entries.RemoveAll([itemId](const FS_InventoryEntry& entry){
            return !entry.isInstanced() && entry.itemId == itemId;
        });
    }
    notifyChanged();
    FS_InventoryResult result;
    result.quantity = quantity;
    return result;
}

FS_InventoryResult UInventoryData::removeInstance(const FGuid& instanceId){
    const FS_InventoryEntry* entry = findInstance(instanceId);
    if(entry == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(entry->reservedQuantity > 0){
        return refusal(T_Inventory_FailReason_ReservedQuantity);
    }
    entries.RemoveAll([&instanceId](const FS_InventoryEntry& candidate){
        return candidate.instanceId == instanceId;
    });
    notifyChanged();
    FS_InventoryResult result;
    result.quantity = 1;
    result.instanceId = instanceId;
    return result;
}

bool UInventoryData::setInstanceState(const FGuid& instanceId, const FS_ItemInstanceState& newState){
    FS_InventoryEntry* entry = findInstanceMutable(instanceId);
    if(entry == nullptr){
        return false;
    }
    if(!(entry->state == newState)){
        entry->state = newState;
        notifyChanged();
    }
    return true;
}

void UInventoryData::setDeathBag(bool newValue){
    deathBag = newValue;
}

void UInventoryData::replaceEntries(const TArray<FS_InventoryEntry>& newEntries){
    entries = newEntries;
    notifyChanged();
}

void UInventoryData::clearAll(){
    entries.Reset();
    notifyChanged();
}

void UInventoryData::evaluateSpoilage(int64 now){
    // Filled in Section 8.
}

// ---------- helpers ----------

const UDA_ItemDefinition* UInventoryData::findDefinition(FName itemId) const{
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    return catalog != nullptr ? catalog->getDefinition(itemId) : nullptr;
}

FS_InventoryEntry* UInventoryData::findStackMutable(FName itemId){
    return entries.FindByPredicate([itemId](const FS_InventoryEntry& entry){
        return !entry.isInstanced() && entry.itemId == itemId;
    });
}

FS_InventoryEntry* UInventoryData::findInstanceMutable(const FGuid& instanceId){
    if(!instanceId.IsValid()){
        return nullptr;
    }
    return entries.FindByPredicate([&instanceId](const FS_InventoryEntry& entry){
        return entry.instanceId == instanceId;
    });
}

void UInventoryData::notifyChanged(){
    onChanged.Broadcast(this);
    // Inventory.Changed is local on every machine (R01 §6). The host raises it here; a client raises it
    // from the same function when its copy is refreshed from the replicated mirror.
    if(UGameplayMessageSubsystem::HasInstance(this)){
        FS_InventoryChangedMessage message;
        message.inventoryId = inventoryId;
        UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Inventory_Changed, message);
    }
}

FS_InventoryResult UInventoryData::refusal(const FGameplayTag& reason){
    FS_InventoryResult result;
    result.failReason = reason;
    return result;
}
```

> **Why the object needs `GetWorld`.** The object is not an actor, so it finds the item catalog and the message subsystem through whatever owns it (the registry on the server, a component on a client). Section 4 always creates it with such an owner.

### Verify (Section 3)

Compile only. The object cannot be created sensibly until the registry exists, so these checks are written as the first automated spec at the end of Section 4 (S4.9), once the test fixture is there. They are listed here so the table sits next to the code it tests:

| Test | Expect |
| --- | --- |
| `addStack(Stick, 5)` into an empty bag (limit 20) | accepted, quantity 5; one entry |
| `addStack(Stick, 30)` | accepted with quantity **15**: it stores what the limit allows; total 20 |
| `addStack(Stick, 1)` again | refused `Interaction.FailReason.BagFull`; nothing changes |
| `addStack(Spear, 1)` (an instanced item) | refused `InvalidRequest` |
| `addStack(NoSuchItem, 1)` | refused `NotFound` |
| `addInstance` of four spears (limit 4), then a fifth | the fifth is refused `BagFull`; `getInstanceCount(Spear)` is 4 |
| `addInstance` twice with the same instance id | the second is refused `InvalidRequest` |
| `removeStack(Stick, 25)` with 20 held | refused `InsufficientQuantity`; still 20 |
| `removeStack(Stick, 20)` | accepted; the entry is gone |
| `canFit({Stick, 50})` on an empty bag | 20 |
| A container with `allowedTags = Item.Category.Food`: `addStack(Stick, 1)` | refused `ItemNotAllowed` |
| A container with `singleItemTag = Item.Category.Water` holding `CleanWater`: `addStack(UncleanWater, 1)` | refused `ItemNotAllowed`; after the clean water is removed it is accepted |
| Add 3 `RawMeat` with spoil time 1000, then 2 with spoil time 400 | one stack of 5 with spoil time **400** |
| `onChanged` | raised once per accepted command, never for a refusal |

### Handoff

`UInventoryData` is the only place where entries change. Nothing outside this framework should call its commands directly: other frameworks use the library functions of Sections 5 to 9.

---

# SECTION 4 — The Registry, the Bag on the PlayerState, and Containers

**Goal:** every inventory has an id and a home; each player has a bag that the owner (and only the owner) sees; a chest in the world has an inventory that outlives its actor.
**Spec:** R01 §2, 3.3, 3.4, 3.4a, §7; Doc02 §8, §18.1, §19.3; Doc B19 3.2; decision D5.
**Depends on:** Section 3.

### Files

```
Public/ItemInventoryFramework/
    UInventoryRegistrySubsystem.h   UPlayerInventoryComponent.h   UContainerComponent.h
Private/ItemInventoryFramework/
    UInventoryRegistrySubsystem.cpp UPlayerInventoryComponent.cpp UContainerComponent.cpp
    Stubs/AInventoryTestPlayerState.h/.cpp
    Stubs/AInventoryTestGameMode.h/.cpp
    Stubs/AInventoryTestChest.h/.cpp
    Tests/InventoryTestFixture.h/.cpp
    Tests/InventoryDataSpec.cpp
```

### Design rules

- **State is data, actors are the loaded view** (Doc02 §18.1). The registry keeps every inventory by id. A chest actor that streams out leaves its inventory in the registry, so a rope can still deliver to it (Doc B09).
- **The bag belongs to the PlayerState** (decision D5, Doc B19 3.2), so it survives the death of the pawn.
- **The owner sees the bag, nobody else does** (R01 §7): the mirror replicates with `COND_OwnerOnly`.
- **Stable ids** (Doc02 §19.3): a bag and a player-built container get a GUID when created (kind 2); a designer-placed container carries a GUID set in the editor (kind 1).

### S4.1 — `UInventoryRegistrySubsystem`

`UInventoryRegistrySubsystem.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UInventoryRegistrySubsystem.generated.h"

class UInventoryData;
class UContainerComponent;

// Every inventory of the world, by id. The server's copy is the truth; a client's registry stays empty.
UCLASS()
class AWAKEN_API UInventoryRegistrySubsystem : public UWorldSubsystem{
    GENERATED_BODY()

public:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

    static UInventoryRegistrySubsystem* get(const UObject* worldContext);

    // ---- queries ----
    UInventoryData* findInventory(const FGuid& inventoryId) const;
    // The loaded actor component of a world container, or null while its area is not loaded.
    UContainerComponent* findHost(const FGuid& inventoryId) const;
    const TMap<FGuid, TObjectPtr<UInventoryData>>& getAllInventories() const;
    // The contents of a Portable Container item (R01 3.4). Null when the entry is not a container.
    UInventoryData* getContainedInventory(const FS_InventoryEntry& containerEntry, bool createIfMissing);
    // True for an item that is not a container, and for a container that holds nothing (R01 3.4a).
    bool isContainerItemEmpty(const FS_InventoryEntry& entry) const;

    // ---- commands (server) ----
    UInventoryData* createInventory(E_InventoryType type, const FGuid& inventoryId, const FS_ContainerRules& rules);
    UInventoryData* findOrCreateInventory(E_InventoryType type, const FGuid& inventoryId, const FS_ContainerRules& rules);
    // Removes the inventory and the contents of every Portable Container inside it.
    void destroyInventory(const FGuid& inventoryId);
    void registerHost(const FGuid& inventoryId, UContainerComponent* host);
    void unregisterHost(const FGuid& inventoryId);

private:
    UPROPERTY() TMap<FGuid, TObjectPtr<UInventoryData>> inventories;
    TMap<FGuid, TWeakObjectPtr<UContainerComponent>> hosts;
};
```

`UInventoryRegistrySubsystem.cpp`

```cpp
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UContainerComponent.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

bool UInventoryRegistrySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

UInventoryRegistrySubsystem* UInventoryRegistrySubsystem::get(const UObject* worldContext){
    const UWorld* world = (GEngine != nullptr && worldContext != nullptr) ? GEngine->GetWorldFromContextObject(worldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    return world != nullptr ? world->GetSubsystem<UInventoryRegistrySubsystem>() : nullptr;
}

UInventoryData* UInventoryRegistrySubsystem::findInventory(const FGuid& inventoryId) const{
    const TObjectPtr<UInventoryData>* found = inventories.Find(inventoryId);
    return found != nullptr ? found->Get() : nullptr;
}

UContainerComponent* UInventoryRegistrySubsystem::findHost(const FGuid& inventoryId) const{
    const TWeakObjectPtr<UContainerComponent>* found = hosts.Find(inventoryId);
    return found != nullptr ? found->Get() : nullptr;
}

const TMap<FGuid, TObjectPtr<UInventoryData>>& UInventoryRegistrySubsystem::getAllInventories() const{
    return inventories;
}

UInventoryData* UInventoryRegistrySubsystem::createInventory(E_InventoryType type, const FGuid& inventoryId, const FS_ContainerRules& rules){
    if(!inventoryId.IsValid() || inventories.Contains(inventoryId)){
        INVENTORY_LOG(Error, this, "createInventory refused: the id %s is empty or already used", *inventoryId.ToString());
        return nullptr;
    }
    UInventoryData* inventory = NewObject<UInventoryData>(this);
    inventory->setup(inventoryId, type, rules);
    inventories.Add(inventoryId, inventory);
    return inventory;
}

UInventoryData* UInventoryRegistrySubsystem::findOrCreateInventory(E_InventoryType type, const FGuid& inventoryId, const FS_ContainerRules& rules){
    UInventoryData* existing = findInventory(inventoryId);
    return existing != nullptr ? existing : createInventory(type, inventoryId, rules);
}

void UInventoryRegistrySubsystem::destroyInventory(const FGuid& inventoryId){
    const UInventoryData* inventory = findInventory(inventoryId);
    if(inventory == nullptr){
        return;
    }
    // A Portable Container cannot hold another one (R01 3.4a), so one level is enough.
    for(const FS_InventoryEntry& entry : inventory->getEntries()){
        if(entry.state.containedInventoryId.IsValid()){
            inventories.Remove(entry.state.containedInventoryId);
        }
    }
    inventories.Remove(inventoryId);
    hosts.Remove(inventoryId);
}

void UInventoryRegistrySubsystem::registerHost(const FGuid& inventoryId, UContainerComponent* host){
    hosts.Add(inventoryId, host);
}

void UInventoryRegistrySubsystem::unregisterHost(const FGuid& inventoryId){
    hosts.Remove(inventoryId);
}

UInventoryData* UInventoryRegistrySubsystem::getContainedInventory(const FS_InventoryEntry& containerEntry, bool createIfMissing){
    const FGuid& containedId = containerEntry.state.containedInventoryId;
    if(!containedId.IsValid()){
        return nullptr;
    }
    UInventoryData* existing = findInventory(containedId);
    if(existing != nullptr || !createIfMissing){
        return existing;
    }
    // Created the first time something is put in or the container is opened, with the rules of its item definition.
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(containerEntry.itemId) : nullptr;
    return createInventory(E_InventoryType::PortableContainer, containedId, definition != nullptr ? definition->capacityRules : FS_ContainerRules());
}

bool UInventoryRegistrySubsystem::isContainerItemEmpty(const FS_InventoryEntry& entry) const{
    const UInventoryData* contained = findInventory(entry.state.containedInventoryId);
    return contained == nullptr || contained->isEmpty();
}
```

> **The contents move with the item for free.** A Portable Container's contents are found through the id in the item's own state. Wherever the item goes (another inventory, a save), the id goes with it and the contents are still one lookup away (R01 3.4).

### S4.2 to S4.4 — `UPlayerInventoryComponent`

`UPlayerInventoryComponent.h` (Sections 5, 6 and 9 add members as marked there)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ItemInventoryFramework/04Delegates/D_ItemInventoryFramework.h"
#include "UPlayerInventoryComponent.generated.h"

class APlayerState;
class UInventoryData;

// The player's link to their bag (R01 §2, decision D5). A PlayerState component, so it survives the pawn.
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class AWAKEN_API UPlayerInventoryComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UPlayerInventoryComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Query. Null when the component is missing.
    static UPlayerInventoryComponent* findFor(const APlayerState* player);

    // ---- queries ----
    // R01 §5 HasBag: true while the Personal Inventory exists. On a machine that is neither the host nor the owner it answers false.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") bool hasBag() const;
    // The bag. Null while Bagless. On the host: the real data. On the owning client: a read-only copy.
    UInventoryData* getPersonalInventory() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") TArray<FS_InventoryEntry> getBagEntries() const;
    // Held minus reserved: the number every reader shows (R01 3.6).
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") int32 getAvailableQuantity(FName itemId) const;
    // The character that owns this player's bags (Index A93). Doc R13 owns the real id (I23).
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FGuid getCharacterId() const;

    // ---- commands (server) ----
    // A new, empty bag. Does nothing when the player already has one.
    void createFreshBag();
    // Makes this inventory the player's bag.
    void attachInventory(UInventoryData* inventory);
    // Takes the bag away and returns it. The player is Bagless afterwards. The inventory stays in the registry.
    UInventoryData* releaseInventory();
    void setCharacterId(const FGuid& newCharacterId);

    // Raised on the host and on the owning client after any change to the bag, the hotbar or the hold slots.
    UPROPERTY(BlueprintAssignable, Category = "Awaken|Inventory") FD_PlayerInventoryChanged onInventoryChanged;

protected:
    bool isServerSide() const;
    APlayerState* getOwnerPlayerState() const;
    void notifyChanged();
    // Host: the real bag changed.
    void handleBagChanged(UInventoryData* inventory);
    // Owning client: a batch of mirror changes arrived.
    void handleMirrorReceived();

    UFUNCTION()
    void OnRep_bagAttached();

    // All four replicate to the owner only (R01 §7).
    UPROPERTY(ReplicatedUsing = OnRep_bagAttached) bool bagAttached = false;
    UPROPERTY(Replicated) FGuid bagInventoryId;
    UPROPERTY(Replicated) FS_InventoryEntryList replicatedBag;
    UPROPERTY(Replicated) FGuid characterId;

    // Host: the bag in the registry. Clients: null.
    UPROPERTY() TObjectPtr<UInventoryData> personalInventory;
    // Owning client: the local read-only copy, rebuilt from the mirror.
    UPROPERTY() TObjectPtr<UInventoryData> clientBagView;
    FDelegateHandle bagChangedHandle;
};
```

`UPlayerInventoryComponent.cpp`

```cpp
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

UPlayerInventoryComponent::UPlayerInventoryComponent(){
    SetIsReplicatedByDefault(true);
}

void UPlayerInventoryComponent::BeginPlay(){
    Super::BeginPlay();
    if(!isServerSide()){
        clientBagView = NewObject<UInventoryData>(this);
        clientBagView->setup(bagInventoryId, E_InventoryType::Personal, FS_ContainerRules());
        replicatedBag.onReceived.BindUObject(this, &UPlayerInventoryComponent::handleMirrorReceived);
        handleMirrorReceived();   // the first data may have arrived before BeginPlay
    }
}

void UPlayerInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason){
    if(isServerSide() && personalInventory != nullptr){
        // The player is gone for good. Doc B20's 15-minute hold must take the bag FIRST with releaseInventory();
        // whatever is still attached here goes with the PlayerState.
        const FGuid inventoryId = personalInventory->getInventoryId();
        releaseInventory();
        UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
        if(registry != nullptr){
            registry->destroyInventory(inventoryId);
        }
    }
    Super::EndPlay(EndPlayReason);
}

void UPlayerInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UPlayerInventoryComponent, bagAttached, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UPlayerInventoryComponent, bagInventoryId, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UPlayerInventoryComponent, replicatedBag, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UPlayerInventoryComponent, characterId, COND_OwnerOnly);
}

UPlayerInventoryComponent* UPlayerInventoryComponent::findFor(const APlayerState* player){
    return player != nullptr ? player->FindComponentByClass<UPlayerInventoryComponent>() : nullptr;
}

bool UPlayerInventoryComponent::isServerSide() const{
    return GetOwner() != nullptr && GetOwner()->HasAuthority();
}

APlayerState* UPlayerInventoryComponent::getOwnerPlayerState() const{
    return Cast<APlayerState>(GetOwner());
}

// ---------- queries ----------

bool UPlayerInventoryComponent::hasBag() const{
    return bagAttached;
}

UInventoryData* UPlayerInventoryComponent::getPersonalInventory() const{
    if(!bagAttached){
        return nullptr;
    }
    return isServerSide() ? personalInventory.Get() : clientBagView.Get();
}

TArray<FS_InventoryEntry> UPlayerInventoryComponent::getBagEntries() const{
    const UInventoryData* bag = getPersonalInventory();
    return bag != nullptr ? bag->getEntries() : TArray<FS_InventoryEntry>();
}

int32 UPlayerInventoryComponent::getAvailableQuantity(FName itemId) const{
    const UInventoryData* bag = getPersonalInventory();
    return bag != nullptr ? bag->getAvailableQuantity(itemId) : 0;
}

FGuid UPlayerInventoryComponent::getCharacterId() const{
    return characterId;
}

// ---------- commands ----------

void UPlayerInventoryComponent::createFreshBag(){
    UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    if(!isServerSide() || registry == nullptr || personalInventory != nullptr){
        return;
    }
    UInventoryData* inventory = registry->createInventory(E_InventoryType::Personal, FGuid::NewGuid(), FS_ContainerRules());
    attachInventory(inventory);
    INVENTORY_LOG(Log, this, "Player %s received a new empty bag %s", *GetNameSafe(GetOwner()), *bagInventoryId.ToString());
}

void UPlayerInventoryComponent::attachInventory(UInventoryData* inventory){
    if(!isServerSide() || inventory == nullptr || personalInventory != nullptr){
        return;
    }
    personalInventory = inventory;
    bagInventoryId = inventory->getInventoryId();
    bagAttached = true;
    bagChangedHandle = inventory->onChanged.AddUObject(this, &UPlayerInventoryComponent::handleBagChanged);
    handleBagChanged(inventory);
}

UInventoryData* UPlayerInventoryComponent::releaseInventory(){
    if(!isServerSide() || personalInventory == nullptr){
        return nullptr;
    }
    UInventoryData* released = personalInventory;
    released->onChanged.Remove(bagChangedHandle);
    bagChangedHandle.Reset();
    personalInventory = nullptr;
    bagInventoryId.Invalidate();
    bagAttached = false;
    replicatedBag.syncFrom(TArray<FS_InventoryEntry>());
    GetOwner()->ForceNetUpdate();
    notifyChanged();
    return released;
}

void UPlayerInventoryComponent::setCharacterId(const FGuid& newCharacterId){
    if(isServerSide()){
        characterId = newCharacterId;
    }
}

// ---------- change handling ----------

void UPlayerInventoryComponent::notifyChanged(){
    onInventoryChanged.Broadcast(getOwnerPlayerState());
}

void UPlayerInventoryComponent::handleBagChanged(UInventoryData* inventory){
    replicatedBag.syncFrom(inventory->getEntries());
    GetOwner()->ForceNetUpdate();   // a PlayerState replicates slowly by default; the bag should not lag a second behind
    notifyChanged();
}

void UPlayerInventoryComponent::handleMirrorReceived(){
    if(clientBagView == nullptr){
        return;
    }
    clientBagView->setup(bagInventoryId, E_InventoryType::Personal, FS_ContainerRules());
    clientBagView->replaceEntries(replicatedBag.toEntries());   // raises Inventory.Changed on this machine
    notifyChanged();
}

void UPlayerInventoryComponent::OnRep_bagAttached(){
    notifyChanged();
}
```

> **What a late joiner gets.** Nothing special is needed: the mirror is ordinary replicated state, so a player who joins late receives the whole bag from it, never from a message (Doc02 §19.1). The explicit `handleMirrorReceived()` in `BeginPlay` covers the case where the first data arrived before the component began play.

### S4.5 — `UContainerComponent`

Any actor becomes a container by carrying this component: a chest, a crate, a water barrel, a station's output box.

`UContainerComponent.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UContainerComponent.generated.h"

class UInventoryData;

// R01 3.4: a world-fixed Container. The actor is only the loaded view; the contents live in the registry.
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class AWAKEN_API UContainerComponent : public UActorComponent{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FGuid getContainerId() const;
    // Host only. Null on a client: a client sees a container through its view component (Section 12).
    UInventoryData* getInventory() const;

    // Designer-placed: set in the editor and never changed (Doc02 19.3, kind 1).
    // Player-built: leave empty, it is created when the container is built (kind 2).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container") FGuid containerId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container") FS_ContainerRules rules;
    // Some containers have no screen and give a quick take or store from a stack (R01 3.4). Read by the UI.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Container") bool hasScreen = true;
};
```

`UContainerComponent.cpp`

```cpp
#include "ItemInventoryFramework/UContainerComponent.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/InventoryLog.h"

void UContainerComponent::BeginPlay(){
    Super::BeginPlay();
    if(GetOwner() == nullptr || !GetOwner()->HasAuthority()){
        return;
    }
    UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    if(registry == nullptr){
        return;
    }
    if(!containerId.IsValid()){
        containerId = FGuid::NewGuid();
        INVENTORY_LOG(Log, this, "Container %s had no id and received %s. A designer-placed container must have its id set in the editor, or it will not find its contents after a load", *GetNameSafe(GetOwner()), *containerId.ToString());
    }
    // Found again when the area streams back in: the data never left (Doc02 18.1).
    UInventoryData* inventory = registry->findOrCreateInventory(E_InventoryType::Container, containerId, rules);
    if(inventory != nullptr){
        inventory->setup(containerId, E_InventoryType::Container, rules);   // the rules always come from the loaded actor
    }
    registry->registerHost(containerId, this);
}

void UContainerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason){
    UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    if(registry != nullptr && GetOwner() != nullptr && GetOwner()->HasAuthority()){
        registry->unregisterHost(containerId);   // the inventory stays in the registry on purpose
    }
    Super::EndPlay(EndPlayReason);
}

FGuid UContainerComponent::getContainerId() const{
    return containerId;
}

UInventoryData* UContainerComponent::getInventory() const{
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    return registry != nullptr ? registry->findInventory(containerId) : nullptr;
}
```

> What happens to the contents when a container is **destroyed** (demolished, burnt) is not in R01: it belongs to whoever destroys it (Doc B01). The safe default of Doc02 §6 applies: drop the contents as loose items, then call `registry->destroyInventory(containerId)`. It is listed in Appendix C.

### S4.6 to S4.8 — Stub actors (owner: Doc B19)

Three small classes so you can press Play before the real game framework classes exist.

`Private/ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h`

```cpp
// TEMPORARY STUB — owner: Doc B19 Game Framework Class Table. The real PlayerState carries the same components.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AInventoryTestPlayerState.generated.h"

class UPlayerInventoryComponent;

UCLASS()
class AInventoryTestPlayerState : public APlayerState{
    GENERATED_BODY()

public:
    AInventoryTestPlayerState();

    UPROPERTY(VisibleAnywhere) TObjectPtr<UPlayerInventoryComponent> inventoryComponent;
};
```

`Private/ItemInventoryFramework/Stubs/AInventoryTestPlayerState.cpp`

```cpp
// TEMPORARY STUB — owner: Doc B19.
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"

AInventoryTestPlayerState::AInventoryTestPlayerState(){
    inventoryComponent = CreateDefaultSubobject<UPlayerInventoryComponent>(TEXT("Inventory"));
}
```

`Private/ItemInventoryFramework/Stubs/AInventoryTestGameMode.h`

```cpp
// TEMPORARY STUB — owners: Doc B19 (classes) and Docs B05 / B20 (who creates a character and grants the starting kit).
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AInventoryTestGameMode.generated.h"

UCLASS()
class AInventoryTestGameMode : public AGameModeBase{
    GENERATED_BODY()

public:
    AInventoryTestGameMode();

    virtual void PostLogin(APlayerController* NewPlayer) override;
};
```

`Private/ItemInventoryFramework/Stubs/AInventoryTestGameMode.cpp`

```cpp
// TEMPORARY STUB — owners: Docs B19, B05, B20.
#include "ItemInventoryFramework/Stubs/AInventoryTestGameMode.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "GameFramework/PlayerController.h"

AInventoryTestGameMode::AInventoryTestGameMode(){
    PlayerStateClass = AInventoryTestPlayerState::StaticClass();
}

void AInventoryTestGameMode::PostLogin(APlayerController* NewPlayer){
    Super::PostLogin(NewPlayer);
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(NewPlayer->PlayerState);
    if(inventory != nullptr){
        inventory->setCharacterId(FGuid::NewGuid());
        inventory->createFreshBag();   // Section 9 replaces these two lines with startNewCharacter
    }
}
```

`Private/ItemInventoryFramework/Stubs/AInventoryTestChest.h`

```cpp
// TEMPORARY STUB — a chest to test with. Real containers are content: any actor with a UContainerComponent.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AInventoryTestChest.generated.h"

class UContainerComponent;
class UStaticMeshComponent;

UCLASS()
class AInventoryTestChest : public AActor{
    GENERATED_BODY()

public:
    AInventoryTestChest();

    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> mesh;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UContainerComponent> container;
};
```

`Private/ItemInventoryFramework/Stubs/AInventoryTestChest.cpp`

```cpp
// TEMPORARY STUB.
#include "ItemInventoryFramework/Stubs/AInventoryTestChest.h"
#include "ItemInventoryFramework/UContainerComponent.h"
#include "Components/StaticMeshComponent.h"

AInventoryTestChest::AInventoryTestChest(){
    bReplicates = true;
    mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(mesh);
    container = CreateDefaultSubobject<UContainerComponent>(TEXT("Container"));
}
```

### S4.9 — The test fixture and the first spec

The fixture builds a small private game world for one test: a game instance (so the catalog and the services exist), an empty world (so the registry exists and actors can be spawned), and a catalog that holds only the items the test adds. Every automated test of this guide and of the R05 guide starts with `FInventoryTestFixture fixture;`.

`Private/ItemInventoryFramework/Tests/InventoryTestFixture.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ItemInventoryFramework/InventoryServices.h"

class AInventoryTestPlayerState;
class UDA_ItemDefinition;
class UGameInstance;
class UInventoryData;
class UInventoryRegistrySubsystem;
class UItemCatalogSubsystem;
class UWorld;

// A private game world for one automated test. Everything it creates is destroyed with it.
struct FInventoryTestFixture{
    FInventoryTestFixture();
    ~FInventoryTestFixture();

    UWorld* world() const;
    UInventoryServices* services() const;
    UItemCatalogSubsystem* catalog() const;
    UInventoryRegistrySubsystem* registry() const;

    // The stub that stands in for a framework, so a test can read what was asked of it.
    template<typename StubClass, typename InterfaceClass>
    StubClass* stub() const{
        return Cast<StubClass>(services()->getSeam(InterfaceClass::StaticClass()));
    }

    // Item definitions that exist only in this fixture's catalog.
    UDA_ItemDefinition* addStackable(FName itemId, int32 limit);
    UDA_ItemDefinition* addInstanced(FName itemId, int32 limit, float durability);

    // A player with a character id and an empty bag.
    AInventoryTestPlayerState* makePlayer();
    // A world container without an actor: the data only.
    UInventoryData* makeContainer(const FS_ContainerRules& rules);

    // The stub game clock.
    void setGameTime(int64 gameTime);
    void advanceHours(float hours);

private:
    UGameInstance* gameInstance = nullptr;
    UWorld* testWorld = nullptr;
};
```

`Private/ItemInventoryFramework/Tests/InventoryTestFixture.cpp`

```cpp
#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/Stubs/InventoryStubs.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/InventoryTime.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

FInventoryTestFixture::FInventoryTestFixture(){
    gameInstance = NewObject<UGameInstance>(GEngine);
    gameInstance->AddToRoot();
    gameInstance->InitializeStandalone();   // a world context, an empty game world, and the game-instance subsystems
    testWorld = gameInstance->GetWorld();
    testWorld->InitializeActorsForPlay(FURL());
    testWorld->GetWorldSettings()->NotifyBeginPlay();   // actors spawned from here on begin play at once

    catalog()->resetForTest();   // only the items this test adds
    // Tests never spawn real loose actors: the recording stub answers instead.
    services()->registerSeam(UBPI_LooseItemSpawner::StaticClass(), NewObject<UStubLooseItemSpawner>(services()));
}

FInventoryTestFixture::~FInventoryTestFixture(){
    if(gameInstance != nullptr){
        gameInstance->Shutdown();
    }
    if(testWorld != nullptr){
        GEngine->DestroyWorldContext(testWorld);
        testWorld->DestroyWorld(false);
    }
    if(gameInstance != nullptr){
        gameInstance->RemoveFromRoot();
    }
}

UWorld* FInventoryTestFixture::world() const{
    return testWorld;
}

UInventoryServices* FInventoryTestFixture::services() const{
    return gameInstance->GetSubsystem<UInventoryServices>();
}

UItemCatalogSubsystem* FInventoryTestFixture::catalog() const{
    return gameInstance->GetSubsystem<UItemCatalogSubsystem>();
}

UInventoryRegistrySubsystem* FInventoryTestFixture::registry() const{
    return testWorld->GetSubsystem<UInventoryRegistrySubsystem>();
}

UDA_ItemDefinition* FInventoryTestFixture::addStackable(FName itemId, int32 limit){
    UDA_ItemDefinition* definition = NewObject<UDA_ItemDefinition>(gameInstance);
    definition->itemId = itemId;
    definition->itemKind = E_ItemKind::Stackable;
    definition->categoryTag = T_Item_Category_Material;
    definition->perTypeLimit = limit;
    catalog()->registerDefinition(definition);
    return definition;
}

UDA_ItemDefinition* FInventoryTestFixture::addInstanced(FName itemId, int32 limit, float durability){
    UDA_ItemDefinition* definition = NewObject<UDA_ItemDefinition>(gameInstance);
    definition->itemId = itemId;
    definition->itemKind = E_ItemKind::Instanced;
    definition->categoryTag = T_Item_Category_Tool;
    definition->perTypeLimit = limit;
    definition->durabilityMax = durability;
    catalog()->registerDefinition(definition);
    return definition;
}

AInventoryTestPlayerState* FInventoryTestFixture::makePlayer(){
    AInventoryTestPlayerState* player = testWorld->SpawnActor<AInventoryTestPlayerState>();
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    inventory->setCharacterId(FGuid::NewGuid());
    inventory->createFreshBag();
    return player;
}

UInventoryData* FInventoryTestFixture::makeContainer(const FS_ContainerRules& rules){
    return registry()->createInventory(E_InventoryType::Container, FGuid::NewGuid(), rules);
}

void FInventoryTestFixture::setGameTime(int64 gameTime){
    stub<UStubGameClock, UBPI_GameClock>()->gameTime = gameTime;
}

void FInventoryTestFixture::advanceHours(float hours){
    stub<UStubGameClock, UBPI_GameClock>()->gameTime += InventoryTime::hoursToGameSeconds(hours);
}
```

> **If the fixture's constructor fails in your editor build** (`InitializeStandalone` is an engine function this guide could not try), paste the error: the fallback is to build the world by hand with `UWorld::CreateWorld` and `GEngine->CreateNewWorldContext`, which is a ten-line change inside this one file.

`Private/ItemInventoryFramework/Tests/InventoryDataSpec.cpp` holds the Verify table of Section 3. The pattern for every row:

```cpp
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"

BEGIN_DEFINE_SPEC(FInventoryDataSpec, "Awaken.Inventory.Data", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FInventoryDataSpec)

void FInventoryDataSpec::Define(){
    Describe("Per-type limit", [this](){
        It("stores what the limit allows, refuses when nothing fits, and never loses a unit", [this](){
            FInventoryTestFixture fixture;
            fixture.addStackable(TEXT("Stick"), 20);
            const UDA_ItemDefinition* spear = fixture.addInstanced(TEXT("Spear"), 4, 50.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            int32 changes = 0;
            bag->onChanged.AddLambda([&changes](UInventoryData* changed){
                changes++;
            });

            TestEqual(TEXT("5 sticks fit"), bag->addStack(TEXT("Stick"), 5).quantity, 5);
            TestEqual(TEXT("30 more: only 15 fit"), bag->addStack(TEXT("Stick"), 30).quantity, 15);
            TestTrue(TEXT("one more is refused with BagFull"), bag->addStack(TEXT("Stick"), 1).failReason == T_Interaction_FailReason_BagFull);
            TestEqual(TEXT("a refusal raises no change"), changes, 2);
            TestTrue(TEXT("an instanced item is not a stack"), bag->addStack(TEXT("Spear"), 1).failReason == T_Inventory_FailReason_InvalidRequest);

            for(int32 index = 0; index < 4; index++){
                FS_InventoryEntry copy;
                copy.itemId = spear->itemId;
                copy.instanceId = FGuid::NewGuid();
                copy.state.durability = spear->durabilityMax;
                TestTrue(TEXT("a spear fits"), bag->addInstance(copy).succeeded());
            }
            FS_InventoryEntry fifth;
            fifth.itemId = spear->itemId;
            fifth.instanceId = FGuid::NewGuid();
            TestTrue(TEXT("the fifth spear is refused"), bag->addInstance(fifth).failReason == T_Interaction_FailReason_BagFull);
            TestEqual(TEXT("four copies"), bag->getInstanceCount(TEXT("Spear")), 4);

            TestTrue(TEXT("cannot remove more than is held"), bag->removeStack(TEXT("Stick"), 25).failReason == T_Inventory_FailReason_InsufficientQuantity);
            TestTrue(TEXT("remove all"), bag->removeStack(TEXT("Stick"), 20).succeeded());
            TestTrue(TEXT("the empty stack is gone"), bag->findStack(TEXT("Stick")) == nullptr);
        });
    });
    // Add one It(...) per remaining row of the Section 3 table: allowedTags, singleItemTag, and the merged spoil time.
}

#endif
```

### Verify (Section 4)

1. **Compile.** Create a level `TestMap_Inventory` in `Content/Awaken/Tests/` with a floor, two PlayerStarts, and one `InventoryTestChest` (give its container an id: in the details panel, *Container Id* → the small arrow → *Generate*). Set the level's *GameMode Override* to `InventoryTestGameMode`.
2. **Play offline.** The log shows `Player ... received a new empty bag ...`.
3. **Automated:** *Tools → Session Frontend → Automation*, filter `Awaken.Inventory.Data`, run. Every row of the Section 3 table passes.
4. **Registry:** in a temporary `BeginPlay` snippet on the host (or wait for the dump command of Section 13): `UInventoryRegistrySubsystem::get(this)->getAllInventories().Num()` is 2 with one player and the chest.
5. **Two players** (*Play As Listen Server*, 2 players): the host's log shows two bags. On the client, `UPlayerInventoryComponent::findFor(myPlayerState)->hasBag()` is true and `getBagEntries()` is empty; for the *other* player's PlayerState it answers false, because the bag is owner-only.
6. **Streaming stand-in:** destroy the chest actor on the host and spawn a new one with the same `containerId`: `getInventory()` on the new one is the same object as before.

### Handoff

- `UPlayerInventoryComponent::findFor(playerState)` is how every framework reaches a player's bag; `UInventoryRegistrySubsystem::get(context)->findInventory(id)` reaches any inventory, also while its actor is not loaded (the request of Doc B09).
- **B19** owner: put `UPlayerInventoryComponent` on the real PlayerState class.
- **B20** owner: before a leaving player's PlayerState ends, take the bag for the 15-minute hold with `releaseInventory()`; anything still attached is destroyed with the PlayerState (see `EndPlay`).
- **B01 / B10** owners: a container or a station box is any actor with a `UContainerComponent`; a player-built one leaves `containerId` empty.

---
# SECTION 5 — Loose Items, Granting, Pickup, Drop and Transfer

**Goal:** the one function every framework calls to give a player an item; the general pickup path; moving items between inventories. An item is never destroyed by a full inventory.
**Spec:** R01 3.4a, 3.7, 3.10, 3.11, §5 (`GrantOrDrop`, `TransferEntry`, `CanFit`), §6; Doc02 §19.5, §19.7.
**Depends on:** Section 4.

### Files

```
Public/ItemInventoryFramework/
    ALooseItemActor.h   ULooseItemSpawner.h   UBFL_Inventory.h
Private/ItemInventoryFramework/
    ALooseItemActor.cpp ULooseItemSpawner.cpp UBFL_Inventory.cpp
```

### The rules, stated once

| Situation | What happens | Rule |
| --- | --- | --- |
| A grant (harvest yield, craft output, processing collect, refund) and the bag has room | stored | R01 3.7 step 1 |
| A grant and the type is at its limit | the part that fits is stored, **the rest drops** next to the player as a loose item | R01 3.7 step 2, Doc02 §19.5 |
| A grant to a Bagless player | everything drops | R01 3.7 |
| A grant of a heavy item (carry cost above 0) | into a free carry slot, otherwise it lands loose. It never enters the bag | R01 3.7 step 0 |
| A **pickup** of a loose item and the type is at its limit | **refused** with `Interaction.FailReason.BagFull`; the item stays where it lies; nothing is dropped, so there is no pickup-and-drop loop | R01 3.11 |
| A pickup and only part of the loose stack fits | the part that fits is taken, the rest stays | R01 3.11 |
| A pickup of a heavy item | to the hands only; `Interaction.FailReason.HandsFull` when they cannot take it | R01 3.11 |
| A transfer into a container that has no room | the part that fits moves, the rest stays in the source | R01 3.7 last paragraph |

The difference between the grant and the pickup is the point of 3.11: a grant has no place to wait, so the surplus must go somewhere; a loose item already has a place.

### S5.1 — `ALooseItemActor`

A loose item is one inventory entry lying in the world: a stack of sticks, or one spear with the durability it has left. It belongs to no inventory, is never saved, and is removed only by its own lifetime (Doc02 §19.7; there is no global cap).

`ALooseItemActor.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ALooseItemActor.generated.h"

class UStaticMeshComponent;

// Doc02 19.7: a loose physical object. The server simulates it; clients receive its position.
// When the shared base class of Doc02 19.7 exists, derive this class from it (I7).
UCLASS()
class AWAKEN_API ALooseItemActor : public AActor{
    GENERATED_BODY()

public:
    ALooseItemActor();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Command, server. Called right after spawning.
    void setEntry(const FS_InventoryEntry& newEntry);
    // Command, server. Part of the stack was taken. The actor destroys itself when nothing is left.
    void reduceQuantity(int32 takenQuantity);

    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FS_InventoryEntry getEntry() const;

protected:
    UFUNCTION()
    void OnRep_entry();

    void refreshMesh();

    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> mesh;
    // Replicated to everyone: anyone may see and pick up a loose item.
    UPROPERTY(ReplicatedUsing = OnRep_entry) FS_InventoryEntry entry;
};
```

`ALooseItemActor.cpp`

```cpp
#include "ItemInventoryFramework/ALooseItemActor.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"

ALooseItemActor::ALooseItemActor(){
    bReplicates = true;
    SetReplicatingMovement(true);
    mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(mesh);
    mesh->SetCollisionProfileName(TEXT("PhysicsActor"));
}

void ALooseItemActor::BeginPlay(){
    Super::BeginPlay();
    refreshMesh();
}

void ALooseItemActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ALooseItemActor, entry);
}

void ALooseItemActor::setEntry(const FS_InventoryEntry& newEntry){
    entry = newEntry;
    entry.reservedQuantity = 0;
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(entry.itemId) : nullptr;
    // Each kind has its own lifetime in data; 0 on the item means the project default (Doc02 19.7).
    const float ownLifetime = definition != nullptr ? definition->looseLifetimeSeconds : 0.0f;
    const float lifetime = ownLifetime > 0.0f ? ownLifetime : UInventorySettings::get()->defaultLooseLifetimeSeconds;
    if(lifetime > 0.0f){
        SetLifeSpan(lifetime);
    }
    refreshMesh();
}

void ALooseItemActor::reduceQuantity(int32 takenQuantity){
    entry.totalQuantity -= takenQuantity;
    if(entry.totalQuantity <= 0){
        Destroy();
    }
}

FS_InventoryEntry ALooseItemActor::getEntry() const{
    return entry;
}

void ALooseItemActor::OnRep_entry(){
    refreshMesh();
}

void ALooseItemActor::refreshMesh(){
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(entry.itemId) : nullptr;
    if(definition == nullptr || definition->worldMesh.IsNull()){
        return;
    }
    // The world mesh is a soft reference (Doc02 19.8): load it only now that something shows it.
    const TSoftObjectPtr<UStaticMesh> wanted = definition->worldMesh;
    UAssetManager::GetStreamableManager().RequestAsyncLoad(wanted.ToSoftObjectPath(), FStreamableDelegate::CreateWeakLambda(this, [this, wanted](){
        mesh->SetStaticMesh(wanted.Get());
        if(HasAuthority()){
            mesh->SetSimulatePhysics(true);   // the server simulates; clients receive the position; a body at rest goes to sleep
        }
    }));
}
```

> A Portable Container that holds something can never become a loose item (R01 3.4a) and neither can the Axe or the map (R01 3.10). Those refusals are in S5.5, before anything is spawned, so a lifetime can never remove contents or a permanent item.

### S5.2 — `ULooseItemSpawner`

`ULooseItemSpawner.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ItemInventoryFramework/03Interfaces/BPI_LooseItemSpawner.h"
#include "ULooseItemSpawner.generated.h"

// The real implementation of the loose-item seam: spawns an ALooseItemActor.
UCLASS()
class AWAKEN_API ULooseItemSpawner : public UObject, public IBPI_LooseItemSpawner{
    GENERATED_BODY()

public:
    virtual AActor* spawnLooseItem_Implementation(APlayerState* instigatorPlayer, const FS_InventoryEntry& entry, FVector location) override;
};
```

`ULooseItemSpawner.cpp`

```cpp
#include "ItemInventoryFramework/ULooseItemSpawner.h"
#include "ItemInventoryFramework/ALooseItemActor.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

AActor* ULooseItemSpawner::spawnLooseItem_Implementation(APlayerState* instigatorPlayer, const FS_InventoryEntry& entry, FVector location){
    UWorld* world = instigatorPlayer != nullptr ? instigatorPlayer->GetWorld() : GetWorld();
    if(world == nullptr || entry.itemId.IsNone() || entry.totalQuantity < 1){
        return nullptr;
    }
    FActorSpawnParameters parameters;
    parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    ALooseItemActor* looseItem = world->SpawnActor<ALooseItemActor>(location, FRotator::ZeroRotator, parameters);
    if(looseItem != nullptr){
        looseItem->setEntry(entry);
        INVENTORY_LOG(Log, world, "Loose item %s x%d at %s", *entry.itemId.ToString(), entry.totalQuantity, *location.ToCompactString());
    }
    return looseItem;
}
```

### S5.3 — Register the real spawner

In `UInventoryServices::Initialize`, replace the `UStubLooseItemSpawner` line with:

```cpp
    registerSeam(UBPI_LooseItemSpawner::StaticClass(), NewObject<ULooseItemSpawner>(this));        // this framework (I7)
```

and add `#include "ItemInventoryFramework/ULooseItemSpawner.h"`. The stub class stays: the test fixture registers it so automated tests record drops instead of spawning actors.

### S5.4 to S5.6 — `UBFL_Inventory`

This library is what other frameworks call. Later sections add functions to it.

`UBFL_Inventory.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UBFL_Inventory.generated.h"

class ALooseItemActor;
class APlayerState;
class UDA_ItemDefinition;
class UInventoryData;

UCLASS()
class AWAKEN_API UBFL_Inventory : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // ---- queries ----
    // True on the host and offline.
    static bool isServerWorld(const UObject* worldContext);
    // Where an item dropped by this player appears: a short way in front of the pawn.
    static FVector getDropLocation(const APlayerState* player);
    // A new copy of an instanced item: a new instance id, full durability, and for a Portable Container the id of its (still empty) contents.
    static FS_InventoryEntry makeFreshInstance(const UObject* worldContext, const UDA_ItemDefinition& definition);
    // R01 §5 CanFit: whether the whole stack fits.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") static bool canFit(const UInventoryData* inventory, const FS_ItemStack& stack);

    // ---- commands, server only ----
    // R01 3.7 GrantOrDrop. Stores what fits, drops the rest, never destroys. spoilStartTime below zero = now;
    // Doc B10 passes its job's finish time (R01 amendment). Every other caller passes nothing.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Inventory") static FS_GrantResult grantOrDrop(APlayerState* player, const FS_ItemStack& stack, int64 spoilStartTime = -1);
    // The same rule for an entry that already exists: one physical item with its state, or a stack with its own spoil time.
    static FS_GrantResult grantEntryOrDrop(APlayerState* player, const FS_InventoryEntry& entry);
    // R01 3.11: the general pickup path.
    static FS_InventoryResult pickUpLooseItem(APlayerState* player, ALooseItemActor* looseItem);
    // Removes available units from the bag and hands them to the caller: the shared first half of a drop and of a throw (Doc B07).
    static FS_InventoryResult takeOutOfBag(APlayerState* player, const FS_ItemRef& ref, int32 quantity, FS_InventoryEntry& outEntry);
    // The "discard" button of the bag screen (Doc B17 3.6): the units land as a loose item in front of the player.
    static FS_InventoryResult dropFromBag(APlayerState* player, const FS_ItemRef& ref, int32 quantity);
    // R01 §5 TransferEntry: moves an entry between two inventories. There is no quick-stack or sort (Index A112).
    static FS_InventoryResult transferEntry(UInventoryData* source, UInventoryData* destination, const FS_ItemRef& ref, int32 quantity);

    // Shared by the library files of this framework.
    static FS_InventoryResult refusal(const FGameplayTag& reason);

private:
    static FS_InventoryEntry makeEntry(const UObject* worldContext, const UDA_ItemDefinition& definition, int32 quantity, int64 spoilAtTime);
    static void reportDropped(APlayerState* player, FName itemId, int32 quantity, const FVector& location);
};
```

`UBFL_Inventory.cpp`

```cpp
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/ALooseItemActor.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/InventoryTime.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

// ---------- helpers ----------

bool UBFL_Inventory::isServerWorld(const UObject* worldContext){
    const UWorld* world = (GEngine != nullptr && worldContext != nullptr) ? GEngine->GetWorldFromContextObject(worldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    return world != nullptr && world->GetNetMode() != NM_Client;
}

FVector UBFL_Inventory::getDropLocation(const APlayerState* player){
    const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
    if(pawn == nullptr){
        return player != nullptr ? player->GetActorLocation() : FVector::ZeroVector;
    }
    return pawn->GetActorLocation() + pawn->GetActorForwardVector() * UInventorySettings::get()->dropForwardDistance;
}

FS_InventoryEntry UBFL_Inventory::makeFreshInstance(const UObject* worldContext, const UDA_ItemDefinition& definition){
    FS_InventoryEntry entry;
    entry.itemId = definition.itemId;
    entry.totalQuantity = 1;
    entry.instanceId = FGuid::NewGuid();              // created with the instance; it never changes (Doc02 19.3)
    entry.state.durability = definition.durabilityMax;   // -1 stays -1: the unbreakable Axe
    if(definition.isPortableContainer()){
        entry.state.containedInventoryId = FGuid::NewGuid();   // the inventory itself is created when first used
    }
    return entry;
}

FS_InventoryEntry UBFL_Inventory::makeEntry(const UObject* worldContext, const UDA_ItemDefinition& definition, int32 quantity, int64 spoilAtTime){
    if(definition.isInstanced()){
        return makeFreshInstance(worldContext, definition);
    }
    FS_InventoryEntry entry;
    entry.itemId = definition.itemId;
    entry.totalQuantity = quantity;
    entry.spoilAtTime = spoilAtTime;
    return entry;
}

bool UBFL_Inventory::canFit(const UInventoryData* inventory, const FS_ItemStack& stack){
    return inventory != nullptr && stack.quantity > 0 && inventory->canFit(stack) == stack.quantity;
}

FS_InventoryResult UBFL_Inventory::refusal(const FGameplayTag& reason){
    FS_InventoryResult result;
    result.failReason = reason;
    return result;
}

void UBFL_Inventory::reportDropped(APlayerState* player, FName itemId, int32 quantity, const FVector& location){
    // Inventory.GrantDropped: server only, then local to the owning player (R01 §6).
    FS_GrantDroppedMessage message;
    message.player = player;
    message.itemId = itemId;
    message.quantity = quantity;
    UGameplayMessageSubsystem::Get(player).BroadcastMessage(T_Inventory_GrantDropped, message);
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(inventory != nullptr){
        inventory->clientGrantDropped(itemId, quantity);
    }
    const UInventoryServices* services = UInventoryServices::get(player);
    if(services != nullptr){
        services->raiseItemDroppedCosmetic(player, location);   // Cosmetic.Event.Interaction.ItemDropped (Index A89)
    }
}

// ---------- S5.4 grantOrDrop ----------

FS_GrantResult UBFL_Inventory::grantOrDrop(APlayerState* player, const FS_ItemStack& stack, int64 spoilStartTime){
    FS_GrantResult result;
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(stack.itemId) : nullptr;
    if(!isServerWorld(player) || services == nullptr || definition == nullptr || stack.quantity < 1){
        INVENTORY_LOG(Warning, player, "grantOrDrop did nothing: not the server, no player, unknown item '%s' or quantity %d", *stack.itemId.ToString(), stack.quantity);
        return result;
    }
    const int64 now = services->getGameTime();
    // The spoil clock starts at the given time, and a start in the future is clamped to now (R01 amendment).
    // The same spoil time is used for the part that is stored and for the part that is dropped.
    const int64 startTime = spoilStartTime < 0 ? now : FMath::Min(spoilStartTime, now);
    const int64 spoilAtTime = definition->canSpoil ? startTime + InventoryTime::hoursToGameSeconds(definition->spoilTimeHours) : 0;
    const FVector dropLocation = getDropLocation(player);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;   // null while Bagless

    if(definition->isHeavy()){
        // Step 0: an item with a carry cost never enters the bag. A free carry slot, otherwise loose beside the player.
        for(int32 unit = 0; unit < stack.quantity; unit++){
            const FS_InventoryEntry entry = makeEntry(player, *definition, 1, spoilAtTime);
            if(services->beginCarry(player, entry)){
                result.quantityCarried++;
            }else{
                services->spawnLooseItem(player, entry, dropLocation);
                result.quantityDropped++;
            }
        }
    }else if(definition->isStackable()){
        // Step 1: store as much as the type's limit allows. Step 2: the rest drops. Nothing is destroyed.
        if(bag != nullptr){
            bag->evaluateSpoilage(now);   // so fresh units are never merged into a stack that is already past its time
            result.quantityStored = bag->addStack(stack.itemId, stack.quantity, spoilAtTime).quantity;
        }
        result.quantityDropped = stack.quantity - result.quantityStored;
        if(result.quantityDropped > 0){
            services->spawnLooseItem(player, makeEntry(player, *definition, result.quantityDropped, spoilAtTime), dropLocation);
        }
    }else{
        for(int32 unit = 0; unit < stack.quantity; unit++){
            const FS_InventoryEntry entry = makeFreshInstance(player, *definition);
            if(bag != nullptr && bag->addInstance(entry).succeeded()){
                result.quantityStored++;
            }else{
                services->spawnLooseItem(player, entry, dropLocation);
                result.quantityDropped++;
            }
        }
    }

    // Step 3: say what happened.
    if(result.quantityDropped > 0){
        reportDropped(player, stack.itemId, result.quantityDropped, dropLocation);
    }
    INVENTORY_LOG(Log, player, "Granted %s x%d to %s: stored %d, carried %d, dropped %d", *stack.itemId.ToString(), stack.quantity, *GetNameSafe(player), result.quantityStored, result.quantityCarried, result.quantityDropped);
    return result;
}

FS_GrantResult UBFL_Inventory::grantEntryOrDrop(APlayerState* player, const FS_InventoryEntry& entry){
    FS_GrantResult result;
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(entry.itemId) : nullptr;
    if(!isServerWorld(player) || services == nullptr || definition == nullptr || entry.totalQuantity < 1){
        return result;
    }
    const FVector dropLocation = getDropLocation(player);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;

    if(definition->isHeavy()){
        if(services->beginCarry(player, entry)){
            result.quantityCarried = entry.totalQuantity;
            return result;
        }
    }else if(bag != nullptr){
        bag->evaluateSpoilage(services->getGameTime());
        if(entry.isInstanced()){
            result.quantityStored = bag->addInstance(entry).succeeded() ? 1 : 0;
        }else{
            result.quantityStored = bag->addStack(entry.itemId, entry.totalQuantity, entry.spoilAtTime).quantity;   // a partly fitting stack goes in in part
        }
    }
    result.quantityDropped = entry.totalQuantity - result.quantityStored;
    if(result.quantityDropped > 0){
        FS_InventoryEntry rest = entry;
        rest.totalQuantity = result.quantityDropped;
        services->spawnLooseItem(player, rest, dropLocation);
        reportDropped(player, entry.itemId, result.quantityDropped, dropLocation);
    }
    return result;
}

// ---------- S5.5 pickup and drop ----------

FS_InventoryResult UBFL_Inventory::pickUpLooseItem(APlayerState* player, ALooseItemActor* looseItem){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(catalog == nullptr || services == nullptr || inventory == nullptr || !IsValid(looseItem)){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    const FS_InventoryEntry entry = looseItem->getEntry();
    const UDA_ItemDefinition* definition = catalog->getDefinition(entry.itemId);
    if(definition == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    FS_InventoryResult result;

    // A heavy item goes to the hands only (R01 3.11).
    if(definition->isHeavy()){
        FS_InventoryEntry unit = entry;
        unit.totalQuantity = 1;
        if(!services->beginCarry(player, unit)){
            return refusal(T_Interaction_FailReason_HandsFull);
        }
        looseItem->reduceQuantity(1);
        result.quantity = 1;
        return result;
    }

    if(!inventory->hasBag()){
        return refusal(T_Inventory_FailReason_NoBag);   // Section 9 replaces this line with the Bagless hold slots
    }

    UInventoryData* bag = inventory->getPersonalInventory();
    bag->evaluateSpoilage(services->getGameTime());
    FS_ItemStack wanted;
    wanted.itemId = entry.itemId;
    wanted.quantity = entry.totalQuantity;
    const int32 fits = bag->canFit(wanted);
    if(fits < 1){
        // Refused, and nothing is dropped: the item stays where it lies (R01 3.11, Index A93).
        return refusal(T_Interaction_FailReason_BagFull);
    }
    const FS_InventoryResult added = entry.isInstanced() ? bag->addInstance(entry) : bag->addStack(entry.itemId, fits, entry.spoilAtTime);
    if(!added.succeeded()){
        return added;
    }
    looseItem->reduceQuantity(added.quantity);   // a partly fitting stack: the rest stays (Index A98)
    INVENTORY_LOG(Log, player, "%s picked up %s x%d", *GetNameSafe(player), *entry.itemId.ToString(), added.quantity);
    return added;
}

FS_InventoryResult UBFL_Inventory::takeOutOfBag(APlayerState* player, const FS_ItemRef& ref, int32 quantity, FS_InventoryEntry& outEntry){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(player);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(catalog == nullptr || services == nullptr || registry == nullptr || inventory == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(!inventory->hasBag()){
        return refusal(T_Inventory_FailReason_NoBag);
    }
    UInventoryData* bag = inventory->getPersonalInventory();
    bag->evaluateSpoilage(services->getGameTime());

    if(ref.isInstance()){
        const FS_InventoryEntry* entry = bag->findInstance(ref.instanceId);
        const UDA_ItemDefinition* definition = entry != nullptr ? catalog->getDefinition(entry->itemId) : nullptr;
        if(definition == nullptr){
            return refusal(T_Inventory_FailReason_NotFound);
        }
        if(definition->isPermanent()){
            return refusal(T_Inventory_FailReason_Permanent);          // the Axe is never thrown or dropped (R01 3.10)
        }
        if(!registry->isContainerItemEmpty(*entry)){
            return refusal(T_Inventory_FailReason_ContainerNotEmpty);  // empty it first (R01 3.4a)
        }
        outEntry = *entry;   // copied before the entry is removed
        return bag->removeInstance(ref.instanceId);                    // refuses a staged instance (R01 3.6)
    }

    const UDA_ItemDefinition* definition = catalog->getDefinition(ref.itemId);
    if(definition == nullptr || !definition->isStackable()){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(definition->isPermanent()){
        return refusal(T_Inventory_FailReason_Permanent);              // the map
    }
    const FS_InventoryEntry* stack = bag->findStack(ref.itemId);
    const int64 spoilAtTime = stack != nullptr ? stack->spoilAtTime : 0;   // a split part keeps the spoil time of its stack
    const FS_InventoryResult removed = bag->removeStack(ref.itemId, quantity);   // only the available part can leave
    if(removed.succeeded()){
        outEntry = FS_InventoryEntry();
        outEntry.itemId = ref.itemId;
        outEntry.totalQuantity = removed.quantity;
        outEntry.spoilAtTime = spoilAtTime;
    }
    return removed;
}

FS_InventoryResult UBFL_Inventory::dropFromBag(APlayerState* player, const FS_ItemRef& ref, int32 quantity){
    FS_InventoryEntry taken;
    const FS_InventoryResult result = takeOutOfBag(player, ref, quantity, taken);
    if(result.succeeded()){
        const UInventoryServices* services = UInventoryServices::get(player);
        services->spawnLooseItem(player, taken, getDropLocation(player));
        INVENTORY_LOG(Log, player, "%s dropped %s x%d from the bag", *GetNameSafe(player), *taken.itemId.ToString(), taken.totalQuantity);
    }
    return result;
}

// ---------- S5.6 transfer ----------

FS_InventoryResult UBFL_Inventory::transferEntry(UInventoryData* source, UInventoryData* destination, const FS_ItemRef& ref, int32 quantity){
    if(source == nullptr || destination == nullptr || source == destination){
        return refusal(T_Inventory_FailReason_InvalidRequest);
    }
    if(!isServerWorld(source)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    if(source->isDeathBag() || destination->isDeathBag()){
        return refusal(T_Inventory_FailReason_DeathBagLocked);   // one interaction only: its owner's pick-up hold (R01 3.4)
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(source);
    const UInventoryServices* services = UInventoryServices::get(source);
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(source);
    if(catalog == nullptr || services == nullptr || registry == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    const int64 now = services->getGameTime();
    source->evaluateSpoilage(now);        // a transfer is a request on the stack (R01 3.2a)
    destination->evaluateSpoilage(now);

    const FS_InventoryEntry* instance = ref.isInstance() ? source->findInstance(ref.instanceId) : nullptr;
    const FName itemId = instance != nullptr ? instance->itemId : ref.itemId;
    const UDA_ItemDefinition* definition = catalog->getDefinition(itemId);
    if(definition == nullptr || ref.isInstance() != definition->isInstanced() || (ref.isInstance() && instance == nullptr)){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    const bool intoBag = destination->getInventoryType() == E_InventoryType::Personal;
    if(definition->isPermanent() && !intoBag){
        return refusal(T_Inventory_FailReason_Permanent);        // never moved into another container (R01 3.10)
    }
    if(definition->isPortableContainer()){
        if(destination->getInventoryType() == E_InventoryType::PortableContainer){
            return refusal(T_Inventory_FailReason_NestedContainer);   // never one inside another (R01 3.4a)
        }
        if(!intoBag && !registry->isContainerItemEmpty(*instance)){
            return refusal(T_Inventory_FailReason_ContainerNotEmpty); // one that holds something only goes into a bag
        }
    }
    const FGameplayTag blocked = destination->checkAccepts(*definition);
    if(blocked.IsValid()){
        return refusal(blocked);
    }

    if(instance != nullptr){
        if(instance->reservedQuantity > 0){
            return refusal(T_Inventory_FailReason_ReservedQuantity);
        }
        const FS_InventoryEntry moved = *instance;   // the same instance id and state: it is the same physical item
        const FS_InventoryResult added = destination->addInstance(moved);
        if(!added.succeeded()){
            return added;
        }
        source->removeInstance(moved.instanceId);
        return added;
    }

    const int32 wanted = FMath::Min(quantity, source->getAvailableQuantity(itemId));   // staged units cannot be moved away (Index A103)
    if(wanted < 1){
        return refusal(T_Inventory_FailReason_InsufficientQuantity);
    }
    FS_ItemStack stack;
    stack.itemId = itemId;
    stack.quantity = wanted;
    const int32 fits = destination->canFit(stack);
    if(fits < 1){
        return refusal(destination->getFullReason());
    }
    const int64 spoilAtTime = source->findStack(itemId)->spoilAtTime;
    source->removeStack(itemId, fits);
    return destination->addStack(itemId, fits, spoilAtTime);   // the leftover stays in the source (R01 3.7)
}
```

Now add the client call that `reportDropped` uses. **Header additions** to `UPlayerInventoryComponent.h` (public):

```cpp
    // Server -> the owning client: an item was dropped because the bag had no room (Inventory.GrantDropped).
    UFUNCTION(Client, Reliable)
    void clientGrantDropped(FName itemId, int32 quantity);
```

and in `UPlayerInventoryComponent.cpp` (add the includes `GameFramework/GameplayMessageSubsystem.h` and `ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h`):

```cpp
void UPlayerInventoryComponent::clientGrantDropped_Implementation(FName itemId, int32 quantity){
    if(isServerSide()){
        return;   // the host raised the message already; this copy is for a remote owner
    }
    FS_GrantDroppedMessage message;
    message.player = getOwnerPlayerState();
    message.itemId = itemId;
    message.quantity = quantity;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Inventory_GrantDropped, message);
}
```

> **Who checks range, hands and the player's state?** Not these functions. They are the *rules*. The *request* layer of Section 12 checks who is asking, from how far, and in which state, before it calls them. A dev command or another server system may call them directly.

### S5.7 — The cosmetic event, when its channel exists (I20)

`raiseItemDroppedCosmetic` only writes a Verbose log line for now. When the seam of Doc B21 exists (the R05 guide creates `IBPI_CosmeticEventSink` in its task E1.4, or B21's owner supplies theirs), replace the body in `InventoryServices.cpp` with:

```cpp
#include "EquipmentFramework/03Interfaces/BPI_CosmeticEventSink.h"

void UInventoryServices::raiseItemDroppedCosmetic(APlayerState* player, const FVector& location) const{
    UObject* sink = getSeam(UBPI_CosmeticEventSink::StaticClass());
    if(sink != nullptr){
        IBPI_CosmeticEventSink::Execute_raiseCosmeticEvent(sink, T_Cosmetic_Event_Interaction_ItemDropped, location, player != nullptr ? player->GetPawn() : nullptr, 1.0f);
    }
}
```

### Verify (Section 5)

Until Section 13 adds the commands, run these as an automated spec with the fixture (its loose spawner records instead of spawning), or as a temporary host-side snippet.

| Test | Expect |
| --- | --- |
| `grantOrDrop(Stick × 5)`, bag empty, limit 20 | stored 5, dropped 0; no `Inventory.GrantDropped` |
| `grantOrDrop(Stick × 30)` with 5 held | stored **15**, dropped **15**; one loose entry of 15; `Inventory.GrantDropped` with quantity 15 |
| `grantOrDrop(Spear × 6)`, limit 4 | stored 4, dropped 2; **two** loose entries, each with its own instance id and full durability |
| `grantOrDrop(Log × 2)` (heavy), carry stub refusing | carried 0, dropped 2; the bag holds no log |
| The same with `acceptsNewCarry = true` | carried 2, dropped 0 |
| `grantOrDrop(NoSuchItem × 1)` | nothing happens; one Warning line |
| Pick up a loose `Stick × 10` with 15 held | **5** taken; the loose item still holds 5 |
| Pick up a loose `Stick × 3` with 20 held | refused `Interaction.FailReason.BagFull`; the loose item is unchanged; **nothing new is dropped** |
| Pick up a loose `Spear` (durability 12) | the bag holds a spear with durability 12 and the same instance id |
| Pick up a loose `Log`, carry stub refusing | refused `Interaction.FailReason.HandsFull` |
| `dropFromBag(StarterAxe)` and `dropFromBag(Map)` | refused `Inventory.FailReason.Permanent` |
| `dropFromBag` of a `Pouch` that holds one stick | refused `Inventory.FailReason.ContainerNotEmpty`; after the stick is taken out it drops |
| `transferEntry(bag → chest, Stick × 10)` | 10 moved; totals add up; both inventories raised `Inventory.Changed` |
| The chest is at its limit for sticks | refused `Inventory.FailReason.ContainerFull`; nothing moved |
| The chest has room for 4 of 10 | 4 moved, 6 stay in the bag |
| `transferEntry(bag → chest, StarterAxe)` | refused `Permanent` |
| `transferEntry(bag → pouch contents, a second Pouch)` | refused `NestedContainer` |
| `transferEntry(bag → chest, a Pouch that holds something)` | refused `ContainerNotEmpty`; an **empty** pouch is accepted (R01 amendment) |
| A transfer where either side is a death bag | refused `DeathBagLocked` |
| Any command on a client | `NotServer`, or for the two grant functions an empty result and a Warning |

**In the world (offline):** temporarily call `UBFL_Inventory::grantOrDrop(PlayerState, {Stick, 30})` from the test GameMode's `PostLogin`. Twenty sticks are in the bag and a stick mesh lies in front of the player; after `defaultLooseLifetimeSeconds` it disappears.

### Handoff

- **B08, B10, R02, B01** call `UBFL_Inventory::grantOrDrop(player, stack)` for every yield, output, refund and blueprint cancel (Doc02 §19.5). **B10** passes its job's finish time as `spoilStartTime`; everyone else passes nothing.
- **B09**: at a rope or slider end there is no player, so `grantOrDrop` is not used. Call `destination->addStack` / `addInstance` on the container's `UInventoryData` and leave whatever did not fit loose at the landing spot with `UInventoryServices::spawnLooseItem(nullptr, entry, location)` (Doc B09 3.10).
- **B07**: the throw pipeline starts with `UBFL_Inventory::takeOutOfBag`, which already refuses the Axe, the map and a container that holds something; a thrown weapon that lands is spawned with `spawnLooseItem` and the durability it has left, and one that reached zero is not spawned (Doc R03 3.5).
- **B18 / B11**: a loose item and a container are interactables. The pickup interaction calls the request of Section 12, not these functions directly.

---
# SECTION 6 — The Hotbar and the Held Item

**Goal:** a hotbar key points at an item type; a component on the Character says what is in the hand; Combat always gets a weapon to read, even if it is Unarmed.
**Spec:** R01 3.5, 3.10, 3.2 (reload), §5 (`BindHotbarSlot`, `ResolveHotbarSlot`, `GetHeldItem`, `ApplyReload`, `GetUsableHotbarSlotCount`), §6; Doc R03 3.1, 3.5; Doc R04 §6; decision D4.
**Depends on:** Section 5.

### Files

```
Public/ItemInventoryFramework/UPlayerInventoryComponent.h      (additions)
Public/ItemInventoryFramework/UHeldItemComponent.h
Public/ItemInventoryFramework/UBFL_Inventory.h                 (additions)
Private/ItemInventoryFramework/UHeldItemComponent.cpp
Private/ItemInventoryFramework/Stubs/AInventoryTestCharacter.h/.cpp
```

### The rules

- **A slot stores a binding to an item id, not to one physical stack** (R01 3.5). Two slots bound to the same stackable item draw from one shared pool. A slot shows empty only when the whole pool is gone, and it works again as soon as the player has that item again, until the player rebinds the key (Index A102).
- **Instanced items: the last used copy first** (decision D4). If it leaves the bag by any route, the next most recently used copy takes over (PROPOSED in the spec).
- **Binding is by a button press, never by dragging.** The request comes in Section 12; this section is the rule.
- **The hotbar bindings are saved; the held item is not** (R01 3.5).
- **There is no sheathed state** (Doc R05 3.1): a weapon is equipped by holding it, from a hotbar key or by a direct pick from the bag.
- **Combat never sees "no weapon".** When nothing is wielded, the held item resolves to the Unarmed entry (R01 3.10).
- **A weapon swap cancels a reload** (R04-R08-IG17): when the held item changes, the server calls `cancelAction(Action.Reload)`. The `Hotbar.Changed` message is not the trigger.

### S6.1 to S6.3 — The hotbar, on `UPlayerInventoryComponent`

The bindings live with the bag on the PlayerState (R01 §2).

**Header additions** to `UPlayerInventoryComponent.h`:

```cpp
public:
    // ---- hotbar (R01 3.5) ----
    // R01 §5 BindHotbarSlot. Server. An empty itemId clears the slot. Replaces any earlier binding on that slot.
    FS_InventoryResult bindHotbarSlot(int32 slotIndex, FName itemId);
    // R01 §5 ResolveHotbarSlot: the entry the slot uses now: the pooled stack, or the last used instance.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FS_ResolvedHotbarSlot resolveHotbarSlot(int32 slotIndex) const;
    // R01 §5 GetUsableHotbarSlotCount: the full hotbar with the bag, 3 while Bagless.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") int32 getUsableHotbarSlotCount() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FName getBoundItemId(int32 slotIndex) const;
    // With the bag: any slot 0 to 9. While Bagless: only keys 1, 2 and 3 (R01 3.8).
    bool isHotbarSlotUsable(int32 slotIndex) const;
    const TArray<FS_HotbarBinding>& getHotbarBindings() const;
    // Server. Puts a whole set of bindings back: bag recovery and restore from a save.
    void setHotbarBindings(const TArray<FS_HotbarBinding>& newBindings);
    // The copy of an instanced item that would be used now: the last used one that is still in the bag and not staged.
    FGuid findUsableInstance(FName itemId) const;
    // Server. Remembers that this copy was just taken in the hand.
    void noteInstanceUsed(const FGuid& instanceId);

protected:
    UFUNCTION()
    void OnRep_hotbar(const TArray<FS_HotbarBinding>& previousBindings);

    void raiseHotbarChanged(int32 slotIndex) const;

    // Entry i is slot i. Replicated to the owner only (R01 §7).
    UPROPERTY(ReplicatedUsing = OnRep_hotbar) TArray<FS_HotbarBinding> hotbar;
    // The most recent is last. Owner only (the hover card marks the copy that will be used next, Doc B17 3.6). Not saved (I9).
    UPROPERTY(Replicated) TArray<FGuid> recentInstances;
```

In `GetLifetimeReplicatedProps` add:

```cpp
    DOREPLIFETIME_CONDITION(UPlayerInventoryComponent, hotbar, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UPlayerInventoryComponent, recentInstances, COND_OwnerOnly);
```

In `BeginPlay`, before the client block, add the empty hotbar on the host:

```cpp
    if(isServerSide()){
        setHotbarBindings(TArray<FS_HotbarBinding>());   // slots 0 to 9, all unbound
    }
```

Append to `UPlayerInventoryComponent.cpp` (add the includes `UItemCatalogSubsystem.h`, `UDA_ItemDefinition.h`, `UInventorySettings.h`, `UHeldItemComponent.h`):

```cpp
// ---------- Section 6: hotbar ----------

bool UPlayerInventoryComponent::isHotbarSlotUsable(int32 slotIndex) const{
    if(bagAttached){
        return hotbar.IsValidIndex(slotIndex);
    }
    return slotIndex >= 1 && slotIndex <= UInventorySettings::get()->baglessSlotCount;   // keys 4 to 9 and 0 are locked
}

int32 UPlayerInventoryComponent::getUsableHotbarSlotCount() const{
    return bagAttached ? hotbar.Num() : UInventorySettings::get()->baglessSlotCount;
}

FName UPlayerInventoryComponent::getBoundItemId(int32 slotIndex) const{
    return hotbar.IsValidIndex(slotIndex) ? hotbar[slotIndex].boundItemId : FName();
}

const TArray<FS_HotbarBinding>& UPlayerInventoryComponent::getHotbarBindings() const{
    return hotbar;
}

void UPlayerInventoryComponent::setHotbarBindings(const TArray<FS_HotbarBinding>& newBindings){
    if(!isServerSide()){
        return;
    }
    const int32 slotCount = UInventorySettings::get()->hotbarSlotCount;
    TArray<FS_HotbarBinding> rebuilt;
    rebuilt.SetNum(slotCount);
    for(int32 index = 0; index < slotCount; index++){
        rebuilt[index].slotIndex = index;
    }
    for(const FS_HotbarBinding& binding : newBindings){
        if(rebuilt.IsValidIndex(binding.slotIndex)){
            rebuilt[binding.slotIndex].boundItemId = binding.boundItemId;
        }
    }
    const TArray<FS_HotbarBinding> previous = hotbar;
    hotbar = rebuilt;
    OnRep_hotbar(previous);   // the host raises Hotbar.Changed for each slot that differs
    GetOwner()->ForceNetUpdate();
}

FS_InventoryResult UPlayerInventoryComponent::bindHotbarSlot(int32 slotIndex, FName itemId){
    if(!isServerSide()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotServer);
    }
    if(!bagAttached){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NoBag);   // a Bagless slot holds one physical item, not a binding (R01 3.8)
    }
    if(!hotbar.IsValidIndex(slotIndex)){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_InvalidSlot);
    }
    if(!itemId.IsNone()){
        const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
        const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(itemId) : nullptr;
        if(definition == nullptr){
            return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
        }
        if(definition->isHeavy() || itemId == UInventorySettings::get()->unarmedItemId){
            return UBFL_Inventory::refusal(T_Inventory_FailReason_InvalidRequest);   // neither can ever be in the bag
        }
    }
    // The player does not need to hold the item: the slot points at a type and works as soon as they have one (Index A102).
    if(hotbar[slotIndex].boundItemId != itemId){
        hotbar[slotIndex].boundItemId = itemId;
        raiseHotbarChanged(slotIndex);
        GetOwner()->ForceNetUpdate();
        notifyChanged();
        INVENTORY_LOG(Log, this, "%s bound hotbar slot %d to %s", *GetNameSafe(GetOwner()), slotIndex, itemId.IsNone() ? TEXT("nothing") : *itemId.ToString());
    }
    return FS_InventoryResult();
}

FGuid UPlayerInventoryComponent::findUsableInstance(FName itemId) const{
    const UInventoryData* bag = getPersonalInventory();
    if(bag == nullptr){
        return FGuid();
    }
    // The last used copy first (decision D4); if it is gone, the next most recently used one.
    for(int32 index = recentInstances.Num() - 1; index >= 0; index--){
        const FS_InventoryEntry* entry = bag->findInstance(recentInstances[index]);
        if(entry != nullptr && entry->itemId == itemId && entry->reservedQuantity == 0){
            return entry->instanceId;
        }
    }
    // No history (for example right after a load): the first copy in the bag (I9).
    for(const FS_InventoryEntry& entry : bag->getEntries()){
        if(entry.isInstanced() && entry.itemId == itemId && entry.reservedQuantity == 0){
            return entry.instanceId;
        }
    }
    return FGuid();
}

void UPlayerInventoryComponent::noteInstanceUsed(const FGuid& instanceId){
    if(!isServerSide() || !instanceId.IsValid()){
        return;
    }
    recentInstances.Remove(instanceId);
    recentInstances.Add(instanceId);
    if(recentInstances.Num() > 32){
        recentInstances.RemoveAt(0);
    }
}

FS_ResolvedHotbarSlot UPlayerInventoryComponent::resolveHotbarSlot(int32 slotIndex) const{
    FS_ResolvedHotbarSlot resolved;
    resolved.slotIndex = slotIndex;
    if(!isHotbarSlotUsable(slotIndex)){
        return resolved;
    }
    const UInventoryData* bag = getPersonalInventory();
    if(bag == nullptr){
        return resolved;   // Bagless: Section 9 fills this branch with the hold slots
    }
    resolved.itemId = hotbar[slotIndex].boundItemId;
    if(resolved.itemId.IsNone()){
        return resolved;
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(resolved.itemId) : nullptr;
    if(definition == nullptr){
        return resolved;
    }
    // Every reader sees the available quantity, never the total (R01 3.6).
    resolved.availableQuantity = bag->getAvailableQuantity(resolved.itemId);
    if(definition->isInstanced()){
        resolved.instanceId = findUsableInstance(resolved.itemId);
    }
    resolved.isUsable = resolved.availableQuantity > 0;
    return resolved;
}

void UPlayerInventoryComponent::raiseHotbarChanged(int32 slotIndex) const{
    FS_HotbarChangedMessage message;
    message.player = getOwnerPlayerState();
    message.slotIndex = slotIndex;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Hotbar_Changed, message);
}

// Hotbar.Changed is local on every machine that holds the bindings, raised from the replicated state (R01 §6).
void UPlayerInventoryComponent::OnRep_hotbar(const TArray<FS_HotbarBinding>& previousBindings){
    for(int32 index = 0; index < hotbar.Num(); index++){
        const FName before = previousBindings.IsValidIndex(index) ? previousBindings[index].boundItemId : FName();
        if(before != hotbar[index].boundItemId){
            raiseHotbarChanged(index);
        }
    }
    notifyChanged();
}
```

Add `#include "ItemInventoryFramework/UBFL_Inventory.h"` and `#include "ItemInventoryFramework/InventoryLog.h"` if they are not there yet.

> **A key bound to fresh food never uses spoiled food.** The binding is an item id, and spoiled food is a different id (R01 3.2a). When the fresh stack spoils, the slot simply resolves to an empty pool.

### S6.4 to S6.6 — `UHeldItemComponent`

`UHeldItemComponent.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ItemInventoryFramework/04Delegates/D_ItemInventoryFramework.h"
#include "UHeldItemComponent.generated.h"

class APlayerState;
class UDA_ItemDefinition;

// R01 3.5: what the character has in the hand. A Character component, owned by this framework.
// Combat (R03), Equipment (R05) and Animation (R07) read it. It is never saved.
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class AWAKEN_API UHeldItemComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UHeldItemComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Query. The component on the player's current pawn, or null.
    static UHeldItemComponent* findFor(const APlayerState* player);

    // ---- queries ----
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FS_HeldItem getHeldState() const;
    // What Combat reads: the wielded weapon, or the Unarmed entry when nothing is wielded. Never null once the catalog is loaded.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") const UDA_ItemDefinition* getHeldDefinition() const;
    // What Animation shows in the hand: any held item, wielded or not. Null for an empty hand.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") const UDA_ItemDefinition* getShownDefinition() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") bool isUnarmed() const;

    // ---- commands (server) ----
    // A hotbar key was pressed: take what that slot resolves to. An empty slot empties the hand.
    FS_InventoryResult selectHotbarSlot(int32 slotIndex);
    // A direct pick from the bag screen (R01 3.5).
    FS_InventoryResult holdFromBag(const FS_ItemRef& ref);
    // Empties the hand: Combat then reads Unarmed.
    void putAway();
    // The bag changed: if the held entry is gone, the next copy takes over, or the hand empties.
    void refreshFromInventory();

    // Raised on every machine when what is in the hand changes.
    UPROPERTY(BlueprintAssignable, Category = "Awaken|Inventory") FD_HeldItemChanged onHeldItemChanged;

protected:
    UFUNCTION()
    void OnRep_held();

    bool isServerSide() const;
    APlayerState* getPlayerState() const;
    void setHeld(const FS_HeldItem& newHeld);

    // Replicated to everyone: other players must see what is in the hand (Doc02 §8).
    UPROPERTY(ReplicatedUsing = OnRep_held) FS_HeldItem held;
};
```

`UHeldItemComponent.cpp`

```cpp
#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

UHeldItemComponent::UHeldItemComponent(){
    SetIsReplicatedByDefault(true);
}

void UHeldItemComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UHeldItemComponent, held);   // COND_None
}

UHeldItemComponent* UHeldItemComponent::findFor(const APlayerState* player){
    const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
    return pawn != nullptr ? pawn->FindComponentByClass<UHeldItemComponent>() : nullptr;
}

bool UHeldItemComponent::isServerSide() const{
    return GetOwner() != nullptr && GetOwner()->HasAuthority();
}

APlayerState* UHeldItemComponent::getPlayerState() const{
    const APawn* pawn = Cast<APawn>(GetOwner());
    return pawn != nullptr ? pawn->GetPlayerState() : nullptr;
}

// ---------- queries ----------

FS_HeldItem UHeldItemComponent::getHeldState() const{
    return held;
}

const UDA_ItemDefinition* UHeldItemComponent::getHeldDefinition() const{
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    if(catalog == nullptr){
        return nullptr;
    }
    const UDA_ItemDefinition* wielded = held.isWielded ? catalog->getDefinition(held.itemId) : nullptr;
    return wielded != nullptr ? wielded : catalog->getUnarmedDefinition();   // the fallback of R01 3.10
}

const UDA_ItemDefinition* UHeldItemComponent::getShownDefinition() const{
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    return (catalog != nullptr && !held.isEmpty()) ? catalog->getDefinition(held.itemId) : nullptr;
}

bool UHeldItemComponent::isUnarmed() const{
    return !held.isWielded;
}

// ---------- commands ----------

FS_InventoryResult UHeldItemComponent::selectHotbarSlot(int32 slotIndex){
    if(!isServerSide()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotServer);
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getPlayerState());
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    if(inventory == nullptr || catalog == nullptr){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    if(!inventory->isHotbarSlotUsable(slotIndex)){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_InvalidSlot);
    }
    const FS_ResolvedHotbarSlot resolved = inventory->resolveHotbarSlot(slotIndex);
    const UDA_ItemDefinition* definition = resolved.isUsable ? catalog->getDefinition(resolved.itemId) : nullptr;
    if(definition == nullptr){
        putAway();   // an empty slot, or a slot whose pool is gone: the hand is empty
        return FS_InventoryResult();
    }
    FS_HeldItem newHeld;
    newHeld.itemId = resolved.itemId;
    newHeld.instanceId = resolved.instanceId;
    newHeld.sourceSlot = slotIndex;
    // Only a weapon held by a player who has the bag is wielded (I10; a Bagless player fights Unarmed, R01 3.8).
    newHeld.isWielded = inventory->hasBag() && definition->isWeapon();
    setHeld(newHeld);
    return FS_InventoryResult();
}

FS_InventoryResult UHeldItemComponent::holdFromBag(const FS_ItemRef& ref){
    if(!isServerSide()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotServer);
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getPlayerState());
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    if(inventory == nullptr || catalog == nullptr){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    const UInventoryData* bag = inventory->getPersonalInventory();
    if(bag == nullptr){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NoBag);
    }
    const FS_InventoryEntry* instance = ref.isInstance() ? bag->findInstance(ref.instanceId) : nullptr;
    const FName itemId = instance != nullptr ? instance->itemId : ref.itemId;
    const UDA_ItemDefinition* definition = catalog->getDefinition(itemId);
    if(definition == nullptr || (ref.isInstance() && instance == nullptr)){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    FS_HeldItem newHeld;
    newHeld.itemId = itemId;
    if(definition->isInstanced()){
        newHeld.instanceId = instance != nullptr ? instance->instanceId : inventory->findUsableInstance(itemId);
        const FS_InventoryEntry* chosen = bag->findInstance(newHeld.instanceId);
        if(chosen == nullptr){
            return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
        }
        if(chosen->reservedQuantity > 0){
            return UBFL_Inventory::refusal(T_Inventory_FailReason_ReservedQuantity);   // it is staged for a craft
        }
    }else if(bag->getAvailableQuantity(itemId) < 1){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_InsufficientQuantity);
    }
    newHeld.isWielded = definition->isWeapon();
    setHeld(newHeld);
    return FS_InventoryResult();
}

void UHeldItemComponent::putAway(){
    if(isServerSide()){
        setHeld(FS_HeldItem());
    }
}

void UHeldItemComponent::refreshFromInventory(){
    if(!isServerSide() || held.isEmpty()){
        return;
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getPlayerState());
    const UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    if(bag == nullptr){
        putAway();   // Section 9 adds the Bagless hold slots before this line
        return;
    }
    if(held.instanceId.IsValid()){
        if(bag->findInstance(held.instanceId) != nullptr){
            return;   // still there (its durability may have changed; that is not a change of what is held)
        }
        // Broken, thrown, stored or given away: the next most recently used copy takes over (R01 3.5, PROPOSED there).
        const FGuid next = inventory->findUsableInstance(held.itemId);
        if(next.IsValid()){
            FS_HeldItem newHeld = held;
            newHeld.instanceId = next;
            setHeld(newHeld);
            return;
        }
        putAway();
        return;
    }
    if(bag->getAvailableQuantity(held.itemId) < 1){
        putAway();   // the last unit of the pool was used or staged
    }
}

void UHeldItemComponent::setHeld(const FS_HeldItem& newHeld){
    if(newHeld == held){
        return;
    }
    held = newHeld;
    APlayerState* player = getPlayerState();
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(inventory != nullptr){
        inventory->noteInstanceUsed(held.instanceId);
    }
    // A weapon swap cancels a reload in progress (Doc R04 3.3, R04-R08-IG17). Server only; Hotbar.Changed is not the trigger.
    const UInventoryServices* services = UInventoryServices::get(this);
    if(services != nullptr && player != nullptr){
        services->cancelAction(player, T_Action_Reload);
    }
    INVENTORY_LOG(Log, this, "%s now holds %s%s", *GetNameSafe(player), held.isEmpty() ? TEXT("nothing") : *held.itemId.ToString(), held.isWielded ? TEXT(" (wielded)") : TEXT(""));
    onHeldItemChanged.Broadcast();
}

void UHeldItemComponent::OnRep_held(){
    onHeldItemChanged.Broadcast();
}
```

**Keep the hand in step with the bag.** In `UPlayerInventoryComponent.cpp`, add this helper call at the end of `handleBagChanged` and at the end of `releaseInventory` (just before its `return`):

```cpp
    UHeldItemComponent* heldItem = UHeldItemComponent::findFor(getOwnerPlayerState());
    if(heldItem != nullptr){
        heldItem->refreshFromInventory();
    }
```

> The Character is replaced when the player dies; the PlayerState is not. A new Character starts with an empty hand, which is exactly "the held item is not saved".

### S6.7 — Library functions for Combat and Abilities

**Header additions** to `UBFL_Inventory.h`:

```cpp
    // ---- Section 6 ----
    // R01 §5 GetHeldItem: what is wielded now, or the Unarmed entry.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") static const UDA_ItemDefinition* getHeldItem(const APlayerState* player);
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") static FS_HeldItem getHeldState(const APlayerState* player);
    // Server. Wears one instance in the player's bag down. At zero the item breaks and disappears (Doc R03 3.5; no repair, Index A87).
    // An unbreakable item and an item without durability are untouched.
    static FS_InventoryResult applyDurabilityLoss(APlayerState* player, const FGuid& instanceId, float amount);
    // R01 §5 ApplyReload. Server. Called by GA_Reload on normal completion (Doc R04 §6). A cancelled reload never calls it.
    static FS_InventoryResult applyReload(APlayerState* player, const FGuid& weaponInstanceId);
    // Server. Called by Doc R03 when a crossbow or a firearm fires: unloads the crossbow, or spends one round (I11).
    static FS_InventoryResult spendLoadedShot(APlayerState* player, const FGuid& weaponInstanceId);
```

Append to `UBFL_Inventory.cpp` (add `#include "ItemInventoryFramework/UHeldItemComponent.h"`):

```cpp
// ---------- Section 6: held item, durability, reload ----------

const UDA_ItemDefinition* UBFL_Inventory::getHeldItem(const APlayerState* player){
    const UHeldItemComponent* heldItem = UHeldItemComponent::findFor(player);
    if(heldItem != nullptr){
        return heldItem->getHeldDefinition();
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    return catalog != nullptr ? catalog->getUnarmedDefinition() : nullptr;   // no pawn: still never "no weapon"
}

FS_HeldItem UBFL_Inventory::getHeldState(const APlayerState* player){
    const UHeldItemComponent* heldItem = UHeldItemComponent::findFor(player);
    return heldItem != nullptr ? heldItem->getHeldState() : FS_HeldItem();
}

FS_InventoryResult UBFL_Inventory::applyDurabilityLoss(APlayerState* player, const FGuid& instanceId, float amount){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    const FS_InventoryEntry* entry = bag != nullptr ? bag->findInstance(instanceId) : nullptr;
    const UDA_ItemDefinition* definition = (entry != nullptr && catalog != nullptr) ? catalog->getDefinition(entry->itemId) : nullptr;
    if(definition == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    FS_InventoryResult result;
    result.instanceId = instanceId;
    if(!definition->hasDurability() || amount <= 0.0f){
        return result;   // the unbreakable Axe, or an item with no durability: nothing to do
    }
    FS_ItemInstanceState state = entry->state;
    state.durability = FMath::Max(0.0f, state.durability - amount);
    if(state.durability > 0.0f){
        bag->setInstanceState(instanceId, state);
        return result;
    }
    // Zero durability: the item breaks and disappears. The hand moves to the next copy by itself (refreshFromInventory).
    const FName itemId = entry->itemId;
    const FS_InventoryResult removed = bag->removeInstance(instanceId);
    if(removed.succeeded()){
        result.quantity = 1;
        INVENTORY_LOG(Log, player, "%s of %s broke and is destroyed", *itemId.ToString(), *GetNameSafe(player));
    }
    return result;
}

FS_InventoryResult UBFL_Inventory::applyReload(APlayerState* player, const FGuid& weaponInstanceId){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    if(bag == nullptr){
        return refusal(T_Inventory_FailReason_NoBag);
    }
    const FS_InventoryEntry* weapon = bag->findInstance(weaponInstanceId);
    const UDA_ItemDefinition* definition = (weapon != nullptr && catalog != nullptr) ? catalog->getDefinition(weapon->itemId) : nullptr;
    if(definition == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    FS_ItemInstanceState state = weapon->state;
    int32 wanted = 0;
    if(definition->weaponType == E_WeaponType::Crossbow){
        wanted = state.isLoaded ? 0 : 1;                                   // one arrow
    }else if(definition->weaponType == E_WeaponType::Firearm){
        wanted = definition->magazineSize - state.loadedRounds;            // up to MagazineSize
    }
    if(wanted < 1){
        return refusal(T_Inventory_FailReason_CannotReload);
    }
    const int32 loaded = FMath::Min(wanted, bag->getAvailableQuantity(definition->ammoItemId));
    if(loaded < 1){
        return refusal(T_Inventory_FailReason_NoAmmo);
    }
    // Everything checked: spend the ammunition, then write the loaded state. One bullet per round loaded.
    const FS_InventoryResult spent = bag->removeStack(definition->ammoItemId, loaded);
    if(!spent.succeeded()){
        return spent;
    }
    if(definition->weaponType == E_WeaponType::Crossbow){
        state.isLoaded = true;
    }else{
        state.loadedRounds += loaded;
    }
    bag->setInstanceState(weaponInstanceId, state);
    FS_InventoryResult result;
    result.quantity = loaded;
    result.instanceId = weaponInstanceId;
    INVENTORY_LOG(Log, player, "%s reloaded %s with %d", *GetNameSafe(player), *definition->itemId.ToString(), loaded);
    return result;
}

FS_InventoryResult UBFL_Inventory::spendLoadedShot(APlayerState* player, const FGuid& weaponInstanceId){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    const FS_InventoryEntry* weapon = bag != nullptr ? bag->findInstance(weaponInstanceId) : nullptr;
    if(weapon == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    FS_ItemInstanceState state = weapon->state;
    if(state.isLoaded){
        state.isLoaded = false;
    }else if(state.loadedRounds > 0){
        state.loadedRounds--;
    }else{
        return refusal(T_Inventory_FailReason_NoAmmo);   // nothing is loaded
    }
    bag->setInstanceState(weaponInstanceId, state);
    FS_InventoryResult result;
    result.quantity = 1;
    result.instanceId = weaponInstanceId;
    return result;
}
```

> The loaded state is part of the instance, so it is saved with it and survives a drop, a throw and a weapon switch (R01 3.2). A bow has no loaded state: Doc R03 spends its arrow at release with `removeStack`.

### S6.8 — Stub Character (owner: Docs B19, R07)

`Private/ItemInventoryFramework/Stubs/AInventoryTestCharacter.h`

```cpp
// TEMPORARY STUB — owners: Doc B19 (class table) and Doc R07 (the real Character). The real Character carries the same component.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AInventoryTestCharacter.generated.h"

class UHeldItemComponent;

UCLASS()
class AInventoryTestCharacter : public ACharacter{
    GENERATED_BODY()

public:
    AInventoryTestCharacter();

    UPROPERTY(VisibleAnywhere) TObjectPtr<UHeldItemComponent> heldItemComponent;
};
```

`Private/ItemInventoryFramework/Stubs/AInventoryTestCharacter.cpp`

```cpp
// TEMPORARY STUB — owners: Docs B19, R07.
#include "ItemInventoryFramework/Stubs/AInventoryTestCharacter.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"

AInventoryTestCharacter::AInventoryTestCharacter(){
    heldItemComponent = CreateDefaultSubobject<UHeldItemComponent>(TEXT("HeldItem"));
}
```

In `AInventoryTestGameMode`'s constructor add `DefaultPawnClass = AInventoryTestCharacter::StaticClass();` (and the include).

For automated tests, add one helper to the fixture. `InventoryTestFixture.h`:

```cpp
    // A stub Character linked to the player, so the held item can be tested without a controller.
    class AInventoryTestCharacter* makeCharacterFor(AInventoryTestPlayerState* player);
```

`InventoryTestFixture.cpp` (add `#include "ItemInventoryFramework/Stubs/AInventoryTestCharacter.h"`):

```cpp
AInventoryTestCharacter* FInventoryTestFixture::makeCharacterFor(AInventoryTestPlayerState* player){
    AInventoryTestCharacter* character = testWorld->SpawnActor<AInventoryTestCharacter>();
    character->SetPlayerState(player);   // also tells the PlayerState which pawn is its own
    return character;
}
```

### Verify (Section 6)

| Test | Expect |
| --- | --- |
| A new player | ten slots, all unbound; `getUsableHotbarSlotCount()` is 10; `getHeldItem(player)` is the `Unarmed` definition |
| `bindHotbarSlot(1, Stick)` with no sticks held | accepted; `resolveHotbarSlot(1).isUsable` is false; after `grantOrDrop(Stick × 4)` it is true with quantity 4 |
| Bind slots 1 and 2 to `Stick`, remove 3 sticks | both slots show 1: one shared pool |
| `bindHotbarSlot(1, Stone)` | replaces the binding; `Hotbar.Changed` with slot 1, once |
| `bindHotbarSlot(12, Stick)` | refused `InvalidSlot` |
| `bindHotbarSlot(1, Log)` (heavy) | refused `InvalidRequest` |
| Hold spear A, then spear B (direct picks), bind slot 3 to `Spear`, `resolveHotbarSlot(3)` | the instance id of **B**, the last used |
| `selectHotbarSlot(3)` | held = spear B, `isWielded` true; `getHeldItem(player)` is the `Spear` definition; the action stub recorded `Action.Reload` cancelled |
| `applyDurabilityLoss(B, 1000)` | B is gone from the bag; the hand now holds spear **A**; the slot still resolves (to A) |
| Break A as well | the hand is empty, `getHeldItem` is `Unarmed`, slot 3 resolves to not usable; grant a new spear and the slot is usable again without rebinding |
| `holdFromBag(Herb)` | held = Herb, `isWielded` **false**; `getHeldItem` is still `Unarmed`; `getShownDefinition` is Herb |
| `transferEntry` of the held spear into a chest | the hand moves to another spear, or empties |
| `applyReload` on a `Crossbow` with 5 arrows | `isLoaded` true; 4 arrows left; a second reload is refused `CannotReload` |
| `applyReload` with no arrows | refused `NoAmmo`; nothing changes |
| A firearm test item, `magazineSize 6`, 4 bullets | `loadedRounds` 4, 0 bullets left |
| `spendLoadedShot` on the loaded crossbow | `isLoaded` false; a second call is refused `NoAmmo` |
| `applyDurabilityLoss(StarterAxe, 1000)` | nothing changes: the Axe never breaks |
| Any command on a client | `NotServer` |

**Two players:** the client selects a spear (use a temporary key binding that calls the Section 12 request, or run the host-side function for now). The **host and a second client** see `getHeldState()` of that client's Character change; only the owner sees their own hotbar bindings.

### Handoff

- **R03** reads `UBFL_Inventory::getHeldItem(player)` for the weapon numbers and `getHeldState(player).instanceId` for the instance; calls `applyDurabilityLoss(player, instanceId, definition->durabilityLossPerUse)` after a successful attack; calls `spendLoadedShot` when a crossbow or firearm fires; spends a bow's arrow with `removeStack` at release.
- **R04**: `GA_Reload` calls `applyReload(player, weaponInstanceId)` on normal completion only; implement `IBPI_ActionService::cancelAction`.
- **R07** binds `onHeldItemChanged` and shows `getShownDefinition()` in the hand; `isWielded` tells a carried weapon from a wielded one.
- **B19 / R07** owners: put `UHeldItemComponent` on the real Character.

---
# SECTION 7 — Craft-Staging Reservations

**Goal:** an item staged for a craft stays in the bag but cannot be used, moved, dropped or thrown; a reservation can never be spent twice and never survives its session.
**Spec:** R01 3.6, §5 (`ReserveForStaging`, `ReleaseStagedReservation`, `ReleaseStagedItem`); Doc R02 3.3, 3.8, 3.10; Doc B22 §3.7 (the tests "Reserve and consume" and "Staged items are reserved").
**Depends on:** Section 6.

### The rules

- Staging moves quantity from *available* to *reserved* **without removing it** from the bag (R01 3.6). `getAvailableQuantity` is total minus reserved, and it is what **every** reader sees: the hotbar, Building's cost check, throwing, dropping, transferring and consuming (Index A103, S4-F29). Sections 3 and 5 already take only from the available part, so this section adds the reserve side.
- A stackable item is reserved by quantity; an instanced item is reserved by its instance id.
- A session's reservations are released when the session ends, however it ends. **Doc R02 decides when a session ends; this framework releases the reservation each time it is told.**
- On a co-op death the reservations are released **first**, and `detachBag` runs after, in the same server step, so a bag record never holds a reserved quantity (Index A103, S4-F28). Section 9 does that.
- Reservations are never saved and never replicated as such. What replicates is each entry's `reservedQuantity`, so the bag screen can show the available number.
- A weapon that is in the hand cannot be staged, and of several copies the most worn one is used first (Doc R02 3.10).
- Permanent items and a Portable Container that holds something cannot be staged (R01 3.10, 3.4a).

### S7.1 — Reservation members of `UInventoryData`

**Header additions** to `UInventoryData.h`:

```cpp
public:
    // ---- Section 7: staging reservations (R01 3.6). Server only ----
    FS_InventoryResult reserveStack(FName itemId, int32 quantity, const FGuid& stationId);
    FS_InventoryResult reserveInstance(const FGuid& instanceId, const FGuid& stationId);
    // Return staged units to available. Each answers how many units it released.
    int32 releaseItem(const FS_ItemRef& ref, int32 quantity);
    int32 releaseStation(const FGuid& stationId);
    int32 releaseAll();
    // The craft happened: reserved units of that station leave the inventory for good.
    FS_InventoryResult consumeReserved(const FGuid& stationId, const FS_ItemRef& ref, int32 quantity);
    const TArray<FS_StagedReservation>& getReservations() const;
    // The copy Crafting takes when it is given an item id: the most worn one that is not staged and not the excluded one.
    FGuid findMostWornInstance(FName itemId, const FGuid& excludedInstanceId) const;

    // C++ only. A staged stack spoiled, so its reservation is gone (R01 3.2a). Doc R02 refuses the craft.
    FD_StagedItemLost onStagedItemLost;

protected:
    // Gives back the units of one record and removes the record when it is empty.
    void releaseRecord(int32 recordIndex, int32 quantity);

    // Server only. Never saved, never replicated.
    UPROPERTY() TArray<FS_StagedReservation> reservations;
```

Append to `UInventoryData.cpp`:

```cpp
// ---------- Section 7: staging reservations ----------

FS_InventoryResult UInventoryData::reserveStack(FName itemId, int32 quantity, const FGuid& stationId){
    FS_InventoryEntry* stack = findStackMutable(itemId);
    if(stack == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(quantity < 1){
        return refusal(T_Inventory_FailReason_InvalidRequest);
    }
    if(stack->getAvailableQuantity() < quantity){
        return refusal(T_Inventory_FailReason_InsufficientQuantity);   // a reserved amount cannot be reserved twice
    }
    stack->reservedQuantity += quantity;
    FS_StagedReservation* record = reservations.FindByPredicate([itemId, &stationId](const FS_StagedReservation& candidate){
        return !candidate.instanceId.IsValid() && candidate.itemId == itemId && candidate.owningStationId == stationId;
    });
    if(record == nullptr){
        record = &reservations.AddDefaulted_GetRef();
        record->itemId = itemId;
        record->owningStationId = stationId;
    }
    record->reservedQuantity += quantity;
    notifyChanged();
    FS_InventoryResult result;
    result.quantity = quantity;
    return result;
}

FS_InventoryResult UInventoryData::reserveInstance(const FGuid& instanceId, const FGuid& stationId){
    FS_InventoryEntry* entry = findInstanceMutable(instanceId);
    if(entry == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(entry->reservedQuantity > 0){
        return refusal(T_Inventory_FailReason_ReservedQuantity);
    }
    entry->reservedQuantity = 1;
    FS_StagedReservation& record = reservations.AddDefaulted_GetRef();
    record.itemId = entry->itemId;
    record.instanceId = instanceId;
    record.reservedQuantity = 1;
    record.owningStationId = stationId;
    notifyChanged();
    FS_InventoryResult result;
    result.quantity = 1;
    result.instanceId = instanceId;
    return result;
}

void UInventoryData::releaseRecord(int32 recordIndex, int32 quantity){
    FS_StagedReservation& record = reservations[recordIndex];
    FS_InventoryEntry* entry = record.instanceId.IsValid() ? findInstanceMutable(record.instanceId) : findStackMutable(record.itemId);
    if(entry != nullptr){
        entry->reservedQuantity = FMath::Max(0, entry->reservedQuantity - quantity);
    }
    record.reservedQuantity -= quantity;
    if(record.reservedQuantity <= 0){
        reservations.RemoveAt(recordIndex);
    }
}

int32 UInventoryData::releaseItem(const FS_ItemRef& ref, int32 quantity){
    int32 released = 0;
    for(int32 index = reservations.Num() - 1; index >= 0 && released < quantity; index--){
        const FS_StagedReservation& record = reservations[index];
        const bool matches = ref.isInstance() ? record.instanceId == ref.instanceId : (!record.instanceId.IsValid() && record.itemId == ref.itemId);
        if(matches){
            const int32 amount = FMath::Min(quantity - released, record.reservedQuantity);
            releaseRecord(index, amount);
            released += amount;
        }
    }
    if(released > 0){
        notifyChanged();
    }
    return released;
}

int32 UInventoryData::releaseStation(const FGuid& stationId){
    int32 released = 0;
    for(int32 index = reservations.Num() - 1; index >= 0; index--){
        if(reservations[index].owningStationId == stationId){
            const int32 amount = reservations[index].reservedQuantity;
            releaseRecord(index, amount);
            released += amount;
        }
    }
    if(released > 0){
        notifyChanged();
    }
    return released;
}

int32 UInventoryData::releaseAll(){
    int32 released = 0;
    for(int32 index = reservations.Num() - 1; index >= 0; index--){
        const int32 amount = reservations[index].reservedQuantity;
        releaseRecord(index, amount);
        released += amount;
    }
    if(released > 0){
        notifyChanged();
    }
    return released;
}

FS_InventoryResult UInventoryData::consumeReserved(const FGuid& stationId, const FS_ItemRef& ref, int32 quantity){
    const int32 recordIndex = reservations.IndexOfByPredicate([&stationId, &ref](const FS_StagedReservation& record){
        const bool sameItem = ref.isInstance() ? record.instanceId == ref.instanceId : (!record.instanceId.IsValid() && record.itemId == ref.itemId);
        return sameItem && record.owningStationId == stationId;
    });
    if(recordIndex == INDEX_NONE || quantity < 1 || reservations[recordIndex].reservedQuantity < quantity){
        return refusal(T_Inventory_FailReason_InsufficientQuantity);   // only what this session staged can be consumed
    }
    // Everything checked. The record first, then the units themselves.
    FS_StagedReservation& record = reservations[recordIndex];
    record.reservedQuantity -= quantity;
    if(record.reservedQuantity <= 0){
        reservations.RemoveAt(recordIndex);
    }
    if(ref.isInstance()){
        entries.RemoveAll([&ref](const FS_InventoryEntry& entry){
            return entry.instanceId == ref.instanceId;
        });
    }else{
        FS_InventoryEntry* stack = findStackMutable(ref.itemId);
        stack->reservedQuantity -= quantity;
        stack->totalQuantity -= quantity;
        if(stack->totalQuantity <= 0){
            const FName itemId = ref.itemId;
            entries.RemoveAll([itemId](const FS_InventoryEntry& entry){
                return !entry.isInstanced() && entry.itemId == itemId;
            });
        }
    }
    notifyChanged();
    FS_InventoryResult result;
    result.quantity = quantity;
    result.instanceId = ref.instanceId;
    return result;
}

const TArray<FS_StagedReservation>& UInventoryData::getReservations() const{
    return reservations;
}

FGuid UInventoryData::findMostWornInstance(FName itemId, const FGuid& excludedInstanceId) const{
    const FS_InventoryEntry* mostWorn = nullptr;
    for(const FS_InventoryEntry& entry : entries){
        if(entry.isInstanced() && entry.itemId == itemId && entry.reservedQuantity == 0 && entry.instanceId != excludedInstanceId){
            if(mostWorn == nullptr || entry.state.durability < mostWorn->state.durability){
                mostWorn = &entry;
            }
        }
    }
    return mostWorn != nullptr ? mostWorn->instanceId : FGuid();
}
```

Two small edits to code from Section 3, so a wholesale replacement never leaves a stale record behind. In `replaceEntries` and in `clearAll`, add as the first line:

```cpp
    reservations.Reset();
```

### S7.2 — The functions Doc R02 calls

**Header additions** to `UBFL_Inventory.h`:

```cpp
    // ---- Section 7: craft staging (R01 3.6). Server only. Called by Doc R02 ----
    // R01 §5 ReserveForStaging. For an instanced item given by item id, the most worn copy is chosen and returned in instanceId.
    // An invalid stationId is a hand-craft session.
    static FS_InventoryResult reserveForStaging(APlayerState* player, const FS_ItemRef& ref, int32 quantity, const FGuid& stationId);
    // R01 §5 ReleaseStagedItem: one click returns one unit, Shift plus click the whole staged stack (Doc R02 UnstageIngredient).
    static FS_InventoryResult releaseStagedItem(APlayerState* player, const FS_ItemRef& ref, int32 quantity);
    // R01 §5 ReleaseStagedReservation: everything a station's session staged. Answers how many units were released.
    static int32 releaseStagedReservation(APlayerState* player, const FGuid& stationId);
    // Everything this player has staged anywhere: death, Downed, a scene, leaving the session.
    static int32 releaseAllStaged(APlayerState* player);
    // The craft completed: one recipe's worth of a staged ingredient is consumed (name PROPOSED, I12).
    static FS_InventoryResult consumeStaged(APlayerState* player, const FGuid& stationId, const FS_ItemRef& ref, int32 quantity);
    // Cleanup for Session.PlayerLeft (Doc B20 3.6): releases whatever this framework holds for the leaving player.
    static void handlePlayerLeft(APlayerState* player);
```

Append to `UBFL_Inventory.cpp`:

```cpp
// ---------- Section 7: craft staging ----------

FS_InventoryResult UBFL_Inventory::reserveForStaging(APlayerState* player, const FS_ItemRef& ref, int32 quantity, const FGuid& stationId){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(player);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(catalog == nullptr || services == nullptr || registry == nullptr || inventory == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    UInventoryData* bag = inventory->getPersonalInventory();
    if(bag == nullptr){
        return refusal(T_Inventory_FailReason_NoBag);   // staging draws only from the bag; a Bagless player cannot craft (R01 3.8)
    }
    bag->evaluateSpoilage(services->getGameTime());

    const FS_InventoryEntry* named = ref.isInstance() ? bag->findInstance(ref.instanceId) : nullptr;
    const FName itemId = named != nullptr ? named->itemId : ref.itemId;
    const UDA_ItemDefinition* definition = catalog->getDefinition(itemId);
    if(definition == nullptr || (ref.isInstance() && named == nullptr)){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(definition->isPermanent()){
        return refusal(T_Inventory_FailReason_Permanent);   // the Axe and the map are never staged (R01 3.10)
    }
    if(definition->isStackable()){
        return bag->reserveStack(itemId, quantity, stationId);
    }

    const FGuid wieldedId = getHeldState(player).instanceId;
    FGuid chosenId = ref.instanceId;
    if(!chosenId.IsValid()){
        chosenId = bag->findMostWornInstance(itemId, wieldedId);   // the most worn one first (Doc R02 3.10)
        if(!chosenId.IsValid()){
            // No free copy. Say why: the only one left is in the hand, or there is none.
            return refusal(bag->findInstance(wieldedId) != nullptr && bag->findInstance(wieldedId)->itemId == itemId ? T_Inventory_FailReason_ItemWielded : T_Inventory_FailReason_InsufficientQuantity);
        }
    }else if(chosenId == wieldedId){
        return refusal(T_Inventory_FailReason_ItemWielded);   // an equipped weapon cannot be staged
    }
    if(!registry->isContainerItemEmpty(*bag->findInstance(chosenId))){
        return refusal(T_Inventory_FailReason_ContainerNotEmpty);   // never a crafting ingredient while it holds something (R01 3.4a)
    }
    return bag->reserveInstance(chosenId, stationId);
}

FS_InventoryResult UBFL_Inventory::releaseStagedItem(APlayerState* player, const FS_ItemRef& ref, int32 quantity){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    if(bag == nullptr){
        return refusal(T_Inventory_FailReason_NoBag);
    }
    FS_InventoryResult result;
    result.quantity = bag->releaseItem(ref, quantity);
    if(result.quantity < 1){
        result.failReason = T_Inventory_FailReason_NotFound;   // nothing of that item was staged
    }
    return result;
}

int32 UBFL_Inventory::releaseStagedReservation(APlayerState* player, const FGuid& stationId){
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = (isServerWorld(player) && inventory != nullptr) ? inventory->getPersonalInventory() : nullptr;
    return bag != nullptr ? bag->releaseStation(stationId) : 0;
}

int32 UBFL_Inventory::releaseAllStaged(APlayerState* player){
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = (isServerWorld(player) && inventory != nullptr) ? inventory->getPersonalInventory() : nullptr;
    const int32 released = bag != nullptr ? bag->releaseAll() : 0;
    if(released > 0){
        INVENTORY_LOG(Log, player, "Released %d staged unit(s) of %s", released, *GetNameSafe(player));
    }
    return released;
}

FS_InventoryResult UBFL_Inventory::consumeStaged(APlayerState* player, const FGuid& stationId, const FS_ItemRef& ref, int32 quantity){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    if(bag == nullptr){
        return refusal(T_Inventory_FailReason_NoBag);
    }
    return bag->consumeReserved(stationId, ref, quantity);
}

void UBFL_Inventory::handlePlayerLeft(APlayerState* player){
    releaseAllStaged(player);
    // Section 12 adds one more line here: the container view of the leaving player is closed.
}
```

In `UPlayerInventoryComponent::EndPlay`, add as the first line inside the `if(isServerSide() && personalInventory != nullptr){` block:

```cpp
        UBFL_Inventory::handlePlayerLeft(getOwnerPlayerState());
```

### Verify (Section 7)

| Test | Expect |
| --- | --- |
| 10 sticks, `reserveForStaging(Stick × 4, stationA)` | accepted; `getTotalQuantity` 10, `getAvailableQuantity` **6**; the entry's `reservedQuantity` is 4 |
| Then `reserveForStaging(Stick × 7, stationA)` | refused `InsufficientQuantity`: a reserved amount cannot be spent twice |
| `removeStack(Stick, 7)`, `dropFromBag(Stick × 7)`, `transferEntry(… Stick × 7)` | the first two refused `ReservedQuantity`; the transfer moves only **6** |
| A hotbar slot bound to `Stick` | resolves to quantity 6 |
| `consumeStaged(stationA, Stick, 3)` | total 7, reserved 1, available 6 |
| `consumeStaged(stationA, Stick, 5)` | refused `InsufficientQuantity`: only 1 is still staged |
| `releaseStagedItem(Stick, 1)` | available 7, no reservation left |
| Stage 2 at station A and 3 at station B, `releaseStagedReservation(A)` | 2 released, 3 still reserved |
| `reserveForStaging(Spear by item id)` holding spear X (durability 10) in the hand and spears Y (40) and Z (25) in the bag | **Z** is staged: the most worn one that is not in the hand |
| `reserveForStaging` naming the spear in the hand | refused `ItemWielded` |
| `reserveForStaging(StarterAxe)`, `(Map)` | refused `Permanent` |
| `reserveForStaging` of a `Pouch` that holds a stick | refused `ContainerNotEmpty` |
| A staged spear: `dropFromBag`, `transferEntry`, `holdFromBag` | all refused `ReservedQuantity` |
| A Bagless player (Section 9) | refused `NoBag` |
| `releaseAllStaged` | everything available again; a second call releases 0 |

This table is the two Doc B22 starter tests "Reserve and consume" and "Staged items are reserved". The spec is written out in S13.5.

### Handoff

- **R02**: `StageIngredient` wraps `reserveForStaging`; `UnstageIngredient` wraps `releaseStagedItem`; `CloseCraftingSession` calls `releaseStagedReservation(player, stationId)`; a completed craft calls `consumeStaged` once per ingredient and then `grantOrDrop` for the output. Listen to `bag->onStagedItemLost` to refuse a craft whose staged stack spoiled (Section 8).
- **B02 / R03**: nothing to call for a death: `detachBag` releases the reservations itself (Section 9). For a player who becomes Downed, or when a scene starts, the owner of that state calls `UBFL_Inventory::releaseAllStaged(player)` unless Doc R02 already ended the session.
- **B20**: call `UBFL_Inventory::handlePlayerLeft(player)` on `Session.PlayerLeft`.
- **B01**: the cost check reads `getAvailableQuantity`, never the total.

---

# SECTION 8 — Spoilage, Consumables and Medicine

**Goal:** food ages by the game clock without any tick; eating, drinking and medicine turn item data into meter and health calls.
**Spec:** R01 3.2a, 3.9, §5 (`UseConsumable`); Doc B03 3.4; Doc R08 3.5; Doc R03 3.9; Doc02 §5, §19.4; decision D6.
**Depends on:** Section 7.

### The rules

- A stack of an item with `canSpoil` carries **one** absolute spoil time. It is evaluated **lazily, by the server only, never per tick**: only while the server processes a request on the stack (use, transfer, save, open, grant, pickup). A client never converts a stack; it shows what it is given. A sleep skip therefore needs nothing extra (decision D6).
- A stack that has reached its spoil time **becomes the same quantity of its spoiled item** (a separate item id). The conversion ignores the per-type limit of the spoiled item. A hotbar key stays on the original id. Any reservation on the converting stack is released and the craft that reserved it is refused by Doc R02 (Index A106, S3-F04).
- Merging keeps the earliest time; a split part keeps the time of the stack it came from (already in Sections 3 and 5).
- Eating a spoiled item or drinking unclean water still restores the meter, at a share of the fresh value (0.5), **can** make the player ill (a chance), and **always** costs HP (Index A96: the chance decides only the illness).
- The HP loss goes through the normal damage path with `DamageType.Illness`. That path, not this framework, applies the Peaceful floor and the refusal during a dialogue (R01 3.2a). With the Illness meter switched off, neither the illness nor the HP loss applies.
- Medicine heals through `ModifyHP` and may lower the Illness meter (R01 3.9).

### S8.1 — Fill in `evaluateSpoilage`

Replace the empty body in `UInventoryData.cpp` (add the includes `UBFL_Inventory.h` and `InventoryTime.h`):

```cpp
void UInventoryData::evaluateSpoilage(int64 now){
    if(!UBFL_Inventory::isServerWorld(this)){
        return;   // a client never converts a stack: it shows the state it is given (R01 3.2a)
    }
    // Collect first: converting changes the array.
    TArray<FS_InventoryEntry> due;
    for(const FS_InventoryEntry& entry : entries){
        if(!entry.isInstanced() && entry.spoilAtTime > 0 && now >= entry.spoilAtTime){
            due.Add(entry);
        }
    }
    for(const FS_InventoryEntry& fresh : due){
        const UDA_ItemDefinition* definition = findDefinition(fresh.itemId);
        const UDA_ItemDefinition* spoiledDefinition = definition != nullptr ? findDefinition(definition->spoiledItemId) : nullptr;
        if(spoiledDefinition == nullptr){
            // Bad data must never destroy food or stop the game: the stack simply stops ageing.
            findStackMutable(fresh.itemId)->spoilAtTime = 0;
            INVENTORY_LOG(Error, this, "%s has reached its spoil time but its spoiledItemId does not exist; the stack was left as it is", *fresh.itemId.ToString());
            continue;
        }
        // Reservations on the converting stack are released; Doc R02 hears it and refuses the craft (Index A106, S3-F04).
        for(int32 index = reservations.Num() - 1; index >= 0; index--){
            if(!reservations[index].instanceId.IsValid() && reservations[index].itemId == fresh.itemId){
                onStagedItemLost.Broadcast(reservations[index].owningStationId, fresh.itemId);
                reservations.RemoveAt(index);
            }
        }
        const FName freshId = fresh.itemId;
        entries.RemoveAll([freshId](const FS_InventoryEntry& entry){
            return !entry.isInstanced() && entry.itemId == freshId;
        });
        // The same quantity of the spoiled item, ignoring its per-type limit. A spoiled item that can itself spoil
        // starts its own clock at the moment the fresh one ran out.
        const int64 nextSpoilAt = spoiledDefinition->canSpoil ? fresh.spoilAtTime + InventoryTime::hoursToGameSeconds(spoiledDefinition->spoilTimeHours) : 0;
        addStackForced(spoiledDefinition->itemId, fresh.totalQuantity, nextSpoilAt);   // raises the change
        INVENTORY_LOG(Log, this, "%d %s spoiled into %s in inventory %s", fresh.totalQuantity, *fresh.itemId.ToString(), *spoiledDefinition->itemId.ToString(), *inventoryId.ToString());
    }
}
```

> **Where it is called.** Every library function that touches a stack already calls `evaluateSpoilage` first: `grantOrDrop`, `grantEntryOrDrop`, `pickUpLooseItem`, `takeOutOfBag`, `transferEntry`, `reserveForStaging`, and below `useConsumable`. Section 11 calls it before a save and Section 12 when a container is opened. No timer, no tick (Doc02 §5).

### S8.2, S8.3 — Using a consumable

**Header additions** to `UBFL_Inventory.h`:

```cpp
    // ---- Section 8 ----
    // The effects a consumable really has: its own entries plus, for a variant, the scaled meter values of its base item.
    static void buildConsumeEffects(const UObject* worldContext, const UDA_ItemDefinition& definition, TArray<FS_ConsumeEffect>& outEffects);
    // R01 §5 UseConsumable. Server. Eats, drinks or uses one unit from the bag.
    static FS_InventoryResult useConsumable(APlayerState* player, FName itemId);
    // Server. Applies an item's effects to a player without touching any inventory (shared with the Bagless hold slots).
    static void applyConsumeEffects(APlayerState* player, const UDA_ItemDefinition& definition);
```

Append to `UBFL_Inventory.cpp`:

```cpp
// ---------- Section 8: consumables ----------

void UBFL_Inventory::buildConsumeEffects(const UObject* worldContext, const UDA_ItemDefinition& definition, TArray<FS_ConsumeEffect>& outEffects){
    outEffects.Reset();
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(worldContext);
    if(catalog == nullptr){
        return;
    }
    // A variant derives its meter values from a base item (R01 3.2a, 3.4):
    //  - unclean water names its base itself and scales by its own uncleanWaterEffectiveness;
    //  - spoiled food is found through the fresh item that names it, and scales by that item's spoiledEffectiveness.
    const UDA_ItemDefinition* base = nullptr;
    float scale = 1.0f;
    if(!definition.effectBaseItemId.IsNone()){
        base = catalog->getDefinition(definition.effectBaseItemId);
        scale = definition.uncleanWaterEffectiveness;
    }else{
        base = catalog->findFreshItemFor(definition.itemId);
        scale = base != nullptr ? base->spoiledEffectiveness : 1.0f;
    }
    if(base != nullptr){
        for(const FS_ConsumeEffect& effect : base->consumeEffects){
            if(effect.kind == E_ConsumeEffectKind::ModifyMeter){
                FS_ConsumeEffect scaled = effect;
                scaled.amount *= scale;   // fresh meat gives 100 hunger, spoiled meat gives 50 (the designer's rule)
                outEffects.Add(scaled);
            }
        }
    }
    outEffects.Append(definition.consumeEffects);   // the item's own entries: for a variant, the illness chance and the HP loss
}

void UBFL_Inventory::applyConsumeEffects(APlayerState* player, const UDA_ItemDefinition& definition){
    const UInventoryServices* services = UInventoryServices::get(player);
    if(services == nullptr){
        return;
    }
    TArray<FS_ConsumeEffect> effects;
    buildConsumeEffects(player, definition, effects);
    const bool isIllnessActive = services->isIllnessActive(player);
    for(const FS_ConsumeEffect& effect : effects){
        switch(effect.kind){
            case E_ConsumeEffectKind::ModifyMeter:{
                services->modifyMeter(player, effect.targetTag, effect.amount, definition.categoryTag);
                break;
            }
            case E_ConsumeEffectKind::IllnessChance:{
                // The chance decides whether the illness source is registered (Doc B03 3.4, Index A96).
                if(isIllnessActive && services->rollChance(effect.chance)){
                    services->registerIllnessSource(player, effect.targetTag, effect.sourceDurationHours);
                }
                break;
            }
            case E_ConsumeEffectKind::HealthLoss:{
                // Every time, not by the chance. Through the normal damage path, which applies the Peaceful floor
                // and refuses during a dialogue (R01 3.2a, Doc R03 3.9).
                if(isIllnessActive && effect.amount > 0.0f){
                    services->damagePlayer(player, effect.amount, T_DamageType_Illness);
                }
                break;
            }
        }
    }
    // Medicine (R01 3.9): the amounts are data on the definition.
    if(definition.healthRestoreAmount > 0.0f){
        services->healPlayer(player, definition.healthRestoreAmount, T_Stats_Source_Medicine);
    }
    if(definition.illnessReductionAmount > 0.0f){
        services->modifyMeter(player, T_Survival_Meter_Illness, -definition.illnessReductionAmount, T_Stats_Source_Medicine);
    }
}

FS_InventoryResult UBFL_Inventory::useConsumable(APlayerState* player, FName itemId){
    if(!isServerWorld(player)){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(catalog == nullptr || services == nullptr || inventory == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    UInventoryData* bag = inventory->getPersonalInventory();
    if(bag == nullptr){
        return refusal(T_Inventory_FailReason_NoBag);   // a Bagless player eats from a hold slot (Section 9)
    }
    // FIRST: a stack past its spoil time is the spoiled item from now on. So an eat at the exact boundary
    // uses the server's answer, and a key bound to the fresh item never eats the spoiled one (R01 3.2a, 3.5).
    bag->evaluateSpoilage(services->getGameTime());

    const UDA_ItemDefinition* definition = catalog->getDefinition(itemId);
    if(definition == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(!definition->isConsumable() || !definition->isStackable()){
        return refusal(T_Inventory_FailReason_NotConsumable);
    }
    const FS_InventoryResult removed = bag->removeStack(itemId, 1);   // only an available unit can be consumed
    if(!removed.succeeded()){
        return removed;
    }
    applyConsumeEffects(player, *definition);
    INVENTORY_LOG(Log, player, "%s consumed %s", *GetNameSafe(player), *itemId.ToString());
    return removed;
}
```

> **Who says a player "has taken medicine"?** Doc R08: a player counts as having taken medicine while the medicine's own timed effect is running (R08 item 31 b). That state belongs to Stats; this framework only calls `healPlayer` with the source `Stats.Source.Medicine`. Medicine is not limited by the regeneration limit, which is also R08's rule (PROPOSED there).

### S8.4 — The data check for variants

A spoiled or unclean variant must not list its own `ModifyMeter` entries, or it would give the meter value twice. This needs two assets at once, so it is one of the catalog checks of Section 10 (S10.4).

### Verify (Section 8)

Use the fixture: `fixture.setGameTime(...)` and `fixture.advanceHours(...)` move the stub clock. Give `RawMeat` the fields of S2.14 (`canSpoil`, 24 hours, `SpoiledMeat`) on the fixture's definitions.

| Test | Expect |
| --- | --- |
| Clock at 0, `grantOrDrop(RawMeat × 3)` | one stack with `spoilAtTime` 86 400 (24 game hours in seconds) |
| Advance 23 hours, `useConsumable(RawMeat)` | accepted; the survival stub received Hunger **100**; health stub untouched; 2 meat left |
| Advance to exactly 24 hours, `useConsumable(RawMeat)` | refused `NotFound` or `InsufficientQuantity` for the fresh id: the stack **is** `SpoiledMeat × 2` now. `Inventory.Changed` was raised |
| `useConsumable(SpoiledMeat)` | Hunger **50** (half); the health stub received **10** with `DamageType.Illness`; with seed 1 the illness source is registered or not, and the same seed gives the same answer every run |
| The same with `illnessActive = false` on the survival stub | Hunger 50; **no** damage and **no** illness source |
| Grant 2 meat at hour 0 and 2 more at hour 10 | one stack of 4 with the spoil time of the **first** grant |
| `grantOrDrop(RawMeat × 1, spoilStartTime = now − 30 hours)` | stored as `RawMeat`, and it is `SpoiledMeat` at the next request on the bag |
| `grantOrDrop` with a `spoilStartTime` in the future | clamped to now |
| Stage 2 `RawMeat`, pass the spoil time, then any request on the bag | the reservation is gone; `onStagedItemLost` was raised with the station id; the bag holds `SpoiledMeat × 2`, none reserved |
| `SpoiledMeat` at its per-type limit, then a `RawMeat` stack spoils | the spoiled stack goes **over** the limit; nothing is lost or dropped |
| Slot 1 bound to `RawMeat`, the stack spoils | `resolveHotbarSlot(1)` is not usable; the binding is still `RawMeat` |
| `useConsumable(UncleanWater)` | Thirst **50**; 10 damage; the illness roll |
| `useConsumable(Herb)` | the health stub healed 20 with `Stats.Source.Medicine`; the Illness meter received −10 |
| `useConsumable(Stick)` | refused `NotConsumable` |
| A client copy of a bag whose stack is past its time | unchanged until the server touches the stack |
| A sleep skip (move the clock forward 10 hours in one step) | nothing runs at the skip; the next request converts what is due |

This is the Doc B22 starter test "Spoiled food" (tier 2). It is written out in S13.5.

### Handoff

- **B03** implements `IBPI_SurvivalService` (`modifyMeter`, `registerIllnessSource`, `isIllnessActive`) and owns the meter and illness-source tags the item assets use. **R08** and **R03** implement `IBPI_HealthService`.
- **R12** implements `IBPI_GameClock`. Nothing here subscribes to the clock.
- **B10**: an output whose item can spoil is granted with `grantOrDrop(player, stack, jobFinishTime)`; boiling unclean water is a B10 job whose output is `CleanWater`.
- **B22**: the fixed seed for the illness roll is the cvar `Awaken.Inventory.RandomSeed`, read when the game instance starts.

---
# SECTION 9 — The Bag Lifecycle: Detach, Bagless, Recover, Starting Kit

**Goal:** the bag can be left in the world and taken back in one atomic step each; a Bagless player holds at most three items; a new character gets its kit exactly once.
**Spec:** R01 3.8, 3.10, 3.11, §5 (`HasBag`, `GetUsableHotbarSlotCount`, `DetachBag`, `ReattachBag`, `DiscardBaglessItems`); Doc B02 3.2, 3.6, 4.3, 10.2; Doc R13 3.6, 3.8; Doc R05 3.3.
**Depends on:** Section 8.

### Files

```
Public/ItemInventoryFramework/UBFL_InventoryBag.h
Private/ItemInventoryFramework/UBFL_InventoryBag.cpp
Public/ItemInventoryFramework/UPlayerInventoryComponent.h   (additions)
Public/ItemInventoryFramework/UInventorySettings.h          (addition)
```

### Who does what

| Piece | Owner | In this guide |
| --- | --- | --- |
| Deciding that a death leaves the bag, where it may lie (the valid-spot rule), the retry limit, the owner's pick-up hold | Doc B02 | B02 calls `detachBag(player, location)` and `reattachBag(player, bagId)` |
| The bag's actor in the world and the registry of bag records | Doc B02 | behind the seam `IBPI_BagActorService` (stub: records only, no actor) |
| The saved record (`S_BagRecord`) | Doc R13 | behind the same seam |
| Worn armor leaving with the bag and coming back | Doc R05 | behind the seam `IBPI_EquipmentGear` (stub: nothing is worn) |
| **The move itself, the Bagless state, the hold slots, the starting kit** | **Doc R01** | this section |

### The Bagless state (R01 3.8), as code will enforce it

1. There is no Personal Inventory: `getPersonalInventory()` is null, so nothing can be stored, staged, bound, dropped from a bag or reloaded.
2. Only hotbar keys 1, 2 and 3 work. Each is a **hold slot** that holds one physical item directly: one item per slot, never a stack, at most three.
3. The player can still pick items up, into a free hold slot. A weapon in a hold slot is shown in the hand but **not wielded**: a Bagless player fights Unarmed (Index A110, Q-S2-10).
4. A Bagless player may eat, drink or use medicine from a hold slot (Index A110, S2-F33).
5. Hold-slot items are never saved, replicate to their owner only, are added to the bag on recovery, and are destroyed if the player dies Bagless.
6. Food in a hold slot does not spoil while held: the time left is kept when the item enters the slot and the spoil time is rebuilt when it leaves (R01 amendment).
7. Worn clothes are not part of the bag and stay on the player. That is Doc R05's state; nothing here touches it.

### S9.1 — Hold slots and the kit flag on `UPlayerInventoryComponent`

**Header additions** to `UPlayerInventoryComponent.h`:

```cpp
public:
    // ---- Bagless hold slots (R01 3.8). Hold index 0, 1, 2 is hotbar key 1, 2, 3 ----
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") TArray<FS_BaglessHoldSlot> getHoldSlots() const;
    // INDEX_NONE when all three are full.
    int32 findFreeHoldSlot() const;
    // Server. Puts ONE unit into an empty hold slot and freezes its spoil clock.
    bool putInHoldSlot(int32 holdIndex, const FS_InventoryEntry& unit);
    // Server. Takes the item out and rebuilds its spoil time from the clock. False when the slot is empty.
    bool takeFromHoldSlot(int32 holdIndex, FS_InventoryEntry& outEntry);
    // Server. Destroys what the hold slots hold. Answers how many items were destroyed.
    int32 clearHoldSlots();

    // ---- starting kit (R01 3.8) ----
    bool isKitGranted() const;
    void setKitGranted(bool newValue);

protected:
    UFUNCTION()
    void OnRep_holdSlots();

    // Owner only. Never saved: the host keeps them in the player's held state while the session runs (Index A110, S2-F30).
    UPROPERTY(ReplicatedUsing = OnRep_holdSlots) TArray<FS_BaglessHoldSlot> holdSlots;
    // Host only. Saved with the Character (Section 11), so a kit is never granted twice.
    UPROPERTY() bool kitGranted = false;
```

In `GetLifetimeReplicatedProps` add:

```cpp
    DOREPLIFETIME_CONDITION(UPlayerInventoryComponent, holdSlots, COND_OwnerOnly);
```

In `BeginPlay`, inside the `if(isServerSide()){` block added in Section 6:

```cpp
        holdSlots.SetNum(UInventorySettings::get()->baglessSlotCount);
```

Append to `UPlayerInventoryComponent.cpp` (add the include `InventoryServices.h`):

```cpp
// ---------- Section 9: Bagless hold slots ----------

TArray<FS_BaglessHoldSlot> UPlayerInventoryComponent::getHoldSlots() const{
    return holdSlots;
}

int32 UPlayerInventoryComponent::findFreeHoldSlot() const{
    return holdSlots.IndexOfByPredicate([](const FS_BaglessHoldSlot& slot){
        return !slot.isOccupied;
    });
}

bool UPlayerInventoryComponent::putInHoldSlot(int32 holdIndex, const FS_InventoryEntry& unit){
    if(!isServerSide() || !holdSlots.IsValidIndex(holdIndex) || holdSlots[holdIndex].isOccupied){
        return false;
    }
    const UInventoryServices* services = UInventoryServices::get(this);
    FS_BaglessHoldSlot& slot = holdSlots[holdIndex];
    slot.isOccupied = true;
    slot.entry = unit;
    slot.entry.totalQuantity = 1;      // exactly one item per slot, no stacking
    slot.entry.reservedQuantity = 0;
    // Frozen in the hand: keep the time LEFT, not the time on the clock (R01 amendment).
    slot.hasSpoilClock = unit.spoilAtTime > 0;
    slot.timeToSpoil = slot.hasSpoilClock && services != nullptr ? FMath::Max<int64>(0, unit.spoilAtTime - services->getGameTime()) : 0;
    slot.entry.spoilAtTime = 0;
    raiseHotbarChanged(holdIndex + 1);
    GetOwner()->ForceNetUpdate();
    notifyChanged();
    return true;
}

bool UPlayerInventoryComponent::takeFromHoldSlot(int32 holdIndex, FS_InventoryEntry& outEntry){
    if(!isServerSide() || !holdSlots.IsValidIndex(holdIndex) || !holdSlots[holdIndex].isOccupied){
        return false;
    }
    const UInventoryServices* services = UInventoryServices::get(this);
    outEntry = holdSlots[holdIndex].entry;
    if(holdSlots[holdIndex].hasSpoilClock && services != nullptr){
        outEntry.spoilAtTime = services->getGameTime() + holdSlots[holdIndex].timeToSpoil;   // the clock starts again
    }
    holdSlots[holdIndex] = FS_BaglessHoldSlot();
    raiseHotbarChanged(holdIndex + 1);
    GetOwner()->ForceNetUpdate();
    notifyChanged();
    UHeldItemComponent* heldItem = UHeldItemComponent::findFor(getOwnerPlayerState());
    if(heldItem != nullptr){
        heldItem->refreshFromInventory();   // if it was shown in the hand, the hand empties
    }
    return true;
}

int32 UPlayerInventoryComponent::clearHoldSlots(){
    int32 destroyed = 0;
    for(int32 index = 0; index < holdSlots.Num(); index++){
        FS_InventoryEntry discarded;
        if(takeFromHoldSlot(index, discarded)){
            destroyed++;
        }
    }
    return destroyed;
}

bool UPlayerInventoryComponent::isKitGranted() const{
    return kitGranted;
}

void UPlayerInventoryComponent::setKitGranted(bool newValue){
    if(isServerSide()){
        kitGranted = newValue;
    }
}

void UPlayerInventoryComponent::OnRep_holdSlots(){
    notifyChanged();
}
```

### S9.2 — Make the earlier code Bagless-aware (three small replacements)

**(a) The hotbar resolves a hold slot.** In `UPlayerInventoryComponent::resolveHotbarSlot`, replace

```cpp
    if(bag == nullptr){
        return resolved;   // Bagless: Section 9 fills this branch with the hold slots
    }
```

with

```cpp
    if(bag == nullptr){
        // Bagless: key 1, 2 or 3 holds one physical item directly (R01 3.8, the Bagless holding mode).
        const int32 holdIndex = slotIndex - 1;
        if(holdSlots.IsValidIndex(holdIndex) && holdSlots[holdIndex].isOccupied){
            resolved.itemId = holdSlots[holdIndex].entry.itemId;
            resolved.instanceId = holdSlots[holdIndex].entry.instanceId;
            resolved.availableQuantity = 1;
            resolved.isUsable = true;
        }
        return resolved;
    }
```

`selectHotbarSlot` (Section 6) then shows that item in the hand with `isWielded = false`, because the player has no bag. Other players see it there when the owner presses the key (Doc B17, Index A109).

**(b) The hand follows the hold slot.** In `UHeldItemComponent::refreshFromInventory`, replace

```cpp
    if(bag == nullptr){
        putAway();   // Section 9 adds the Bagless hold slots before this line
        return;
    }
```

with

```cpp
    if(bag == nullptr){
        // Bagless: the hand shows a hold slot. Keep it only while that slot still holds the same item.
        const FS_ResolvedHotbarSlot shown = inventory != nullptr ? inventory->resolveHotbarSlot(held.sourceSlot) : FS_ResolvedHotbarSlot();
        if(!shown.isUsable || shown.itemId != held.itemId || shown.instanceId != held.instanceId || held.isWielded){
            putAway();
        }
        return;
    }
```

**(c) A Bagless player picks up into a hold slot.** In `UBFL_Inventory::pickUpLooseItem`, replace

```cpp
    if(!inventory->hasBag()){
        return refusal(T_Inventory_FailReason_NoBag);   // Section 9 replaces this line with the Bagless hold slots
    }
```

with

```cpp
    if(!inventory->hasBag()){
        // One item into a free hold slot; a slot never takes a stack, so the rest of a loose stack stays (R01 3.8, 3.11).
        UPlayerInventoryComponent* holder = UPlayerInventoryComponent::findFor(player);
        const int32 holdIndex = holder->findFreeHoldSlot();
        if(holdIndex == INDEX_NONE){
            return refusal(T_Interaction_FailReason_HandsFull);   // a fourth pickup is refused with a short reason (I8)
        }
        FS_InventoryEntry unit = entry;
        unit.totalQuantity = 1;
        holder->putInHoldSlot(holdIndex, unit);
        looseItem->reduceQuantity(1);
        result.quantity = 1;
        result.instanceId = unit.instanceId;
        return result;
    }
```

A grant is different from a pickup: `grantOrDrop` to a Bagless player still drops everything (R01 3.7), which Section 5 already does because the bag is null.

### S9.3 — The starting kit in the settings

**Header addition** to `UInventorySettings.h` (add `#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"`):

```cpp
    // ---- starting kit (R01 3.8): what a new character's bag holds. The bag itself is created with it ----
    UPROPERTY(Config, EditAnywhere, Category = "Starting Kit") TArray<FS_ItemStack> startingKit;
```

and in the constructor in `UInventorySettings.cpp`, the designer-confirmed kit as the default (data, so it can change without code):

```cpp
    FS_ItemStack axe;
    axe.itemId = TEXT("StarterAxe");
    FS_ItemStack map;
    map.itemId = TEXT("Map");
    startingKit = {axe, map};
```

### S9.4 to S9.8 — `UBFL_InventoryBag`

`UBFL_InventoryBag.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UBFL_InventoryBag.generated.h"

class APlayerState;

UCLASS()
class AWAKEN_API UBFL_InventoryBag : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // ---- queries ----
    // R01 §5 HasBag: true while the Personal Inventory exists.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") static bool hasBag(const APlayerState* player);
    // R01 §5 GetUsableHotbarSlotCount: the full hotbar with the bag, 3 while Bagless.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") static int32 getUsableHotbarSlotCount(const APlayerState* player);

    // ---- commands, server only ----
    // R01 §5 DetachBag. Atomic: everything moves or nothing does. Called by Doc B02 with the spot the bag will lie at.
    static FS_BagDetachResult detachBag(APlayerState* player, const FVector& location);
    // R01 §5 ReattachBag. Atomic. Called by Doc B02 when the owner's pick-up hold completes.
    static FS_InventoryResult reattachBag(APlayerState* player, const FGuid& bagId);
    // R01 §5 DiscardBaglessItems: a player died Bagless, so what the hold slots held is destroyed.
    static int32 discardBaglessItems(APlayerState* player);
    // A Bagless player eats, drinks or uses medicine from a hold slot (Index A110, S2-F33).
    static FS_InventoryResult useConsumableFromHoldSlot(APlayerState* player, int32 holdIndex);
    // A Bagless player puts a held item down as a loose item (I19).
    static FS_InventoryResult dropHoldSlot(APlayerState* player, int32 holdIndex);
    // The bag, the Axe and the map, once per character (R01 3.8). A second call is refused with KitAlreadyGranted.
    static FS_InventoryResult grantStartingKit(APlayerState* player);
    // A fresh start, Start Again, or a character given by the system: the old character's bags are removed for good.
    static void startNewCharacter(APlayerState* player, const FGuid& newCharacterId);

private:
    // Tests only: lets a test make step 1 or step 2 of detachBag fail.
    static bool shouldFailStep(int32 step);
};
```

`UBFL_InventoryBag.cpp`

```cpp
#include "ItemInventoryFramework/UBFL_InventoryBag.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "ItemInventoryFramework/03Interfaces/BPI_BagActorService.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
// Tests only: 1 or 2 makes that step of detachBag fail, to prove that nothing moves (R01 3.8). 0 = off.
static TAutoConsoleVariable<int32> cvarFailBagStep(TEXT("Awaken.Inventory.FailBagStep"), 0, TEXT("Make step 1 (create the bag) or step 2 (release worn gear) of detachBag fail. Tests only. 0 = off."), ECVF_Cheat);
#endif

bool UBFL_InventoryBag::shouldFailStep(int32 step){
#if !UE_BUILD_SHIPPING
    return cvarFailBagStep.GetValueOnGameThread() == step;
#else
    return false;
#endif
}

// ---------- queries ----------

bool UBFL_InventoryBag::hasBag(const APlayerState* player){
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    return inventory != nullptr && inventory->hasBag();
}

int32 UBFL_InventoryBag::getUsableHotbarSlotCount(const APlayerState* player){
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    return inventory != nullptr ? inventory->getUsableHotbarSlotCount() : 0;
}

// ---------- S9.4 detachBag ----------

FS_BagDetachResult UBFL_InventoryBag::detachBag(APlayerState* player, const FVector& location){
    FS_BagDetachResult result;
    if(!UBFL_Inventory::isServerWorld(player)){
        result.failReason = T_Inventory_FailReason_NotServer;
        return result;
    }
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    UObject* bagService = services != nullptr ? services->getBagService() : nullptr;
    if(inventory == nullptr || bagService == nullptr){
        result.failReason = T_Inventory_FailReason_NotFound;
        return result;
    }
    if(!inventory->hasBag()){
        result.failReason = T_Inventory_FailReason_NoBag;   // dying Bagless: Doc B02 calls discardBaglessItems instead
        return result;
    }

    // Before anything moves, the staged reservations are released, so the record never holds a reserved quantity
    // (Index A103, S4-F28). This is safe even if a later step fails: the units are simply available again.
    UBFL_Inventory::releaseAllStaged(player);

    // Step 1: the bag record and its actor are created first (Doc B02 3.6).
    const FGuid bagId = FGuid::NewGuid();
    if(shouldFailStep(1) || !IBPI_BagActorService::Execute_createBag(bagService, bagId, inventory->getCharacterId(), location)){
        IBPI_BagActorService::Execute_deleteBag(bagService, bagId);
        result.failReason = T_Inventory_FailReason_BagCreateFailed;
        return result;
    }

    // Step 2: worn armor goes back to the bag record (Doc R05 3.3). The injected failure happens BEFORE the call,
    // because a real release empties the worn stack and could not be undone.
    FS_GearReleaseResult gear;
    gear.succeeded = false;
    if(!shouldFailStep(2)){
        gear = services->releaseEquippedGear(player);
    }
    if(!gear.succeeded){
        IBPI_BagActorService::Execute_deleteBag(bagService, bagId);   // the record and the actor created in step 1 are deleted
        result.failReason = T_Inventory_FailReason_GearReleaseFailed;
        return result;
    }

    // Step 3: the move. Nothing below can fail: it is memory only, in one server step.
    FS_BagContents contents;
    contents.hotbar = inventory->getHotbarBindings();                 // the snapshot that recovery puts back exactly
    contents.gear = gear.gearSlots;
    const FS_HeldItem held = UBFL_Inventory::getHeldState(player);    // read before the bag leaves and the hand empties
    contents.wielded.itemId = held.itemId;
    contents.wielded.instanceId = held.instanceId;

    UInventoryData* bag = inventory->releaseInventory();              // the player is Bagless from here; the hand is empty
    for(const FS_InventoryEntry& piece : gear.gearEntries){
        bag->addInstanceForced(piece);                                // limits are ignored so no piece can be lost
    }
    bag->setDeathBag(true);                                           // nobody takes from it or stores into it (R01 3.4)
    contents.inventoryId = bag->getInventoryId();
    inventory->setHotbarBindings(TArray<FS_HotbarBinding>());         // the Bagless holding mode has no bindings
    IBPI_BagActorService::Execute_setBagContents(bagService, bagId, contents);

    result.bagId = bagId;
    INVENTORY_LOG(Log, player, "Bag %s of %s detached at %s with %d entries and %d worn piece(s); the player is Bagless", *bagId.ToString(), *GetNameSafe(player), *location.ToCompactString(), bag->getEntries().Num(), gear.gearSlots.Num());
    return result;
}

// ---------- S9.5 reattachBag ----------

FS_InventoryResult UBFL_InventoryBag::reattachBag(APlayerState* player, const FGuid& bagId){
    if(!UBFL_Inventory::isServerWorld(player)){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotServer);
    }
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(player);
    UObject* bagService = services != nullptr ? services->getBagService() : nullptr;
    if(inventory == nullptr || registry == nullptr || bagService == nullptr){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    if(inventory->hasBag()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_AlreadyHasBag);
    }
    const FS_BagInfo info = IBPI_BagActorService::Execute_findBag(bagService, bagId);
    if(!info.isValid){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    if(info.ownerCharacterId != inventory->getCharacterId()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotBagOwner);   // a bag belongs to a character (Index A93)
    }
    UInventoryData* bag = registry->findInventory(info.contents.inventoryId);
    if(bag == nullptr){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }

    // Everything checked. All four steps below happen in this one server step, or none did (R01 3.8).
    // (1) The Personal Inventory is back.
    TArray<FS_InventoryEntry> heldItems;
    for(int32 holdIndex = 0; holdIndex < inventory->getHoldSlots().Num(); holdIndex++){
        FS_InventoryEntry heldEntry;
        if(inventory->takeFromHoldSlot(holdIndex, heldEntry)){
            heldItems.Add(heldEntry);   // taken out first: the slots become bindings again in step 3
        }
    }
    bag->setDeathBag(false);
    inventory->attachInventory(bag);

    // (2) Each held item is added to the bag; what does not fit drops next to the bag. A stack that fits in part goes in in part.
    int32 droppedQuantity = 0;
    for(const FS_InventoryEntry& heldEntry : heldItems){
        droppedQuantity += UBFL_Inventory::grantEntryOrDrop(player, heldEntry).quantityDropped;
    }

    // (3) The hotbar bindings return exactly as they were at death: a sword that was on key 1 is on key 1 again.
    inventory->setHotbarBindings(info.contents.hotbar);

    // (4) The worn gear is re-equipped in the places it occupied (Doc B02); Equipment takes each piece out of the bag again.
    services->reEquipGear(player, info.contents.gear);
    UHeldItemComponent* heldItem = UHeldItemComponent::findFor(player);
    if(heldItem != nullptr && !info.contents.wielded.itemId.IsNone()){
        heldItem->holdFromBag(info.contents.wielded);   // the weapon that was in the hand is in the hand again (I18)
    }

    IBPI_BagActorService::Execute_deleteBag(bagService, bagId);   // the record and the actor end; the inventory lives on as the bag
    FS_InventoryResult result;
    result.quantity = droppedQuantity;
    INVENTORY_LOG(Log, player, "Bag %s recovered by %s: %d held item(s) merged, %d unit(s) dropped beside it, %d piece(s) re-equipped", *bagId.ToString(), *GetNameSafe(player), heldItems.Num(), droppedQuantity, info.contents.gear.Num());
    return result;
}

// ---------- S9.6 Bagless helpers ----------

int32 UBFL_InventoryBag::discardBaglessItems(APlayerState* player){
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(!UBFL_Inventory::isServerWorld(player) || inventory == nullptr || inventory->hasBag()){
        return 0;
    }
    const int32 destroyed = inventory->clearHoldSlots();
    INVENTORY_LOG(Log, player, "%s died Bagless: %d held item(s) destroyed", *GetNameSafe(player), destroyed);
    return destroyed;
}

FS_InventoryResult UBFL_InventoryBag::useConsumableFromHoldSlot(APlayerState* player, int32 holdIndex){
    if(!UBFL_Inventory::isServerWorld(player)){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotServer);
    }
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    if(inventory == nullptr || catalog == nullptr || services == nullptr){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    const TArray<FS_BaglessHoldSlot> slots = inventory->getHoldSlots();
    if(inventory->hasBag() || !slots.IsValidIndex(holdIndex) || !slots[holdIndex].isOccupied){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_InvalidSlot);
    }
    const UDA_ItemDefinition* definition = catalog->getDefinition(slots[holdIndex].entry.itemId);
    if(definition == nullptr || !definition->isConsumable()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotConsumable);
    }
    FS_InventoryEntry eaten;
    inventory->takeFromHoldSlot(holdIndex, eaten);   // the item leaves the slot; its spoil time is rebuilt from the clock
    // A ration whose time ran out before it was picked up is the spoiled item, never a fresh one (R01 amendment).
    if(definition->canSpoil && eaten.spoilAtTime > 0 && services->getGameTime() >= eaten.spoilAtTime){
        const UDA_ItemDefinition* spoiled = catalog->getDefinition(definition->spoiledItemId);
        definition = spoiled != nullptr ? spoiled : definition;
    }
    UBFL_Inventory::applyConsumeEffects(player, *definition);
    INVENTORY_LOG(Log, player, "%s consumed %s from hold slot %d", *GetNameSafe(player), *definition->itemId.ToString(), holdIndex);
    FS_InventoryResult result;
    result.quantity = 1;
    return result;
}

FS_InventoryResult UBFL_InventoryBag::dropHoldSlot(APlayerState* player, int32 holdIndex){
    if(!UBFL_Inventory::isServerWorld(player)){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotServer);
    }
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    FS_InventoryEntry dropped;
    if(inventory == nullptr || services == nullptr || inventory->hasBag() || !inventory->takeFromHoldSlot(holdIndex, dropped)){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_InvalidSlot);
    }
    services->spawnLooseItem(player, dropped, UBFL_Inventory::getDropLocation(player));
    FS_InventoryResult result;
    result.quantity = 1;
    return result;
}

// ---------- S9.7 starting kit ----------

FS_InventoryResult UBFL_InventoryBag::grantStartingKit(APlayerState* player){
    if(!UBFL_Inventory::isServerWorld(player)){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotServer);
    }
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(inventory == nullptr){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    if(inventory->isKitGranted()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_KitAlreadyGranted);   // granted once, when the character is created
    }
    inventory->createFreshBag();   // the kit is the bag, with the Axe and the map inside it; does nothing if a bag exists
    FS_InventoryResult result;
    for(const FS_ItemStack& stack : UInventorySettings::get()->startingKit){
        result.quantity += UBFL_Inventory::grantOrDrop(player, stack).quantityStored;
    }
    if(result.quantity < 1){
        // Marked as granted only when at least one item arrived, so a broken data setup can be fixed and tried again.
        INVENTORY_LOG(Error, player, "The starting kit of %s is empty or names items that do not exist; check Project Settings -> Awaken Inventory", *GetNameSafe(player));
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    inventory->setKitGranted(true);
    INVENTORY_LOG(Log, player, "Starting kit granted to %s: %d item(s)", *GetNameSafe(player), result.quantity);
    return result;
}

// ---------- S9.8 fresh character ----------

void UBFL_InventoryBag::startNewCharacter(APlayerState* player, const FGuid& newCharacterId){
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UInventoryServices* services = UInventoryServices::get(player);
    UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(player);
    if(!UBFL_Inventory::isServerWorld(player) || inventory == nullptr || services == nullptr || registry == nullptr){
        return;
    }
    UBFL_Inventory::releaseAllStaged(player);

    // The old character's bags are removed completely: nobody else can pick them up and the new character
    // is not their owner (designer-confirmed, Index A93).
    UObject* bagService = services->getBagService();
    const FGuid oldCharacterId = inventory->getCharacterId();
    if(bagService != nullptr && oldCharacterId.IsValid()){
        for(const FGuid& inventoryId : IBPI_BagActorService::Execute_removeBagsOwnedBy(bagService, oldCharacterId)){
            registry->destroyInventory(inventoryId);
        }
    }
    UInventoryData* carriedBag = inventory->releaseInventory();
    if(carriedBag != nullptr){
        registry->destroyInventory(carriedBag->getInventoryId());
    }
    inventory->clearHoldSlots();
    inventory->setHotbarBindings(TArray<FS_HotbarBinding>());

    inventory->setCharacterId(newCharacterId);
    inventory->setKitGranted(false);
    grantStartingKit(player);
    INVENTORY_LOG(Log, player, "%s starts a new character %s", *GetNameSafe(player), *newCharacterId.ToString());
}
```

> **Why `detachBag` cannot leave a half-moved state.** Only steps 1 and 2 can fail, and both fail *before* anything of the player's has moved: step 1 creates something new, and step 2's contract is that a failed release changes nothing (S1.10). Step 3 is plain memory in one function, with no call that can refuse. The retry "up to a data limit" and the fallback "respawn with the bag kept and log an error" are Doc B02's (its 10.2): B02 simply calls `detachBag` again.

> **A hit or a disconnect in the middle of a recovery** is processed after it, because the whole recovery is one function call on the server (Doc02 §19.9). A lethal hit in the same server step as the end of the pick-up hold goes first and cancels the hold, so `reattachBag` is never called: that order is Doc B02's.

### S9.9 — Use the kit in the stub game mode

In `AInventoryTestGameMode::PostLogin`, replace the two lines marked for Section 9 with (add `#include "ItemInventoryFramework/UBFL_InventoryBag.h"`):

```cpp
    UBFL_InventoryBag::startNewCharacter(NewPlayer->PlayerState, FGuid::NewGuid());
```

The fixture's `makePlayer()` keeps giving an **empty** bag with no kit, so tests start from a known state.

> When the Equipment guide is built: wherever `startNewCharacter` is called, its `resetForNewCharacter()` is called right after (R05 guide, E7.4). This function cannot know about worn clothes.

### Verify (Section 9)

| Test | Expect |
| --- | --- |
| Press Play | the bag holds `StarterAxe` and `Map`; log `Starting kit granted ...: 2 item(s)` |
| `grantStartingKit` a second time | refused `KitAlreadyGranted`; the bag is unchanged |
| Clear `startingKit` in the settings, start a character | an Error line; `isKitGranted()` stays false; fix the data and `grantStartingKit` works |
| Bag with 5 sticks and a spear in the hand, slot 1 bound to `Spear`; `detachBag(player, location)` | `succeeded()`; `hasBag()` false; `getPersonalInventory()` null; `getUsableHotbarSlotCount()` 3; the hand is empty and `getHeldItem` is `Unarmed`; the registry still holds the inventory, `isDeathBag()` true; the stub bag service has one record with the hotbar snapshot |
| 2 sticks staged, then `detachBag` | the bag inventory has `reservedQuantity` 0 everywhere |
| `Awaken.Inventory.FailBagStep 1`, then `detachBag` | `BagCreateFailed`; the player **still has the bag**, the hotbar and the spear in the hand; the stub holds no record |
| `Awaken.Inventory.FailBagStep 2`, then `detachBag` | `GearReleaseFailed`; the same: nothing moved, no record left |
| While Bagless: `grantOrDrop(Stick × 3)` | all 3 dropped; nothing stored |
| While Bagless: pick up a loose `Stick × 5` | **1** stick in hold slot 0; the loose item holds 4 |
| Pick up a loose spear, then a third item, then a fourth | slots 0, 1, 2 full; the fourth is refused `Interaction.FailReason.HandsFull` and stays on the ground |
| `selectHotbarSlot(2)` (the spear) | the hand shows the spear, `isWielded` **false**; `getHeldItem` is still `Unarmed` |
| `selectHotbarSlot(5)` while Bagless | refused `InvalidSlot`: keys 4 to 9 and 0 are locked |
| `bindHotbarSlot`, `reserveForStaging`, `dropFromBag`, `useConsumable`, `applyReload` while Bagless | each refused `NoBag` |
| Pick up `RawMeat` with 10 hours left, advance 20 hours, `useConsumableFromHoldSlot` | Hunger **100**: it did not spoil in the hand |
| Pick up `RawMeat` that was already past its time on the ground, eat it from the slot | Hunger **50** and the HP loss: it is the spoiled item |
| `dropHoldSlot(0)` | the item lies in front of the player; the slot is free |
| `discardBaglessItems` | the slots are empty; nothing was dropped |
| `transferEntry` into or out of the detached bag's inventory | refused `DeathBagLocked` |
| `reattachBag(player, bagId)` holding 2 items in hold slots | `hasBag()` true; the 5 sticks are back; the two held items are **in the bag**; slot 1 is bound to `Spear` again; the spear that was in the hand is in the hand, wielded; the stub record is gone; `isDeathBag()` false |
| Recovery when the bag is at its limit for a held item's type | that item lies as a loose item beside the player; a stack that fits in part went in in part |
| `reattachBag` by a player with another character id | refused `NotBagOwner` |
| `reattachBag` while holding a bag | refused `AlreadyHasBag` |
| Food in the bag while it lay in the world for longer than its spoil time | it is the spoiled item after recovery (the bag kept absolute times) |
| `startNewCharacter` while a bag of the old character lies in the world | that bag's record and inventory are gone; the player has a new bag with a new kit; the old bag id answers `NotFound` |

**Two players:** host kills the client's bag with the Section 13 command `Awaken.Bag.DropHere p1`. On the **client**: `hasBag()` turns false, `getBagEntries()` is empty, the hand empties on **both** machines. The client picks up one item: only the client sees its hold slot. `Awaken.Bag.Recover p1`: the client's bag and hotbar are back.

### Handoff

- **B02** calls `detachBag(player, validSpot)` on a multiplayer death and retries on failure up to its own limit; calls `discardBaglessItems(player)` when a Bagless player dies; calls `reattachBag(player, bagId)` when the owner's pick-up hold completes; implements `IBPI_BagActorService` with `ADeathBagActor` and its registry. `removeBagsOwnedBy` must return the inventory id of every record it removes (the request of the R01 spec).
- **R13** stores the record behind the same seam as `S_BagRecord`; the bag's inventory itself is saved by this framework (Section 11).
- **B05 / B20 / R13** call `startNewCharacter(player, newCharacterId)` when a character is created (a new player, a fresh start, Start Again, a system-given character), and `setCharacterId` after a load. The opening bag scene (Doc R17) is presentation only: the grant never waits for it.
- **B17**: `hasBag` gates the bag screen and the build screen; a Bagless player learns what a key holds by pressing it (`selectHotbarSlot`).
- **R05**: its guide replaces `UStubEquipmentGear` with the real seam (E7.2). Its E7.3 patch is already in `detachBag` above.

---
# SECTION 10 — Bag-Screen Data: Pages, Fixed Slots, Piles, and the Catalog Checks

**Goal:** the UI can ask "where is this item drawn and how many units does its pile show" without searching; item data that breaks a rule of 3.1a is caught before a build.
**Spec:** R01 3.1a, 3.1b, §11; Doc B17 3.6; Doc02 §19.8; Doc B22 §3.7 (the tests "Slot data check", "Slot lookup", "Page mapping").
**Depends on:** Section 2 (the layout assets and item fields). Nothing in Sections 3 to 9 depends on this section, and nothing here is read by gameplay.

### Files

```
Public/ItemInventoryFramework/UInventorySettings.h          (addition)
Public/ItemInventoryFramework/UItemCatalogSubsystem.h       (additions)
Public/ItemInventoryFramework/UBFL_InventoryValidation.h
Private/ItemInventoryFramework/UBFL_InventoryValidation.cpp
```

### The rules (R01 3.1a, 3.1b)

- The bag screen is not a flexible list. Every item has **one fixed place**: a page and a slot on it. A slot never moves, the player cannot rearrange anything, and nothing sorts or scrolls inside a page.
- **Five pages and no All page**: Weapons, Tools, Consumables, Materials, Others. The page is **derived** from `categoryTag` through one mapping; a category that the mapping does not name goes to Others.
- A slot is a place on the screen, **not a capacity**: the bag still limits each item type by `perTypeLimit`.
- A pickup never looks for a free place: it changes a quantity, and the item's own slot reads that quantity. Arrival order never matters.
- A slot shows the real quantity as a pile of small 3D units, one per unit, up to the length of the item's stack layout. The exact number shows only on hover.
- Two items share a slot only as a declared variant pair (`slotSharedWith`): fresh and spoiled food, clean and unclean water. Each id is its own pile in its own half.
- A slot is always drawn, also when empty, with no silhouette.
- **Page and slot are presentation only.** They are never saved, never replicated and never read by gameplay, so adding, moving or removing a slot never needs a save migration.
- Fixed slots belong to the bag screen. Chests and other container screens are not changed (R01 §10: not decided).

### S10.1 — The page mapping in the settings

**Header additions** to `UInventorySettings.h`:

```cpp
    // ---- bag screen (R01 3.1a). The one mapping from item category to page ----
    UPROPERTY(Config, EditAnywhere, Category = "Bag Screen") TArray<FS_InventoryPageMapping> pageMappings;
    // The page of every category the mapping does not name: Others.
    UPROPERTY(Config, EditAnywhere, Category = "Bag Screen", meta = (Categories = "Inventory.Page")) FGameplayTag fallbackPageTag;
```

Then fill it in *Project Settings → Game → Awaken Inventory → Bag Screen* (the mapping of R01 3.1a):

| `pageTag` | `categories` | `layout` |
| --- | --- | --- |
| `Inventory.Page.Weapons` | `Item.Category.Weapon` | `DA_Page_Weapons` |
| `Inventory.Page.Tools` | `Item.Category.Tool` | `DA_Page_Tools` |
| `Inventory.Page.Consumables` | `Item.Category.Medicine`, `.Food`, `.Water` | `DA_Page_Consumables` |
| `Inventory.Page.Materials` | `Item.Category.Material` | `DA_Page_Materials` |
| `Inventory.Page.Others` | *(empty: it is the fallback)* | `DA_Page_Others` |

and `fallbackPageTag = Inventory.Page.Others`. Armor, clothing, container and any category added later land on Others without another edit.

> The Axe is on the Weapons page because its category is weapon, and the map is on the Tools page because its category is tool. Both are data. The Axe's chopping role is tied to the Axe item, never to its category (R01 3.1a), so changing a category only moves the item to another page.

### S10.2 — The lookup on the catalog (R01 3.1b)

Two tables are built once, when the item data loads. The first maps an item id to its page and slot; the second maps a slot to the one item that owns it, or to the two items of a variant pair.

**Header additions** to `UItemCatalogSubsystem.h` (add the includes `GameplayTagContainer.h` and `ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h`, and the forward declaration `class UInventoryData;`):

```cpp
public:
    // ---- Section 10: the bag-screen lookup (R01 3.1b). Presentation only ----
    // The page of an item, derived from its category. The fallback page for a null definition.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FGameplayTag getPageFor(const UDA_ItemDefinition* definition) const;
    // Table 1. False, with one Error line naming the item, when the item has no slot: it is still held and usable, only not drawn.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") bool getSlotInfo(FName itemId, FS_ItemSlotInfo& outInfo) const;
    // Every item id drawn on a page.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") TArray<FName> getItemsOnPage(FGameplayTag pageTag) const;
    // What the slot of an item shows: the available quantity of a stack, or the number of free copies (R01 3.1b, 3.6).
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") int32 getDisplayQuantity(const UInventoryData* inventory, FName itemId) const;
    // What the staging widget shows for an item: the reserved quantity (Doc R02 3.3).
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") int32 getStagedQuantity(const UInventoryData* inventory, FName itemId) const;
    // The slot of the staging widget: the item's stagingSlot, or its inventory slot when none is set.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FS_InventorySlotRef getStagingSlot(FName itemId) const;

    // Command. Builds both tables. Runs when the item data loads; a test calls it after adding its definitions.
    void rebuildSlotLookup();

private:
    static FString makeSlotKey(const FS_InventorySlotRef& slot);

    TMap<FName, FS_ItemSlotInfo> slotByItem;       // table 1
    TMap<FString, TArray<FName>> itemsBySlot;      // table 2
    mutable TSet<FName> reportedMissingSlots;
```

Append to `UItemCatalogSubsystem.cpp` (add `#include "ItemInventoryFramework/UInventoryData.h"`):

```cpp
// ---------- Section 10: the bag-screen lookup ----------

FString UItemCatalogSubsystem::makeSlotKey(const FS_InventorySlotRef& slot){
    return slot.pageLayout.ToSoftObjectPath().ToString() + TEXT("|") + slot.slotId.ToString();
}

FGameplayTag UItemCatalogSubsystem::getPageFor(const UDA_ItemDefinition* definition) const{
    const UInventorySettings* settings = UInventorySettings::get();
    if(definition != nullptr){
        for(const FS_InventoryPageMapping& mapping : settings->pageMappings){
            if(definition->categoryTag.MatchesAny(mapping.categories)){
                return mapping.pageTag;
            }
        }
    }
    return settings->fallbackPageTag;   // "Others holds everything the other four do not"
}

void UItemCatalogSubsystem::rebuildSlotLookup(){
    slotByItem.Reset();
    itemsBySlot.Reset();
    reportedMissingSlots.Reset();
    // Table 2: slot -> the item that owns it, or the two items of a declared variant pair.
    for(const TPair<FName, TObjectPtr<UDA_ItemDefinition>>& pair : definitions){
        if(pair.Value->inventorySlot.isSet()){
            itemsBySlot.FindOrAdd(makeSlotKey(pair.Value->inventorySlot)).Add(pair.Key);
        }
    }
    for(TPair<FString, TArray<FName>>& pair : itemsBySlot){
        // The item that owns the slot first, its variant second: the two halves of the pocket never swap.
        pair.Value.Sort([this](const FName& left, const FName& right){
            const bool isLeftVariant = !getDefinition(left)->slotSharedWith.IsNone();
            const bool isRightVariant = !getDefinition(right)->slotSharedWith.IsNone();
            return isLeftVariant != isRightVariant ? !isLeftVariant : left.LexicalLess(right);
        });
    }
    // Table 1: item -> page and slot.
    for(const TPair<FName, TObjectPtr<UDA_ItemDefinition>>& pair : definitions){
        if(pair.Value->inventorySlot.isSet()){
            FS_ItemSlotInfo info;
            info.pageTag = getPageFor(pair.Value);
            info.slot = pair.Value->inventorySlot;
            info.itemsInSlot = itemsBySlot.FindRef(makeSlotKey(pair.Value->inventorySlot));
            slotByItem.Add(pair.Key, info);
        }
    }
}

bool UItemCatalogSubsystem::getSlotInfo(FName itemId, FS_ItemSlotInfo& outInfo) const{
    const FS_ItemSlotInfo* found = slotByItem.Find(itemId);
    if(found != nullptr){
        outInfo = *found;
        return true;
    }
    // Fail-safe (R01 3.1b): the data check makes this impossible in a build. If it still happens, say so once,
    // do not draw the item, and never invent a slot at runtime.
    if(!reportedMissingSlots.Contains(itemId)){
        reportedMissingSlots.Add(itemId);
        INVENTORY_LOG(Error, this, "Item %s has no slot on the bag screen: it is held and usable but is not drawn", *itemId.ToString());
    }
    return false;
}

TArray<FName> UItemCatalogSubsystem::getItemsOnPage(FGameplayTag pageTag) const{
    TArray<FName> items;
    for(const TPair<FName, FS_ItemSlotInfo>& pair : slotByItem){
        if(pair.Value.pageTag == pageTag){
            items.Add(pair.Key);
        }
    }
    return items;
}

int32 UItemCatalogSubsystem::getDisplayQuantity(const UInventoryData* inventory, FName itemId) const{
    return inventory != nullptr ? inventory->getAvailableQuantity(itemId) : 0;   // held minus reserved: what every reader sees
}

int32 UItemCatalogSubsystem::getStagedQuantity(const UInventoryData* inventory, FName itemId) const{
    return inventory != nullptr ? inventory->getTotalQuantity(itemId) - inventory->getAvailableQuantity(itemId) : 0;
}

FS_InventorySlotRef UItemCatalogSubsystem::getStagingSlot(FName itemId) const{
    const UDA_ItemDefinition* definition = getDefinition(itemId);
    if(definition == nullptr){
        return FS_InventorySlotRef();
    }
    return definition->stagingSlot.isSet() ? definition->stagingSlot : definition->inventorySlot;
}
```

Two one-line edits to code from Section 2: at the end of `loadAllDefinitions()` add `rebuildSlotLookup();`, and at the end of `resetForTest()` add the same call (it empties the tables).

> **Worked example (R01 3.1b).** The Spear points at slot `W3` with the Row layout; the Stick points at `M1` with the Bundle layout. The player picks up 3 sticks, then 2 spears. The bag holds `Stick 3` and `Spear × 2`. `getSlotInfo(Stick)` answers page Materials, slot `M1`; `getDisplayQuantity` answers 3, so the UI draws units 1 to 3 of the Bundle layout. Picking the spears up first gives the same screen. A fourth stick draws unit 4 and moves nothing else.

### S10.3 — What the UI reads, in one place

Doc B17 builds the screen. Everything it needs from this framework:

| The UI needs | It calls |
| --- | --- |
| The items of the open page | `catalog->getItemsOnPage(pageTag)`, then `getSlotInfo(itemId)` for each |
| The slot's place and size | `info.slot.pageLayout` (load it) → `findSlot(info.slot.slotId)` |
| How many units a pile draws | `stackLayout->getVisibleUnitCount(catalog->getDisplayQuantity(bag, itemId))`; unit *n* is `stackLayout->units[n]` |
| The exact number on hover, and the colour change at the limit | `getDisplayQuantity` and `definition->perTypeLimit` |
| The two piles of a shared slot | `info.itemsInSlot` (the owner first, the tinted variant second) |
| The copies of a weapon and which one is used next | `bag->getEntries()` filtered by item id; `inventory->findUsableInstance(itemId)` (hotbar) and `bag->findMostWornInstance(itemId, heldId)` (crafting) |
| The staging widget | `getStagedQuantity` and `getStagingSlot` |
| When to redraw | the message `Inventory.Changed` with the bag's id, or the delegate `onInventoryChanged` |
| The text of a name | the key `definition->getDisplayNameKey()` through Doc R10 |

### S10.4 — The catalog-wide data check

The per-asset check of S2.12 covers what one asset knows about itself. These rules need the whole catalog. The function is used by the command `Awaken.Item.Validate` (Section 13) and by an automated test, and it also runs every item's own check, so one command reports everything.

`Public/ItemInventoryFramework/UBFL_InventoryValidation.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UBFL_InventoryValidation.generated.h"

UCLASS()
class AWAKEN_API UBFL_InventoryValidation : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // Query. Checks every loaded item against the rules of R01 3.1a, 3.2a and 3.10 and Doc02 19.8.
    // Fills the list with one line per error and returns how many there are.
    static int32 validateCatalog(const UObject* worldContext, TArray<FString>& outErrors);
};
```

`Private/ItemInventoryFramework/UBFL_InventoryValidation.cpp`

```cpp
#include "ItemInventoryFramework/UBFL_InventoryValidation.h"
#include "ItemInventoryFramework/UDA_InventoryPageLayout.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UDA_StackLayout.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace{
    struct FSlottedItem{
        const UDA_ItemDefinition* item = nullptr;
        const FS_InventorySlotDef* slot = nullptr;
    };
}

int32 UBFL_InventoryValidation::validateCatalog(const UObject* worldContext, TArray<FString>& outErrors){
    outErrors.Reset();
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(worldContext);
    const UInventorySettings* settings = UInventorySettings::get();
    if(catalog == nullptr){
        outErrors.Add(TEXT("The item catalog is not available."));
        return outErrors.Num();
    }

    // ---- the page mapping: no item may map to a page that has no layout ----
    TMap<FGameplayTag, const UDA_InventoryPageLayout*> layoutByPage;
    for(const FS_InventoryPageMapping& mapping : settings->pageMappings){
        const UDA_InventoryPageLayout* layout = mapping.layout.LoadSynchronous();
        if(!mapping.pageTag.IsValid() || layout == nullptr){
            outErrors.Add(FString::Printf(TEXT("Page mapping '%s' has no page tag or no layout."), *mapping.pageTag.ToString()));
            continue;
        }
        if(layout->pageTag != mapping.pageTag){
            outErrors.Add(FString::Printf(TEXT("Layout %s says page %s but is mapped to page %s."), *layout->GetName(), *layout->pageTag.ToString(), *mapping.pageTag.ToString()));
        }
        layoutByPage.Add(mapping.pageTag, layout);
    }
    if(!layoutByPage.Contains(settings->fallbackPageTag)){
        outErrors.Add(TEXT("The fallback page (Others) has no layout: an item of an unlisted category would have no page."));
    }

    int32 unbreakableCount = 0;
    TMap<FString, TArray<FSlottedItem>> itemsBySlot;
    for(const TPair<FName, TObjectPtr<UDA_ItemDefinition>>& pair : catalog->getAllDefinitions()){
        const UDA_ItemDefinition* item = pair.Value;
        const FString name = item->itemId.ToString();
        unbreakableCount += item->isUnbreakable() ? 1 : 0;

#if WITH_EDITOR
        // The item's own check (S2.12, and the armor and clothes rules the R05 guide adds to it).
        FDataValidationContext context;
        if(item->IsDataValid(context) == EDataValidationResult::Invalid){
            TArray<FText> warnings;
            TArray<FText> errors;
            context.SplitIssues(warnings, errors);
            for(const FText& error : errors){
                outErrors.Add(error.ToString());
            }
        }
#endif

        // ---- a reference to something that does not exist (Doc02 19.8) ----
        if(item->canSpoil){
            const UDA_ItemDefinition* spoiled = catalog->getDefinition(item->spoiledItemId);
            if(spoiled == nullptr || !spoiled->isStackable()){
                outErrors.Add(FString::Printf(TEXT("%s: spoiledItemId '%s' does not exist or is not stackable."), *name, *item->spoiledItemId.ToString()));
            }
        }
        if(!item->effectBaseItemId.IsNone() && catalog->getDefinition(item->effectBaseItemId) == nullptr){
            outErrors.Add(FString::Printf(TEXT("%s: effectBaseItemId '%s' does not exist."), *name, *item->effectBaseItemId.ToString()));
        }
        if(!item->ammoItemId.IsNone()){
            const UDA_ItemDefinition* ammo = catalog->getDefinition(item->ammoItemId);
            if(ammo == nullptr || !ammo->isStackable()){
                outErrors.Add(FString::Printf(TEXT("%s: ammoItemId '%s' does not exist or is not stackable."), *name, *item->ammoItemId.ToString()));
            }
        }
        // A variant derives its meter values from its base item; its own list holds only the illness chance and the HP loss.
        const bool isVariant = !item->effectBaseItemId.IsNone() || catalog->findFreshItemFor(item->itemId) != nullptr;
        const bool hasOwnMeterEntry = item->consumeEffects.ContainsByPredicate([](const FS_ConsumeEffect& effect){
            return effect.kind == E_ConsumeEffectKind::ModifyMeter;
        });
        if(isVariant && hasOwnMeterEntry){
            outErrors.Add(FString::Printf(TEXT("%s: a spoiled or unclean variant must not list ModifyMeter entries; they are derived from its base item."), *name));
        }

        // ---- the slot (R01 3.1a, 3.1b) ----
        const bool isBagItem = item->itemId != settings->unarmedItemId && !item->isHeavy();
        if(!isBagItem || !item->inventorySlot.isSet()){
            continue;   // a bag item without a slot is reported by its own check above
        }
        const UDA_InventoryPageLayout* pageLayout = layoutByPage.FindRef(catalog->getPageFor(item));
        const UDA_InventoryPageLayout* slotLayout = item->inventorySlot.pageLayout.LoadSynchronous();
        if(slotLayout != pageLayout){
            outErrors.Add(FString::Printf(TEXT("%s: its category puts it on page %s, but its slot is in another layout. After a category change the slot must point into the layout of the new page."), *name, *catalog->getPageFor(item).ToString()));
        }
        const FS_InventorySlotDef* slot = slotLayout != nullptr ? slotLayout->findSlot(item->inventorySlot.slotId) : nullptr;
        if(slot == nullptr){
            outErrors.Add(FString::Printf(TEXT("%s: slot '%s' does not exist in its layout (deleted or renumbered?)."), *name, *item->inventorySlot.slotId.ToString()));
            continue;
        }
        FSlottedItem slotted;
        slotted.item = item;
        slotted.slot = slot;
        itemsBySlot.FindOrAdd(item->inventorySlot.pageLayout.ToSoftObjectPath().ToString() + TEXT("|") + item->inventorySlot.slotId.ToString()).Add(slotted);
    }

    // ---- no two items share a slot unless they are declared variants; a full pile fits inside its pocket ----
    for(const TPair<FString, TArray<FSlottedItem>>& pair : itemsBySlot){
        const TArray<FSlottedItem>& sharing = pair.Value;
        const bool isDeclaredPair = sharing.Num() == 2 && (sharing[0].item->slotSharedWith == sharing[1].item->itemId || sharing[1].item->slotSharedWith == sharing[0].item->itemId);
        if(sharing.Num() > 1 && !isDeclaredPair){
            outErrors.Add(FString::Printf(TEXT("Slot %s is used by %d items (%s, %s, ...) that are not a declared variant pair."), *pair.Key, sharing.Num(), *sharing[0].item->itemId.ToString(), *sharing[1].item->itemId.ToString()));
        }
        for(const FSlottedItem& slotted : sharing){
            const UDA_StackLayout* stackLayout = slotted.item->stackLayout.LoadSynchronous();
            // A shared pocket is split into halves (PROPOSED in R01 3.2a), so each pile has half the width.
            const double pocketWidth = slotted.slot->size.X / sharing.Num();
            if(stackLayout != nullptr && (stackLayout->pileFootprint.X > pocketWidth || stackLayout->pileFootprint.Y > slotted.slot->size.Y)){
                outErrors.Add(FString::Printf(TEXT("%s: a full pile of layout %s does not fit inside slot '%s'."), *slotted.item->itemId.ToString(), *stackLayout->GetName(), *slotted.slot->slotId.ToString()));
            }
        }
    }
    for(const TPair<FName, TObjectPtr<UDA_ItemDefinition>>& pair : catalog->getAllDefinitions()){
        const UDA_ItemDefinition* item = pair.Value;
        if(item->slotSharedWith.IsNone()){
            continue;
        }
        const UDA_ItemDefinition* partner = catalog->getDefinition(item->slotSharedWith);
        if(partner == nullptr || partner->inventorySlot.slotId != item->inventorySlot.slotId || partner->inventorySlot.pageLayout != item->inventorySlot.pageLayout){
            outErrors.Add(FString::Printf(TEXT("%s: slotSharedWith '%s' does not exist or does not use the same slot."), *item->itemId.ToString(), *item->slotSharedWith.ToString()));
        }
    }

    // ---- the special items (R01 3.1, 3.8, 3.10) ----
    if(unbreakableCount != 1){
        outErrors.Add(FString::Printf(TEXT("%d items are unbreakable. Exactly one item in the whole catalog is: the starter Axe."), unbreakableCount));
    }
    const UDA_ItemDefinition* unarmed = catalog->getUnarmedDefinition();
    if(unarmed == nullptr || unarmed->weaponType != E_WeaponType::Melee || unarmed->durabilityMax != 0.0f){
        outErrors.Add(TEXT("The Unarmed entry is missing, is not a Melee weapon, or has durability."));
    }
    for(const FS_ItemStack& stack : settings->startingKit){
        if(catalog->getDefinition(stack.itemId) == nullptr){
            outErrors.Add(FString::Printf(TEXT("The starting kit names '%s', which does not exist."), *stack.itemId.ToString()));
        }
    }
    return outErrors.Num();
}
```

> "Every page layout has enough slots for the items mapped to it" needs no rule of its own: every item must name an existing slot of its page's layout, and no two unrelated items may name the same one. If the page is full, the author's item has no free slot to pick and the check fails until a slot is added to the layout (R01 3.1b).

### Verify (Section 10)

| Test | Expect |
| --- | --- |
| `getPageFor` for `Spear`, `Map`, `RawMeat`, `Stick` | Weapons, Tools, Consumables, Materials |
| `getPageFor` for `IronVest`, a clothing item, `Pouch`, and a test item of a brand-new category `Item.Category.Trophy` | all **Others** |
| `getSlotInfo(Stick)` | true; page Materials; slot `M1`; `itemsInSlot` = `[Stick]` |
| `getSlotInfo(SpoiledMeat)` | slot `C1`; `itemsInSlot` = `[RawMeat, SpoiledMeat]`, the fresh one first |
| `getSlotInfo(Log)` (heavy, no slot) | false and one Error line; asking again adds no second line |
| Pick up 3 sticks then 2 spears; then, in a second run, the spears first | the same answers from `getDisplayQuantity` both times |
| Stage 2 of 5 sticks | `getDisplayQuantity` 3, `getStagedQuantity` 2 |
| `DA_Stack_Row` (4 units) with quantity 7 | `getVisibleUnitCount` is 4: the pile stays as full as it can look |
| `validateCatalog` on the S2.14 content | 0 errors |
| Give `Stone` the slot of `Stick` | "Slot … is used by 2 items … not a declared variant pair" |
| Change `Stick.categoryTag` to Tool, leave its slot | "its category puts it on page Inventory.Page.Tools, but its slot is in another layout" |
| Delete slot `M2` from `DA_Page_Materials` | "Stone: slot 'M2' does not exist in its layout" |
| Give `SpoiledMeat` a `ModifyMeter` entry | "a spoiled or unclean variant must not list ModifyMeter entries" |
| Set `Spear.durabilityMax` to −1 and add the Permanent tag | "2 items are unbreakable" |
| Remove the layout of the Others row | "The fallback page (Others) has no layout" |
| Set `DA_Stack_Heap.pileFootprint` larger than slot `M2` | "a full pile of layout DA_Stack_Heap does not fit inside slot 'M2'" |
| Save the game, add a slot to a page, load | nothing to migrate: the save holds no page and no slot |

Undo each change after seeing its message. These rows are the three Doc B22 starter tests of this section; as automated tests they build the items with the fixture and call `rebuildSlotLookup()` and `validateCatalog`.

### Handoff

- **B17** builds the bag screen from the table in S10.3. The performance fallback of R01 3.1a (pre-made pile sizes or pictures) changes only how a slot is drawn, never these functions.
- **R02** uses `getStagingSlot` and `getStagedQuantity` for the staging widgets.
- **B22**: `validateCatalog` is the editor data check of R01 §11; hook it into the build or release check when that pipeline exists.
- Whether chests and other container screens get fixed slots is not decided (R01 §10). Until it is, a container screen lists `getEntries()` as it is.

---

# SECTION 11 — Save Integration

**Goal:** bags, hotbar bindings, containers and the contents of pouches survive save and reload; nothing transient does.
**Spec:** R01 §8, 3.2a (spoil times across files); Doc00 §9; Doc02 §9; Doc R13 3.3, 3.4, 3.6, 3.7, 3.11; Doc B06 §3.6.
**Depends on:** Section 9.

### Files

```
Public/ItemInventoryFramework/03Interfaces/BPI_SaveParticipantStub.h   (temporary: owner Doc R13)
Public/ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h      (append)
Public/ItemInventoryFramework/UBFL_InventorySave.h
Private/ItemInventoryFramework/UBFL_InventorySave.cpp
Private/ItemInventoryFramework/Stubs/StubSaveRegistry.h/.cpp
Public/ItemInventoryFramework/UPlayerInventoryComponent.h              (additions)
Public/ItemInventoryFramework/UInventoryRegistrySubsystem.h            (additions)
```

### What is saved, and where (R01 §8)

| State | File | By |
| --- | --- | --- |
| The bag's entries (with each instance's durability, loaded flag and rounds), the hotbar bindings, the kit flag | **Character** | `UPlayerInventoryComponent`, key `Inventory.Character` |
| The contents of a Portable Container that is in the bag | **Character**, with the bag, once | the same snapshot |
| The contents of world Containers | **World** | `UInventoryRegistrySubsystem`, key `Inventory.World` |
| The inventory of a bag lying in the world, and of pouches inside it | **World** | the same snapshot |
| The record of a bag lying in the world (owner, location, hotbar snapshot, worn gear) | World | Doc R13 / Doc B02 (`S_BagRecord`), **not here** |
| Worn armor and clothes | Character | Doc R05 |

| Never saved | Why |
| --- | --- |
| Reservations | a crafting session never survives a load (R01 3.6) |
| The held item, the "last used" history | R01 3.5, I9 |
| The three Bagless hold slots | Index A110, S2-F30 |
| Loose items | Doc00 §9: items outside an inventory do not persist |
| Page and slot of an item | presentation only (R01 3.1b) |

**Spoil times across files (R01 3.2a amendment, Doc R13 3.4).** A Character file stores the **time left** of each stack and rebuilds the spoil time from the clock on load, because a character can be carried into a world with another clock. The World file keeps the **absolute** spoil time, because it carries its own clock.

**A Bagless player is never captured** (Doc R13 3.3): their previous Character file is kept, so a file never says "Bagless". If our snapshot function is asked while the player is Bagless, it returns nothing and logs a warning.

### S11.1 — The save-participant contract (temporary copy; owner Doc R13)

`Public/ItemInventoryFramework/03Interfaces/BPI_SaveParticipantStub.h`

```cpp
// TEMPORARY STUB FILE — owner: Doc R13 Save. Delete this file when R13's BPI_SaveParticipant.h exists,
// and include that header instead. The names follow Doc R13 3.7 and Doc02 §9.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BPI_SaveParticipantStub.generated.h"

UENUM(BlueprintType)
enum class E_SaveScope : uint8{
    World,
    Character
};

UINTERFACE(BlueprintType)
class UBPI_SaveParticipant : public UInterface{
    GENERATED_BODY()
};

class IBPI_SaveParticipant{
    GENERATED_BODY()

public:
    // A stable key: the name this framework's blob is stored under.
    UFUNCTION(BlueprintNativeEvent)
    FName getSaveKey();

    UFUNCTION(BlueprintNativeEvent)
    E_SaveScope getSaveScope();

    // For migration: one step per version (Doc R13 3.11).
    UFUNCTION(BlueprintNativeEvent)
    int32 getSnapshotVersion();

    // Query: never mutates (Doc02 §9).
    UFUNCTION(BlueprintNativeEvent)
    TArray<uint8> getSnapshotForSave();

    // Command. False = the blob was refused and nothing changed.
    UFUNCTION(BlueprintNativeEvent)
    bool restoreFromSnapshot(const TArray<uint8>& snapshot);
};
```

### S11.2 — Save records

Append to `S_ItemInventoryFramework.h`:

```cpp
// =====================================================================
// S11.2  Save records (R01 §8; Doc02 §9). Plain data only: ids, never object pointers (Doc R13 3.11).
// =====================================================================

// One inventory as it is written to a file.
USTRUCT()
struct FS_SavedInventory{
    GENERATED_BODY()

    UPROPERTY() FGuid inventoryId;
    UPROPERTY() E_InventoryType inventoryType = E_InventoryType::Personal;
    UPROPERTY() bool isDeathBag = false;
    // True in a Character file: each entry's spoilAtTime field then holds the time LEFT, not the time on the clock.
    UPROPERTY() bool usesTimeLeft = false;
    // reservedQuantity is always 0 here: reservations are never saved.
    UPROPERTY() TArray<FS_InventoryEntry> entries;
};

// Key Inventory.Character.
USTRUCT()
struct FS_PlayerInventorySave{
    GENERATED_BODY()

    UPROPERTY() int32 version = 1;
    UPROPERTY() bool isKitGranted = false;
    UPROPERTY() TArray<FS_HotbarBinding> hotbar;
    UPROPERTY() FS_SavedInventory bag;
    // The contents of the Portable Containers that are in the bag: saved once, with what holds them (R01 3.4).
    UPROPERTY() TArray<FS_SavedInventory> containedInventories;
};

// Key Inventory.World.
USTRUCT()
struct FS_WorldInventorySave{
    GENERATED_BODY()

    UPROPERTY() int32 version = 1;
    // World containers and bags lying in the world.
    UPROPERTY() TArray<FS_SavedInventory> inventories;
    UPROPERTY() TArray<FS_SavedInventory> containedInventories;
};
```

### S11.3 — `UBFL_InventorySave`: bytes and inventories

`Public/ItemInventoryFramework/UBFL_InventorySave.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UBFL_InventorySave.generated.h"

class UInventoryData;
class UInventoryRegistrySubsystem;

// Helpers shared by every save participant of this module (the R05 guide uses toBytes and fromBytes too).
UCLASS()
class AWAKEN_API UBFL_InventorySave : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // A struct as a byte blob. Every property is written with its name, so adding a field later does not break old saves.
    static TArray<uint8> toBytes(UScriptStruct* structType, const void* data);
    // False when the blob is empty or cannot be read.
    static bool fromBytes(UScriptStruct* structType, void* data, const TArray<uint8>& bytes);

    // Query. One inventory, and the contents of every Portable Container inside it, as save records.
    static void capture(const UInventoryRegistrySubsystem* registry, const UInventoryData* inventory, bool usesTimeLeft, int64 now, FS_SavedInventory& outInventory, TArray<FS_SavedInventory>& outContained);
    // Command. Puts a saved inventory back into the registry, with its Portable Containers. Entries whose item no longer
    // exists are skipped with one log line (Doc R13 3.11). Null when the record cannot be restored.
    static UInventoryData* restore(UInventoryRegistrySubsystem* registry, const FS_SavedInventory& saved, const TArray<FS_SavedInventory>& containedPool, int64 now);

private:
    static TArray<FS_InventoryEntry> toSavedEntries(const UInventoryData* inventory, bool usesTimeLeft, int64 now);
    static TArray<FS_InventoryEntry> toLiveEntries(const UObject* worldContext, const FS_SavedInventory& saved, int64 now);
};
```

`Private/ItemInventoryFramework/UBFL_InventorySave.cpp`

```cpp
#include "ItemInventoryFramework/UBFL_InventorySave.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

TArray<uint8> UBFL_InventorySave::toBytes(UScriptStruct* structType, const void* data){
    TArray<uint8> bytes;
    FMemoryWriter writer(bytes, true);
    FObjectAndNameAsStringProxyArchive archive(writer, false);
    structType->SerializeItem(archive, const_cast<void*>(data), nullptr);   // tagged properties: name + value
    return bytes;
}

bool UBFL_InventorySave::fromBytes(UScriptStruct* structType, void* data, const TArray<uint8>& bytes){
    if(bytes.Num() == 0){
        return false;
    }
    FMemoryReader reader(bytes, true);
    FObjectAndNameAsStringProxyArchive archive(reader, true);
    structType->SerializeItem(archive, data, nullptr);
    return !archive.IsError() && !reader.IsError();
}

TArray<FS_InventoryEntry> UBFL_InventorySave::toSavedEntries(const UInventoryData* inventory, bool usesTimeLeft, int64 now){
    TArray<FS_InventoryEntry> saved = inventory->getEntries();
    for(FS_InventoryEntry& entry : saved){
        entry.reservedQuantity = 0;   // reservations are never saved
        if(usesTimeLeft && entry.spoilAtTime > 0){
            // A stack that is already due is saved with one second left: it spoils at the first request after the load.
            // This keeps the capture a pure query (Doc02 §9).
            entry.spoilAtTime = FMath::Max<int64>(1, entry.spoilAtTime - now);
        }
    }
    return saved;
}

TArray<FS_InventoryEntry> UBFL_InventorySave::toLiveEntries(const UObject* worldContext, const FS_SavedInventory& saved, int64 now){
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(worldContext);
    TArray<FS_InventoryEntry> live;
    for(const FS_InventoryEntry& savedEntry : saved.entries){
        if(catalog == nullptr || catalog->getDefinition(savedEntry.itemId) == nullptr){
            // Doc R13 3.11 (Index A102, ST-13): skip the entry, one log line, the load continues. Never crash.
            INVENTORY_LOG(Warning, worldContext, "Skipped saved item '%s': it no longer exists", *savedEntry.itemId.ToString());
            continue;
        }
        FS_InventoryEntry entry = savedEntry;
        entry.reservedQuantity = 0;
        if(saved.usesTimeLeft && entry.spoilAtTime > 0){
            entry.spoilAtTime = now + entry.spoilAtTime;   // the spoil time is rebuilt from the current clock
        }
        live.Add(entry);
    }
    return live;
}

void UBFL_InventorySave::capture(const UInventoryRegistrySubsystem* registry, const UInventoryData* inventory, bool usesTimeLeft, int64 now, FS_SavedInventory& outInventory, TArray<FS_SavedInventory>& outContained){
    outInventory.inventoryId = inventory->getInventoryId();
    outInventory.inventoryType = inventory->getInventoryType();
    outInventory.isDeathBag = inventory->isDeathBag();
    outInventory.usesTimeLeft = usesTimeLeft;
    outInventory.entries = toSavedEntries(inventory, usesTimeLeft, now);
    // A Portable Container cannot hold another one, so one level is enough (R01 3.4a).
    for(const FS_InventoryEntry& entry : inventory->getEntries()){
        const UInventoryData* contained = registry->findInventory(entry.state.containedInventoryId);
        if(contained != nullptr && !contained->isEmpty()){
            FS_SavedInventory& savedContained = outContained.AddDefaulted_GetRef();
            savedContained.inventoryId = contained->getInventoryId();
            savedContained.inventoryType = E_InventoryType::PortableContainer;
            savedContained.usesTimeLeft = usesTimeLeft;
            savedContained.entries = toSavedEntries(contained, usesTimeLeft, now);
        }
    }
}

UInventoryData* UBFL_InventorySave::restore(UInventoryRegistrySubsystem* registry, const FS_SavedInventory& saved, const TArray<FS_SavedInventory>& containedPool, int64 now){
    if(registry == nullptr || !saved.inventoryId.IsValid()){
        return nullptr;
    }
    // A world container whose actor is loaded already has its inventory (and its rules): reuse it.
    UInventoryData* inventory = registry->findOrCreateInventory(saved.inventoryType, saved.inventoryId, FS_ContainerRules());
    if(inventory == nullptr){
        return nullptr;
    }
    inventory->setDeathBag(saved.isDeathBag);
    inventory->replaceEntries(toLiveEntries(registry, saved, now));
    // Each Portable Container gets its contents back, with the rules of its own item definition.
    for(const FS_InventoryEntry& entry : inventory->getEntries()){
        const FGuid& containedId = entry.state.containedInventoryId;
        const FS_SavedInventory* savedContained = containedPool.FindByPredicate([&containedId](const FS_SavedInventory& candidate){
            return candidate.inventoryId == containedId;
        });
        if(containedId.IsValid() && savedContained != nullptr){
            UInventoryData* contained = registry->getContainedInventory(entry, true);
            if(contained != nullptr){
                contained->replaceEntries(toLiveEntries(registry, *savedContained, now));
            }
        }
    }
    return inventory;
}
```

### S11.4 — The stub save registry (owner: Doc R13)

Until `USaveSubsystem` exists, this keeps the list of participants and can do an in-memory round trip, so save and restore can be tried by hand.

`Private/ItemInventoryFramework/Stubs/StubSaveRegistry.h`

```cpp
// TEMPORARY STUB — owner: Doc R13 Save (USaveSubsystem). Nothing is written to disk.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "StubSaveRegistry.generated.h"

UCLASS()
class UStubSaveRegistry : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    static UStubSaveRegistry* get(const UObject* worldContext);

    void registerParticipant(UObject* participant);
    void unregisterParticipant(UObject* participant);

    // Keeps the snapshot of every participant in memory. Answers how many were captured.
    int32 captureAll();
    // Hands each kept snapshot back to the participant it came from: World scope first, then Character (Doc B06 3.6).
    int32 restoreAll();

private:
    struct FCapturedSnapshot{
        TWeakObjectPtr<UObject> participant;
        bool isWorldScope = false;
        TArray<uint8> bytes;
    };

    TArray<TWeakObjectPtr<UObject>> participants;
    TArray<FCapturedSnapshot> captured;
};
```

`Private/ItemInventoryFramework/Stubs/StubSaveRegistry.cpp`

```cpp
// TEMPORARY STUB — owner: Doc R13 Save.
#include "ItemInventoryFramework/Stubs/StubSaveRegistry.h"
#include "ItemInventoryFramework/03Interfaces/BPI_SaveParticipantStub.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

UStubSaveRegistry* UStubSaveRegistry::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    return gameInstance != nullptr ? gameInstance->GetSubsystem<UStubSaveRegistry>() : nullptr;
}

void UStubSaveRegistry::registerParticipant(UObject* participant){
    if(participant != nullptr && participant->GetClass()->ImplementsInterface(UBPI_SaveParticipant::StaticClass())){
        participants.AddUnique(participant);
    }
}

void UStubSaveRegistry::unregisterParticipant(UObject* participant){
    participants.Remove(participant);
}

int32 UStubSaveRegistry::captureAll(){
    captured.Reset();
    for(const TWeakObjectPtr<UObject>& weakParticipant : participants){
        UObject* participant = weakParticipant.Get();
        if(participant == nullptr){
            continue;
        }
        FCapturedSnapshot snapshot;
        snapshot.participant = participant;
        snapshot.isWorldScope = IBPI_SaveParticipant::Execute_getSaveScope(participant) == E_SaveScope::World;
        snapshot.bytes = IBPI_SaveParticipant::Execute_getSnapshotForSave(participant);
        if(snapshot.bytes.Num() > 0){
            captured.Add(MoveTemp(snapshot));   // an empty blob means "keep my previous file" (a Bagless player)
        }
    }
    INVENTORY_LOG(Log, this, "Stub save: captured %d snapshot(s) in memory", captured.Num());
    return captured.Num();
}

int32 UStubSaveRegistry::restoreAll(){
    int32 restored = 0;
    for(int32 pass = 0; pass < 2; pass++){
        for(const FCapturedSnapshot& snapshot : captured){
            UObject* participant = snapshot.participant.Get();
            if(participant != nullptr && snapshot.isWorldScope == (pass == 0)){
                restored += IBPI_SaveParticipant::Execute_restoreFromSnapshot(participant, snapshot.bytes) ? 1 : 0;
            }
        }
    }
    INVENTORY_LOG(Log, this, "Stub save: restored %d of %d snapshot(s)", restored, captured.Num());
    return restored;
}
```

### S11.5 — The Character snapshot

**Header changes** to `UPlayerInventoryComponent.h`:

```cpp
#include "ItemInventoryFramework/03Interfaces/BPI_SaveParticipantStub.h"

class AWAKEN_API UPlayerInventoryComponent : public UActorComponent, public IBPI_SaveParticipant{
    GENERATED_BODY()

public:
    inline static constexpr int32 currentSnapshotVersion = 1;

    // ---- IBPI_SaveParticipant (Doc R13) ----
    virtual FName getSaveKey_Implementation() override;
    virtual E_SaveScope getSaveScope_Implementation() override;
    virtual int32 getSnapshotVersion_Implementation() override;
    virtual TArray<uint8> getSnapshotForSave_Implementation() override;
    virtual bool restoreFromSnapshot_Implementation(const TArray<uint8>& snapshot) override;

protected:
    // Chained migration, one step per version. False when a step is missing or the file is newer (Doc R13 3.11).
    static bool migrateSave(FS_PlayerInventorySave& record);
```

Append to `UPlayerInventoryComponent.cpp` (add the includes `UBFL_InventorySave.h` and `Stubs/StubSaveRegistry.h`):

```cpp
// ---------- Section 11: save ----------

FName UPlayerInventoryComponent::getSaveKey_Implementation(){
    return TEXT("Inventory.Character");
}

E_SaveScope UPlayerInventoryComponent::getSaveScope_Implementation(){
    return E_SaveScope::Character;
}

int32 UPlayerInventoryComponent::getSnapshotVersion_Implementation(){
    return currentSnapshotVersion;
}

// Query: never mutates.
TArray<uint8> UPlayerInventoryComponent::getSnapshotForSave_Implementation(){
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    const UInventoryServices* services = UInventoryServices::get(this);
    if(!isServerSide() || registry == nullptr || services == nullptr || personalInventory == nullptr){
        // A Bagless player is never captured: their previous Character file is kept (Doc R13 3.3).
        INVENTORY_LOG(Warning, this, "No inventory snapshot for %s: the player is Bagless or this is not the host", *GetNameSafe(GetOwner()));
        return TArray<uint8>();
    }
    FS_PlayerInventorySave record;
    record.version = currentSnapshotVersion;
    record.isKitGranted = kitGranted;
    record.hotbar = hotbar;
    // A Character file stores the time LEFT of each stack (R01 3.2a amendment).
    UBFL_InventorySave::capture(registry, personalInventory, true, services->getGameTime(), record.bag, record.containedInventories);
    return UBFL_InventorySave::toBytes(FS_PlayerInventorySave::StaticStruct(), &record);
}

bool UPlayerInventoryComponent::migrateSave(FS_PlayerInventorySave& record){
    while(record.version < currentSnapshotVersion){
        switch(record.version){
            // case 0:{ ...upgrade 0 -> 1...; record.version = 1; break; }
            default:{
                return false;   // a missing step: the file is refused
            }
        }
    }
    return record.version == currentSnapshotVersion;   // a NEWER file is refused as well
}

bool UPlayerInventoryComponent::restoreFromSnapshot_Implementation(const TArray<uint8>& snapshot){
    UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    const UInventoryServices* services = UInventoryServices::get(this);
    if(!isServerSide() || registry == nullptr || services == nullptr){
        return false;
    }
    FS_PlayerInventorySave record;
    if(!UBFL_InventorySave::fromBytes(FS_PlayerInventorySave::StaticStruct(), &record, snapshot) || !migrateSave(record)){
        return false;   // refused; nothing changed, and a failed load never overwrites a file (Doc R13 3.11)
    }

    // What the player holds now is replaced by what the file says.
    UBFL_Inventory::releaseAllStaged(getOwnerPlayerState());
    UInventoryData* current = releaseInventory();
    if(current != nullptr){
        registry->destroyInventory(current->getInventoryId());
    }
    clearHoldSlots();
    kitGranted = record.isKitGranted;

    // The one case where the file's bag does not come back: the world already holds this very bag, lying at a
    // death site (Doc R13 3.6, "the world bag wins"). In every other case Doc B06 P4 removed the world bag first.
    const UInventoryData* worldBag = registry->findInventory(record.bag.inventoryId);
    if(worldBag != nullptr && worldBag->isDeathBag()){
        setHotbarBindings(TArray<FS_HotbarBinding>());
        INVENTORY_LOG(Log, this, "%s is restored Bagless: bag %s lies in the world", *GetNameSafe(GetOwner()), *record.bag.inventoryId.ToString());
        return true;
    }

    UInventoryData* bag = UBFL_InventorySave::restore(registry, record.bag, record.containedInventories, services->getGameTime());
    if(bag == nullptr){
        return false;
    }
    attachInventory(bag);
    setHotbarBindings(record.hotbar);
    INVENTORY_LOG(Log, this, "Restored the bag of %s: %d entries", *GetNameSafe(GetOwner()), bag->getEntries().Num());
    return true;
}
```

Register with the stub. In `BeginPlay`, inside the `if(isServerSide()){` block:

```cpp
        UStubSaveRegistry* saveRegistry = UStubSaveRegistry::get(this);
        if(saveRegistry != nullptr){
            saveRegistry->registerParticipant(this);   // when Doc R13 lands, replace with its registration call
        }
```

and at the top of `EndPlay`:

```cpp
    UStubSaveRegistry* saveRegistry = UStubSaveRegistry::get(this);
    if(saveRegistry != nullptr){
        saveRegistry->unregisterParticipant(this);
    }
```

### S11.6 — The World snapshot

**Header changes** to `UInventoryRegistrySubsystem.h`:

```cpp
#include "ItemInventoryFramework/03Interfaces/BPI_SaveParticipantStub.h"

class AWAKEN_API UInventoryRegistrySubsystem : public UWorldSubsystem, public IBPI_SaveParticipant{
    GENERATED_BODY()

public:
    inline static constexpr int32 currentSnapshotVersion = 1;

    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

    // ---- IBPI_SaveParticipant (Doc R13) ----
    virtual FName getSaveKey_Implementation() override;
    virtual E_SaveScope getSaveScope_Implementation() override;
    virtual int32 getSnapshotVersion_Implementation() override;
    virtual TArray<uint8> getSnapshotForSave_Implementation() override;
    virtual bool restoreFromSnapshot_Implementation(const TArray<uint8>& snapshot) override;

private:
    // What belongs in the World file: a world Container, or a bag lying in the world.
    static bool isWorldRoot(const UInventoryData* inventory);
```

Append to `UInventoryRegistrySubsystem.cpp` (add the includes `InventoryServices.h`, `UBFL_Inventory.h`, `UBFL_InventorySave.h`, `Stubs/StubSaveRegistry.h`):

```cpp
// ---------- Section 11: save ----------

void UInventoryRegistrySubsystem::OnWorldBeginPlay(UWorld& InWorld){
    Super::OnWorldBeginPlay(InWorld);
    UStubSaveRegistry* saveRegistry = UStubSaveRegistry::get(this);
    if(saveRegistry != nullptr && UBFL_Inventory::isServerWorld(this)){
        saveRegistry->registerParticipant(this);   // when Doc R13 lands, replace with its registration call
    }
}

bool UInventoryRegistrySubsystem::isWorldRoot(const UInventoryData* inventory){
    return inventory->getInventoryType() == E_InventoryType::Container || (inventory->getInventoryType() == E_InventoryType::Personal && inventory->isDeathBag());
}

FName UInventoryRegistrySubsystem::getSaveKey_Implementation(){
    return TEXT("Inventory.World");
}

E_SaveScope UInventoryRegistrySubsystem::getSaveScope_Implementation(){
    return E_SaveScope::World;
}

int32 UInventoryRegistrySubsystem::getSnapshotVersion_Implementation(){
    return currentSnapshotVersion;
}

// Query: never mutates.
TArray<uint8> UInventoryRegistrySubsystem::getSnapshotForSave_Implementation(){
    const UInventoryServices* services = UInventoryServices::get(this);
    FS_WorldInventorySave record;
    record.version = currentSnapshotVersion;
    const int64 now = services != nullptr ? services->getGameTime() : 0;
    for(const TPair<FGuid, TObjectPtr<UInventoryData>>& pair : inventories){
        if(isWorldRoot(pair.Value)){
            // The World file keeps ABSOLUTE spoil times: it carries its own clock (Doc R13 3.4).
            UBFL_InventorySave::capture(this, pair.Value, false, now, record.inventories.AddDefaulted_GetRef(), record.containedInventories);
        }
    }
    return UBFL_InventorySave::toBytes(FS_WorldInventorySave::StaticStruct(), &record);
}

bool UInventoryRegistrySubsystem::restoreFromSnapshot_Implementation(const TArray<uint8>& snapshot){
    FS_WorldInventorySave record;
    if(!UBFL_Inventory::isServerWorld(this) || !UBFL_InventorySave::fromBytes(FS_WorldInventorySave::StaticStruct(), &record, snapshot) || record.version != currentSnapshotVersion){
        return false;   // unreadable, or a version with no migration step: refused, nothing changed
    }
    // World inventories that the file does not know: a container keeps its (now empty) data, a stray bag goes.
    TArray<FGuid> staleBags;
    for(const TPair<FGuid, TObjectPtr<UInventoryData>>& pair : inventories){
        const FGuid& id = pair.Key;
        const bool isInFile = record.inventories.ContainsByPredicate([&id](const FS_SavedInventory& saved){
            return saved.inventoryId == id;
        });
        if(isWorldRoot(pair.Value) && !isInFile){
            if(pair.Value->isDeathBag()){
                staleBags.Add(id);
            }else{
                pair.Value->clearAll();
            }
        }
    }
    for(const FGuid& id : staleBags){
        destroyInventory(id);
    }
    const UInventoryServices* services = UInventoryServices::get(this);
    const int64 now = services != nullptr ? services->getGameTime() : 0;
    int32 restored = 0;
    for(const FS_SavedInventory& saved : record.inventories){
        restored += UBFL_InventorySave::restore(this, saved, record.containedInventories, now) != nullptr ? 1 : 0;
    }
    INVENTORY_LOG(Log, this, "Restored %d world inventories (containers and bags lying in the world)", restored);
    return true;
}
```

> When version 2 of the World record exists, add a `migrateSave` here in the same shape as the component's.

### S11.7 — Restore order (Doc B06 §3.6): what R01 needs

| B06 step | R01 piece | Requirement |
| --- | --- | --- |
| World restore | `UInventoryRegistrySubsystem::restoreFromSnapshot` | Before any player restore. A container whose actor is not loaded is restored as data and found by its actor later. Doc R12 must have restored the clock first, because spoil times are read against it |
| Player P4 | bag reconciliation (Doc R13 3.6, owned by B06 / R13) | "The saved state wins": B06 removes the world bag owned by this character (the bag service's `deleteBag`, then `registry->destroyInventory`). The one exception, a newer world: B06 leaves the world bag alone |
| Player P5 | `UPlayerInventoryComponent::restoreFromSnapshot` | After P4. If the world still holds this bag as a death bag, the player is restored Bagless (the exception). No order against the other P5 restores: none of them reads the bag while restoring |
| After P5 | `setCharacterId` | B05 / R13 hand us the CharacterID of the loaded character (I23) |

### Verify (Section 11): the round trip

`Tests/InventorySaveSpec.cpp`, tier 2. Call the two interface functions directly on the component and the registry.

1. Build a player with: 12 sticks; two spears, one at durability 7; a crossbow with `isLoaded` true; a pouch holding 3 stones; 4 `RawMeat` with 10 hours left; slot 1 bound to `Spear`, slot 4 to `Stick`; 2 sticks staged; a spear in the hand. Put 5 stones and 2 `RawMeat` in a chest.
2. Capture the Character bytes and the World bytes.
3. Empty everything (clear the bag and the chest, unbind the keys, advance the clock 6 hours), to prove the restore really writes.
4. Restore World, then Character. Compare:
   - the same entries, the same **instance ids**, durability 7, `isLoaded` true; the pouch holds 3 stones;
   - the hotbar is as it was; **no** reservation exists; the hand is empty (the held item is not saved);
   - the bag's meat has **10 hours left from now** (the time left was saved and the spoil time rebuilt), so it spoils 6 hours later than it would have; the chest's meat has the **same absolute** spoil time as before, so it now has 4 hours left.
5. Unknown item: patch one saved `itemId` to `NoSuchItem` and restore: one Warning line, the other entries are restored, the load continues.
6. A record with `version = 99`: `restoreFromSnapshot` returns false and the bag is unchanged.
7. Bagless capture: `detachBag`, then ask the component for a snapshot: empty bytes and a Warning. The World bytes now hold the bag's inventory with `isDeathBag` true.
8. "The world bag wins": capture the Character while holding the bag, `detachBag`, capture the World, restore the World, then restore the **older** Character bytes: the player is Bagless and the registry still holds one death bag, not two.
9. Hold-slot items are not in any snapshot: capture while Bagless with an item in a hold slot, search both blobs for its item id: absent.

Doc B22's starter test "R13 Save round trip" lists inventories in its equal-list; this spec is R01's part of it.

### Handoff

- **R13** replaces the stub interface file and the stub registry; stores the blobs under `Inventory.Character` and `Inventory.World`; confirms that container contents are in the World file and keep absolute times (the open wording noted in the R01 spec).
- **B06** restores in the order of S11.7 and performs P4.
- **B02 / R13** save the bag *records* (owner, location, hotbar snapshot, worn gear). With only the stub bag service, a bag lying in the world keeps its items across a stub round trip but its record lives in memory only.
- **B20**: the 15-minute hold keeps the live state; restoring from it uses the same steps P1 to P6 (Doc B20 3.8).

---
# SECTION 12 — Networking: Requests, Container Views and Messages

**Goal:** a client can only *ask*; the host checks who is asking, from where and in which state, changes the data, and every machine that should see the result sees it.
**Spec:** R01 §6, §7; Doc02 §6, §8, §19.1, §19.2, §19.6, §19.9; Doc B18 3.5, 3.7; Doc B17 3.2, 3.14.
**Depends on:** Section 11.

### Files

```
Public/ItemInventoryFramework/
    UContainerViewComponent.h    UInventoryRequestComponent.h
Private/ItemInventoryFramework/
    UContainerViewComponent.cpp  UInventoryRequestComponent.cpp
    Stubs/AInventoryTestPlayerState.h/.cpp   (patch)
```

### S12.1 — Replication audit (R01 §7, Doc02 §19.1)

Walk this table with a listen server and two clients. Every row must be true.

| State | Replicates to | Mechanism | A late joiner gets it from |
| --- | --- | --- | --- |
| The bag's entries | **the owner only** | the mirror `replicatedBag`, `COND_OwnerOnly`, on the PlayerState component | the mirror; `Inventory.Changed` is then raised locally |
| `bagAttached`, `bagInventoryId`, `characterId` | the owner only | `COND_OwnerOnly` | replicated data |
| Hotbar bindings, the "last used" list | the owner only | `COND_OwnerOnly` | replicated data; `Hotbar.Changed` is raised from `OnRep_hotbar` |
| The three Bagless hold slots | the owner only | `COND_OwnerOnly` | replicated data |
| The held item | **everyone** | `COND_None` on the Character component | replicated data; `onHeldItemChanged` from `OnRep_held` |
| A world container's contents | **only the players who have it open** | each viewer's own mirror `replicatedView`, `COND_OwnerOnly`, on their view component (the per-viewer mirror of the R01 amendment). The real container never replicates | n/a: a view starts when the player opens the container |
| A loose item's entry and position | everyone | the actor replicates | the actor |
| Reservations | nobody. Each entry's `reservedQuantity` rides in the mirror | n/a | n/a |
| The inventory registry | nobody. Server only; a client's registry is empty | n/a | n/a |

Other players learn what a player *holds in the hand* and nothing else about their items.

### S12.2 — `UContainerViewComponent`

One per player, on the PlayerState. It holds a copy of the **one** container that player has open, replicated to that player only. Two players may open the same chest (Doc B18 3.7, Shared): each has their own view, and the server checks every move of each.

`UContainerViewComponent.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ItemInventoryFramework/04Delegates/D_ItemInventoryFramework.h"
#include "UContainerViewComponent.generated.h"

class APlayerState;
class UInventoryData;

// R01 §7: "a Container replicates to the players who have it open".
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class AWAKEN_API UContainerViewComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UContainerViewComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    static UContainerViewComponent* findFor(const APlayerState* player);

    // ---- queries ----
    // An invalid id while nothing is open.
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") FGuid getViewedContainerId() const;
    // Host: the real inventory. Owning client: the local read-only copy. Null while nothing is open.
    UInventoryData* getViewedInventory() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Inventory") TArray<FS_InventoryEntry> getViewedEntries() const;

    // ---- commands (server) ----
    // Starts viewing an inventory of the registry: a world container, or the contents of a Portable Container.
    FS_InventoryResult openContainer(const FGuid& containerId);
    void closeContainer();

    // Raised on the host and the owning client when a view opens (a valid id) or closes (an invalid id).
    UPROPERTY(BlueprintAssignable, Category = "Awaken|Inventory") FD_ContainerViewChanged onViewChanged;

protected:
    UFUNCTION()
    void OnRep_viewedContainerId();

    bool isServerSide() const;
    void handleViewedChanged(UInventoryData* inventory);
    void handleMirrorReceived();
    // Runs twice a second, only while a view is open (Doc02 §5): closes the view when the player has walked away.
    void checkReach();

    // Both replicate to the owner only.
    UPROPERTY(ReplicatedUsing = OnRep_viewedContainerId) FGuid viewedContainerId;
    UPROPERTY(Replicated) FS_InventoryEntryList replicatedView;

    UPROPERTY() TObjectPtr<UInventoryData> viewedInventory;   // host
    UPROPERTY() TObjectPtr<UInventoryData> clientView;        // owning client
    FDelegateHandle changedHandle;
    FTimerHandle reachTimer;
};
```

`UContainerViewComponent.cpp`

```cpp
#include "ItemInventoryFramework/UContainerViewComponent.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UInventoryRequestComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "Engine/World.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UContainerViewComponent::UContainerViewComponent(){
    SetIsReplicatedByDefault(true);
}

void UContainerViewComponent::BeginPlay(){
    Super::BeginPlay();
    if(!isServerSide()){
        clientView = NewObject<UInventoryData>(this);
        clientView->setup(viewedContainerId, E_InventoryType::Container, FS_ContainerRules());
        replicatedView.onReceived.BindUObject(this, &UContainerViewComponent::handleMirrorReceived);
    }
}

void UContainerViewComponent::EndPlay(const EEndPlayReason::Type EndPlayReason){
    closeContainer();
    Super::EndPlay(EndPlayReason);
}

void UContainerViewComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UContainerViewComponent, viewedContainerId, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(UContainerViewComponent, replicatedView, COND_OwnerOnly);
}

UContainerViewComponent* UContainerViewComponent::findFor(const APlayerState* player){
    return player != nullptr ? player->FindComponentByClass<UContainerViewComponent>() : nullptr;
}

bool UContainerViewComponent::isServerSide() const{
    return GetOwner() != nullptr && GetOwner()->HasAuthority();
}

FGuid UContainerViewComponent::getViewedContainerId() const{
    return viewedContainerId;
}

UInventoryData* UContainerViewComponent::getViewedInventory() const{
    if(!viewedContainerId.IsValid()){
        return nullptr;
    }
    return isServerSide() ? viewedInventory.Get() : clientView.Get();
}

TArray<FS_InventoryEntry> UContainerViewComponent::getViewedEntries() const{
    const UInventoryData* inventory = getViewedInventory();
    return inventory != nullptr ? inventory->getEntries() : TArray<FS_InventoryEntry>();
}

FS_InventoryResult UContainerViewComponent::openContainer(const FGuid& containerId){
    if(!isServerSide()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotServer);
    }
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    const UInventoryServices* services = UInventoryServices::get(this);
    UInventoryData* inventory = registry != nullptr ? registry->findInventory(containerId) : nullptr;
    if(inventory == nullptr || services == nullptr){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_NotFound);
    }
    if(inventory->isDeathBag()){
        return UBFL_Inventory::refusal(T_Inventory_FailReason_DeathBagLocked);   // nobody opens a death bag (R01 3.4)
    }
    closeContainer();   // one view at a time
    viewedInventory = inventory;
    viewedContainerId = containerId;
    inventory->evaluateSpoilage(services->getGameTime());   // opening is a request on the stacks (R01 3.2a)
    changedHandle = inventory->onChanged.AddUObject(this, &UContainerViewComponent::handleViewedChanged);
    handleViewedChanged(inventory);
    GetWorld()->GetTimerManager().SetTimer(reachTimer, this, &UContainerViewComponent::checkReach, 0.5f, true);
    onViewChanged.Broadcast(viewedContainerId);
    INVENTORY_LOG(Log, this, "%s opened container %s", *GetNameSafe(GetOwner()), *containerId.ToString());
    return FS_InventoryResult();
}

void UContainerViewComponent::closeContainer(){
    if(!isServerSide() || !viewedContainerId.IsValid()){
        return;
    }
    if(viewedInventory != nullptr){
        viewedInventory->onChanged.Remove(changedHandle);
    }
    changedHandle.Reset();
    if(GetWorld() != nullptr){
        GetWorld()->GetTimerManager().ClearTimer(reachTimer);
    }
    viewedInventory = nullptr;
    viewedContainerId.Invalidate();
    replicatedView.syncFrom(TArray<FS_InventoryEntry>());
    GetOwner()->ForceNetUpdate();
    onViewChanged.Broadcast(viewedContainerId);
}

void UContainerViewComponent::handleViewedChanged(UInventoryData* inventory){
    replicatedView.syncFrom(inventory->getEntries());
    GetOwner()->ForceNetUpdate();
}

void UContainerViewComponent::handleMirrorReceived(){
    if(clientView != nullptr){
        clientView->setup(viewedContainerId, E_InventoryType::Container, FS_ContainerRules());
        clientView->replaceEntries(replicatedView.toEntries());   // raises Inventory.Changed with the container's id, on this machine
    }
}

void UContainerViewComponent::OnRep_viewedContainerId(){
    handleMirrorReceived();
    onViewChanged.Broadcast(viewedContainerId);
}

void UContainerViewComponent::checkReach(){
    APlayerState* player = Cast<APlayerState>(GetOwner());
    const UInventoryRequestComponent* requests = UInventoryRequestComponent::findFor(player);
    if(requests == nullptr || !requests->isContainerReachable(player, viewedContainerId)){
        closeContainer();   // too far away, or the container is gone (Doc B17 3.2)
    }
}
```

### S12.3 to S12.5 — `UInventoryRequestComponent`

The request layer. A client never changes anything: it sends **intent**. For each request the server checks, as relevant (Doc02 §19.6): the rate, the player's state, the hands, range and line of sight. Then it runs the rule of an earlier section, and replies to the requester **only when it refuses**, with the same reason tag the UI turns into text.

Which checks apply to which request:

| Request | Rate | State (not Downed, Restrained, Arriving, in a scene) | Free hands | Range and line of sight | Needs the bag |
| --- | --- | --- | --- | --- | --- |
| `serverBindHotbarSlot` | yes | yes | | | yes |
| `serverSelectHotbarSlot`, `serverHoldFromBag`, `serverPutAway` | yes | yes | | | hold-from-bag only |
| `serverUseConsumable`, `serverUseHoldSlot` | yes | yes | yes | | bag / Bagless |
| `serverDropFromBag`, `serverDropHoldSlot` | yes | yes | | | bag / Bagless |
| `serverPickUp` | yes | yes | yes, except for a heavy item (it goes *to* the hands) | `pickupRange` | |
| `serverOpenContainer`, `serverQuickTransfer` | yes | yes | yes | `containerRange` | yes |
| `serverOpenPortableContainer` | yes | yes | yes | the item is in the player's own bag | yes |
| `serverTransfer` | yes | yes | yes | the viewed container is still reachable | yes |
| `serverCloseContainer` | | | | | |

Range checks are inclusive: exactly at the limit is allowed (Doc02 §19.9). The staging requests are not here: Doc R02 owns them and wraps the functions of Section 7.

`UInventoryRequestComponent.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "ItemInventoryFramework/04Delegates/D_ItemInventoryFramework.h"
#include "UInventoryRequestComponent.generated.h"

class ALooseItemActor;
class APlayerState;

// The only door between a client and the inventory rules (Doc02 §19.6). A PlayerState component:
// the owning client may call its server requests, and no other client can.
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class AWAKEN_API UInventoryRequestComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UInventoryRequestComponent();

    virtual void BeginPlay() override;

    static UInventoryRequestComponent* findFor(const APlayerState* player);

    // ---- requests: the UI or the interaction system calls these on the owning client ----
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverBindHotbarSlot(int32 slotIndex, FName itemId);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverSelectHotbarSlot(int32 slotIndex);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverHoldFromBag(const FS_ItemRef& ref);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverPutAway();
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverUseConsumable(FName itemId);
    // Bagless: slotIndex is the hotbar key, 1 to 3.
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverUseHoldSlot(int32 slotIndex);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverDropHoldSlot(int32 slotIndex);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverDropFromBag(const FS_ItemRef& ref, int32 quantity);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverPickUp(ALooseItemActor* looseItem);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverOpenContainer(AActor* containerActor);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverOpenPortableContainer(FGuid instanceId);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverCloseContainer();
    // Between the bag and the container the player has open.
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverTransfer(const FS_ItemRef& ref, int32 quantity, bool toContainer);
    // A container without a screen: a quick take or store from a stack, with no view (R01 3.4).
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Inventory") void serverQuickTransfer(AActor* containerActor, const FS_ItemRef& ref, int32 quantity, bool toContainer);

    // ---- the shared gate. Public: Equipment (Doc R05) reuses it, so one rate limit covers both frameworks ----
    // Rate limit + player state + hands. An empty tag = allowed (Doc02 §19.6).
    FGameplayTag gate(bool needsHands, bool allowWhenIncapacitated);
    // A world container in range with a clear line, or a Portable Container that is in the player's own bag.
    bool isContainerReachable(APlayerState* player, const FGuid& containerId) const;
    // Logs the reason tag and tells only the requester (Doc02 §6, Doc B22 §3.6).
    void refuse(const FGameplayTag& reason, const TCHAR* operation);

    UFUNCTION(Client, Reliable)
    void clientRefused(FGameplayTag reason);

    // Raised on the requester's machine. Doc B17 shows the one-line refusal from the reason tag (3.14).
    UPROPERTY(BlueprintAssignable, Category = "Awaken|Inventory") FD_InventoryRequestRefused onRequestRefused;

protected:
    APlayerState* getOwnerPlayerState() const;
    // Distance from the pawn's eye (inclusive) and a clear line to the target.
    bool isInReach(const APlayerState* player, const AActor* target, float range) const;
    // Tells the requester only when the result is a refusal.
    void reply(const FS_InventoryResult& result, const TCHAR* operation);
    // The shared body of the two transfer requests.
    void transferWith(const FGuid& containerId, const FS_ItemRef& ref, int32 quantity, bool toContainer, const TCHAR* operation);

    float requestTokens = 0.0f;
    double lastTokenTime = 0.0;
};
```

`UInventoryRequestComponent.cpp`

```cpp
#include "ItemInventoryFramework/UInventoryRequestComponent.h"
#include "ItemInventoryFramework/ALooseItemActor.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UBFL_InventoryBag.h"
#include "ItemInventoryFramework/UContainerComponent.h"
#include "ItemInventoryFramework/UContainerViewComponent.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UInventorySettings.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"

UInventoryRequestComponent::UInventoryRequestComponent(){
    SetIsReplicatedByDefault(true);
}

void UInventoryRequestComponent::BeginPlay(){
    Super::BeginPlay();
    requestTokens = UInventorySettings::get()->requestsPerSecond;
    lastTokenTime = FPlatformTime::Seconds();
}

UInventoryRequestComponent* UInventoryRequestComponent::findFor(const APlayerState* player){
    return player != nullptr ? player->FindComponentByClass<UInventoryRequestComponent>() : nullptr;
}

APlayerState* UInventoryRequestComponent::getOwnerPlayerState() const{
    return Cast<APlayerState>(GetOwner());
}

// ---------- S12.3 the gate ----------

FGameplayTag UInventoryRequestComponent::gate(bool needsHands, bool allowWhenIncapacitated){
    // A token bucket on real time (a rate limit is a short timer, Doc02 §19.4). It refills at requestsPerSecond
    // and holds at most one second's worth.
    const float ratePerSecond = UInventorySettings::get()->requestsPerSecond;
    const double now = FPlatformTime::Seconds();
    requestTokens = FMath::Min(ratePerSecond, requestTokens + static_cast<float>(now - lastTokenTime) * ratePerSecond);
    lastTokenTime = now;
    if(requestTokens < 1.0f){
        return T_Inventory_FailReason_RateLimited;
    }
    requestTokens -= 1.0f;

    const UInventoryServices* services = UInventoryServices::get(this);
    const APlayerState* player = getOwnerPlayerState();
    if(services == nullptr || player == nullptr){
        return T_Inventory_FailReason_NotFound;
    }
    if(!allowWhenIncapacitated && services->isIncapacitated(player)){
        return T_Interaction_FailReason_NotAllowedInState;
    }
    if(needsHands && services->isCarryActive(player)){
        return T_Interaction_FailReason_HandsFull;
    }
    return FGameplayTag();
}

bool UInventoryRequestComponent::isInReach(const APlayerState* player, const AActor* target, float range) const{
    const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
    if(pawn == nullptr || !IsValid(target)){
        return false;
    }
    const FVector eye = pawn->GetPawnViewLocation();
    const FVector targetLocation = target->GetActorLocation();
    if(FVector::Dist(eye, targetLocation) > range){   // inclusive: exactly at the limit is allowed (Doc02 §19.9)
        return false;
    }
    FHitResult hit;
    FCollisionQueryParams parameters(SCENE_QUERY_STAT(InventoryReach), false, pawn);
    const bool isBlocked = GetWorld()->LineTraceSingleByChannel(hit, eye, targetLocation, ECC_Visibility, parameters);
    return !isBlocked || hit.GetActor() == target;
}

bool UInventoryRequestComponent::isContainerReachable(APlayerState* player, const FGuid& containerId) const{
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    if(registry == nullptr || !containerId.IsValid()){
        return false;
    }
    const UContainerComponent* host = registry->findHost(containerId);
    if(host != nullptr){
        return isInReach(player, host->GetOwner(), UInventorySettings::get()->containerRange);
    }
    // Not a loaded world container: it is reachable only as the contents of a Portable Container in the player's own bag.
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    if(bag == nullptr){
        return false;
    }
    return bag->getEntries().ContainsByPredicate([&containerId](const FS_InventoryEntry& entry){
        return entry.state.containedInventoryId == containerId;
    });
}

void UInventoryRequestComponent::refuse(const FGameplayTag& reason, const TCHAR* operation){
    INVENTORY_LOG(Log, this, "%s of %s refused: %s", operation, *GetNameSafe(GetOwner()), *reason.ToString());
    clientRefused(reason);
}

void UInventoryRequestComponent::clientRefused_Implementation(FGameplayTag reason){
    onRequestRefused.Broadcast(reason);
}

void UInventoryRequestComponent::reply(const FS_InventoryResult& result, const TCHAR* operation){
    if(!result.succeeded()){
        refuse(result.failReason, operation);
    }
}

// ---------- S12.4 hotbar, hand, consumables, drop, pickup ----------

void UInventoryRequestComponent::serverBindHotbarSlot_Implementation(int32 slotIndex, FName itemId){
    const FGameplayTag blocked = gate(false, false);
    UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getOwnerPlayerState());
    if(blocked.IsValid() || inventory == nullptr){
        refuse(blocked.IsValid() ? blocked : T_Inventory_FailReason_NotFound.GetTag(), TEXT("bindHotbarSlot"));
        return;
    }
    reply(inventory->bindHotbarSlot(slotIndex, itemId), TEXT("bindHotbarSlot"));
}

void UInventoryRequestComponent::serverSelectHotbarSlot_Implementation(int32 slotIndex){
    const FGameplayTag blocked = gate(false, false);
    UHeldItemComponent* heldItem = UHeldItemComponent::findFor(getOwnerPlayerState());
    if(blocked.IsValid() || heldItem == nullptr){
        refuse(blocked.IsValid() ? blocked : T_Inventory_FailReason_NotFound.GetTag(), TEXT("selectHotbarSlot"));
        return;
    }
    reply(heldItem->selectHotbarSlot(slotIndex), TEXT("selectHotbarSlot"));
}

void UInventoryRequestComponent::serverHoldFromBag_Implementation(const FS_ItemRef& ref){
    const FGameplayTag blocked = gate(false, false);
    UHeldItemComponent* heldItem = UHeldItemComponent::findFor(getOwnerPlayerState());
    if(blocked.IsValid() || heldItem == nullptr){
        refuse(blocked.IsValid() ? blocked : T_Inventory_FailReason_NotFound.GetTag(), TEXT("holdFromBag"));
        return;
    }
    reply(heldItem->holdFromBag(ref), TEXT("holdFromBag"));
}

void UInventoryRequestComponent::serverPutAway_Implementation(){
    const FGameplayTag blocked = gate(false, false);
    UHeldItemComponent* heldItem = UHeldItemComponent::findFor(getOwnerPlayerState());
    if(blocked.IsValid()){
        refuse(blocked, TEXT("putAway"));
        return;
    }
    if(heldItem != nullptr){
        heldItem->putAway();
    }
}

void UInventoryRequestComponent::serverUseConsumable_Implementation(FName itemId){
    const FGameplayTag blocked = gate(true, false);   // busy hands cannot eat (Index A103, S4-F36)
    if(blocked.IsValid()){
        refuse(blocked, TEXT("useConsumable"));
        return;
    }
    reply(UBFL_Inventory::useConsumable(getOwnerPlayerState(), itemId), TEXT("useConsumable"));
}

void UInventoryRequestComponent::serverUseHoldSlot_Implementation(int32 slotIndex){
    const FGameplayTag blocked = gate(true, false);
    if(blocked.IsValid()){
        refuse(blocked, TEXT("useHoldSlot"));
        return;
    }
    reply(UBFL_InventoryBag::useConsumableFromHoldSlot(getOwnerPlayerState(), slotIndex - 1), TEXT("useHoldSlot"));
}

void UInventoryRequestComponent::serverDropHoldSlot_Implementation(int32 slotIndex){
    const FGameplayTag blocked = gate(false, false);
    if(blocked.IsValid()){
        refuse(blocked, TEXT("dropHoldSlot"));
        return;
    }
    reply(UBFL_InventoryBag::dropHoldSlot(getOwnerPlayerState(), slotIndex - 1), TEXT("dropHoldSlot"));
}

void UInventoryRequestComponent::serverDropFromBag_Implementation(const FS_ItemRef& ref, int32 quantity){
    const FGameplayTag blocked = gate(false, false);
    if(blocked.IsValid()){
        refuse(blocked, TEXT("dropFromBag"));
        return;
    }
    reply(UBFL_Inventory::dropFromBag(getOwnerPlayerState(), ref, quantity), TEXT("dropFromBag"));
}

void UInventoryRequestComponent::serverPickUp_Implementation(ALooseItemActor* looseItem){
    APlayerState* player = getOwnerPlayerState();
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UDA_ItemDefinition* definition = (IsValid(looseItem) && catalog != nullptr) ? catalog->getDefinition(looseItem->getEntry().itemId) : nullptr;
    // A heavy item goes TO the hands, so carrying does not block the request itself: Carry answers whether a slot is free.
    const bool needsHands = definition == nullptr || !definition->isHeavy();
    const FGameplayTag blocked = gate(needsHands, false);
    if(blocked.IsValid()){
        refuse(blocked, TEXT("pickUp"));
        return;
    }
    if(!isInReach(player, looseItem, UInventorySettings::get()->pickupRange)){
        refuse(T_Inventory_FailReason_OutOfRange, TEXT("pickUp"));
        return;
    }
    reply(UBFL_Inventory::pickUpLooseItem(player, looseItem), TEXT("pickUp"));
}

// ---------- S12.5 containers ----------

void UInventoryRequestComponent::serverOpenContainer_Implementation(AActor* containerActor){
    APlayerState* player = getOwnerPlayerState();
    const FGameplayTag blocked = gate(true, false);
    if(blocked.IsValid()){
        refuse(blocked, TEXT("openContainer"));
        return;
    }
    const UContainerComponent* container = IsValid(containerActor) ? containerActor->FindComponentByClass<UContainerComponent>() : nullptr;
    UContainerViewComponent* view = UContainerViewComponent::findFor(player);
    if(container == nullptr || view == nullptr){
        refuse(T_Inventory_FailReason_NotFound, TEXT("openContainer"));
        return;
    }
    if(!UBFL_InventoryBag::hasBag(player)){
        refuse(T_Inventory_FailReason_NoBag, TEXT("openContainer"));   // a Bagless player cannot use container storage (R01 3.8)
        return;
    }
    if(!isContainerReachable(player, container->getContainerId())){
        refuse(T_Inventory_FailReason_OutOfRange, TEXT("openContainer"));
        return;
    }
    reply(view->openContainer(container->getContainerId()), TEXT("openContainer"));
}

void UInventoryRequestComponent::serverOpenPortableContainer_Implementation(FGuid instanceId){
    APlayerState* player = getOwnerPlayerState();
    const FGameplayTag blocked = gate(true, false);
    if(blocked.IsValid()){
        refuse(blocked, TEXT("openPortableContainer"));
        return;
    }
    UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UContainerViewComponent* view = UContainerViewComponent::findFor(player);
    const UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    if(bag == nullptr){
        refuse(T_Inventory_FailReason_NoBag, TEXT("openPortableContainer"));
        return;
    }
    const FS_InventoryEntry* entry = bag->findInstance(instanceId);
    // The contents are created the first time the container is opened, with the rules of its item definition.
    const UInventoryData* contained = (entry != nullptr && registry != nullptr) ? registry->getContainedInventory(*entry, true) : nullptr;
    if(contained == nullptr || view == nullptr){
        refuse(T_Inventory_FailReason_NotFound, TEXT("openPortableContainer"));
        return;
    }
    reply(view->openContainer(contained->getInventoryId()), TEXT("openPortableContainer"));
}

void UInventoryRequestComponent::serverCloseContainer_Implementation(){
    UContainerViewComponent* view = UContainerViewComponent::findFor(getOwnerPlayerState());
    if(view != nullptr){
        view->closeContainer();
    }
}

void UInventoryRequestComponent::transferWith(const FGuid& containerId, const FS_ItemRef& ref, int32 quantity, bool toContainer, const TCHAR* operation){
    APlayerState* player = getOwnerPlayerState();
    const FGameplayTag blocked = gate(true, false);
    if(blocked.IsValid()){
        refuse(blocked, operation);
        return;
    }
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    UInventoryData* container = registry != nullptr ? registry->findInventory(containerId) : nullptr;
    if(bag == nullptr){
        refuse(T_Inventory_FailReason_NoBag, operation);
        return;
    }
    if(container == nullptr){
        refuse(T_Inventory_FailReason_NotFound, operation);
        return;
    }
    // Checked again on every move: the player may have walked away since the container was opened (Doc02 §19.6).
    if(!isContainerReachable(player, containerId)){
        refuse(T_Inventory_FailReason_OutOfRange, operation);
        return;
    }
    reply(UBFL_Inventory::transferEntry(toContainer ? bag : container, toContainer ? container : bag, ref, quantity), operation);
}

void UInventoryRequestComponent::serverTransfer_Implementation(const FS_ItemRef& ref, int32 quantity, bool toContainer){
    const UContainerViewComponent* view = UContainerViewComponent::findFor(getOwnerPlayerState());
    transferWith(view != nullptr ? view->getViewedContainerId() : FGuid(), ref, quantity, toContainer, TEXT("transfer"));
}

void UInventoryRequestComponent::serverQuickTransfer_Implementation(AActor* containerActor, const FS_ItemRef& ref, int32 quantity, bool toContainer){
    const UContainerComponent* container = IsValid(containerActor) ? containerActor->FindComponentByClass<UContainerComponent>() : nullptr;
    transferWith(container != nullptr ? container->getContainerId() : FGuid(), ref, quantity, toContainer, TEXT("quickTransfer"));
}
```

> **Why the requests can live on a PlayerState component.** A client may call a server request on an actor its connection owns. A PlayerState is owned by its PlayerController, so the owning client can call these and no other client can. If the two-player check logs *"No owning connection for actor"*, your real PlayerState is not owned by the controller: tell me, and we move the requests to a PlayerController component.

> **A request about another player's things is impossible by construction:** every request acts on `getOwnerPlayerState()`, never on a player named in its arguments.

### S12.6 — Finish the cleanup and the stub PlayerState

In `UBFL_Inventory::handlePlayerLeft` (Section 7), replace the comment line with (add `#include "ItemInventoryFramework/UContainerViewComponent.h"`):

```cpp
    UContainerViewComponent* view = UContainerViewComponent::findFor(player);
    if(view != nullptr){
        view->closeContainer();
    }
```

In `AInventoryTestPlayerState` add the two components (owner: Doc B19; the real PlayerState gets the same lines):

```cpp
// header
class UContainerViewComponent;
class UInventoryRequestComponent;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UInventoryRequestComponent> requestComponent;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UContainerViewComponent> containerViewComponent;

// constructor (.cpp), with the two includes
    requestComponent = CreateDefaultSubobject<UInventoryRequestComponent>(TEXT("InventoryRequests"));
    containerViewComponent = CreateDefaultSubobject<UContainerViewComponent>(TEXT("ContainerView"));
```

### S12.7 — Message delivery (R01 §6, Doc02 §19.2)

A Gameplay Message is local to the machine that sends it. This is how each of ours reaches every machine that needs it:

| Message | Payload | Delivery | How it is met |
| --- | --- | --- | --- |
| `Inventory.Changed` | `FS_InventoryChangedMessage` (the inventory id) | **local on every machine**, raised from the replicated inventory | the host raises it in `UInventoryData::notifyChanged`; a client raises it from the same function when its copy is refreshed from the mirror (the bag, or the open container) |
| `Inventory.GrantDropped` | `FS_GrantDroppedMessage` | **server only, then local to the owning player** | the host raises it in `reportDropped`; `clientGrantDropped` raises it on the owner's machine |
| `Hotbar.Changed` | `FS_HotbarChangedMessage` | **local on every machine** that holds the bindings | the host raises it when a binding or a hold slot changes; the owning client raises it from `OnRep_hotbar` |
| `Cosmetic.Event.Interaction.ItemDropped` | the cosmetic event of Doc B21 | **needs the cosmetic channel** | `raiseItemDroppedCosmetic` (S5.7) |

Notes for the listeners:

- **UI (Doc B17)** filters `Inventory.Changed` by id: the local player's `bagInventoryId`, or `getViewedContainerId()`. On a client only those two ever arrive.
- A refusal is not a message: it arrives on the delegate `onRequestRefused` of the requester's own request component, with the reason tag.
- A client's hand item for *other* players arrives through `onHeldItemChanged` of that Character's component.

### S12.8 — When a player leaves (Doc B20 3.6)

`UBFL_Inventory::handlePlayerLeft(player)` releases what this framework holds for the leaving player: every staging reservation and the open container view. It runs from the component's `EndPlay`, and Doc B20 calls it on `Session.PlayerLeft` before it takes the bag for the 15-minute hold. The hold-slot items of a Bagless player stay in the held state while the session runs (Doc B20 3.8).

### Verify (Section 12)

Listen server + one client (*Play As Listen Server*, 2 players), `TestMap_Inventory`. Until the real UI exists, bind a few keys in a test Blueprint on the PlayerController: *Get Player State → Get Component by Class (InventoryRequestComponent)* → the request.

| Check | Expect |
| --- | --- |
| The client walks to a loose `Stick × 5` and calls `serverPickUp` | the client's bag shows 5 sticks; the loose item disappears on **both** machines; a second client that asks that PlayerState for its bag gets nothing, because the mirror is owner-only |
| `serverPickUp` from 10 metres away | refused `Inventory.FailReason.OutOfRange`; **only** the client's `onRequestRefused` fires; no state differs anywhere |
| `serverPickUp` with a wall between | refused `OutOfRange` |
| `serverPickUp` with the bag at its limit | refused `Interaction.FailReason.BagFull`; the item stays |
| The client calls `serverOpenContainer(chest)` | the client's `getViewedEntries()` shows the chest; `onViewChanged` fired with the chest's id; the host's own view is unaffected |
| The host opens the same chest and stores 3 stones | the **client's** view shows the 3 stones at once; each machine raised `Inventory.Changed` with the chest's id |
| The client `serverTransfer(Stick × 2, toContainer = true)` | bag −2, chest +2 on the client; the host sees the chest change |
| The client walks away with the chest open | within about half a second the view closes on both sides; a late `serverTransfer` is refused `NotFound` |
| A Bagless client calls `serverOpenContainer` | refused `Inventory.FailReason.NoBag` |
| `serverOpenPortableContainer(pouch)` then a transfer into it | the pouch's contents show; putting a second pouch in is refused `NestedContainer` |
| The client carrying a heavy item (stub flag) calls `serverUseConsumable` | refused `Interaction.FailReason.HandsFull` |
| The client Downed (stub flag) calls any request | refused `Interaction.FailReason.NotAllowedInState` |
| The client sends 100 `serverBindHotbarSlot` in one frame | about `requestsPerSecond` are accepted, the rest are `RateLimited` |
| `serverBindHotbarSlot(1, Spear)` then `serverSelectHotbarSlot(1)` on the client | the client's hotbar changes for the client only; the spear in the hand is seen by **everyone** |
| A **late joiner** connects while the client holds a spear and has 20 items | the newcomer sees the spear in the client's hand, from replicated data; its own bag, hotbar and hold slots arrive the same way; no message from before the join is needed |
| Client 2 tries a request on client 1's component | impossible: the call is dropped, client 2 does not own that PlayerState |
| `grantOrDrop` on the host for the client with a full bag | the client's machine raises `Inventory.GrantDropped`; the host's raises it once, not twice |
| Offline (standalone) | every request works the same; the machine is its own host |

### Handoff

- **B17** calls the requests of S12.4 and S12.5 from its buttons (assign to hotbar, discard, use), listens as described in S12.7, and maps every reason tag through its reason table; add text for the `Inventory.FailReason.*` tags of S1.8.
- **B18 / B11**: the pickup interaction calls `serverPickUp(looseItem)`; the `Interaction.Container` verb calls `serverOpenContainer(actor)`, or `serverQuickTransfer` for a container whose `hasScreen` is false.
- **R05**: its request functions call `gate`, `refuse` and `isContainerReachable` (its guide, E9); they are already public here.
- **B19**: put `UInventoryRequestComponent` and `UContainerViewComponent` on the real PlayerState.
- **B20**: call `UBFL_Inventory::handlePlayerLeft(player)` on `Session.PlayerLeft`.

---
# SECTION 13 — Dev Tools, Tests and Final Acceptance

**Goal:** the closing "Dev Tools and Testing" section that Doc B22 §3.10 requires of every implementation guide.
**Spec:** Doc B22 §3.2 to §3.10.
**Depends on:** Sections 0–12.

### Files

```
Private/ItemInventoryFramework/DevTools/InventoryDevCommands.cpp   (non-shipping only)
Private/ItemInventoryFramework/DevTools/InventoryDebugView.cpp     (non-shipping only)
Private/ItemInventoryFramework/Tests/
    InventoryTestFixture.h/.cpp      (from S4.9)
    InventoryDataSpec.cpp            (from S4.9)
    InventorySpec.cpp                (S13.5)
    InventorySlotSpec.cpp            (Section 10's table)
    InventorySaveSpec.cpp            (Section 11's steps)
Content/Awaken/Tests/TestMap_Inventory.umap
```

### S13.1, S13.2 — Dev commands (Doc B22 §3.2, §3.4)

The rules (B22 §3.2): every name starts `Awaken.<Domain>.<Verb>`; a command that changes something runs **on the host only**; it goes **through this framework's public functions**, so the same checks and messages run as in play; it writes one line to `LogAwakenInventory` saying what happened; **none saves**; they are compiled out of Shipping. The optional last argument `pN` picks player N of the player list (default 0), so the host can act on a connected client.

`InventoryDevCommands.cpp`

```cpp
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "ItemInventoryFramework/InventoryLog.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/InventoryTime.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UBFL_InventoryBag.h"
#include "ItemInventoryFramework/UBFL_InventoryValidation.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/03Interfaces/BPI_BagActorService.h"
#include "ItemInventoryFramework/Stubs/InventoryStubs.h"
#include "ItemInventoryFramework/Stubs/StubSaveRegistry.h"

namespace InventoryDev{
    bool isPlayerArg(const FString& argument){
        return argument.Len() > 1 && argument.StartsWith(TEXT("p"), ESearchCase::CaseSensitive) && argument.Mid(1).IsNumeric();
    }

    // The arguments that are not the player selector, in order.
    TArray<FString> plainArgs(const TArray<FString>& arguments){
        TArray<FString> plain;
        for(const FString& argument : arguments){
            if(!isPlayerArg(argument)){
                plain.Add(argument);
            }
        }
        return plain;
    }

    // Null, with a log line, when this is not the host or the player does not exist.
    APlayerState* findPlayer(UWorld* world, const TArray<FString>& arguments){
        if(world == nullptr || !UBFL_Inventory::isServerWorld(world)){
            INVENTORY_LOG(Warning, world, "Host command: refused on a client (the Doc B22 cheat manager forwards it)");
            return nullptr;
        }
        int32 playerIndex = 0;
        for(const FString& argument : arguments){
            if(isPlayerArg(argument)){
                playerIndex = FCString::Atoi(*argument.Mid(1));
            }
        }
        const AGameStateBase* gameState = world->GetGameState();
        if(gameState == nullptr || !gameState->PlayerArray.IsValidIndex(playerIndex)){
            INVENTORY_LOG(Warning, world, "No player %d", playerIndex);
            return nullptr;
        }
        return gameState->PlayerArray[playerIndex];
    }

    FS_ItemStack makeStack(const FString& itemId, int32 quantity){
        FS_ItemStack stack;
        stack.itemId = FName(*itemId);
        stack.quantity = quantity;
        return stack;
    }

    // Awaken.Item.Give <itemId> [amount] [pN]. Adds things, so it is not safe to repeat.
    void give(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = findPlayer(world, arguments);
        const TArray<FString> plain = plainArgs(arguments);
        if(player == nullptr || plain.Num() < 1){
            INVENTORY_LOG(Warning, world, "Usage: Awaken.Item.Give <itemId> [amount] [pN]");
            return;
        }
        const int32 amount = plain.Num() > 1 ? FMath::Max(1, FCString::Atoi(*plain[1])) : 1;
        const FS_GrantResult result = UBFL_Inventory::grantOrDrop(player, makeStack(plain[0], amount));   // a full bag drops it (Doc02 §19.5)
        INVENTORY_LOG(Log, world, "Give %s x%d: stored %d, carried %d, dropped %d", *plain[0], amount, result.quantityStored, result.quantityCarried, result.quantityDropped);
    }

    // Awaken.Item.FillBag <itemId> [pN]. The situation most likely to break: a type at its limit, plus three more.
    void fillBag(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = findPlayer(world, arguments);
        const TArray<FString> plain = plainArgs(arguments);
        const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(world);
        const UDA_ItemDefinition* definition = (catalog != nullptr && plain.Num() > 0) ? catalog->getDefinition(FName(*plain[0])) : nullptr;
        if(player == nullptr || definition == nullptr){
            INVENTORY_LOG(Warning, world, "Usage: Awaken.Item.FillBag <itemId> [pN] (unknown item or no player)");
            return;
        }
        const FS_GrantResult result = UBFL_Inventory::grantOrDrop(player, makeStack(plain[0], definition->perTypeLimit + 3));
        INVENTORY_LOG(Log, world, "FillBag %s (limit %d): stored %d, dropped %d as loose items", *plain[0], definition->perTypeLimit, result.quantityStored, result.quantityDropped);
    }

    // Awaken.Item.SpawnLoose <itemId> [amount]. A loose item in front of player 0.
    void spawnLoose(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = findPlayer(world, arguments);
        const TArray<FString> plain = plainArgs(arguments);
        const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(world);
        const UInventoryServices* services = UInventoryServices::get(world);
        const UDA_ItemDefinition* definition = (catalog != nullptr && plain.Num() > 0) ? catalog->getDefinition(FName(*plain[0])) : nullptr;
        if(player == nullptr || services == nullptr || definition == nullptr){
            INVENTORY_LOG(Warning, world, "Usage: Awaken.Item.SpawnLoose <itemId> [amount] (unknown item or no player)");
            return;
        }
        FS_InventoryEntry entry = definition->isInstanced() ? UBFL_Inventory::makeFreshInstance(world, *definition) : FS_InventoryEntry();
        entry.itemId = definition->itemId;
        entry.totalQuantity = definition->isInstanced() ? 1 : (plain.Num() > 1 ? FMath::Max(1, FCString::Atoi(*plain[1])) : 1);
        if(definition->canSpoil){
            entry.spoilAtTime = services->getGameTime() + InventoryTime::hoursToGameSeconds(definition->spoilTimeHours);
        }
        services->spawnLooseItem(player, entry, UBFL_Inventory::getDropLocation(player));
        INVENTORY_LOG(Log, world, "SpawnLoose %s x%d", *plain[0], entry.totalQuantity);
    }

    // Awaken.Item.ClearBag [pN]. Keeps the Axe and the map (Doc B22 §3.4: a dev command uses the normal rule). Safe to repeat.
    void clearBag(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = findPlayer(world, arguments);
        const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
        const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(world);
        UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(world);
        UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
        if(bag == nullptr || catalog == nullptr || registry == nullptr){
            INVENTORY_LOG(Log, world, "ClearBag: nothing to do (no player or no bag)");
            return;
        }
        UBFL_Inventory::releaseAllStaged(player);
        const TArray<FS_InventoryEntry> entries = bag->getEntries();   // a copy: removing changes the array
        int32 removed = 0;
        for(const FS_InventoryEntry& entry : entries){
            const UDA_ItemDefinition* definition = catalog->getDefinition(entry.itemId);
            if(definition != nullptr && definition->isPermanent()){
                continue;
            }
            if(entry.isInstanced()){
                registry->destroyInventory(entry.state.containedInventoryId);   // the contents of a pouch go with it
                bag->removeInstance(entry.instanceId);
            }else{
                bag->removeStack(entry.itemId, entry.totalQuantity);
            }
            removed++;
        }
        INVENTORY_LOG(Log, world, "ClearBag: removed %d entries, kept the permanent items", removed);
    }

    // Awaken.Bag.DropHere [pN] and Awaken.Bag.SetBagless [pN].
    void dropBag(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = findPlayer(world, arguments);
        if(player == nullptr){
            return;
        }
        const APawn* pawn = player->GetPawn();
        const FS_BagDetachResult result = UBFL_InventoryBag::detachBag(player, pawn != nullptr ? pawn->GetActorLocation() : FVector::ZeroVector);
        INVENTORY_LOG(Log, world, "Bag drop for %s: %s", *player->GetPlayerName(), result.succeeded() ? *result.bagId.ToString() : *result.failReason.ToString());
    }

    // Awaken.Bag.Recover [pN]. Recovers the bag that this player's character left in the world.
    void recoverBag(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = findPlayer(world, arguments);
        const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
        const UInventoryServices* services = UInventoryServices::get(world);
        UObject* bagService = services != nullptr ? services->getBagService() : nullptr;
        if(inventory == nullptr || bagService == nullptr){
            return;
        }
        const FS_BagInfo info = IBPI_BagActorService::Execute_findBagOwnedBy(bagService, inventory->getCharacterId());
        if(!info.isValid){
            INVENTORY_LOG(Log, world, "Recover: %s has no bag lying in the world", *player->GetPlayerName());
            return;
        }
        const FS_InventoryResult result = UBFL_InventoryBag::reattachBag(player, info.bagId);
        INVENTORY_LOG(Log, world, "Recover for %s: %s", *player->GetPlayerName(), result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    // Awaken.Hotbar.Bind <slot> <itemId|none> [pN] and Awaken.Hotbar.Select <slot> [pN].
    void bindSlot(const TArray<FString>& arguments, UWorld* world){
        UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(findPlayer(world, arguments));
        const TArray<FString> plain = plainArgs(arguments);
        if(inventory == nullptr || plain.Num() < 2){
            INVENTORY_LOG(Warning, world, "Usage: Awaken.Hotbar.Bind <slot> <itemId|none> [pN]");
            return;
        }
        const FName itemId = plain[1].Equals(TEXT("none"), ESearchCase::IgnoreCase) ? FName() : FName(*plain[1]);
        const FS_InventoryResult result = inventory->bindHotbarSlot(FCString::Atoi(*plain[0]), itemId);
        INVENTORY_LOG(Log, world, "Bind slot %s to %s: %s", *plain[0], *plain[1], result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    void selectSlot(const TArray<FString>& arguments, UWorld* world){
        UHeldItemComponent* heldItem = UHeldItemComponent::findFor(findPlayer(world, arguments));
        const TArray<FString> plain = plainArgs(arguments);
        if(heldItem == nullptr || plain.Num() < 1){
            INVENTORY_LOG(Warning, world, "Usage: Awaken.Hotbar.Select <slot> [pN] (the player needs a pawn)");
            return;
        }
        const FS_InventoryResult result = heldItem->selectHotbarSlot(FCString::Atoi(*plain[0]));
        INVENTORY_LOG(Log, world, "Select slot %s: %s", *plain[0], result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    // Awaken.Item.Dump (read). Runs on any machine and prints what THIS machine knows.
    void dump(const TArray<FString>& arguments, UWorld* world){
        const AGameStateBase* gameState = world != nullptr ? world->GetGameState() : nullptr;
        if(gameState == nullptr){
            return;
        }
        for(const APlayerState* player : gameState->PlayerArray){
            const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
            if(inventory == nullptr){
                continue;
            }
            const FS_HeldItem held = UBFL_Inventory::getHeldState(player);
            INVENTORY_LOG(Log, world, "%s: bag %s, %d entries, usable slots %d, hand %s%s", *player->GetPlayerName(), inventory->hasBag() ? TEXT("yes") : TEXT("NO (Bagless)"), inventory->getBagEntries().Num(), inventory->getUsableHotbarSlotCount(), held.isEmpty() ? TEXT("empty") : *held.itemId.ToString(), held.isWielded ? TEXT(" (wielded)") : TEXT(""));
            for(const FS_InventoryEntry& entry : inventory->getBagEntries()){
                INVENTORY_LOG(Log, world, "   %s x%d (reserved %d) durability %.1f spoilAt %lld %s", *entry.itemId.ToString(), entry.totalQuantity, entry.reservedQuantity, entry.state.durability, entry.spoilAtTime, entry.isInstanced() ? *entry.instanceId.ToString().Left(8) : TEXT(""));
            }
            for(const FS_HotbarBinding& binding : inventory->getHotbarBindings()){
                if(!binding.boundItemId.IsNone()){
                    INVENTORY_LOG(Log, world, "   key %d -> %s", binding.slotIndex, *binding.boundItemId.ToString());
                }
            }
            const TArray<FS_BaglessHoldSlot> holdSlots = inventory->getHoldSlots();
            for(int32 index = 0; index < holdSlots.Num(); index++){
                if(holdSlots[index].isOccupied){
                    INVENTORY_LOG(Log, world, "   hold slot (key %d): %s", index + 1, *holdSlots[index].entry.itemId.ToString());
                }
            }
        }
        const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(world);
        if(registry != nullptr){
            INVENTORY_LOG(Log, world, "Registry: %d inventories (0 on a client)", registry->getAllInventories().Num());
        }
    }

    // Awaken.Item.Validate (read). Every item's own check plus the catalog checks of S10.4.
    void validate(const TArray<FString>& arguments, UWorld* world){
        TArray<FString> errors;
        const int32 count = UBFL_InventoryValidation::validateCatalog(world, errors);
        for(const FString& error : errors){
            INVENTORY_LOG(Error, world, "Validate: %s", *error);
        }
        INVENTORY_LOG(Log, world, "Validate: %d error(s)", count);
    }

    // ---- TEMPORARY commands for the stubs. Delete each with its stub (S13.8) ----

    // Awaken.Inventory.StubClock <hours>. Moves the stub game clock forward. Owner: Doc R12 (Awaken.Time.*).
    void stubClock(const TArray<FString>& arguments, UWorld* world){
        const UInventoryServices* services = UInventoryServices::get(world);
        UStubGameClock* clock = services != nullptr ? Cast<UStubGameClock>(services->getSeam(UBPI_GameClock::StaticClass())) : nullptr;
        if(clock == nullptr || arguments.Num() < 1 || !UBFL_Inventory::isServerWorld(world)){
            INVENTORY_LOG(Warning, world, "Usage: Awaken.Inventory.StubClock <hours> (host only; does nothing once Doc R12's clock is registered)");
            return;
        }
        clock->gameTime += InventoryTime::hoursToGameSeconds(FCString::Atof(*arguments[0]));
        INVENTORY_LOG(Log, world, "Stub clock is now at %lld game seconds", clock->gameTime);
    }

    // Awaken.Inventory.StubFlag <carrying|downed> <0|1> [pN]. Owners: Doc B07 and Docs B02, R03.
    void stubFlag(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = findPlayer(world, arguments);
        const TArray<FString> plain = plainArgs(arguments);
        const UInventoryServices* services = UInventoryServices::get(world);
        if(player == nullptr || services == nullptr || plain.Num() < 2){
            INVENTORY_LOG(Warning, world, "Usage: Awaken.Inventory.StubFlag <carrying|downed> <0|1> [pN]");
            return;
        }
        const bool isOn = FCString::Atoi(*plain[1]) != 0;
        UStubCarryService* carry = Cast<UStubCarryService>(services->getSeam(UBPI_CarryService::StaticClass()));
        UStubPlayerStateQuery* state = Cast<UStubPlayerStateQuery>(services->getSeam(UBPI_PlayerStateQuery::StaticClass()));
        TSet<TWeakObjectPtr<APlayerState>>* flagged = plain[0] == TEXT("carrying") ? (carry != nullptr ? &carry->carryingPlayers : nullptr) : (state != nullptr ? &state->incapacitatedPlayers : nullptr);
        if(flagged == nullptr){
            INVENTORY_LOG(Log, world, "StubFlag: that stub is no longer registered");
            return;
        }
        if(isOn){
            flagged->Add(player);
        }else{
            flagged->Remove(player);
        }
        INVENTORY_LOG(Log, world, "StubFlag %s = %d for %s", *plain[0], isOn ? 1 : 0, *player->GetPlayerName());
    }

    // Awaken.Inventory.StubSave and Awaken.Inventory.StubLoad. An in-memory round trip. Owner: Doc R13 (Awaken.Save.*).
    void stubSave(const TArray<FString>& arguments, UWorld* world){
        UStubSaveRegistry* saveRegistry = UStubSaveRegistry::get(world);
        if(saveRegistry != nullptr && UBFL_Inventory::isServerWorld(world)){
            saveRegistry->captureAll();   // kept in memory only: nothing is written to disk
        }
    }

    void stubLoad(const TArray<FString>& arguments, UWorld* world){
        UStubSaveRegistry* saveRegistry = UStubSaveRegistry::get(world);
        if(saveRegistry != nullptr && UBFL_Inventory::isServerWorld(world)){
            saveRegistry->restoreAll();
        }
    }

    FAutoConsoleCommandWithWorldAndArgs cmdGive(TEXT("Awaken.Item.Give"), TEXT("<itemId> [amount] [pN]: grant through grantOrDrop, so a full bag drops it"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&give));
    FAutoConsoleCommandWithWorldAndArgs cmdFillBag(TEXT("Awaken.Item.FillBag"), TEXT("<itemId> [pN]: grant the per-type limit plus three (the full-bag situation)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&fillBag));
    FAutoConsoleCommandWithWorldAndArgs cmdSpawnLoose(TEXT("Awaken.Item.SpawnLoose"), TEXT("<itemId> [amount]: a loose item in front of the player"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&spawnLoose));
    FAutoConsoleCommandWithWorldAndArgs cmdClearBag(TEXT("Awaken.Item.ClearBag"), TEXT("[pN]: empty the bag, keeping the Axe and the map"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&clearBag));
    FAutoConsoleCommandWithWorldAndArgs cmdDump(TEXT("Awaken.Item.Dump"), TEXT(": print every player's bag, hotbar, hold slots and hand as this machine sees them (read)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&dump));
    FAutoConsoleCommandWithWorldAndArgs cmdValidate(TEXT("Awaken.Item.Validate"), TEXT(": run the item data checks (read)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&validate));
    FAutoConsoleCommandWithWorldAndArgs cmdDropHere(TEXT("Awaken.Bag.DropHere"), TEXT("[pN]: detach the bag where the player stands"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&dropBag));
    FAutoConsoleCommandWithWorldAndArgs cmdSetBagless(TEXT("Awaken.Bag.SetBagless"), TEXT("[pN]: force the Bagless state by detaching the bag the normal way"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&dropBag));
    FAutoConsoleCommandWithWorldAndArgs cmdRecover(TEXT("Awaken.Bag.Recover"), TEXT("[pN]: recover the bag this player's character left in the world"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&recoverBag));
    FAutoConsoleCommandWithWorldAndArgs cmdBind(TEXT("Awaken.Hotbar.Bind"), TEXT("<slot> <itemId|none> [pN]: bind a hotbar slot"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&bindSlot));
    FAutoConsoleCommandWithWorldAndArgs cmdSelect(TEXT("Awaken.Hotbar.Select"), TEXT("<slot> [pN]: take what the slot resolves to in the hand"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&selectSlot));
    FAutoConsoleCommandWithWorldAndArgs cmdStubClock(TEXT("Awaken.Inventory.StubClock"), TEXT("<hours>: TEMPORARY, move the stub game clock forward"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&stubClock));
    FAutoConsoleCommandWithWorldAndArgs cmdStubFlag(TEXT("Awaken.Inventory.StubFlag"), TEXT("<carrying|downed> <0|1> [pN]: TEMPORARY, set a stub player state"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&stubFlag));
    FAutoConsoleCommandWithWorldAndArgs cmdStubSave(TEXT("Awaken.Inventory.StubSave"), TEXT(": TEMPORARY, keep every save snapshot in memory"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&stubSave));
    FAutoConsoleCommandWithWorldAndArgs cmdStubLoad(TEXT("Awaken.Inventory.StubLoad"), TEXT(": TEMPORARY, restore the snapshots kept by StubSave"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&stubLoad));
}

#endif
```

| Command | What it does | Where it runs |
| --- | --- | --- |
| `Awaken.Item.Give <itemId> [amount] [pN]` | grants through `grantOrDrop`, so a full bag drops the surplus | Host with target |
| `Awaken.Item.FillBag <itemId> [pN]` | grants the per-type limit plus three and logs what was stored and what dropped. **The situation most likely to break: a full bag** | Host with target |
| `Awaken.Item.SpawnLoose <itemId> [amount]` | a loose item in front of the player | Host |
| `Awaken.Item.ClearBag [pN]` | empties the bag; keeps the Axe and the map | Host with target |
| `Awaken.Item.Dump` (read) | prints every player's bag, hotbar, hold slots and hand as this machine sees them. Run it on a client to check replication | Local |
| `Awaken.Item.Validate` (read) | runs every item's data check and the catalog checks | Local |
| `Awaken.Bag.DropHere [pN]` | detaches the bag where the player stands | Host with target |
| `Awaken.Bag.SetBagless [pN]` | forces the Bagless state by dropping the bag the normal way. Until Doc B02 supplies its valid-spot rule, the same spot as `DropHere` | Host with target |
| `Awaken.Bag.Recover [pN]` | recovers the bag that player's character left in the world | Host with target |
| `Awaken.Hotbar.Bind <slot> <itemId\|none> [pN]`, `Awaken.Hotbar.Select <slot> [pN]` | bind a key; take what a key resolves to in the hand | Host with target |
| `Awaken.Inventory.FailBagStep <0\|1\|2>` (cvar) | makes step 1 or 2 of `detachBag` fail | Local to the host |
| `Awaken.Inventory.RandomSeed <n>` (cvar) | fixes the illness-chance roll; read when the game instance starts | Local to the host |
| `Awaken.Inventory.StubClock`, `.StubFlag`, `.StubSave`, `.StubLoad` | **temporary**, for the stubs of Docs R12, B07, B02 / R03 and R13 | Host |

The first, third and fourth rows and the two `Awaken.Bag` rows are the names Doc B22 §3.4 fixes; the rest are additions the owner may make.

### S13.3 — Debug view `Awaken.Debug.Inventory` (Doc B22 §3.5)

A view draws what the system knows, **locally**, only in non-shipping builds, and never replicates. B22's starter list has no inventory view; this adds one, as each owner may.

`InventoryDebugView.cpp`

```cpp
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UContainerViewComponent.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"

namespace InventoryDebug{
    TAutoConsoleVariable<int32> cvarDebugInventory(TEXT("Awaken.Debug.Inventory"), 0, TEXT("1 = draw each player's bag, staged quantities, hotbar, hold slots, hand and open container as THIS machine knows them."), ECVF_Default);
    FTSTicker::FDelegateHandle tickerHandle;

    bool draw(float deltaTime){
        if(GEngine == nullptr){
            return true;
        }
        int32 messageKey = 9100;
        for(const FWorldContext& context : GEngine->GetWorldContexts()){
            const UWorld* world = context.World();
            if(world == nullptr || world->GetGameState() == nullptr){
                continue;
            }
            for(const APlayerState* player : world->GetGameState()->PlayerArray){
                const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
                if(inventory == nullptr){
                    continue;
                }
                const FS_HeldItem held = UBFL_Inventory::getHeldState(player);
                const UContainerViewComponent* view = UContainerViewComponent::findFor(player);
                GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::Cyan, FString::Printf(TEXT("[Inventory] %s: %s, hand %s%s, open container %s"), *player->GetPlayerName(), inventory->hasBag() ? TEXT("bag") : TEXT("BAGLESS"), held.isEmpty() ? TEXT("empty") : *held.itemId.ToString(), held.isWielded ? TEXT(" (wielded)") : TEXT(""), (view != nullptr && view->getViewedContainerId().IsValid()) ? *view->getViewedContainerId().ToString().Left(8) : TEXT("none")));
                for(const FS_InventoryEntry& entry : inventory->getBagEntries()){
                    GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::White, FString::Printf(TEXT("   %s  available %d  staged %d"), *entry.itemId.ToString(), entry.getAvailableQuantity(), entry.reservedQuantity));
                }
                for(const FS_HotbarBinding& binding : inventory->getHotbarBindings()){
                    if(!binding.boundItemId.IsNone()){
                        GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::Yellow, FString::Printf(TEXT("   key %d -> %s (%d)"), binding.slotIndex, *binding.boundItemId.ToString(), inventory->resolveHotbarSlot(binding.slotIndex).availableQuantity));
                    }
                }
                const TArray<FS_BaglessHoldSlot> holdSlots = inventory->getHoldSlots();
                for(int32 index = 0; index < holdSlots.Num(); index++){
                    if(holdSlots[index].isOccupied){
                        GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::Orange, FString::Printf(TEXT("   hold key %d: %s"), index + 1, *holdSlots[index].entry.itemId.ToString()));
                    }
                }
            }
        }
        return true;
    }

    void onChanged(IConsoleVariable* variable){
        if(variable->GetInt() != 0 && !tickerHandle.IsValid()){
            tickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&draw), 0.25f);   // only while the view is on (Doc02 §5)
        }else if(variable->GetInt() == 0 && tickerHandle.IsValid()){
            FTSTicker::GetCoreTicker().RemoveTicker(tickerHandle);
            tickerHandle.Reset();
        }
    }

    struct FRegistrar{
        FRegistrar(){
            cvarDebugInventory->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&onChanged));
        }
    };
    FRegistrar registrar;
}

#endif
```

On a client the view shows only that client's own bag (and an empty one for everyone else), which is itself the check that the bag is owner-only.

### S13.4 — Log lines (Doc B22 §3.6)

Category `LogAwakenInventory`; every line names the machine role (done by `INVENTORY_LOG`). There is a Log-level line for every state change:

| State change | Where the line is |
| --- | --- |
| Framework started; catalog loaded | `UInventoryServices::Initialize`, `UItemCatalogSubsystem::Initialize` |
| A new empty bag; a container given an id | `createFreshBag`, `UContainerComponent::BeginPlay` |
| A grant (stored, carried, dropped); a loose item spawned; a pickup; a drop from the bag | `grantOrDrop`, `ULooseItemSpawner`, `pickUpLooseItem`, `dropFromBag` |
| A hotbar key bound; the hand changed | `bindHotbarSlot`, `UHeldItemComponent::setHeld` |
| An item broke; a reload | `applyDurabilityLoss`, `applyReload` |
| Staged units released | `releaseAllStaged` |
| A stack spoiled; an item consumed | `evaluateSpoilage`, `useConsumable`, `useConsumableFromHoldSlot` |
| Bag detached, recovered; held items destroyed; kit granted; new character | `UBFL_InventoryBag` |
| A container opened | `openContainer` |
| Restored from a save (each skipped entry as a Warning) | the two `restoreFromSnapshot` functions |
| **Every refusal returned to a client, with its reason tag** | `UInventoryRequestComponent::refuse` |
| Dev commands | each command logs its result |

Nothing here logs per frame. A plain transfer and a change of durability are not logged at Log level because they repeat; add `Verbose` lines in `transferEntry` and `setInstanceState` if you want to watch them.

### S13.5 — Automation specs

Run from *Tools → Session Frontend → Automation* filtered by `Awaken.Inventory`, or:

```
UnrealEditor-Cmd.exe <Project>.uproject -ExecCmds="Automation RunTests Awaken.Inventory; Quit" -unattended -nullrhi -log
```

`Tests/InventorySpec.cpp`

```cpp
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/IConsoleManager.h"
#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestCharacter.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/Stubs/InventoryStubs.h"
#include "ItemInventoryFramework/ALooseItemActor.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UBFL_InventoryBag.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UHeldItemComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "Engine/World.h"

namespace InventorySpecHelpers{
    FS_ItemStack stackOf(FName itemId, int32 quantity){
        FS_ItemStack stack;
        stack.itemId = itemId;
        stack.quantity = quantity;
        return stack;
    }

    FS_ItemRef refTo(FName itemId){
        FS_ItemRef ref;
        ref.itemId = itemId;
        return ref;
    }

    // A real loose actor in the fixture's world, holding a stack.
    ALooseItemActor* spawnLooseStack(const FInventoryTestFixture& fixture, FName itemId, int32 quantity, int64 spoilAtTime = 0){
        FS_InventoryEntry entry;
        entry.itemId = itemId;
        entry.totalQuantity = quantity;
        entry.spoilAtTime = spoilAtTime;
        ALooseItemActor* looseItem = fixture.world()->SpawnActor<ALooseItemActor>();
        looseItem->setEntry(entry);
        return looseItem;
    }

    void setFailBagStep(int32 step){
        IConsoleVariable* variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Awaken.Inventory.FailBagStep"));
        if(variable != nullptr){
            variable->Set(step);
        }
    }
}

BEGIN_DEFINE_SPEC(FInventorySpec, "Awaken.Inventory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FInventorySpec)

void FInventorySpec::Define(){
    using namespace InventorySpecHelpers;

    // ---- Spec 1 — Reserve and consume; staged items are reserved (tier 1; Doc B22 §3.7) ----
    Describe("Reserve and consume", [this](){
        It("a reserved amount cannot be spent twice, cannot leave the bag, and comes back on release", [this](){
            FInventoryTestFixture fixture;
            fixture.addStackable(TEXT("Stick"), 20);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            bag->addStack(TEXT("Stick"), 10);
            const FGuid station = FGuid::NewGuid();

            TestTrue(TEXT("stage 4"), UBFL_Inventory::reserveForStaging(player, refTo(TEXT("Stick")), 4, station).succeeded());
            TestEqual(TEXT("nothing left the bag"), bag->getTotalQuantity(TEXT("Stick")), 10);
            TestEqual(TEXT("every reader sees 6"), bag->getAvailableQuantity(TEXT("Stick")), 6);
            TestTrue(TEXT("cannot stage more than is available"), UBFL_Inventory::reserveForStaging(player, refTo(TEXT("Stick")), 7, station).failReason == T_Inventory_FailReason_InsufficientQuantity);
            TestTrue(TEXT("a staged unit cannot be removed"), bag->removeStack(TEXT("Stick"), 7).failReason == T_Inventory_FailReason_ReservedQuantity);
            TestTrue(TEXT("or dropped"), UBFL_Inventory::dropFromBag(player, refTo(TEXT("Stick")), 7).failReason == T_Inventory_FailReason_ReservedQuantity);

            TestTrue(TEXT("the craft consumes 3"), UBFL_Inventory::consumeStaged(player, station, refTo(TEXT("Stick")), 3).succeeded());
            TestEqual(TEXT("7 held"), bag->getTotalQuantity(TEXT("Stick")), 7);
            TestEqual(TEXT("6 still available"), bag->getAvailableQuantity(TEXT("Stick")), 6);
            TestTrue(TEXT("cannot consume what was not staged"), UBFL_Inventory::consumeStaged(player, station, refTo(TEXT("Stick")), 5).failReason == T_Inventory_FailReason_InsufficientQuantity);

            TestEqual(TEXT("a failed or closed craft releases the rest"), UBFL_Inventory::releaseAllStaged(player), 1);
            TestEqual(TEXT("all 7 available again"), bag->getAvailableQuantity(TEXT("Stick")), 7);
        });
    });

    // ---- Spec 2 — grantOrDrop and the pickup at the limit (tier 1) ----
    Describe("Grant and pickup", [this](){
        It("a grant drops the surplus and never destroys it; a pickup at the limit is refused, not dropped again", [this](){
            FInventoryTestFixture fixture;
            fixture.addStackable(TEXT("Stick"), 20);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            const UStubLooseItemSpawner* spawner = fixture.stub<UStubLooseItemSpawner, UBPI_LooseItemSpawner>();

            const FS_GrantResult grant = UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("Stick"), 30));
            TestEqual(TEXT("the limit is stored"), grant.quantityStored, 20);
            TestEqual(TEXT("the rest is dropped"), grant.quantityDropped, 10);
            TestTrue(TEXT("as one loose stack of 10"), spawner->spawnedEntries.Num() == 1 && spawner->spawnedEntries[0].totalQuantity == 10);

            ALooseItemActor* looseItem = spawnLooseStack(fixture, TEXT("Stick"), 3);
            TestTrue(TEXT("a pickup at the limit is refused"), UBFL_Inventory::pickUpLooseItem(player, looseItem).failReason == T_Interaction_FailReason_BagFull);
            TestEqual(TEXT("the item stays where it lies"), looseItem->getEntry().totalQuantity, 3);
            TestEqual(TEXT("and nothing new was dropped"), spawner->spawnedEntries.Num(), 1);

            bag->removeStack(TEXT("Stick"), 2);
            TestEqual(TEXT("a partly fitting stack is taken in part"), UBFL_Inventory::pickUpLooseItem(player, looseItem).quantity, 2);
            TestEqual(TEXT("the rest stays"), looseItem->getEntry().totalQuantity, 1);
        });
    });

    // ---- Spec 3 — Spoiled food (tier 2: the clock, the bag and three other frameworks' seams together) ----
    Describe("Spoiled food", [this](){
        It("fresh gives 100, spoiled gives 50 and costs HP, and a merged stack keeps the earliest time", [this](){
            FInventoryTestFixture fixture;
            // Any registered tag works as a meter key for the stub; Doc B03 owns the real meter tags.
            const FGameplayTag hungerMeter = T_Survival_Meter_Illness;
            UDA_ItemDefinition* meat = fixture.addStackable(TEXT("RawMeat"), 10);
            meat->canSpoil = true;
            meat->spoilTimeHours = 24.0f;
            meat->spoiledItemId = TEXT("SpoiledMeat");
            FS_ConsumeEffect hunger;
            hunger.targetTag = hungerMeter;
            hunger.amount = 100.0f;
            meat->consumeEffects.Add(hunger);
            UDA_ItemDefinition* spoiled = fixture.addStackable(TEXT("SpoiledMeat"), 10);
            FS_ConsumeEffect illness;
            illness.kind = E_ConsumeEffectKind::IllnessChance;
            illness.targetTag = T_Item_Category_Food;   // stands in for Survival.IllnessSource.SpoiledFood
            illness.chance = 1.0f;                      // certain, so the test does not depend on the seed
            spoiled->consumeEffects.Add(illness);
            FS_ConsumeEffect healthLoss;
            healthLoss.kind = E_ConsumeEffectKind::HealthLoss;
            healthLoss.amount = 10.0f;
            spoiled->consumeEffects.Add(healthLoss);

            AInventoryTestPlayerState* player = fixture.makePlayer();
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            const UStubSurvivalService* survival = fixture.stub<UStubSurvivalService, UBPI_SurvivalService>();
            const UStubHealthService* health = fixture.stub<UStubHealthService, UBPI_HealthService>();

            fixture.setGameTime(0);
            UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("RawMeat"), 2));
            fixture.advanceHours(10.0f);
            UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("RawMeat"), 2));
            TestEqual(TEXT("a merged stack keeps the earliest spoil time"), bag->findStack(TEXT("RawMeat"))->spoilAtTime, static_cast<int64>(24 * 3600));

            fixture.advanceHours(13.0f);   // hour 23
            TestTrue(TEXT("still fresh"), UBFL_Inventory::useConsumable(player, TEXT("RawMeat")).succeeded());
            TestEqual(TEXT("fresh meat gives 100"), survival->meterTotals.FindRef(hungerMeter), 100.0f);
            TestEqual(TEXT("and costs no HP"), health->totalDamaged, 0.0f);

            fixture.advanceHours(1.0f);    // hour 24: exactly at the boundary the server's answer is "spoiled"
            TestFalse(TEXT("the fresh item is gone"), UBFL_Inventory::useConsumable(player, TEXT("RawMeat")).succeeded());
            TestEqual(TEXT("the stack became the spoiled item"), bag->getTotalQuantity(TEXT("SpoiledMeat")), 3);

            TestTrue(TEXT("spoiled food can still be eaten"), UBFL_Inventory::useConsumable(player, TEXT("SpoiledMeat")).succeeded());
            TestEqual(TEXT("it gives half"), survival->meterTotals.FindRef(hungerMeter), 150.0f);
            TestEqual(TEXT("it costs HP every time"), health->totalDamaged, 10.0f);
            TestTrue(TEXT("through DamageType.Illness"), health->lastDamageType == T_DamageType_Illness);
            TestEqual(TEXT("the illness source was registered"), survival->registeredSources.Num(), 1);
        });
    });

    // ---- Spec 4 — Bag lifecycle (tier 2; this is R01's side of Doc B22's "Respawn and bag") ----
    Describe("Bag lifecycle", [this](){
        It("detach is all or nothing; Bagless holds one item per slot; recovery restores bag, hotbar and hand", [this](){
            FInventoryTestFixture fixture;
            fixture.addStackable(TEXT("Stick"), 20);
            UDA_ItemDefinition* spear = fixture.addInstanced(TEXT("Spear"), 4, 50.0f);
            spear->categoryTag = T_Item_Category_Weapon;
            spear->weaponType = E_WeaponType::Thrown;
            AInventoryTestPlayerState* player = fixture.makePlayer();
            fixture.makeCharacterFor(player);
            UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
            UHeldItemComponent* heldItem = UHeldItemComponent::findFor(player);
            UInventoryData* bag = inventory->getPersonalInventory();
            bag->addStack(TEXT("Stick"), 5);
            UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("Spear"), 1));
            const FGuid spearId = inventory->findUsableInstance(TEXT("Spear"));
            inventory->bindHotbarSlot(1, TEXT("Spear"));
            heldItem->selectHotbarSlot(1);
            TestTrue(TEXT("the spear is wielded"), heldItem->getHeldState().isWielded);

            setFailBagStep(1);
            TestTrue(TEXT("a failed step 1 is reported"), UBFL_InventoryBag::detachBag(player, FVector::ZeroVector).failReason == T_Inventory_FailReason_BagCreateFailed);
            TestTrue(TEXT("and nothing moved"), inventory->hasBag() && heldItem->getHeldState().instanceId == spearId);
            setFailBagStep(0);

            const FS_BagDetachResult detached = UBFL_InventoryBag::detachBag(player, FVector::ZeroVector);
            TestTrue(TEXT("detached"), detached.succeeded());
            TestFalse(TEXT("the player is Bagless"), inventory->hasBag());
            TestEqual(TEXT("three usable keys"), UBFL_InventoryBag::getUsableHotbarSlotCount(player), 3);
            TestTrue(TEXT("the hand is empty"), heldItem->getHeldState().isEmpty());
            TestTrue(TEXT("the bag lies in the world, locked"), fixture.registry()->findInventory(bag->getInventoryId())->isDeathBag());
            TestTrue(TEXT("a death bag cannot be taken from"), UBFL_Inventory::transferEntry(bag, fixture.makeContainer(FS_ContainerRules()), refTo(TEXT("Stick")), 1).failReason == T_Inventory_FailReason_DeathBagLocked);

            TestEqual(TEXT("a grant to a Bagless player drops"), UBFL_Inventory::grantOrDrop(player, stackOf(TEXT("Stick"), 2)).quantityDropped, 2);
            ALooseItemActor* looseItem = spawnLooseStack(fixture, TEXT("Stick"), 5);
            TestEqual(TEXT("a pickup takes one item into a hold slot"), UBFL_Inventory::pickUpLooseItem(player, looseItem).quantity, 1);
            TestEqual(TEXT("the rest of the stack stays"), looseItem->getEntry().totalQuantity, 4);
            TestTrue(TEXT("binding is refused while Bagless"), inventory->bindHotbarSlot(1, TEXT("Stick")).failReason == T_Inventory_FailReason_NoBag);

            TestTrue(TEXT("recovered"), UBFL_InventoryBag::reattachBag(player, detached.bagId).succeeded());
            TestTrue(TEXT("the bag is back"), inventory->hasBag());
            TestEqual(TEXT("the held stick was added to it"), inventory->getPersonalInventory()->getTotalQuantity(TEXT("Stick")), 6);
            TestTrue(TEXT("key 1 is the spear again"), inventory->getBoundItemId(1) == FName(TEXT("Spear")));
            TestTrue(TEXT("the same spear is in the hand, wielded"), heldItem->getHeldState().instanceId == spearId && heldItem->getHeldState().isWielded);
            TestTrue(TEXT("a second recovery is refused"), UBFL_InventoryBag::reattachBag(player, detached.bagId).failReason == T_Inventory_FailReason_AlreadyHasBag);
        });
    });
}

#endif
```

> If your project treats log warnings as test failures, a test that expects a Warning line (for example "grantOrDrop did nothing" for an unknown item) declares it first with `AddExpectedError(TEXT("grantOrDrop did nothing"), EAutomationExpectedErrorFlags::Contains, 1);`.

The full test list. Doc B22 §3.10 asks for at least three tests, not all of one tier:

| Test (Doc B22 §3.7 name where it has one) | Tier | File | Sets up | Checks |
| --- | --- | --- | --- | --- |
| Per-type limit | 1 | `InventoryDataSpec` | sticks, spears, a restricted container | Section 3's table |
| Reserve and consume / Staged items are reserved | 1 | `InventorySpec` | 10 sticks, one station | above |
| Grant and pickup | 1 | `InventorySpec` | a full bag, a loose stack | above |
| Hotbar key and spoiled food | 1 | add to `InventorySpec` | slot 1 bound to `RawMeat`, pass the spoil time | the slot is not usable; the binding is unchanged; it never resolves to `SpoiledMeat` |
| Slot data check / Slot lookup / Page mapping | 1 | `InventorySlotSpec` | fixture items with slots, `rebuildSlotLookup` | Section 10's table |
| Fresh and spoiled share a slot (with B17) | 1 | `InventorySlotSpec` | `RawMeat` + `SpoiledMeat` in one slot | two ids in `itemsInSlot`, two separate quantities; spoiling moves the count; fresh is never merged into spoiled |
| Spoiled food | 2 | `InventorySpec` | the stub clock, the survival and health stubs | above |
| Bag lifecycle (R01's side of "Respawn and bag") | 2 | `InventorySpec` | a wielded spear, a bound key, loose items | above |
| Save round trip (R01's part) | 2 | `InventorySaveSpec` | Section 11's nine steps | ids, durability, hotbar, spoil times; no reservation, no held item, no hold slot |
| Loose-object flood | 2 | manual on `TestMap_Inventory` | `Awaken.Item.SpawnLoose Stick` 200 times | each is removed only by its own lifetime; the Axe, the map and a full pouch can never be among them |
| Replication and requests | 3 (manual) | S13.7 | a listen server and a client | what the client must see |

### S13.6 — Test map and scenario

`TestMap_Inventory` (from S4): GameMode override `InventoryTestGameMode`, two PlayerStarts, one `InventoryTestChest` with an id. Add a second chest whose rules are `allowedTags = Item.Category.Water` and `singleItemTag = Item.Category.Water` (a water barrel) for the container-rule checks. The **"full bag" scenario** is `Awaken.Item.FillBag Stick`: the bag ends at its limit, three sticks lie on the ground, and the log shows `Inventory.GrantDropped`.

### S13.7 — The two-player check (Doc B22 §3.8, §3.10)

Editor *Play As Listen Server*, 2 players (host = player 0, client = player 1), then once offline. Turn on `Awaken.Debug.Inventory 1` on **both** machines.

1. Both players spawn with a bag holding the Axe and the map. **Each machine's view shows only its own bag**; the other player's reads empty.
2. Host: `Awaken.Item.Give Stick 5 p1`. The **client** sees 5 sticks; the host's own bag is unchanged.
3. Host: `Awaken.Item.FillBag Stick p1`. The client's bag is at the limit; three sticks lie in front of the client and **both** machines see the loose item; only the client hears `Inventory.GrantDropped`.
4. Client: walk to those sticks and pick them up (the request). Refused `Interaction.FailReason.BagFull`; **only the client** is told; the sticks stay on both machines.
5. Host: `Awaken.Item.Give Spear 2 p1`, `Awaken.Hotbar.Bind 1 Spear p1`, `Awaken.Hotbar.Select 1 p1`. The client's key 1 is bound (client only); **both** machines show the spear in the client's hand.
6. Client: open the chest and store 5 sticks. The host opens the same chest and sees them; the host takes 2 and the client's view shows 3 at once.
7. Client: walk away from the chest. The view closes by itself.
8. Host: `Awaken.Inventory.StubClock 30` after `Awaken.Item.Give RawMeat 2 p1`. Nothing changes until the client's next request; then the client's bag shows `SpoiledMeat × 2`.
9. Host: `Awaken.Bag.DropHere p1`. On the client: Bagless, the bag view is empty, the hand empties on **both** machines, `Awaken.Item.Dump` shows `usable slots 3`.
10. Client: pick up one stick (it sits on key 1), press key 1: both machines show it in the hand, not wielded. A fourth pickup is refused `HandsFull`.
11. Host: `Awaken.Bag.Recover p1`. The client's bag, the bound key and the spear in the hand are back; the held stick is in the bag.
12. Host: `Awaken.Inventory.StubSave`, then `Awaken.Item.ClearBag p1`, then `Awaken.Inventory.StubLoad`. The client's bag is as it was at the save; no reservation, nothing in the hand.
13. A **late joiner** connects now: it sees the spear state of the client's hand and the loose sticks; it never sees the client's bag.
14. Host: `Awaken.Inventory.StubFlag downed 1 p1`; the client tries to eat: refused `NotAllowedInState`, and only the client is told.
15. Repeat steps 2, 3, 5, 8, 9 and 11 **offline** (standalone): the same results on the one machine.

### S13.8 — Stub register (what to delete, and when)

| Stub / temporary item | File | Owner | Remove when |
| --- | --- | --- | --- |
| `UStubGameClock`, `Awaken.Inventory.StubClock` | `Stubs/InventoryStubs.*`, dev commands | Doc R12 Time | R12 registers an `IBPI_GameClock` |
| `UStubPlayerStateQuery`, the `downed` stub flag | same | Docs B02, R03, R11, R17 | one real `IBPI_PlayerStateQuery` is registered |
| `UStubCarryService`, the `carrying` stub flag | same | Doc B07 Carry | B07 registers an `IBPI_CarryService` |
| `UStubSurvivalService` | same | Doc B03 Survival Tick | B03 registers an `IBPI_SurvivalService` |
| `UStubHealthService` | same | Docs R08, R03 | a real `IBPI_HealthService` is registered |
| `UStubActionService` | same | Doc R04 | R04 registers an `IBPI_ActionService` |
| `UStubEquipmentGear` | same | Doc R05 Equipment | **the R05 guide, task E7.2** |
| `UStubBagActorService` | same | Docs B02, R13 | B02 registers an `IBPI_BagActorService` backed by `ADeathBagActor` and `S_BagRecord` |
| `UStubLooseItemSpawner` | same | this framework | never: tests use it. It is not registered in play |
| `BPI_SaveParticipantStub.h`, `E_SaveScope`, `UStubSaveRegistry`, `StubSave` / `StubLoad` | `03Interfaces/`, `Stubs/` | Doc R13 Save | R13's `BPI_SaveParticipant.h` and `USaveSubsystem` exist |
| The temporary body of `raiseItemDroppedCosmetic` | `InventoryServices.cpp` | Doc B21 | the cosmetic seam exists (S5.7) |
| `AInventoryTestPlayerState`, `AInventoryTestCharacter`, `AInventoryTestGameMode` | `Stubs/` | Docs B19, R07, B05 | the real classes carry the components and create characters. Keep the three for the test map and the fixture if you like |
| `AInventoryTestChest` | `Stubs/` | content | real containers exist |
| Duplicate tag definitions: `Interaction.FailReason.BagFull`, `.HandsFull`, `.NotAllowedInState`, `Action.Reload`, `DamageType.Illness`, `Survival.Meter.Illness`, `Stats.Source.Medicine`, `Cosmetic.Event.Interaction.ItemDropped` | `T_ItemInventoryFramework.*` | Docs B18, R04, R03, B03, R08, B21 | each owner's tag header exists: remove the definition here and include theirs |
| The four B03 tags added by hand for content (S1.8) | Project Settings | Doc B03 | B03 declares them natively |

### Handoff notes for teammates

| Doc | You must |
| --- | --- |
| **B01** (Building) | read `getAvailableQuantity` for a cost check, never the total; refund and blueprint cancel through `grantOrDrop`; a storage piece is an actor with `UContainerComponent`; on demolition drop the contents, then `destroyInventory` |
| **B02** (Checkpoint & Respawn) | call `detachBag`, `reattachBag`, `discardBaglessItems`; implement `IBPI_BagActorService`; own the retry limit and the valid-spot rule |
| **B03** (Survival Tick) | implement `IBPI_SurvivalService`; own the meter and illness-source tags and the sign of a meter change |
| **B05 / B20** (Handshake, Session) | call `startNewCharacter` when a character is created and `setCharacterId` after a load; call `handlePlayerLeft` on `Session.PlayerLeft`; take the bag with `releaseInventory` for the 15-minute hold |
| **B06** (Game Flow) | restore in the order of S11.7 |
| **B07** (Carry) | implement `IBPI_CarryService`; start a throw with `takeOutOfBag`; spawn a landed item with `spawnLooseItem` |
| **B08, B10, R02** | every yield and output through `grantOrDrop` (B10 passes the job's finish time); R02 wraps the staging functions of Section 7 and listens to `onStagedItemLost` |
| **B09** (Item Transport) | reach a container as data with `findInventory(containerId)`; no `grantOrDrop` at a line end; a line never ends on a death bag (`isDeathBag`) |
| **B17** (UI) | Section 10's table; the requests of Section 12; `onRequestRefused` for the refusal line; text for the reason tags |
| **B18 / B11** (Interaction) | `serverPickUp`, `serverOpenContainer`, `serverQuickTransfer`; no prompt on a death bag for anyone but its owner |
| **B19** (Class Table) | `UPlayerInventoryComponent`, `UInventoryRequestComponent`, `UContainerViewComponent` on the PlayerState; `UHeldItemComponent` on the Character |
| **B21** | the cosmetic event `Cosmetic.Event.Interaction.ItemDropped` (S5.7) |
| **R03** (Combat) | `getHeldItem`, `getHeldState`, `applyDurabilityLoss`, `spendLoadedShot`; confirm `ammoItemId` and the firearm field names; implement the damage half of `IBPI_HealthService` |
| **R04** (Abilities) | `GA_Reload` calls `applyReload` on completion; implement `IBPI_ActionService`; add `Action.Reload` to the tag list |
| **R05** (Equipment) | its own guide: fill the Equipment block, replace `UStubEquipmentGear` |
| **R07** (Animation) | show `getShownDefinition()` from `onHeldItemChanged` |
| **R08** (Stats) | implement the heal half of `IBPI_HealthService` |
| **R10** (Localization) | `ST_Items` rows `Item.<itemId>.Name` / `.Description`; `ST_Reasons` rows for the new reason tags |
| **R12** (Time) | implement `IBPI_GameClock` |
| **R13** (Save) | replace the save stubs; store `Inventory.Character` and `Inventory.World`; own `S_BagRecord` |

### S13.9 — Final review: every confirmed R01 rule maps to a task

| R01 rule | Where |
| --- | --- |
| Every item is a `DA_ItemDefinition` with a PrimaryAssetId; a new item is data, never code (3.1) | S2.4, S2.10 |
| Two kinds: stackable and instanced; an instance has a GUID and its own state (3.2, D4) | S1.1, S1.2, S5.4 `makeFreshInstance` |
| Durability is the default; exactly one unbreakable item, by a sentinel (3.1) | S2.6, S2.12, S6.7, S10.4 |
| The loaded flag and loaded rounds live on the instance; `ApplyReload` writes them; a cancelled reload moves nothing (3.2) | S1.2, S6.7 |
| Spoilage: a flag, a spoiled item id, one time per stack, lazy server evaluation, earliest time on merge, the conversion ignores the limit (3.2a) | S2.7, S3 `addStackForced`, S8.1 |
| Spoiled food and unclean water: half the meter value, a chance of illness, HP loss every time through `DamageType.Illness` (3.2a, A95, A96) | S8.2 |
| An inventory is a data object, not a component; it limits the amount of each type, with no slot count (3.3) | S3 |
| It replicates as a fast array; the bag to its owner only; a container to the players who have it open (3.3, §7) | S1.7, S4, S12.2 |
| Three inventory types; a Portable Container's contents live in the registry under an id on the instance (3.4) | S1.1, S4.1 |
| The death bag is neither kind of container and has one interaction (3.4, A110) | S3 `isDeathBag`, S5.6, S12.2 |
| A container holds one kind of water at a time (3.4) | S1.2 `singleItemTag`, S3 `checkAccepts` |
| A Portable Container that holds something cannot be dropped, thrown, crafted with or nested; an empty one may go into a world container (3.4a and amendment) | S5.5, S5.6, S7.2 |
| The hotbar binds an item id; two slots share one pool; the last used instance first; the binding stays when the last one leaves (3.5, A102) | S6.1–S6.3 |
| The held item lives on the Character, is never saved, resolves to Unarmed; a swap cancels a reload (3.5) | S6.4–S6.6 |
| Staging reserves without removing; every reader sees the available quantity; released at session end and before `DetachBag`; never saved (3.6) | S3, S7, S9.4, S11 |
| `ReleaseStagedItem` next to `ReleaseStagedReservation` (3.6, A115) | S7 |
| `GrantOrDrop`: step 0 heavy items, store what fits, drop the rest, report; a Bagless grant drops (3.7) | S5.4 |
| `GrantOrDrop` takes an optional spoil start time, clamped to now (amendment) | S5.4 |
| The bag is the Personal Inventory; it takes the gear and a hotbar snapshot; `DetachBag` and recovery are atomic (3.8) | S9.4, S9.5 |
| Bagless: three hold slots of one item each; can pick up, eat and fight Unarmed; cannot craft, build, store or equip (3.8) | S9.1, S9.2, S9.6, S12 |
| Food in a hold slot does not spoil while held (amendment) | S9.1, S9.6 |
| Dying Bagless destroys the held items (3.8) | S9.6 `discardBaglessItems` |
| A bag belongs to a character; a new character's old bag is removed completely; the kit is granted once; a second grant is refused (3.8, A93, amendment) | S9.7, S9.8 |
| Medicine heals and may lower Illness; the amounts are data (3.9) | S2.7, S8.2 |
| Unarmed is an always-available definition, never stored (3.10) | S2.14, S6.5 |
| The Axe and the map are permanent: never thrown, dropped, moved into a container, staged or loose; they live in the bag (3.10, A97–A99) | S2.14, S5.5, S5.6, S7.2, S13.1 `ClearBag` |
| The general pickup path: heavy to the hands, the rest to the bag or a hold slot; at the limit refused with `BagFull`; a partial stack taken in part (3.11) | S5.5, S9.2 |
| Fixed slots, five pages derived from the category, piles from a stack layout, variants sharing a slot, the data checks (3.1a, 3.1b, A115) | S2.1–S2.3, S2.9, S2.12, S10 |
| Page and slot are never saved or replicated (3.1b) | S10, S11 |
| The three messages and their delivery (§6); the cosmetic event (A89) | S3 `notifyChanged`, S5, S6.3, S12.7, S5.7 |
| Every change on the server; the host validates range, state and ownership (§7, Doc02 §19.6) | S12.3–S12.5 |
| Save: the table of §8, time left in a Character file and absolute times in the World file (§8, amendment) | S11 |
| Cleanup on `Session.PlayerLeft` (A89) | S7.2, S12.8 |
| Text is a key derived from the id, never a sentence (A89) | S2.9 |

### The closing section every guide must carry (Doc B22 §3.10)

| Part | R01 |
| --- | --- |
| **Commands** | `Awaken.Item.Give`, `.SpawnLoose`, `.ClearBag`, `.Dump` (read), `.Validate` (read), `.FillBag` (the situation most likely to break: a full bag); `Awaken.Bag.DropHere`, `.SetBagless`, `.Recover`; `Awaken.Hotbar.Bind`, `.Select`; cvars `Awaken.Inventory.FailBagStep`, `.RandomSeed`; four temporary stub commands |
| **Debug view** | `Awaken.Debug.Inventory` (each player's bag with available and staged quantities, bound keys, hold slots, hand, open container, as this machine knows them) |
| **Log category** | `LogAwakenInventory`, role-prefixed, one Log line per state change; every refusal logs its reason tag |
| **Automated tests** | Per-type limit, Reserve and consume, Grant and pickup, Hotbar key and spoiled food, the three slot tests (tier 1); Spoiled food, Bag lifecycle, Save round trip (tier 2); Replication and requests (tier 3, manual two-player) |
| **Two-player check** | S13.7 |
| **Done when** | the commands run, the tests pass, S13.7 has been done on a listen server plus one client and once offline |

---
# Appendix A — Files To Create (Doc02 §20.3 layout)

```
Public/ItemInventoryFramework/
    00Enums/E_ItemInventoryFramework.h
    01Structs/S_ItemInventoryFramework.h
    02Tags/T_ItemInventoryFramework.h
    03Interfaces/
        BPI_GameClock.h   BPI_PlayerStateQuery.h   BPI_CarryService.h   BPI_SurvivalService.h
        BPI_HealthService.h   BPI_ActionService.h   BPI_LooseItemSpawner.h
        BPI_EquipmentGear.h   BPI_BagActorService.h
        BPI_SaveParticipantStub.h                       (temporary: owner Doc R13)
    04Delegates/D_ItemInventoryFramework.h
    InventoryLog.h   InventoryServices.h   InventoryTime.h   UInventorySettings.h
    UDA_ItemDefinition.h   UDA_InventoryPageLayout.h   UDA_StackLayout.h
    UItemCatalogSubsystem.h   UInventoryData.h   UInventoryRegistrySubsystem.h
    UPlayerInventoryComponent.h   UHeldItemComponent.h
    UContainerComponent.h   UContainerViewComponent.h   UInventoryRequestComponent.h
    ALooseItemActor.h   ULooseItemSpawner.h
    UBFL_Inventory.h   UBFL_InventoryBag.h   UBFL_InventorySave.h   UBFL_InventoryValidation.h
Private/ItemInventoryFramework/
    02Tags/T_ItemInventoryFramework.cpp
    S_ItemInventoryFramework.cpp
    (one .cpp for every header above that declares functions)
    Stubs/
        InventoryStubs.h/.cpp   StubSaveRegistry.h/.cpp
        AInventoryTestPlayerState.h/.cpp   AInventoryTestCharacter.h/.cpp
        AInventoryTestGameMode.h/.cpp      AInventoryTestChest.h/.cpp
    DevTools/InventoryDevCommands.cpp   InventoryDebugView.cpp
    Tests/
        InventoryTestFixture.h/.cpp
        InventoryDataSpec.cpp   InventorySpec.cpp   InventorySlotSpec.cpp   InventorySaveSpec.cpp
Config/DefaultGame.ini                      (Asset Manager entry; Awaken Inventory settings)
Content/Awaken/Items/DA_Item_*              (item definitions, including DA_Item_Unarmed and DA_Item_StarterAxe)
Content/Awaken/Inventory/DA_Page_*, DA_Stack_*
Content/Awaken/Tests/TestMap_Inventory
```

> R01 §11 lists one header per struct (`S_ItemInstanceState.h`, `S_InventoryEntry.h`, and so on). That list is older than Doc02 §20.3, which asks for one struct file per framework; this guide follows §20.3 and puts every struct in `S_ItemInventoryFramework.h`.

# Appendix B — Tags and data fields to register

**Tags already in the Tag Registry (owner R01):** `Inventory.Changed`, `Inventory.GrantDropped`, `Hotbar.Changed`, `Inventory.FailReason.ContainerNotEmpty`, `Inventory.FailReason.Permanent`, `Inventory.FailReason.CannotOpenHere` (with Doc B17), `Item.Category.*`.

**From the R01 amendment (PROPOSED there):** `Inventory.FailReason.KitAlreadyGranted`, `Item.Flag.Permanent`.

**New, PROPOSED by this guide (I13, I15):**

- `Inventory.Page.Weapons`, `.Tools`, `.Consumables`, `.Materials`, `.Others`
- `Inventory.FailReason.NotServer`, `.InvalidRequest`, `.NotFound`, `.NoBag`, `.AlreadyHasBag`, `.NotBagOwner`, `.InsufficientQuantity`, `.ReservedQuantity`, `.ItemWielded`, `.ContainerFull`, `.ItemNotAllowed`, `.NestedContainer`, `.DeathBagLocked`, `.InvalidSlot`, `.NotConsumable`, `.CannotReload`, `.NoAmmo`, `.OutOfRange`, `.RateLimited`, `.BagCreateFailed`, `.GearReleaseFailed`

Each reason tag needs a row in `ST_Reasons` (Doc R10), for example `Reason.Inventory.ContainerFull`. `NotServer`, `InvalidRequest`, `BagCreateFailed` and `GearReleaseFailed` should never reach a player; give them one shared neutral line.

**Reused from other owners:** `Interaction.FailReason.BagFull`, `.HandsFull`, `.NotAllowedInState` (B18); `Action.Reload` (R04); `DamageType.Illness` (R03); `Survival.Meter.*` and `Survival.IllnessSource.*` (B03); `Stats.Source.Medicine` (R08); `Cosmetic.Event.Interaction.ItemDropped` (B21); `Carry.WeightClass.*` (B07).

**Data fields and names to log in the Index:**

- On `DA_ItemDefinition`, as the spec names them, in Doc02 §20 spelling: `itemId`, `itemKind`, `categoryTag`, `itemTags`, `perTypeLimit`, `icon`, `worldMesh`, `carryCapacityCost`, `weightClassTag`, `requiresProcessing`, `linkedProcessingRecipe`, `weaponType`, `attack`, `attackSpeed`, `damage`, `armorPenetrationPercent`, `ammoItemId`, `magazineSize`, `durabilityMax`, `durabilityLossPerUse`, `consumeEffects`, `canSpoil`, `spoilTimeHours`, `spoiledItemId`, `spoiledEffectiveness`, `effectBaseItemId`, `uncleanWaterEffectiveness`, `healthRestoreAmount`, `illnessReductionAmount`, `capacityRules`, `looseLifetimeSeconds`, `inventorySlot`, `inventoryMesh`, `slotSharedWith`, `stackLayout`, `stagingSlot`.
- New names chosen here: the firearm fields `bulletSpeed`, `bulletDrop`, `bulletLongestRange`, `recoilPitch`, `recoilYaw`; `E_WeaponType::None`; in `FS_ItemInstanceState` the spellings `isLoaded` and `loadedRounds`; `FS_GrantResult.quantityCarried`; `FS_ContainerRules` (`defaultPerTypeLimit`, `perItemLimits`, `allowedTags`, `singleItemTag`); `FS_ConsumeEffect` (`kind`, `targetTag`, `amount`, `chance`, `sourceDurationHours`); `UDA_StackLayout.pileFootprint`.
- Functions added to the spec's table: `consumeStaged`, `spendLoadedShot`, `applyDurabilityLoss`, `takeOutOfBag`, `dropFromBag`, `grantEntryOrDrop`, `useConsumableFromHoldSlot`, `dropHoldSlot`, `grantStartingKit`, `startNewCharacter`, `handlePlayerLeft`, `validateCatalog`.
- Settings (*Awaken Inventory*): `hotbarSlotCount`, `baglessSlotCount`, `unarmedItemId`, `pickupRange`, `containerRange`, `requestsPerSecond`, `defaultLooseLifetimeSeconds`, `dropForwardDistance`, `startingKit`, `pageMappings`, `fallbackPageTag`.
- Save keys: `Inventory.Character`, `Inventory.World`. Asset types: `DA_InventoryPageLayout`, `DA_StackLayout`. Primary asset type: `Item`.

# Appendix C — Open points to confirm with the designer or teammates

| # | Point | Who |
| --- | --- | --- |
| 1 | **I8.** The reason shown when all three Bagless hold slots are full is `Interaction.FailReason.HandsFull`. R01 3.11 says only "a short reason" | Designer, B18 owner |
| 2 | **I19.** A Bagless player can put a held item down (`dropHoldSlot`). The spec does not say whether they can | Designer |
| 3 | **I10.** A non-weapon in the hand (food, a tool) reads as Unarmed for Combat. Only a weapon is "wielded" | Designer, R03 owner |
| 4 | **I9.** The "last used" order of weapon copies is not saved; after a load the first copy in the bag is used | Designer |
| 5 | **I18.** The weapon that was in the hand at death is put back in the hand on recovery | Designer, B02 owner |
| 6 | **Hands and the hotbar.** While carrying a heavy item, selecting a hotbar key or binding one is allowed here; only eating, pickups of ordinary items and containers need free hands. Doc B07 may want to refuse the key as well | B07 owner |
| 7 | **I11, I12.** `spendLoadedShot` (the shot unloads the crossbow or spends a round) and `consumeStaged` are names this guide adds. Also confirm `ammoItemId` and the five firearm field names | R03, R02 owners |
| 8 | **I17.** The six calls of `IBPI_BagActorService`, and that `removeBagsOwnedBy` returns inventory ids (already a request of the R01 spec) | B02, R13 owners |
| 9 | **I23.** The inventory component keeps a copy of the CharacterID for bag ownership and does not save it; R13 or B05 sets it after a load | R13, B05 owners |
| 10 | **Leaving players.** A bag still attached when a PlayerState ends is destroyed. Doc B20's 15-minute hold must take it first with `releaseInventory()` | B20 owner |
| 11 | **I7.** `ALooseItemActor` lives in this framework until the shared loose-object base of Doc02 §19.7 has an owner. Its physics is the plain engine default: "sleeps quickly" and the low-rate position are that owner's to tune | Doc02 owner, B07 owner |
| 12 | **A destroyed container.** What happens to the contents when a container is demolished or destroyed is not in R01. Suggested: drop them as loose items (Doc02 §6) | Designer, B01 owner |
| 13 | **I21.** Container limits use the bag's model with an optional default and per-item limits; "one kind of water" is `singleItemTag`. The real limits are not decided (R01 3.4) | Designer |
| 14 | **I13, I14.** The five `Inventory.Page.*` tags, and the "pile fits its pocket" check by a declared footprint. A shared slot gives each pile half the width (PROPOSED in R01 3.2a) | Designer, B17 owner |
| 15 | **Item kinds of the special items.** The map is Stackable with a limit of 1 (it has no state); Unarmed is Instanced with a limit of 0 and no slot | Designer |
| 16 | **Signs of meter amounts.** The assets hold the number handed to `modifyMeter`; which sign means "less hungry" is Doc B03's | B03 owner |
| 17 | **I20.** The cosmetic event `ItemDropped` has a temporary body until the B21 seam exists (S5.7) | B21 owner |
| 18 | **Doc R13 wording** (already noted in the R01 spec): container contents are in the World file and keep absolute spoil times; R13 says "Character or container file" in one place | R13 owner |
| 19 | **Container screens.** Whether chests use fixed slots like the bag is not decided (R01 §10). Until it is, a container view is a plain list | Designer |
| 20 | **`Awaken.Bag.SetBagless`** and `DropHere` are the same call until Doc B02 supplies the valid-spot rule | B02 owner |
| 21 | **Text in Doc R01 to tidy at the next edit:** §4 lists `S_ItemInstanceState` without the loaded flag and rounds that 3.2 adds; §4 `S_GrantResult` has two fields (this guide adds `quantityCarried`); §5 `DetachBag(Player)` has no location and `ReattachBag(Player, BagActor)` takes an actor where this guide takes the bag id; §11 lists one file per struct (see Appendix A) | You (R01 owner) |

# Appendix D — Definition of done for the framework

Fourteen sections (0 to 13) delivered and reviewed with every task ticked; `Awaken.Item.Validate` reports zero errors on all content; every test in the Verify tables passes and the automated specs of S13.5 are green; S13.7 has been done on a listen server plus a client and once offline; every stub in the register has an owner and a removal condition; every PROPOSED choice of 0.2 is either confirmed or changed; the closing Dev Tools section is in place.

**For the R05 guide, built after this one:** its tasks E7.3 and E9.2 need no change (both are already in this code); everything else in it applies as written, and its E1.4 cosmetic seam is the one S5.7 of this guide then calls.

> **Working method from here:** tell me **"start Section 0"** (or paste the first compiler error of whichever section you built) and we go section by section, fixing as we compile. If you already have R01 code from an earlier pass, send me the class list first and I will mark which tasks you can skip and which names differ.
