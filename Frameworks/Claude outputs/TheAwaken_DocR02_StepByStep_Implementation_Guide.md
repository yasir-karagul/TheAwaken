# TheAwaken — Doc R02 Step-by-Step Implementation Guide
## Crafting Framework (recipes, the crafting session, staging, the craft button list, stations, weapon upgrades)

Prepared for Yasir · Target: Unreal Engine 5.8 · C++ core with a thin layer of Blueprints
Companion to: *TheAwaken_DocR02_CraftingFramework_ReworkSpec* (the spec wins on any conflict) and *TheAwaken_DocR01_StepByStep_Implementation_Guide* (this guide builds on the classes that guide created and uses the same format, task ids and rules). One small piece comes from *TheAwaken_DocR05_StepByStep_Implementation_Guide* (the cosmetic seam, see 0.4), and the *TheAwaken_DocB07_StepByStep_Implementation_Guide* (Carry) supplies the "hands are full" answer through R01's seam.

---

## How to use this guide

1. Work top to bottom. Each section ends with **Verify** and **Handoff**. Do not start a section until the previous one compiles and its checks pass.
2. Task ids read `CR3.4` = Crafting Section 3, task 4. The `CR` keeps them apart from the `S` ids of the R01 guide, the `E` ids of R05 and the `C` ids of B07. Each task cites the R02 clause (or related doc) it implements.
3. **Honesty note:** this code was written against UE 5.8 conventions and against the class and function names of the R01 guide, but **was not compiled**. Expect a few small API or include fixes. After each section, paste the first compiler error (if any) and we fix it together; do not "fix forward" across sections.
4. **This guide assumes the R01 guide's code exists** (Sections 0 to 7, 12 and 13). Section 7 of that guide already built the whole reservation side (`reserveForStaging`, `releaseStagedItem`, `releaseStagedReservation`, `consumeStaged`); this guide never touches a bag directly. The exact R01 names this guide calls are listed in 0.4, so you can check them in one pass before you start.
5. Everything marked **PROPOSED** is implemented as written but kept easy to change: every number and every content choice lives in data (Doc00 §2.7).
6. Placeholders are the same as in R01: `AWAKEN_API` is the module export macro and `Awaken` the module name.
7. A later section sometimes adds members to a class an earlier section created. Those places say **Header additions** and show only the new lines; the earlier code stays as it is.

**What this framework is, in six sentences.** A recipe is a data asset: some ingredients, one output, and optionally a station and an unlock it needs. The player opens the bag and *stages* ingredients; a staged item never leaves the bag, it is only reserved (Doc R01). From what is staged the game builds a list of craft buttons, one per recipe that uses at least one staged item: green (can be made now), white (nothing missing, but something extra is staged) or red (something is missing). Pressing a green button plays a short animation, then the server consumes exactly one recipe's worth of the reserved ingredients and grants the output with `grantOrDrop`. The session starts at the first staged ingredient and ends when the bag closes, or when the player is Downed, dies, leaves, is locked in a scene, or the station is lost; everything still staged is simply available again. Nothing about crafting is ever saved.

---

## 0.1 Code style reminder (Doc02 §20, designer-confirmed)

The full cheat sheet is section 0.1 of the R01 guide. The short version, because every block below follows it:

| Rule | Right | Wrong |
| --- | --- | --- |
| Variables, params, members, functions | `sessionState`, `requestCraft()` | `SessionState`, `RequestCraft()` |
| Booleans: no `b` prefix | `isOpen`, `hasStation`, `inUse` | `bIsOpen` |
| Enum / struct / interface / delegate / tag | `E_RecipeListState`, `FS_AvailableRecipeOption`, `UBPI_ProgressionQuery` + `IBPI_ProgressionQuery`, `FD_CraftingChanged`, `T_Crafting_ItemCrafted` | anything else |
| Keyword spacing and braces | `if(x){` … `}else{` … closing `}` alone on its line, braces always | `if (x) {`, `} else {`, `if(x) return;` |
| Indent | 4 spaces | tabs |

Names Unreal forces on us that are new in this guide (Doc02 §20.5): `GetPrimaryAssetId`, `IsLocallyControlled`, and `OnRep_sessionState` (the replication-notify rule). Everything else PascalCase in the code is already on the R01 list (`BeginPlay`, `EndPlay`, `Initialize`, `GetLifetimeReplicatedProps`, `_Implementation`, `IsDataValid`, the macros).

> The spec writes names the old way (`RequestCraft`, `S_AvailableRecipeOption`, `DA_CraftingRecipe`). In code they become `requestCraft`, `FS_AvailableRecipeOption`, `UDA_CraftingRecipe`. Same thing, Doc02 §20 spelling.

---

## 0.2 Decisions this guide makes where the spec is silent

R02 is a complete specification of the rules, but it leaves the mechanics of several of them PROPOSED or unnamed. These are the choices I made so you can build without waiting. Each is small and easy to reverse. **Confirm or override any of them** (they are repeated in Appendix C as questions); none blocks Section 0.

| # | Open point | Choice in this guide | Why |
| --- | --- | --- | --- |
| K1 | What a RecipeID is in code (R02 3.1 says "PrimaryAssetId") | An `FName recipeId` field on the recipe; the `PrimaryAssetId` is `Recipe:<recipeId>` | The same rule as R01's item ids (R01 guide I1): renaming the asset file never changes the id |
| K2 | "Its length is the whole crafting time" (R02 3.1), but the server is the craft timer (R02 §8) and must not load an animation to learn a number | One more recipe field, `craftSeconds`. The server waits that long. It must equal the length of the animation behind `animationTag`; Doc R07's data check compares the two | A timer needs a number that exists without Doc R07. `0` means "at once", which is also what makes automated tests exact |
| K3 | Where the craft button list is worked out | One pure function, `UBFL_Crafting::buildRecipeList(staged, stationTags)`. The server calls it to check a request; the owning client calls the **same** function to draw the buttons from its replicated bag | R02 §8: "the colours can be worked out on the client". One function means the screen and the server can never disagree about the rule, only about stale data, and then the server wins |
| K4 | How the staged quantities are read | From the bag entries' `reservedQuantity` (R01 replicates it to the owner). No second list is kept | R02 3.3: "the data it shows already exists: the reserved quantity". A player has one session, so everything reserved in their bag belongs to it |
| K5 | "Reds are ordered by fewest missing things first" (R02 3.4): what counts as one thing | Every missing unit counts 1 and a missing station counts 1. Ties, and the order inside green and inside white, are by recipe id | A stable order: the list must not shuffle while the player stages |
| K6 | What a station is in code. Doc B01 owns the piece and Doc B18 the lock, and neither exists yet | `UCraftingStationComponent`, owned by this framework. Any actor that carries it is a crafting station: it declares its `Station.*` tags, has a stable id and holds the Exclusive lock as one replicated flag | The same pattern as R01's `UContainerComponent`. Building adds the component to its table and forge pieces; B18's interaction calls one function. No seam and no stub actor registry |
| K7 | How the server learns that the bag closed. Opening the bag is local and never replicated (Doc B17 3.6, S12b-D10) | A request, `serverCloseBag`, that the bag screen sends when it closes. It ends the session and frees the station | The server cannot see a local screen. If a client never sends it, its own items stay reserved until it leaves; nobody else is affected |
| K8 | A station opened, but nothing staged yet. The session starts only at the first staged ingredient (R02 3.2), yet station recipes must already be listed "normal" | A *station context* separate from the session: `enterStation` locks the station and replicates its tags to the owner; the first `stageIngredient` then opens the session with that station | Both sentences of R02 3.2 stay true |
| K9 | `OpenCraftingSession(OptionalStationID)` | `openCraftingSession()` takes no argument: the station is the one the player entered (K8). It is called by the first stage, never by the screen | A client must not be able to name a station it did not walk up to |
| K10 | "One unit per drag, the whole stack with Shift" (R02 3.3) | The rule functions take a quantity. The requests take a flag, `wholeStack`, and the **server** works out how many that is | The client should not be trusted to know the size of a stack |
| K11 | `CloseCraftingSession(Reason)`: the type of Reason | An enum, `E_CraftingCloseReason` | A small closed list that drives which notice the screen shows |
| K12 | Who tells Crafting that the player is Downed, in a scene, or gone | The owner of each state calls `UBFL_Crafting::closeSession(character, reason)`. Two safety nets are built in: the component closes the session when its pawn is destroyed, and when the station is destroyed | B02, R17 and B20 may not exist yet. R01 already releases the reservations on a death and on leaving, so items are safe even without the call |
| K13 | A press while a craft is running: "ignored" (R02 3.7) and also a tag `CraftInProgress` (R02 §5) | Refused with `Crafting.FailReason.CraftInProgress`. Staging and unstaging are refused the same way while a craft runs | A refusal with a reason *is* how a request is ignored (Doc02 §6). Changing the staged items under a running craft would change what it consumes |
| K14 | A recipe that is missing both an ingredient and the station: which reason | `StationRequired` | The player should walk to the station first |
| K15 | A recipe that needs several copies of an instanced item | Supported: the staged copies are counted by item type and consumed one by one | Nothing in the spec forbids it, and it costs three lines |
| K16 | Shift on an instanced item (a weapon) | Stages one copy per call, the most worn free one (R02 3.10). There is no "whole stack" of weapons | Each copy is its own physical item |
| K17 | Who owns the unlocked set (Doc R06) | A seam, `IBPI_ProgressionQuery::isUnlocked`, with a stub | R06 belongs to a teammate and may not exist yet (Doc00 §4.2) |
| K18 | The cosmetic events `Cosmetic.Event.Craft.Started` / `.Finished` (R02 amendment A89) | Raised through `IBPI_CosmeticEventSink`, the interface the R05 guide created (E1.4) | Two guides must not declare the same interface |
| K19 | How the owner's machine knows a craft is running, to play the animation | The session state replicates to the owner only, with the id of the running recipe. No new message | R02 §7 lists three messages; this needs none |
| K20 | Range to a station | One setting, `stationUseRange`, checked when the station is entered. No timer watches the distance afterwards | The bag screen takes no movement input (Doc B17 3.3), and a forced move is released by Doc B18's lock (R02 3.8) |
| K21 | Data checks beyond R02 §12 | No ingredient may be heavy or permanent; no output may be a firearm or a firearm's ammunition; a heavy output is a warning | Index A103 S4-F01, Index A92, Index A116 |

---

## 0.3 Section map and dependency order

```
CR0 scaffolding ─ CR1 types/tags/seam ─ CR2 recipe data and the recipe catalog
CR3 the craft button list (pure rules) ─ CR4 the station component
CR5 the crafting component: session, staging ─ CR6 requesting and completing a craft
CR7 ending a session ─ CR8 networking ─ CR9 dev tools, tests, acceptance
```

Each section depends only on earlier ones.

## 0.4 What this guide uses from the other guides

Check these names against your real code once. If one differs, tell me and I adjust the guide.

| Piece | Used for |
| --- | --- |
| `UInventoryServices` (`get`, `registerSeam`, `getSeam`, `isIncapacitated`, `isCarryActive`) | the shared seam registry and the two player-state checks |
| `UBFL_Inventory` (`isServerWorld`, `reserveForStaging`, `releaseStagedItem`, `releaseStagedReservation`, `consumeStaged`, `grantOrDrop`, `makeFreshInstance`) | everything that touches the bag (R01 guide S5, S7) |
| `UPlayerInventoryComponent` (`findFor`, `hasBag`, `getPersonalInventory`, `getBagEntries`) | reading the staged quantities on the server **and** on the owning client |
| `UInventoryData` (`getReservations`, `getAvailableQuantity`, `getTotalQuantity`, `findInstance`, `addStack`, `addInstance`, `onStagedItemLost`), `FS_StagedReservation` | consuming instanced ingredients; a staged stack that spoiled |
| `FS_InventoryEntry`, `FS_ItemRef`, `FS_ItemStack`, `FS_InventoryResult` | parameters and results. Crafting invents no second result type |
| `UItemCatalogSubsystem` (`get`, `getDefinition`, `getAllDefinitions`), `UDA_ItemDefinition` (`isInstanced()`, `isHeavy()`, `isPermanent()`, `weaponType`, `ammoItemId`) | item data and the data checks |
| `UInventoryRequestComponent` (`findFor`, `gate`, `refuse`) | request gating (public since the R05 patch E9.2) |
| `InventoryLog::roleName`, `FInventoryTestFixture` (`addStackable`, `addInstanced`, `makePlayer`, `makeCharacterFor`, `services`, `stub`, `world`), `AInventoryTestCharacter`, `UStubPlayerStateQuery` | logging and tests |
| Tags `T_Inventory_FailReason_NotServer` / `_NotFound` / `_NoBag` / `_OutOfRange`, `T_Interaction_FailReason_HandsFull` / `_NotAllowedInState` | refusal reasons |
| **From the R05 guide:** `IBPI_CosmeticEventSink` (`EquipmentFramework/03Interfaces/BPI_CosmeticEventSink.h`, E1.4) | the two cosmetic events (K18) |
| **From the B07 guide:** nothing by name. `UInventoryServices::isCarryActive` answers from the real Carry once that guide is built, and from R01's stub before | "crafting is refused while carrying" |

> **If you have not built the R05 guide yet:** do only its task E1.4 (the file `BPI_CosmeticEventSink.h`) before Section 1 here. If no implementation is registered, the two cosmetic events are simply not raised.

---

# SECTION 0 — Scaffolding

**Goal:** an empty Crafting framework that compiles and logs.
**Spec:** Doc02 §14, §20.3; Doc B22 §3.6; R02 3.4 (`MaxVisibleRecipes` is data).
**Depends on:** the R01 guide's Sections 0 and 1.

### Files

```
Source/Awaken/Public/CraftingFramework/
    00Enums/   01Structs/   02Tags/   03Interfaces/   04Delegates/
    CraftingLog.h
    CraftingServices.h
    UCraftingSettings.h
Source/Awaken/Private/CraftingFramework/
    02Tags/   Stubs/   DevTools/   Tests/
    CraftingLog.cpp
    CraftingServices.cpp
    UCraftingSettings.cpp
```

### CR0.1 — Create the folder tree (Doc02 §20.3)

From `Source/<Module>`:

```bash
for dir in 00Enums 01Structs 02Tags 03Interfaces 04Delegates; do
  mkdir -p "Public/CraftingFramework/$dir"
done
mkdir -p Private/CraftingFramework/02Tags Private/CraftingFramework/Stubs
mkdir -p Private/CraftingFramework/DevTools Private/CraftingFramework/Tests
```

No change to `Awaken.Build.cs`: Crafting lives in the same module as Item & Inventory and needs no new dependency.

### CR0.2 — Log category (Doc B22 §3.6 names it `LogAwakenCrafting`)

`Public/CraftingFramework/CraftingLog.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "ItemInventoryFramework/InventoryLog.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAwakenCrafting, Log, All);

// Every line names the machine role (Host, Client, Offline). Use: CRAFTING_LOG(Log, this, "text %d", value);
#define CRAFTING_LOG(verbosity, worldContext, format, ...) \
    UE_LOG(LogAwakenCrafting, verbosity, TEXT("[%s] " format), *InventoryLog::roleName(worldContext), ##__VA_ARGS__)
```

`Private/CraftingFramework/CraftingLog.cpp`

```cpp
#include "CraftingFramework/CraftingLog.h"

DEFINE_LOG_CATEGORY(LogAwakenCrafting);
```

### CR0.3 — Project settings (every tuning number in one place)

`Public/CraftingFramework/UCraftingSettings.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UCraftingSettings.generated.h"

// Project Settings -> Game -> Awaken Crafting. Every value is tuning data (Doc00 2.7).
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Awaken Crafting"))
class AWAKEN_API UCraftingSettings : public UDeveloperSettings{
    GENERATED_BODY()

public:
    UCraftingSettings();

    static const UCraftingSettings* get(){
        return GetDefault<UCraftingSettings>();
    }

    // R02 3.4: "At most 15 recipe entries are visible. 15 is data (PROPOSED name MaxVisibleRecipes)."
    UPROPERTY(Config, EditAnywhere, Category = "Craft List", meta = (ClampMin = "1")) int32 maxVisibleRecipes = 15;
    // How close the pawn's eye must be to a station to enter it (K20). Inclusive (Doc02 19.9). PROPOSED value;
    // Doc B18 3.7 owns the range at which the station's lock is released later.
    UPROPERTY(Config, EditAnywhere, Category = "Station", meta = (ClampMin = "0")) float stationUseRange = 250.0f;
};
```

`Private/CraftingFramework/UCraftingSettings.cpp`

```cpp
#include "CraftingFramework/UCraftingSettings.h"

UCraftingSettings::UCraftingSettings(){
    CategoryName = TEXT("Game");
}
```

### CR0.4 — Services subsystem skeleton (startup line)

`Public/CraftingFramework/CraftingServices.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CraftingServices.generated.h"

UCLASS()
class AWAKEN_API UCraftingServices : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static UCraftingServices* get(const UObject* worldContext);
};
```

`Private/CraftingFramework/CraftingServices.cpp`

```cpp
#include "CraftingFramework/CraftingServices.h"
#include "CraftingFramework/CraftingLog.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

void UCraftingServices::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);
    CRAFTING_LOG(Log, this, "Crafting framework started");
}

UCraftingServices* UCraftingServices::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    if(gameInstance == nullptr){
        return nullptr;
    }
    return gameInstance->GetSubsystem<UCraftingServices>();
}
```

### Verify
Clean Editor + Development build, zero warnings. The log shows `LogAwakenCrafting: [...] Crafting framework started` when a game instance starts, and *Project Settings → Game → Awaken Crafting* shows `maxVisibleRecipes = 15`.

### Handoff
Folder tree, log helper, settings and the services skeleton exist. Nothing is exported to other frameworks yet.

---

# SECTION 1 — Types, Tags, Delegate and the Progression Seam

**Goal:** every shared type in one place, plus the one outgoing seam and its stub.
**Spec:** R02 §5, §6, §7; Doc R06 §4 (`IsUnlocked`); Doc02 §2, §20.2; Doc00 §4.2.
**Depends on:** Section 0.

### CR1.1 — Enums (R02 §6)

`Public/CraftingFramework/00Enums/E_CraftingFramework.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "E_CraftingFramework.generated.h"

// R02 3.4, §6 (names PROPOSED, Index A115). The three states of a craft button.
UENUM(BlueprintType)
enum class E_RecipeListState : uint8{
    Craftable,     // green: nothing missing, nothing extra. The only state that can be pressed
    ExtraItems,    // white: nothing missing, but an item type the recipe does not use is staged
    Missing        // red: an ingredient quantity or the station is missing. Red wins over white
};

// R02 3.8: why a session ended (K11). The screen uses it to choose its notice.
UENUM(BlueprintType)
enum class E_CraftingCloseReason : uint8{
    BagClosed,       // the player closed the bag: the normal end
    Downed,          // the player is Downed (Index A115, S12-P3)
    Died,
    PlayerLeft,      // disconnect or leaving the session (Doc B20 3.6)
    SceneStarted,    // a scene locked the player's input (Doc R17)
    StationLost,     // the station was destroyed or collapsed (Index A103, S4-F24)
    OutOfRange,      // something moved the player away from the station (Doc B18 3.7)
    BagLost,         // the bag is gone while the session was open
    PawnRemoved      // the safety net of K12: the Character was destroyed and nobody closed the session first
};
```

### CR1.2 — Structs

`Public/CraftingFramework/01Structs/S_CraftingFramework.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CraftingFramework/00Enums/E_CraftingFramework.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "S_CraftingFramework.generated.h"

class AActor;

// ---------- Recipe data (R02 3.1, §6) ----------

// One ingredient of a recipe. Also used for "still missing" in a list entry.
USTRUCT(BlueprintType)
struct FS_CraftingRecipeRequirement{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName itemId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "1")) int32 quantity = 1;
};

// ---------- The craft button list (R02 3.4, §6; names PROPOSED, Index A115) ----------

// One craft button.
USTRUCT(BlueprintType)
struct FS_AvailableRecipeOption{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName recipeId;
    UPROPERTY(BlueprintReadOnly) E_RecipeListState state = E_RecipeListState::Missing;
    // The item id and the quantity still missing, one entry per ingredient that is short.
    UPROPERTY(BlueprintReadOnly) TArray<FS_CraftingRecipeRequirement> missingIngredients;
    // True when the recipe needs a station and the player is not at one that matches. The hover card then
    // shows the text key UI.Crafting.NeedsStation.
    UPROPERTY(BlueprintReadOnly) bool isStationMissing = false;
    // The staged item types the recipe does not use.
    UPROPERTY(BlueprintReadOnly) TArray<FName> extraItems;
    // How many times the staged items would make it. Data: the screen does not have to show it.
    UPROPERTY(BlueprintReadOnly) int32 maxCraftableCount = 0;

    // The number that orders the red entries: every missing unit counts 1, a missing station counts 1 (K5).
    int32 getMissingCount() const{
        int32 count = isStationMissing ? 1 : 0;
        for(const FS_CraftingRecipeRequirement& missing : missingIngredients){
            count += missing.quantity;
        }
        return count;
    }
};

// ---------- Session state: replicated to the owning player only (K8, K19) ----------

USTRUCT(BlueprintType)
struct FS_CraftingSessionState{
    GENERATED_BODY()

    // True from the first staged ingredient until the session ends (R02 3.2).
    UPROPERTY(BlueprintReadOnly) bool isOpen = false;
    // The station context (K8): set when the player enters a station, before anything is staged.
    UPROPERTY(BlueprintReadOnly) bool hasStation = false;
    // Invalid for a hand-craft session. It is also the key of the reservations (R01 guide S7).
    UPROPERTY(BlueprintReadOnly) FGuid stationId;
    UPROPERTY(BlueprintReadOnly) FGameplayTagContainer stationTags;
    // The recipe being made right now. None = no craft is running.
    UPROPERTY(BlueprintReadOnly) FName activeRecipeId;
    // The length of the running craft, for the progress the screen may show.
    UPROPERTY(BlueprintReadOnly) float activeCraftSeconds = 0.0f;
};

// ---------- Message payloads (R02 §7, Doc02 §11) ----------

// Crafting.SessionOpened
USTRUCT(BlueprintType)
struct FS_CraftingSessionOpenedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> crafter = nullptr;
    // Invalid for a hand-craft session.
    UPROPERTY(BlueprintReadOnly) FGuid stationId;
};

// Crafting.ItemCrafted
USTRUCT(BlueprintType)
struct FS_CraftingItemCraftedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> crafter = nullptr;
    UPROPERTY(BlueprintReadOnly) FName recipeId;
    UPROPERTY(BlueprintReadOnly) FName outputItemId;
    UPROPERTY(BlueprintReadOnly) int32 outputQuantity = 0;
};

// Crafting.SessionClosed
USTRUCT(BlueprintType)
struct FS_CraftingSessionClosedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<AActor> crafter = nullptr;
    // What was still staged and is available again. The items never left the bag.
    UPROPERTY(BlueprintReadOnly) TArray<FS_ItemStack> returnedItems;
    UPROPERTY(BlueprintReadOnly) E_CraftingCloseReason reason = E_CraftingCloseReason::BagClosed;
};
```

### CR1.3 — Tags

`Public/CraftingFramework/02Tags/T_CraftingFramework.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

// ---- Messages (R02 §7) ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_SessionOpened);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_ItemCrafted);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_SessionClosed);

// ---- Refusal reasons (R02 §5). The last three are PROPOSED names in the spec ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_FailReason_StationRequired);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_FailReason_BlueprintLocked);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_FailReason_InsufficientStaged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_FailReason_BagRequired);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_FailReason_CraftInProgress);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Crafting_FailReason_ExtraIngredients);

// ---- Cosmetic events (R02 amendment A89; the Tag Registry names R02 as their owner) ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Craft_Started);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Craft_Finished);

// ---- Tags OWNED BY ANOTHER FRAMEWORK, referenced here (see the stub register, CR9.8) ----
// TEMPORARY duplicates: remove the definitions here when the owner's tag header exists and include that header instead.
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Station_CraftingTable);          // Docs B01, B10 (the shared Station.* namespace)
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Station_Forge);                  // Docs B01, B10
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FailReason_InUse);   // Doc B18: an Exclusive object is used by someone else
```

`Private/CraftingFramework/02Tags/T_CraftingFramework.cpp`

```cpp
#include "CraftingFramework/02Tags/T_CraftingFramework.h"

UE_DEFINE_GAMEPLAY_TAG(T_Crafting_SessionOpened, "Crafting.SessionOpened");
UE_DEFINE_GAMEPLAY_TAG(T_Crafting_ItemCrafted, "Crafting.ItemCrafted");
UE_DEFINE_GAMEPLAY_TAG(T_Crafting_SessionClosed, "Crafting.SessionClosed");

UE_DEFINE_GAMEPLAY_TAG(T_Crafting_FailReason_StationRequired, "Crafting.FailReason.StationRequired");
UE_DEFINE_GAMEPLAY_TAG(T_Crafting_FailReason_BlueprintLocked, "Crafting.FailReason.BlueprintLocked");
UE_DEFINE_GAMEPLAY_TAG(T_Crafting_FailReason_InsufficientStaged, "Crafting.FailReason.InsufficientStaged");
UE_DEFINE_GAMEPLAY_TAG(T_Crafting_FailReason_BagRequired, "Crafting.FailReason.BagRequired");
UE_DEFINE_GAMEPLAY_TAG(T_Crafting_FailReason_CraftInProgress, "Crafting.FailReason.CraftInProgress");
UE_DEFINE_GAMEPLAY_TAG(T_Crafting_FailReason_ExtraIngredients, "Crafting.FailReason.ExtraIngredients");

UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Craft_Started, "Cosmetic.Event.Craft.Started");
UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Craft_Finished, "Cosmetic.Event.Craft.Finished");

UE_DEFINE_GAMEPLAY_TAG(T_Station_CraftingTable, "Station.CraftingTable");
UE_DEFINE_GAMEPLAY_TAG(T_Station_Forge, "Station.Forge");
UE_DEFINE_GAMEPLAY_TAG(T_Interaction_FailReason_InUse, "Interaction.FailReason.InUse");
```

> A recipe names its station, its unlock and its animation **by tag in data**, so no other tag has to exist in code. The unlock tags live under `Progression.Recipe.*` (Doc R06 §5); create each one in *Project Settings → Gameplay Tags* when a recipe first needs it, until R06 has its own tag file.

### CR1.4 — Delegate

`Public/CraftingFramework/04Delegates/D_CraftingFramework.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "D_CraftingFramework.generated.h"

class AActor;

// Raised on the server and on the owning client after any change to the session state: a station entered or left,
// the session opened or closed, a craft started or ended.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FD_CraftingChanged, AActor*, crafter);
```

> Same caveat as R01's `D_ItemInventoryFramework.h`: if Unreal Header Tool rejected a header that holds only delegates there, apply here whatever fix you settled on for that file.

### CR1.5 — The seam interface (outgoing)

`Public/CraftingFramework/03Interfaces/BPI_ProgressionQuery.h` (owner: Doc R06 Progression — `IsUnlocked`)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "BPI_ProgressionQuery.generated.h"

UINTERFACE(BlueprintType)
class UBPI_ProgressionQuery : public UInterface{
    GENERATED_BODY()
};

class IBPI_ProgressionQuery{
    GENERATED_BODY()

public:
    // Doc R06 §4 IsUnlocked(Tag): is this tag in the shared unlocked set? The set is on the GameState and replicates,
    // so the real implementation answers the same on every machine. Unlocks are shared by everyone in co-op.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool isUnlocked(FGameplayTag unlockTag);
};
```

> Doc B01's blueprint gate asks Progression the same question. If the Building guide (or Doc R06) already declared this interface in the project, do not create it twice: use theirs and delete this file and its stub.

### CR1.6 — Stub implementation

`Private/CraftingFramework/Stubs/CraftingStubs.h`

```cpp
// TEMPORARY STUBS — the owner of each is named on its class. Delete a class when the real framework lands.
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Object.h"
#include "CraftingFramework/03Interfaces/BPI_ProgressionQuery.h"
#include "CraftingStubs.generated.h"

// TEMPORARY STUB — owner: Doc R06 Progression. A local set of tags. It does NOT replicate: on a client it knows only
// what was set on that client, so two-player tests of a locked recipe set the same tag on both machines.
UCLASS()
class UStubProgressionQuery : public UObject, public IBPI_ProgressionQuery{
    GENERATED_BODY()

public:
    UPROPERTY() FGameplayTagContainer unlockedTags;
    // The Peaceful bulk grant of Doc B13 / R06 in one switch.
    UPROPERTY() bool unlockEverything = false;

    virtual bool isUnlocked_Implementation(FGameplayTag unlockTag) override;
};
```

`Private/CraftingFramework/Stubs/CraftingStubs.cpp`

```cpp
// TEMPORARY STUBS — see CraftingStubs.h for the owner of each.
#include "CraftingFramework/Stubs/CraftingStubs.h"

bool UStubProgressionQuery::isUnlocked_Implementation(FGameplayTag unlockTag){
    return unlockEverything || unlockedTags.HasTagExact(unlockTag);
}
```

### CR1.7 — Fill `UCraftingServices`

Replace the Section 0 skeleton. Crafting keeps no seam map of its own; it uses R01's `UInventoryServices`, like the R05 and B07 guides do. A teammate replaces the stub with one line: `UInventoryServices::get(world)->registerSeam(UBPI_ProgressionQuery::StaticClass(), myProgressionObject);`

`CraftingServices.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CraftingServices.generated.h"

class AActor;
class UInventoryServices;

UCLASS()
class AWAKEN_API UCraftingServices : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static UCraftingServices* get(const UObject* worldContext);

    // ---- facade ----
    // Query. Doc R06. An empty tag means "no blueprint needed" and is always unlocked (R02 3.9).
    bool isUnlocked(const FGameplayTag& unlockTag) const;
    // Command. Doc B21. Does nothing while no cosmetic sink is registered.
    void raiseCosmeticEvent(const FGameplayTag& eventTag, const FVector& location, AActor* instigator) const;

private:
    UInventoryServices* getRegistry() const;
};
```

`CraftingServices.cpp`

```cpp
#include "CraftingFramework/CraftingServices.h"
#include "CraftingFramework/CraftingLog.h"
#include "CraftingFramework/03Interfaces/BPI_ProgressionQuery.h"
#include "CraftingFramework/Stubs/CraftingStubs.h"
#include "EquipmentFramework/03Interfaces/BPI_CosmeticEventSink.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

void UCraftingServices::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);
    Collection.InitializeDependency<UInventoryServices>();   // the registry must exist first

    UInventoryServices* registry = getRegistry();
    if(registry == nullptr){
        CRAFTING_LOG(Error, this, "Crafting framework: UInventoryServices is missing");
        return;
    }
    // Default the outgoing seam to its stub, unless a real framework registered first.
    if(registry->getSeam(UBPI_ProgressionQuery::StaticClass()) == nullptr){
        registry->registerSeam(UBPI_ProgressionQuery::StaticClass(), NewObject<UStubProgressionQuery>(this));
    }
    CRAFTING_LOG(Log, this, "Crafting framework started");
}

UCraftingServices* UCraftingServices::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    if(gameInstance == nullptr){
        return nullptr;
    }
    return gameInstance->GetSubsystem<UCraftingServices>();
}

UInventoryServices* UCraftingServices::getRegistry() const{
    const UGameInstance* gameInstance = GetGameInstance();
    return gameInstance != nullptr ? gameInstance->GetSubsystem<UInventoryServices>() : nullptr;
}

bool UCraftingServices::isUnlocked(const FGameplayTag& unlockTag) const{
    if(!unlockTag.IsValid()){
        return true;
    }
    const UInventoryServices* registry = getRegistry();
    UObject* progression = registry != nullptr ? registry->getSeam(UBPI_ProgressionQuery::StaticClass()) : nullptr;
    // Nobody to ask means locked: a gated recipe must never slip through because a framework is missing.
    return progression != nullptr && IBPI_ProgressionQuery::Execute_isUnlocked(progression, unlockTag);
}

void UCraftingServices::raiseCosmeticEvent(const FGameplayTag& eventTag, const FVector& location, AActor* instigator) const{
    const UInventoryServices* registry = getRegistry();
    UObject* sink = registry != nullptr ? registry->getSeam(UBPI_CosmeticEventSink::StaticClass()) : nullptr;
    if(sink != nullptr){
        IBPI_CosmeticEventSink::Execute_raiseCosmeticEvent(sink, eventTag, location, instigator, 1.0f);
    }
}
```

### Verify (Section 1)

1. **Compile** Editor + Development. Zero warnings.
2. **Tags:** *Project Settings → Gameplay Tags*, search `Crafting.` — the three messages and six fail reasons show with *Native* source. `Cosmetic.Event.Craft.Started` and `.Finished`, `Station.CraftingTable` and `Station.Forge` show as well.
3. **Seam:** a breakpoint (or a temporary log) after `Initialize`: `getSeam(UBPI_ProgressionQuery::StaticClass())` is not null.
4. **Default-value test (tier 1):** a fresh `FS_AvailableRecipeOption` has the state `Missing` and `getMissingCount() == 0`; a fresh `FS_CraftingSessionState` has `isOpen == false`; `UCraftingServices::isUnlocked(FGameplayTag())` is true and any real tag is false until the stub is told otherwise.

### Handoff
- Every type, tag and seam interface exists.
- Tell the R06 owner: implement `IBPI_ProgressionQuery` on the progression object and register it. Tell the B01 / B10 owners: `Station.CraftingTable` and `Station.Forge` are temporary duplicates of theirs. Tell the B18 owner the same for `Interaction.FailReason.InUse`.

---

# SECTION 2 — Recipe Data and the Recipe Catalog

**Goal:** a recipe is a data asset, never code; recipes are found fast by ingredient; bad data is caught in the editor.
**Spec:** R02 3.1, 3.10, §6, §12 (the editor data check), amendments A92, A103 (S4-F01), A116; Doc02 §19.8; Doc B22 §3.7 ("No firearm recipe").
**Depends on:** Section 1.

### Files

```
Public/CraftingFramework/UDA_CraftingRecipe.h           Private/…/UDA_CraftingRecipe.cpp
Public/CraftingFramework/UCraftingRecipeSubsystem.h     Private/…/UCraftingRecipeSubsystem.cpp
Public/CraftingFramework/UBFL_CraftingValidation.h      Private/…/UBFL_CraftingValidation.cpp
```

### CR2.1 — `UDA_CraftingRecipe`

`UDA_CraftingRecipe.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "CraftingFramework/01Structs/S_CraftingFramework.h"
#include "UDA_CraftingRecipe.generated.h"

// R02 3.1: one recipe. A new recipe is a new asset of this class, never new code.
UCLASS(BlueprintType)
class AWAKEN_API UDA_CraftingRecipe : public UPrimaryDataAsset{
    GENERATED_BODY()

public:
    static FPrimaryAssetType getPrimaryAssetType(){
        return FPrimaryAssetType(TEXT("Recipe"));
    }

    virtual FPrimaryAssetId GetPrimaryAssetId() const override{
        return FPrimaryAssetId(getPrimaryAssetType(), recipeId);
    }

    // The stable id (K1). Requests and messages carry this name. Never rename it after release.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity") FName recipeId;

    // One entry per ingredient. An item may appear only once. An ingredient may be a weapon (R02 3.10).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe") TArray<FS_CraftingRecipeRequirement> requirementList;
    // What one craft makes. An instanced output is made new, at full condition (R02 3.10).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe") FName outputItemId;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recipe", meta = (ClampMin = "1")) int32 outputQuantity = 1;

    // Optional. Empty = can be made anywhere (R02 3.9).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gating", meta = (Categories = "Station")) FGameplayTag requiredStationTag;
    // Optional. A tag that must be in the shared unlocked set (Doc R06). Until then the recipe is never listed.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gating", meta = (Categories = "Progression")) FGameplayTag requiredBlueprintTag;

    // The short animation played while the item is made (Doc R07). Presentation only.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation") FGameplayTag animationTag;
    // How long the server waits before the item is made (K2). It must equal the length of the animation above.
    // 0 = the item is made at once.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation", meta = (ClampMin = "0")) float craftSeconds = 0.0f;
    // Kept as data. No tab row of the screen uses it for now (Index A115, S12-P8).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Presentation") FGameplayTag recipeCategory;

    // ---- queries ----
    // How many units of that item one craft needs. 0 when the recipe does not use it.
    int32 getRequiredQuantity(FName itemId) const;
    bool usesItem(FName itemId) const{
        return getRequiredQuantity(itemId) > 0;
    }

#if WITH_EDITOR
    // The checks that need only this asset. The checks against the item catalog are in UBFL_CraftingValidation.
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
```

`UDA_CraftingRecipe.cpp`

```cpp
#include "CraftingFramework/UDA_CraftingRecipe.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

int32 UDA_CraftingRecipe::getRequiredQuantity(FName itemId) const{
    for(const FS_CraftingRecipeRequirement& requirement : requirementList){
        if(requirement.itemId == itemId){
            return requirement.quantity;
        }
    }
    return 0;
}

#if WITH_EDITOR
EDataValidationResult UDA_CraftingRecipe::IsDataValid(FDataValidationContext& Context) const{
    EDataValidationResult result = Super::IsDataValid(Context);
    const auto fail = [&Context, &result](const TCHAR* text){
        Context.AddError(FText::FromString(text));
        result = EDataValidationResult::Invalid;
    };

    if(recipeId.IsNone()){
        fail(TEXT("recipeId is empty. It is the stable id of the recipe (Doc02 19.8)."));
    }
    if(requirementList.Num() == 0){
        fail(TEXT("A recipe needs at least one ingredient: a button is listed only for a staged item (R02 3.4)."));
    }
    if(outputItemId.IsNone()){
        fail(TEXT("outputItemId is empty."));
    }
    TSet<FName> seenItems;
    for(const FS_CraftingRecipeRequirement& requirement : requirementList){
        if(requirement.itemId.IsNone() || requirement.quantity < 1){
            fail(TEXT("Every ingredient needs an item id and a quantity of at least 1."));
        }
        if(seenItems.Contains(requirement.itemId)){
            fail(TEXT("An item appears twice in requirementList. Use one entry with the total quantity."));
        }
        seenItems.Add(requirement.itemId);
    }
    if(animationTag.IsValid() && craftSeconds <= 0.0f){
        Context.AddWarning(FText::FromString(TEXT("The recipe has an animation but craftSeconds is 0, so the item is made before the animation ends (K2).")));
    }
    return result;
}
#endif
```

### CR2.2 — Register recipes with the Asset Manager (Doc02 §19.8)

*Project Settings → Game → Asset Manager → Primary Asset Types to Scan*, add one entry:

| Field | Value |
| --- | --- |
| Primary Asset Type | `Recipe` |
| Asset Base Class | `DA_CraftingRecipe` (`UDA_CraftingRecipe`) |
| Has Blueprint Classes | off |
| Directories | `/Game/Awaken/Recipes` |
| Rules → Cook Rule | Always Cook |

### CR2.3 — `UCraftingRecipeSubsystem`

The recipe catalog. It loads every recipe once, when the game instance starts (a recipe holds ids and tags only, so this loads no art), and builds the index R02 3.1 asks for: from an item type to the recipes that use it. The craft button list is rebuilt on every staging change, and with this index it only ever looks at recipes that touch a staged item.

`UCraftingRecipeSubsystem.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UCraftingRecipeSubsystem.generated.h"

class UDA_CraftingRecipe;

// Looks a recipe up by its id, or by an ingredient. Exists on every machine; the data is static content.
UCLASS()
class AWAKEN_API UCraftingRecipeSubsystem : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static UCraftingRecipeSubsystem* get(const UObject* worldContext);

    // ---- queries ----
    // Null when no recipe has that id. Never crashes.
    const UDA_CraftingRecipe* getRecipe(FName recipeId) const;
    // Every recipe that has this item among its ingredients (R02 3.1, review R02-06).
    TArray<const UDA_CraftingRecipe*> getRecipesUsing(FName itemId) const;
    const TMap<FName, TObjectPtr<UDA_CraftingRecipe>>& getAllRecipes() const;
    // Recipe ids that were found more than once when loading. The data check reports them (R02 §12).
    const TArray<FName>& getDuplicateRecipeIds() const;

    // ---- commands ----
    // Adds one recipe and indexes it. Used by the loader and by tests.
    void registerRecipe(UDA_CraftingRecipe* recipe);
    // Tests only: forget every recipe.
    void resetForTest();

private:
    void loadAllRecipes();

    UPROPERTY() TMap<FName, TObjectPtr<UDA_CraftingRecipe>> recipes;
    // The index: ingredient item id -> the ids of the recipes that use it.
    TMap<FName, TArray<FName>> recipeIdsByIngredient;
    TArray<FName> duplicateRecipeIds;
};
```

`UCraftingRecipeSubsystem.cpp`

```cpp
#include "CraftingFramework/UCraftingRecipeSubsystem.h"
#include "CraftingFramework/UDA_CraftingRecipe.h"
#include "CraftingFramework/CraftingLog.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

void UCraftingRecipeSubsystem::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);
    loadAllRecipes();
    CRAFTING_LOG(Log, this, "Recipe catalog loaded: %d recipes", recipes.Num());
}

UCraftingRecipeSubsystem* UCraftingRecipeSubsystem::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    if(gameInstance == nullptr){
        return nullptr;
    }
    return gameInstance->GetSubsystem<UCraftingRecipeSubsystem>();
}

void UCraftingRecipeSubsystem::loadAllRecipes(){
    UAssetManager* manager = UAssetManager::GetIfInitialized();
    if(manager == nullptr){
        return;
    }
    TArray<FPrimaryAssetId> ids;
    manager->GetPrimaryAssetIdList(UDA_CraftingRecipe::getPrimaryAssetType(), ids);
    for(const FPrimaryAssetId& id : ids){
        UDA_CraftingRecipe* recipe = Cast<UDA_CraftingRecipe>(manager->GetPrimaryAssetPath(id).TryLoad());
        if(recipe != nullptr){
            registerRecipe(recipe);
        }
    }
}

void UCraftingRecipeSubsystem::registerRecipe(UDA_CraftingRecipe* recipe){
    if(recipe == nullptr || recipe->recipeId.IsNone()){
        CRAFTING_LOG(Error, this, "Recipe %s has no recipeId and was not registered", *GetNameSafe(recipe));
        return;
    }
    if(recipes.Contains(recipe->recipeId)){
        duplicateRecipeIds.AddUnique(recipe->recipeId);
        CRAFTING_LOG(Error, this, "Duplicate recipeId %s: %s was not registered", *recipe->recipeId.ToString(), *recipe->GetName());
        return;
    }
    recipes.Add(recipe->recipeId, recipe);
    for(const FS_CraftingRecipeRequirement& requirement : recipe->requirementList){
        recipeIdsByIngredient.FindOrAdd(requirement.itemId).AddUnique(recipe->recipeId);
    }
}

void UCraftingRecipeSubsystem::resetForTest(){
    recipes.Reset();
    recipeIdsByIngredient.Reset();
    duplicateRecipeIds.Reset();
}

const UDA_CraftingRecipe* UCraftingRecipeSubsystem::getRecipe(FName recipeId) const{
    const TObjectPtr<UDA_CraftingRecipe>* found = recipes.Find(recipeId);
    return found != nullptr ? found->Get() : nullptr;
}

TArray<const UDA_CraftingRecipe*> UCraftingRecipeSubsystem::getRecipesUsing(FName itemId) const{
    TArray<const UDA_CraftingRecipe*> result;
    const TArray<FName>* recipeIds = recipeIdsByIngredient.Find(itemId);
    if(recipeIds != nullptr){
        for(const FName& recipeId : *recipeIds){
            const UDA_CraftingRecipe* recipe = getRecipe(recipeId);
            if(recipe != nullptr){
                result.Add(recipe);
            }
        }
    }
    return result;
}

const TMap<FName, TObjectPtr<UDA_CraftingRecipe>>& UCraftingRecipeSubsystem::getAllRecipes() const{
    return recipes;
}

const TArray<FName>& UCraftingRecipeSubsystem::getDuplicateRecipeIds() const{
    return duplicateRecipeIds;
}
```

### CR2.4 — The catalog-wide data check (R02 §12, K21)

One recipe cannot check itself against the item catalog, so these checks run over all recipes at once, like R01's `validateCatalog`.

`UBFL_CraftingValidation.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UBFL_CraftingValidation.generated.h"

UCLASS()
class AWAKEN_API UBFL_CraftingValidation : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // R02 §12. Checks every loaded recipe against the item catalog. Returns the number of errors and fills outErrors
    // with one line each. Warnings go to the log only.
    static int32 validateRecipes(const UObject* worldContext, TArray<FString>& outErrors);
};
```

`UBFL_CraftingValidation.cpp`

```cpp
#include "CraftingFramework/UBFL_CraftingValidation.h"
#include "CraftingFramework/CraftingLog.h"
#include "CraftingFramework/UCraftingRecipeSubsystem.h"
#include "CraftingFramework/UDA_CraftingRecipe.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"

int32 UBFL_CraftingValidation::validateRecipes(const UObject* worldContext, TArray<FString>& outErrors){
    const UCraftingRecipeSubsystem* recipes = UCraftingRecipeSubsystem::get(worldContext);
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(worldContext);
    if(recipes == nullptr || catalog == nullptr){
        outErrors.Add(TEXT("The recipe catalog or the item catalog is not available."));
        return outErrors.Num();
    }

    // The ammunition of every firearm: none of it is crafted (Index A116, PROPOSED for bullets).
    TSet<FName> firearmAmmoIds;
    for(const TPair<FName, TObjectPtr<UDA_ItemDefinition>>& pair : catalog->getAllDefinitions()){
        if(pair.Value->weaponType == E_WeaponType::Firearm && !pair.Value->ammoItemId.IsNone()){
            firearmAmmoIds.Add(pair.Value->ammoItemId);
        }
    }

    for(const FName& duplicateId : recipes->getDuplicateRecipeIds()){
        outErrors.Add(FString::Printf(TEXT("Recipe id %s is used by more than one asset (R02 §12)."), *duplicateId.ToString()));
    }

    for(const TPair<FName, TObjectPtr<UDA_CraftingRecipe>>& pair : recipes->getAllRecipes()){
        const UDA_CraftingRecipe* recipe = pair.Value;
        const FString name = recipe->recipeId.ToString();

        for(const FS_CraftingRecipeRequirement& requirement : recipe->requirementList){
            const UDA_ItemDefinition* ingredient = catalog->getDefinition(requirement.itemId);
            if(ingredient == nullptr){
                outErrors.Add(FString::Printf(TEXT("%s: ingredient %s does not exist (R02 §12)."), *name, *requirement.itemId.ToString()));
                continue;
            }
            if(ingredient->isHeavy()){
                outErrors.Add(FString::Printf(TEXT("%s: ingredient %s has a carry cost. A heavy item never enters the bag, so it can never be staged (Index A103, S4-F01)."), *name, *requirement.itemId.ToString()));
            }
            if(ingredient->isPermanent()){
                outErrors.Add(FString::Printf(TEXT("%s: ingredient %s is a permanent item. The Axe and the map are never ingredients (Index A92)."), *name, *requirement.itemId.ToString()));
            }
        }

        const UDA_ItemDefinition* output = catalog->getDefinition(recipe->outputItemId);
        if(output == nullptr){
            outErrors.Add(FString::Printf(TEXT("%s: output %s does not exist (R02 §12)."), *name, *recipe->outputItemId.ToString()));
            continue;
        }
        if(output->weaponType == E_WeaponType::Firearm){
            outErrors.Add(FString::Printf(TEXT("%s: the output %s is a firearm. Firearms are found in the world and never crafted (Index A116)."), *name, *recipe->outputItemId.ToString()));
        }
        if(firearmAmmoIds.Contains(recipe->outputItemId)){
            outErrors.Add(FString::Printf(TEXT("%s: the output %s is firearm ammunition, which is not crafted (Index A116)."), *name, *recipe->outputItemId.ToString()));
        }
        if(output->isPermanent()){
            outErrors.Add(FString::Printf(TEXT("%s: the output %s is a permanent item."), *name, *recipe->outputItemId.ToString()));
        }
        if(output->isHeavy()){
            CRAFTING_LOG(Warning, worldContext, "Recipe %s makes the heavy item %s: it goes to the hands or lands on the ground, never into the bag (Doc B07 3.1)", *name, *recipe->outputItemId.ToString());
        }
        if(output->isInstanced() && recipe->outputQuantity > 1){
            CRAFTING_LOG(Warning, worldContext, "Recipe %s makes %d copies of the instanced item %s; copies that do not fit the bag limit drop on the ground", *name, recipe->outputQuantity, *recipe->outputItemId.ToString());
        }
    }
    return outErrors.Num();
}
```

> The data check for recipes that share the same ingredient types (S12-P7) was **rejected** by the designer and is not here: each recipe has its own button, so two recipes with the same ingredients are simply two green buttons.

### CR2.5 — Author the starter content (Blueprint / editor)

Create these `DA_CraftingRecipe` assets in `Content/Awaken/Recipes/`. The names and numbers are the designer's worked examples, for testing only. They use R01's starter items; create the few that are missing as ordinary `DA_ItemDefinition` assets (`Hatchling`, `Arrow`, `StoneSpear`).

| Asset | `requirementList` | Output | Gating | `craftSeconds` |
| --- | --- | --- | --- | --- |
| `Spear` | `Stick` × 2, `Rock` × 1 | `Spear` × 1 | none (made anywhere) | 1.5 |
| `Hatchling` | `Stick` × 1, `Rock` × 1 | `Hatchling` × 1 | none | 1.0 |
| `Arrow` | `Stick` × 2, `Rock` × 2 | `Arrow` × 1 | none | 1.0 |
| `StoneSpear` (a weapon upgrade, R02 3.10) | `Spear` × 1, `Rock` × 1 | `StoneSpear` × 1 | `requiredStationTag = Station.CraftingTable`, `requiredBlueprintTag = Progression.Recipe.StoneSpear` | 2.0 |

> `Rock` is the **ordinary** ingredient (carry cost 0). The heavy `Stone` of the Carry guide can never be an ingredient (R02 amendment A103: "the spear's rock is an ordinary item"). The designer's second worked example says "stone"; in this project's data that ingredient is `Rock`.

### Verify (Section 2)

1. Compile. Start the editor: the log shows `Recipe catalog loaded: 4 recipes`.
2. Open `StoneSpear`: the categories *Identity, Recipe, Gating, Presentation* show their fields; the station picker offers only `Station.*` tags.
3. Break one on purpose and validate the asset (*right-click → Asset Actions → Validate*): add `Stick` twice → "appears twice"; clear `recipeId` → "recipeId is empty". Undo.
4. Give two recipe assets the same `recipeId` and restart: the log shows the *Duplicate recipeId* error and only one is registered. Undo.
5. `validateRecipes` (from a test, or `Awaken.Craft.Validate` once Section 9 exists) on the four assets: 0 errors. Then set `Spear`'s ingredient to the heavy `Stone` → one error naming S4-F01. Undo.

### Handoff
- **R07**: each recipe names an `animationTag`; its length must equal `craftSeconds` (K2). Add that comparison to your data check when the montage table exists.
- **R06**: each gated recipe names a `Progression.Recipe.*` tag.
- **R10**: no recipe text key is needed. The hover card shows the *result's* name, which is the item's derived key (`Item.<itemId>.Name`, R01 guide I22).
- **B22**: `validateRecipes` is the editor data check of R02 §12; hook it into the build or release check when that pipeline exists.

---

# SECTION 3 — The Craft Button List (the rules, as pure functions)

**Goal:** from "what is staged" and "which station am I at", produce the list of craft buttons: which recipes, in which state, in which order, capped. No session, no network, no bag: only the rule.
**Spec:** R02 3.4, 3.5 (both worked examples), 3.9, §6; Doc B22 §3.7 (the tests "Craft button states", "Craft leftovers", "Craft list cap and order", "Locked blueprint").
**Depends on:** Section 2.

### The rules, stated once

| Rule | Source |
| --- | --- |
| A recipe is listed only if it uses **at least one staged item type**. An empty crafting area gives an empty list | 3.4, S12-P1 |
| A recipe whose blueprint is locked is **never** listed, even when the staged items would fully satisfy it | 3.4, 3.9, S12-D6 |
| **Green** (`Craftable`): nothing missing and nothing staged that the recipe does not use. Extra *quantity* of an item it does use is fine | 3.4 |
| **White** (`ExtraItems`): nothing missing, but at least one staged item type the recipe does not use | 3.4 |
| **Red** (`Missing`): an ingredient quantity or the required station is missing. **Red wins over white** | 3.4 |
| A recipe that needs a station is red away from the station and normal next to it | 3.4, 3.9 |
| Every green and every white is always listed. Reds fill the remaining places up to the cap (15, data). If green and white alone reach the cap, all of them are listed and **no** red is shown | 3.4, S12-D6, S12-P9 |
| Order: green, then white, then red. Reds by fewest missing things first | 3.4 |
| Recipes are never merged: one button per recipe | 3.4, S12-P7 |

Decision K3 makes these two functions the **only** place the rule exists. The server uses them to accept or refuse a request (Section 6); the owning client uses them to colour the buttons.

### CR3.1, CR3.2 — `UBFL_Crafting`

`Public/CraftingFramework/UBFL_Crafting.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CraftingFramework/01Structs/S_CraftingFramework.h"
#include "UBFL_Crafting.generated.h"

class UDA_CraftingRecipe;

UCLASS()
class AWAKEN_API UBFL_Crafting : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // ---- queries: the rule of R02 3.4, with no state of their own (K3) ----
    // The state of ONE recipe for these staged items and these station tags. It does not look at the blueprint.
    static FS_AvailableRecipeOption evaluateRecipe(const UDA_CraftingRecipe& recipe, const TMap<FName, int32>& stagedQuantities, const FGameplayTagContainer& stationTags);
    // The capped, ordered craft button list: green, then white, then red. Locked recipes are left out.
    static TArray<FS_AvailableRecipeOption> buildRecipeList(const UObject* worldContext, const TMap<FName, int32>& stagedQuantities, const FGameplayTagContainer& stationTags);
};
```

`Private/CraftingFramework/UBFL_Crafting.cpp`

```cpp
#include "CraftingFramework/UBFL_Crafting.h"
#include "CraftingFramework/CraftingServices.h"
#include "CraftingFramework/UCraftingRecipeSubsystem.h"
#include "CraftingFramework/UCraftingSettings.h"
#include "CraftingFramework/UDA_CraftingRecipe.h"

FS_AvailableRecipeOption UBFL_Crafting::evaluateRecipe(const UDA_CraftingRecipe& recipe, const TMap<FName, int32>& stagedQuantities, const FGameplayTagContainer& stationTags){
    FS_AvailableRecipeOption option;
    option.recipeId = recipe.recipeId;

    // What is missing, and how many times the staged items would make the recipe.
    int32 craftableCount = MAX_int32;
    for(const FS_CraftingRecipeRequirement& requirement : recipe.requirementList){
        const int32 staged = stagedQuantities.FindRef(requirement.itemId);
        if(staged < requirement.quantity){
            FS_CraftingRecipeRequirement missing;
            missing.itemId = requirement.itemId;
            missing.quantity = requirement.quantity - staged;
            option.missingIngredients.Add(missing);
        }
        craftableCount = FMath::Min(craftableCount, staged / FMath::Max(1, requirement.quantity));
    }
    // A station recipe is red away from the station (R02 3.9).
    option.isStationMissing = recipe.requiredStationTag.IsValid() && !stationTags.HasTag(recipe.requiredStationTag);

    // What is staged that the recipe does not use. Extra QUANTITY of a used item is not an extra item.
    for(const TPair<FName, int32>& staged : stagedQuantities){
        if(staged.Value > 0 && !recipe.usesItem(staged.Key)){
            option.extraItems.Add(staged.Key);
        }
    }
    option.extraItems.Sort([](const FName& left, const FName& right){
        return left.LexicalLess(right);
    });

    // Red wins over white (R02 3.4).
    if(option.missingIngredients.Num() > 0 || option.isStationMissing || recipe.requirementList.Num() == 0){
        option.state = E_RecipeListState::Missing;
        option.maxCraftableCount = 0;
    }else{
        option.state = option.extraItems.Num() > 0 ? E_RecipeListState::ExtraItems : E_RecipeListState::Craftable;
        option.maxCraftableCount = craftableCount;
    }
    return option;
}

TArray<FS_AvailableRecipeOption> UBFL_Crafting::buildRecipeList(const UObject* worldContext, const TMap<FName, int32>& stagedQuantities, const FGameplayTagContainer& stationTags){
    TArray<FS_AvailableRecipeOption> green;
    TArray<FS_AvailableRecipeOption> white;
    TArray<FS_AvailableRecipeOption> red;
    const UCraftingRecipeSubsystem* recipes = UCraftingRecipeSubsystem::get(worldContext);
    const UCraftingServices* services = UCraftingServices::get(worldContext);
    if(recipes == nullptr || services == nullptr){
        return green;
    }

    // From the staged item types to the recipes that use them: nothing else is looked at (R02 3.1, review R02-06).
    TSet<FName> seenRecipeIds;
    for(const TPair<FName, int32>& staged : stagedQuantities){
        if(staged.Value <= 0){
            continue;
        }
        for(const UDA_CraftingRecipe* recipe : recipes->getRecipesUsing(staged.Key)){
            if(seenRecipeIds.Contains(recipe->recipeId)){
                continue;
            }
            seenRecipeIds.Add(recipe->recipeId);
            if(!services->isUnlocked(recipe->requiredBlueprintTag)){
                continue;   // a locked recipe is never listed until it is unlocked (R02 3.9)
            }
            const FS_AvailableRecipeOption option = evaluateRecipe(*recipe, stagedQuantities, stationTags);
            if(option.state == E_RecipeListState::Craftable){
                green.Add(option);
            }else if(option.state == E_RecipeListState::ExtraItems){
                white.Add(option);
            }else{
                red.Add(option);
            }
        }
    }

    // A stable order, so the list does not shuffle while the player stages (K5).
    const auto byRecipeId = [](const FS_AvailableRecipeOption& left, const FS_AvailableRecipeOption& right){
        return left.recipeId.LexicalLess(right.recipeId);
    };
    green.Sort(byRecipeId);
    white.Sort(byRecipeId);
    red.Sort([](const FS_AvailableRecipeOption& left, const FS_AvailableRecipeOption& right){
        const int32 leftMissing = left.getMissingCount();
        const int32 rightMissing = right.getMissingCount();
        return leftMissing != rightMissing ? leftMissing < rightMissing : left.recipeId.LexicalLess(right.recipeId);
    });

    // Every green and every white is always listed; reds only fill what is left of the cap (R02 3.4).
    TArray<FS_AvailableRecipeOption> list = MoveTemp(green);
    list.Append(white);
    const int32 placesLeft = UCraftingSettings::get()->maxVisibleRecipes - list.Num();
    for(int32 index = 0; index < red.Num() && index < placesLeft; index++){
        list.Add(red[index]);
    }
    return list;
}
```

### Verify (Section 3)

These are tier 1 tests: a fixture, a few recipes, a staged map, one function call. The automated versions are in CR9.5. Recipes: **hatchling** (1 stick, 1 rock), **arrow** (2 sticks, 2 rocks), **spear upgrade** (the spear, 1 stick), as in the designer's worked example 2.

| Staged | Hatchling | Arrow | Spear upgrade |
| --- | --- | --- | --- |
| *(nothing)* | not listed | not listed | not listed |
| 1 stick, 1 rock | **green** | **red**, missing 1 stick and 1 rock | **red**, missing the spear |
| 3 sticks, 4 rocks | green, `maxCraftableCount` 3 | green, `maxCraftableCount` 1 | red |
| stick, rock, spear | **white**, extra `Spear` | red (red wins over white) | **white**, extra `Rock` |
| stick, spear | red, missing 1 rock | red | **green** |
| 1 rock only | red | red | **not listed**: it uses no staged item type |

| More checks | Expect |
| --- | --- |
| Order of the third row | hatchling and arrow are both green; the list is `Arrow`, `Hatchling` (by recipe id), then the red spear upgrade |
| Spear upgrade with `requiredBlueprintTag` set and not unlocked; stage stick and spear | **not listed at all**; unlock the tag in the stub → green |
| A recipe with `requiredStationTag = Station.Forge`, fully staged, `stationTags` empty | red, `isStationMissing` true, `missingIngredients` empty, `getMissingCount()` 1 |
| The same with `stationTags` holding `Station.Forge` | green |
| 20 recipes that are all red for the staged items, missing 1, 2 … 20 units | 15 entries, the one missing 1 unit first |
| 16 recipes that are green or white, plus red ones | all 16 are listed and **no** red |

### Handoff
- **B17** draws one button per entry of this list, in this order, with the colour and the second cue of its state (a check mark on green, a lock on red: the style is yours). The hover card reads `missingIngredients`, `isStationMissing` and `extraItems`. An empty list shows the empty state (the plus sign and the text key `UI.Crafting.DropHere`).

---

# SECTION 4 — The Crafting Station Component

**Goal:** any actor becomes a crafting station by carrying one component: it says which station it is, has a stable id, and lets one player use it at a time.
**Spec:** R02 3.2, 3.9, §1 (Building, Interaction Core); Doc B18 3.7 (Exclusive); Doc B01 (the piece); Doc02 §19.3.
**Depends on:** Section 1.

### Design rules (K6)

- **Building owns the piece; Crafting owns what makes it a station.** A Crafting Table or a Forge is a Building piece that carries this component with the tag `Station.CraftingTable` or `Station.Forge`. The tags come from the same `Station.*` namespace Processing uses (Doc B10), but a crafting station runs no job: it only gates recipes.
- **Exclusive** (Doc B18 3.7): one player at a time; others see it as in use. The in-use state replicates as **one flag**, so a prompt can show it. Who is using it stays on the server.
- **No idle release** (Index A103, S4-F27): a player who stays at the station with the bag open keeps it.
- **A stable id** (Doc02 §19.3): Building passes its PieceID in; a station that nobody gave an id makes its own when play begins. The id is the key of the session's reservations in R01.

### CR4.1 — `UCraftingStationComponent`

`Public/CraftingFramework/UCraftingStationComponent.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CraftingFramework/00Enums/E_CraftingFramework.h"
#include "UCraftingStationComponent.generated.h"

class UCraftingComponent;

// R02 3.9: makes its owner a crafting station. The owner is normally a Building piece (Doc B01).
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class AWAKEN_API UCraftingStationComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UCraftingStationComponent();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Query. Null when the actor is not a crafting station.
    static UCraftingStationComponent* findOn(const AActor* actor);

    // Which stations this piece is. A recipe's requiredStationTag must be among them (R02 3.9).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Station", meta = (Categories = "Station")) FGameplayTagContainer stationTags;

    // ---- queries ----
    UFUNCTION(BlueprintPure, Category = "Awaken|Crafting") FGuid getStationId() const;
    // True while a player uses the station. Replicated, so the interaction prompt can show "in use" (Doc B18 3.7).
    UFUNCTION(BlueprintPure, Category = "Awaken|Crafting") bool isInUse() const;
    // Server only. Null when nobody uses it.
    UCraftingComponent* getUser() const;

    // ---- commands (server) ----
    // Doc B01 passes the PieceID of the piece right after spawning it, and again after a load.
    void setStationId(const FGuid& newStationId);
    // The Exclusive lock. An empty tag = locked for this user. Interaction.FailReason.InUse when someone else has it.
    FGameplayTag tryLock(UCraftingComponent* user);
    // Does nothing unless this user holds the lock.
    void release(UCraftingComponent* user);

protected:
    UPROPERTY(Replicated) FGuid stationId;
    UPROPERTY(Replicated) bool inUse = false;

    // Server only.
    TWeakObjectPtr<UCraftingComponent> currentUser;
};
```

`Private/CraftingFramework/UCraftingStationComponent.cpp`

```cpp
#include "CraftingFramework/UCraftingStationComponent.h"
#include "CraftingFramework/CraftingLog.h"
#include "CraftingFramework/UCraftingComponent.h"
#include "CraftingFramework/02Tags/T_CraftingFramework.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UCraftingStationComponent::UCraftingStationComponent(){
    SetIsReplicatedByDefault(true);
}

void UCraftingStationComponent::BeginPlay(){
    Super::BeginPlay();
    if(GetOwner()->HasAuthority() && !stationId.IsValid()){
        stationId = FGuid::NewGuid();   // nobody gave it one (a test station, a designer-placed prop): a player-created kind id
    }
}

void UCraftingStationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UCraftingStationComponent, stationId);
    DOREPLIFETIME(UCraftingStationComponent, inUse);   // the single in-use flag of Doc B18 3.7
}

UCraftingStationComponent* UCraftingStationComponent::findOn(const AActor* actor){
    return actor != nullptr ? actor->FindComponentByClass<UCraftingStationComponent>() : nullptr;
}

FGuid UCraftingStationComponent::getStationId() const{
    return stationId;
}

bool UCraftingStationComponent::isInUse() const{
    return inUse;
}

UCraftingComponent* UCraftingStationComponent::getUser() const{
    return currentUser.Get();
}

void UCraftingStationComponent::setStationId(const FGuid& newStationId){
    if(GetOwner()->HasAuthority() && newStationId.IsValid()){
        stationId = newStationId;
    }
}

FGameplayTag UCraftingStationComponent::tryLock(UCraftingComponent* user){
    if(user == nullptr){
        return T_Interaction_FailReason_InUse;
    }
    if(currentUser.IsValid() && currentUser.Get() != user){
        return T_Interaction_FailReason_InUse;   // one player at a time; the first request wins (Doc B18 3.7)
    }
    currentUser = user;
    inUse = true;
    return FGameplayTag();
}

void UCraftingStationComponent::release(UCraftingComponent* user){
    if(currentUser.Get() != user){
        return;
    }
    currentUser = nullptr;
    inUse = false;
}
```

(`UCraftingComponent.h` does not exist until Section 5. If you compile now, comment the include out and put it back in Section 5; only the pointer type is used here, and the forward declaration in the header covers it.)

### CR4.2 — A stub station actor (owner: Doc B01)

Until Building has a Crafting Table piece, tests and the dev command need something to stand next to. Add to `Private/CraftingFramework/Stubs/CraftingStubs.h` (and `#include "GameFramework/Actor.h"`):

```cpp
class UCraftingStationComponent;

// TEMPORARY STUB — owner: Doc B01 Building. An actor that is only a crafting station.
UCLASS()
class AStubCraftingStation : public AActor{
    GENERATED_BODY()

public:
    AStubCraftingStation();

    UPROPERTY(VisibleAnywhere) TObjectPtr<UCraftingStationComponent> stationComponent;
};
```

and to `CraftingStubs.cpp` (with `#include "CraftingFramework/UCraftingStationComponent.h"` and `#include "Components/SceneComponent.h"`):

```cpp
AStubCraftingStation::AStubCraftingStation(){
    bReplicates = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    stationComponent = CreateDefaultSubobject<UCraftingStationComponent>(TEXT("Station"));
}
```

### Verify (Section 4)

1. Compile.
2. Spawn an `AStubCraftingStation` in a test: after it begins play, `getStationId()` is valid and `isInUse()` is false.
3. `setStationId(someGuid)` before or after `BeginPlay` → `getStationId()` returns that guid, and it is not replaced.
4. The lock itself is tested with two real users in Section 5.

### Handoff
- **B01**: add `UCraftingStationComponent` to the Crafting Table and Forge piece classes, set `stationTags` on each definition's actor, and call `setStationId(pieceId)` after spawning a piece and after restoring one from a save. Nothing about a crafting station is saved by Crafting.
- **B18**: the station's `CanInteract` refuses with `Interaction.FailReason.InUse` while `isInUse()` is true and the asker is not the user; its interaction calls `enterStation` (Section 5). If Interaction Core gets its own lock object, `tryLock` and `release` become two thin calls into it.

---

# SECTION 5 — The Crafting Component: the Station Context, the Session, Staging

**Goal:** a player can enter a station, stage and unstage ingredients, and read the craft button list; the session opens at the first staged ingredient and can be closed.
**Spec:** R02 §2, 3.2, 3.3, 3.11, §4.1 (`OpenCraftingSession`, `StageIngredient`, `UnstageIngredient`, `GetAvailableRecipes`, `CloseCraftingSession`), §7, §8; R01 3.6, 3.8; Doc B19 3.3; Doc02 §6, §19.2.
**Depends on:** Sections 3 and 4, and the R01 guide's Section 7.

### Files

```
Public/CraftingFramework/UCraftingComponent.h
Private/CraftingFramework/UCraftingComponent.cpp
Private/ItemInventoryFramework/Stubs/AInventoryTestCharacter.h/.cpp   (patch)
```

### Design rules this section enforces

- **The component lives on the Character** (R02 §2, decision D5, Doc B19 3.3). A session belongs to one player and to one body.
- **Server only mutates.** Every command begins with the server check.
- **Staging never moves an item** (R02 3.3). `stageIngredient` wraps R01's `reserveForStaging`: quantity moves from available to reserved inside the bag. This component holds no list of staged items; it reads the bag (K4).
- **The session state replicates to the owning player only** (`COND_OwnerOnly`). Nobody else needs to know that this player has the bag open: opening the bag is local presentation (Doc B17 3.6, S12b-D10). Other players see only the craft animation, through the cosmetic channel.
- **Validate everything, then mutate once** (Doc02 §6). A refusal changes nothing and names its reason with a tag.
- **Never saved** (R02 §9). This component is not a save participant.
- **No tick.** One timer exists, only while a craft runs (Section 6).

### Who may craft at all (R02 3.2, 3.8, 3.11)

| The player | Result | Reason tag |
| --- | --- | --- |
| is Bagless | cannot stage, craft or enter a station | `Crafting.FailReason.BagRequired` |
| carries a heavy item (Doc B07) | the same | `Interaction.FailReason.HandsFull` |
| is Downed, Restrained, Arriving, dead or in a scene | the same | `Interaction.FailReason.NotAllowedInState` |

The bag screen normally never opens in these states (Doc B17 `CanOpenBag`), so these refusals are the server's second layer, not what the player usually sees.

### CR5.1 to CR5.6 — `UCraftingComponent`

`UCraftingComponent.h` (Section 5 version; later sections add members as marked)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CraftingFramework/01Structs/S_CraftingFramework.h"
#include "CraftingFramework/04Delegates/D_CraftingFramework.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UCraftingComponent.generated.h"

class APlayerState;
class UCraftingStationComponent;

// R02 §2: the crafting session of one player. A Character component (decision D5). Never saved (R02 §9).
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class AWAKEN_API UCraftingComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UCraftingComponent();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Query. The component on that character. A PlayerState is accepted too and means its current pawn. Null when missing.
    static UCraftingComponent* findOn(const AActor* crafter);
    // Query. The component on the player's current pawn, or null.
    static UCraftingComponent* findFor(const APlayerState* player);

    // ---- queries: valid on the server and on the owning client ----
    UFUNCTION(BlueprintPure, Category = "Awaken|Crafting") FS_CraftingSessionState getSessionState() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Crafting") bool isSessionOpen() const;
    // A craft was requested and its animation has not ended yet (R02 3.7).
    UFUNCTION(BlueprintPure, Category = "Awaken|Crafting") bool isCraftInProgress() const;
    // What is in the crafting area: item id -> reserved quantity, read from the bag (K4).
    TMap<FName, int32> getStagedQuantities() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Crafting") int32 getStagedQuantity(FName itemId) const;
    // R02 §4.1 GetAvailableRecipes(): the capped, ordered craft button list for what is staged right now (Section 3).
    UFUNCTION(BlueprintPure, Category = "Awaken|Crafting") TArray<FS_AvailableRecipeOption> getAvailableRecipes() const;

    // ---- commands (server only; each returns an explained result) ----
    // The player interacted with a crafting station (Doc B18). Locks it and remembers it; nothing is staged yet (K8).
    FS_InventoryResult enterStation(UCraftingStationComponent* newStation);
    // R02 §4.1 OpenCraftingSession. Called by the first stageIngredient of a bag that has no session (R02 3.2, K9).
    FS_InventoryResult openCraftingSession();
    // R02 §4.1 StageIngredient. Wraps ReserveForStaging: the quantity moves from available to reserved.
    // For an instanced item named by item id, the most worn free copy is taken (R02 3.10).
    FS_InventoryResult stageIngredient(const FS_ItemRef& ref, int32 quantity);
    // R02 §4.1 UnstageIngredient. Wraps ReleaseStagedItem: the quantity is available again.
    FS_InventoryResult unstageIngredient(const FS_ItemRef& ref, int32 quantity);
    // R02 §4.1 CloseCraftingSession(Reason). Releases every reservation of the session and leaves the station (R02 3.8).
    // Safe to call when no session is open.
    void closeCraftingSession(E_CraftingCloseReason reason);

    // Raised on the server and on the owning client after any change to the session state.
    UPROPERTY(BlueprintAssignable, Category = "Awaken|Crafting") FD_CraftingChanged onCraftingChanged;

protected:
    // Replicated to the owning player only (Doc02 §8).
    UPROPERTY(ReplicatedUsing = OnRep_sessionState) FS_CraftingSessionState sessionState;

    UFUNCTION()
    void OnRep_sessionState(const FS_CraftingSessionState& previousState);

    // Crafting.SessionClosed is local to the crafting player (R02 §7). The payload cannot be rebuilt from state.
    UFUNCTION(Client, Reliable)
    void clientSessionClosed(const TArray<FS_ItemStack>& returnedItems, E_CraftingCloseReason reason);

    bool isServerSide() const;
    APlayerState* getCrafterPlayerState() const;
    // True on the machine whose player this character is.
    bool isLocalCrafter() const;

    // Bag, hands and state. Empty tag = allowed.
    FGameplayTag checkPlayerCanCraft() const;
    // Server. After any change to sessionState: replicate now, notify.
    void pushState();
    // Server. Unlocks the station and forgets it.
    void releaseStation();
    void raiseSessionOpened() const;

    static FS_InventoryResult refusal(const FGameplayTag& reason);

    // Server only.
    TWeakObjectPtr<UCraftingStationComponent> station;
};
```

`UCraftingComponent.cpp`

```cpp
#include "CraftingFramework/UCraftingComponent.h"
#include "CraftingFramework/CraftingLog.h"
#include "CraftingFramework/CraftingServices.h"
#include "CraftingFramework/UBFL_Crafting.h"
#include "CraftingFramework/UCraftingStationComponent.h"
#include "CraftingFramework/02Tags/T_CraftingFramework.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Net/UnrealNetwork.h"

// ---------- construction and replication ----------

UCraftingComponent::UCraftingComponent(){
    SetIsReplicatedByDefault(true);
}

void UCraftingComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME_CONDITION(UCraftingComponent, sessionState, COND_OwnerOnly);   // only the crafting player needs it
}

UCraftingComponent* UCraftingComponent::findOn(const AActor* crafter){
    if(crafter == nullptr){
        return nullptr;
    }
    const APlayerState* player = Cast<APlayerState>(crafter);
    if(player != nullptr){
        return findFor(player);
    }
    return crafter->FindComponentByClass<UCraftingComponent>();
}

UCraftingComponent* UCraftingComponent::findFor(const APlayerState* player){
    const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
    return pawn != nullptr ? pawn->FindComponentByClass<UCraftingComponent>() : nullptr;
}

bool UCraftingComponent::isServerSide() const{
    return GetOwner() != nullptr && GetOwner()->HasAuthority();
}

APlayerState* UCraftingComponent::getCrafterPlayerState() const{
    const APawn* pawn = Cast<APawn>(GetOwner());
    return pawn != nullptr ? pawn->GetPlayerState() : nullptr;
}

bool UCraftingComponent::isLocalCrafter() const{
    const APawn* pawn = Cast<APawn>(GetOwner());
    return pawn != nullptr && pawn->IsLocallyControlled();
}

FS_InventoryResult UCraftingComponent::refusal(const FGameplayTag& reason){
    FS_InventoryResult result;
    result.failReason = reason;
    return result;
}

// ---------- queries ----------

FS_CraftingSessionState UCraftingComponent::getSessionState() const{
    return sessionState;
}

bool UCraftingComponent::isSessionOpen() const{
    return sessionState.isOpen;
}

bool UCraftingComponent::isCraftInProgress() const{
    return !sessionState.activeRecipeId.IsNone();
}

TMap<FName, int32> UCraftingComponent::getStagedQuantities() const{
    TMap<FName, int32> staged;
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getCrafterPlayerState());
    if(inventory == nullptr){
        return staged;
    }
    // The same code on the server (the real bag) and on the owning client (its replicated copy of the bag).
    // A staged weapon is an entry with a reserved quantity of 1; several staged copies of one type add up.
    for(const FS_InventoryEntry& entry : inventory->getBagEntries()){
        if(entry.reservedQuantity > 0){
            staged.FindOrAdd(entry.itemId) += entry.reservedQuantity;
        }
    }
    return staged;
}

int32 UCraftingComponent::getStagedQuantity(FName itemId) const{
    return getStagedQuantities().FindRef(itemId);
}

TArray<FS_AvailableRecipeOption> UCraftingComponent::getAvailableRecipes() const{
    return UBFL_Crafting::buildRecipeList(this, getStagedQuantities(), sessionState.stationTags);
}

// ---------- shared checks and helpers ----------

FGameplayTag UCraftingComponent::checkPlayerCanCraft() const{
    const APlayerState* player = getCrafterPlayerState();
    const UInventoryServices* services = UInventoryServices::get(this);
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    if(player == nullptr || services == nullptr || inventory == nullptr){
        return T_Inventory_FailReason_NotFound;
    }
    if(!inventory->hasBag()){
        return T_Crafting_FailReason_BagRequired;   // a Bagless player cannot craft at all (R02 3.11, R01 3.8)
    }
    if(services->isIncapacitated(player)){
        return T_Interaction_FailReason_NotAllowedInState;
    }
    if(services->isCarryActive(player)){
        return T_Interaction_FailReason_HandsFull;   // refused while carrying; refusing never loses an item (Index A93, A115 S12-P5)
    }
    return FGameplayTag();
}

void UCraftingComponent::pushState(){
    if(isServerSide()){
        GetOwner()->ForceNetUpdate();
    }
    onCraftingChanged.Broadcast(GetOwner());
}

void UCraftingComponent::releaseStation(){
    if(station.IsValid()){
        station->release(this);
    }
    station = nullptr;
    sessionState.hasStation = false;
    sessionState.stationId = FGuid();
    sessionState.stationTags.Reset();
}

// ---------- messages (R02 §7: local to the crafting player) ----------

void UCraftingComponent::raiseSessionOpened() const{
    FS_CraftingSessionOpenedMessage message;
    message.crafter = GetOwner();
    message.stationId = sessionState.stationId;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Crafting_SessionOpened, message);
}

// The owning client learns of a change only here.
void UCraftingComponent::OnRep_sessionState(const FS_CraftingSessionState& previousState){
    if(!previousState.isOpen && sessionState.isOpen){
        raiseSessionOpened();
    }
    onCraftingChanged.Broadcast(GetOwner());
}

void UCraftingComponent::clientSessionClosed_Implementation(const TArray<FS_ItemStack>& returnedItems, E_CraftingCloseReason reason){
    FS_CraftingSessionClosedMessage message;
    message.crafter = GetOwner();
    message.returnedItems = returnedItems;
    message.reason = reason;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Crafting_SessionClosed, message);
}

// ---------- the station context (K8) ----------

FS_InventoryResult UCraftingComponent::enterStation(UCraftingStationComponent* newStation){
    if(!isServerSide()){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    if(newStation == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    const FGameplayTag blocked = checkPlayerCanCraft();
    if(blocked.IsValid()){
        return refusal(blocked);
    }
    if(station.Get() == newStation){
        return FS_InventoryResult();   // already there
    }
    if(newStation->isInUse()){
        return refusal(T_Interaction_FailReason_InUse);   // checked before anything of ours is given up
    }
    // Everything checked. Whatever was open belongs to another place and ends first.
    if(sessionState.isOpen || station.IsValid()){
        closeCraftingSession(E_CraftingCloseReason::BagClosed);
    }
    const FGameplayTag lockRefusal = newStation->tryLock(this);
    if(lockRefusal.IsValid()){
        return refusal(lockRefusal);
    }
    station = newStation;
    sessionState.hasStation = true;
    sessionState.stationId = newStation->getStationId();
    sessionState.stationTags = newStation->stationTags;
    pushState();
    CRAFTING_LOG(Log, this, "%s entered station %s (%s)", *GetNameSafe(GetOwner()), *GetNameSafe(newStation->GetOwner()), *newStation->stationTags.ToStringSimple());
    return FS_InventoryResult();
}

// ---------- the session (R02 3.2) ----------

FS_InventoryResult UCraftingComponent::openCraftingSession(){
    if(!isServerSide()){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    if(sessionState.isOpen){
        return FS_InventoryResult();
    }
    const FGameplayTag blocked = checkPlayerCanCraft();
    if(blocked.IsValid()){
        return refusal(blocked);
    }
    sessionState.isOpen = true;
    pushState();
    if(isLocalCrafter()){
        raiseSessionOpened();   // the host's own player; a client raises it from OnRep_sessionState
    }
    CRAFTING_LOG(Log, this, "%s opened a crafting session (%s)", *GetNameSafe(GetOwner()), sessionState.hasStation ? TEXT("at a station") : TEXT("by hand"));
    return FS_InventoryResult();
}

FS_InventoryResult UCraftingComponent::stageIngredient(const FS_ItemRef& ref, int32 quantity){
    if(!isServerSide()){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const FGameplayTag blocked = checkPlayerCanCraft();
    if(blocked.IsValid()){
        return refusal(blocked);
    }
    if(isCraftInProgress()){
        return refusal(T_Crafting_FailReason_CraftInProgress);   // the crafting area is fixed while an item is being made (K13)
    }
    APlayerState* player = getCrafterPlayerState();
    // R01 does the real work and every item check: the Axe, the map, a wielded weapon, a container that holds something,
    // a quantity that is not available (R01 guide S7.2). The station id is the key of this session's reservations.
    const FS_InventoryResult reserved = UBFL_Inventory::reserveForStaging(player, ref, quantity, sessionState.stationId);
    if(!reserved.succeeded()){
        return reserved;
    }
    // The session starts when the first ingredient is staged (R02 3.2, Index A115 S12-P2).
    if(!sessionState.isOpen){
        const FS_InventoryResult opened = openCraftingSession();
        if(!opened.succeeded()){
            UBFL_Inventory::releaseStagedReservation(player, sessionState.stationId);   // cannot happen after the check above; never leave it staged
            return opened;
        }
    }
    CRAFTING_LOG(Verbose, this, "%s staged %s x%d", *GetNameSafe(GetOwner()), *ref.itemId.ToString(), reserved.quantity);
    return reserved;
}

FS_InventoryResult UCraftingComponent::unstageIngredient(const FS_ItemRef& ref, int32 quantity){
    if(!isServerSide()){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    if(isCraftInProgress()){
        return refusal(T_Crafting_FailReason_CraftInProgress);
    }
    // No state check: giving something back is always allowed. The session stays open with an empty crafting area;
    // it ends when the bag closes (R02 3.2).
    return UBFL_Inventory::releaseStagedItem(getCrafterPlayerState(), ref, quantity);
}

void UCraftingComponent::closeCraftingSession(E_CraftingCloseReason reason){
    if(!isServerSide()){
        return;
    }
    const bool wasOpen = sessionState.isOpen;
    // What was still staged, for the message. The items never left the bag: releasing only makes them available again.
    TArray<FS_ItemStack> returnedItems;
    if(wasOpen){
        for(const TPair<FName, int32>& staged : getStagedQuantities()){
            FS_ItemStack stack;
            stack.itemId = staged.Key;
            stack.quantity = staged.Value;
            returnedItems.Add(stack);
        }
        APlayerState* player = getCrafterPlayerState();
        if(player != nullptr){
            UBFL_Inventory::releaseStagedReservation(player, sessionState.stationId);
        }
    }
    const bool hadAnything = wasOpen || sessionState.hasStation;
    releaseStation();
    sessionState = FS_CraftingSessionState();
    if(!hadAnything){
        return;   // nothing was open: nothing to tell anyone
    }
    pushState();
    if(wasOpen){
        clientSessionClosed(returnedItems, reason);
        CRAFTING_LOG(Log, this, "%s's crafting session closed (%s), %d staged item type(s) returned", *GetNameSafe(GetOwner()), *UEnum::GetValueAsString(reason), returnedItems.Num());
    }
}
```

> **Why there is no list of staged items in this class.** R01 already stores the reservation in the bag and replicates each entry's `reservedQuantity` to the owner. A second list here could only drift from it. `getStagedQuantities()` reads the one truth, and reads it the same way on both machines.

> **`clientSessionClosed` on a listen server.** A client RPC called on the host for the host's own character runs on the host, so the host's player gets the message too. For a character with no owning connection it is simply dropped.

### CR5.7 — Put the component on the Character

In the R01 stub `AInventoryTestCharacter` (owners: Docs B19 and R07; the real Character gets the same line):

```cpp
// header
class UCraftingComponent;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCraftingComponent> craftingComponent;

// constructor (.cpp), with #include "CraftingFramework/UCraftingComponent.h"
    craftingComponent = CreateDefaultSubobject<UCraftingComponent>(TEXT("Crafting"));
```

### CR5.8 — Blueprint exposure (BP)

The queries are `BlueprintPure` and `onCraftingChanged` is `BlueprintAssignable`. A screen rebuilds its craft buttons from `getAvailableRecipes` on two events: R01's `Inventory.Changed` for the local player's bag (a staged quantity changed), and `onCraftingChanged` (a station was entered or left, a craft started or ended).

### Verify (Section 5)

Until Section 9 adds the commands, use a small test or a temporary `BeginPlay` snippet on the host. Bag: 10 sticks, 5 rocks.

| Test | Expect |
| --- | --- |
| New player | `isSessionOpen()` false; `getStagedQuantities()` empty; `getAvailableRecipes()` empty |
| `stageIngredient(Stick, 1)` | accepted; the bag still holds **10** sticks, 9 available, 1 reserved; `isSessionOpen()` **true**; `Crafting.SessionOpened` raised once with an invalid station id; `getStagedQuantity(Stick)` 1 |
| `stageIngredient(Stick, 1)` again | staged 2; **no** second `SessionOpened` |
| `stageIngredient(Rock, 5)` (the whole stack) | staged 5 rocks |
| `stageIngredient(Stick, 9)` | refused `Inventory.FailReason.InsufficientQuantity` (only 8 are available); nothing changes |
| `getAvailableRecipes()` with 2 sticks and 5 rocks staged | `Arrow`, `Hatchling`, `Spear` green, in that order (by recipe id); `StoneSpear` is **not listed**, because its blueprint is locked |
| `unstageIngredient(Rock, 1)` | 4 rocks staged, 1 more available |
| `unstageIngredient(Rock, 4)` and `unstageIngredient(Stick, 2)` | nothing staged; the session is **still open**; the list is empty |
| `closeCraftingSession(BagClosed)` after staging 3 sticks | all 10 sticks available; `isSessionOpen()` false; `Crafting.SessionClosed` once, with `Stick × 3` and the reason `BagClosed` |
| `closeCraftingSession` with nothing open | nothing happens, no message, no log line |
| Stage a `Spear` by item id while holding one and with two more in the bag | the most worn free copy is staged (R01 does this); naming the held one is refused `ItemWielded` |
| Stage the `StarterAxe` | refused `Inventory.FailReason.Permanent` |
| Bagless player, `stageIngredient` | `Crafting.FailReason.BagRequired` |
| Carrying a heavy item, `stageIngredient` | `Interaction.FailReason.HandsFull` |
| `enterStation(table)` | accepted; `getSessionState().hasStation` true with the table's tags; the table's `isInUse()` true; **no** session yet |
| Then `stageIngredient(Stick, 1)` | the session opens; `SessionOpened` carries the table's id; the reservation in the bag is keyed by that id |
| A second player `enterStation(table)` | refused `Interaction.FailReason.InUse`; the first player keeps it |
| First player `closeCraftingSession(BagClosed)` | the table's `isInUse()` false; the second player can now enter |
| Any command on a client | `Inventory.FailReason.NotServer`; nothing changes |

### Handoff
- `UCraftingComponent::findOn(character)` and `findFor(playerState)` are how every other framework reaches a session.
- **B19 / R07** owners: add `UCraftingComponent` to the real Character class.
- **B18**: the station interaction calls `enterStation` on the server; the client then opens the bag screen (Section 8 has a stand-in request until your `RequestInteract` exists).

---

# SECTION 6 — Requesting and Completing a Craft

**Goal:** pressing a green button makes exactly one recipe's worth: the server checks the recipe is green *now*, waits the crafting time, consumes the reserved ingredients and grants the output. Anything else is refused with a reason and changes nothing.
**Spec:** R02 3.5, 3.6, 3.7, 3.9, 3.10, §4.1 (`RequestCraft`), §5, §7; R01 3.6, 3.7; Doc02 §19.5, §19.9; Doc B22 §3.7 ("Craft leftovers", "Extra item refusal", "Locked blueprint", "Station recipe").
**Depends on:** Section 5.

### The rules

1. **The server decides** (R02 3.7, S12-D7). The colours on the screen are presentation. `requestCraft` evaluates the recipe again on the server, with the server's own staged quantities and station, and succeeds only for a recipe that is green at that moment. A client that claims a white or red button was green is refused.
2. **The refusal reasons** (R02 3.7, §5):

| What the server finds | Reason tag |
| --- | --- |
| The blueprint is locked | `Crafting.FailReason.BlueprintLocked` |
| The recipe needs a station and the player is not at a matching one | `Crafting.FailReason.StationRequired` (it also wins when an ingredient is missing as well, K14) |
| An ingredient quantity is missing, or no session is open | `Crafting.FailReason.InsufficientStaged` |
| Nothing is missing, but an item type the recipe does not use is staged | `Crafting.FailReason.ExtraIngredients` |
| A craft is already running | `Crafting.FailReason.CraftInProgress` (K13) |
| No recipe has that id | `Inventory.FailReason.NotFound` |
| Bagless / carrying / Downed, Restrained or in a scene | `BagRequired` / `HandsFull` / `NotAllowedInState` |

3. **Crafting is a small animation and nothing more** (R02 3.7, designer-confirmed). The server waits `craftSeconds` (K2), then makes the item. The ingredients are consumed **at completion**, not at the press.
4. **One press, one recipe's worth** (R02 3.5). No confirmation step. Surplus stays staged. Worked example: with 3 sticks and 5 rocks staged, one press of `Spear` (2 sticks, 1 rock) leaves 1 stick and 4 rocks staged, and the button turns red.
5. **The output is granted with `grantOrDrop`** (R02 3.6, Doc02 §19.5). If the bag is at its limit for that type the item drops next to the player as a loose item. It is never destroyed.
6. **A weapon can be an ingredient** (R02 3.10). It is reserved by its instance id; R01 picks the most worn free copy and refuses the one in the hand. **The upgraded weapon is made at full condition**: it does not keep the wear of the old one, because `grantOrDrop` creates a new instance.
7. **Order inside one server frame** (Doc02 §19.9, Index A95 UD31): damage, then Downed or death, then completion. A craft completes only if the player is neither Downed nor dead after the damage of that frame. A hit that does not Down or kill the player changes nothing (Index A115, S12-P3).

> **How rule 7 is met without extra machinery.** Inside one engine frame, incoming network requests are handled first, then the actors tick, and timers fire after that. A hit this frame has therefore been applied before the craft timer fires, and `completeCraft` asks "is the player incapacitated?" as its first check. If Doc B02 has already closed the session because the player went Downed, the timer was cleared and never fires at all.

### CR6.1 to CR6.3 — `requestCraft`, `completeCraft`, `cancelCraft`

Header additions (`UCraftingComponent.h`, public, then protected):

```cpp
    // R02 §4.1 RequestCraft(RecipeID). Server. Starts one craft. Succeeds only for a recipe that is green at this moment.
    FS_InventoryResult requestCraft(FName recipeId);

protected:
    // Crafting.ItemCrafted: "server, then local to the crafting player" (R02 §7).
    UFUNCTION(Client, Reliable)
    void clientItemCrafted(FName recipeId, FName outputItemId, int32 outputQuantity);

    // Server. The crafting time has passed: consume one recipe's worth and grant the output.
    void completeCraft();
    // Server. Stops a running craft. Nothing is consumed (R02 3.7).
    void cancelCraft();
    void raiseItemCrafted(FName recipeId, FName outputItemId, int32 outputQuantity) const;

    // The one timer of this framework. It exists only while a craft runs (Doc02 §5).
    FTimerHandle craftTimer;
```

Implementation (append to `UCraftingComponent.cpp`; add the includes at the top):

```cpp
#include "CraftingFramework/UCraftingRecipeSubsystem.h"
#include "CraftingFramework/UDA_CraftingRecipe.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "Engine/World.h"
#include "TimerManager.h"
```

```cpp
// ---------- Section 6: crafting ----------

FS_InventoryResult UCraftingComponent::requestCraft(FName recipeId){
    if(!isServerSide()){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    const FGameplayTag blocked = checkPlayerCanCraft();
    if(blocked.IsValid()){
        return refusal(blocked);
    }
    if(isCraftInProgress()){
        return refusal(T_Crafting_FailReason_CraftInProgress);   // a press while a craft is running is ignored (R02 3.7, K13)
    }
    const UCraftingRecipeSubsystem* recipes = UCraftingRecipeSubsystem::get(this);
    const UCraftingServices* services = UCraftingServices::get(this);
    const UDA_CraftingRecipe* recipe = recipes != nullptr ? recipes->getRecipe(recipeId) : nullptr;
    if(recipe == nullptr || services == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(!services->isUnlocked(recipe->requiredBlueprintTag)){
        return refusal(T_Crafting_FailReason_BlueprintLocked);
    }
    if(!sessionState.isOpen){
        return refusal(T_Crafting_FailReason_InsufficientStaged);   // nothing is staged at all
    }
    // The same rule the screen used, with the SERVER's data (K3). Only green passes.
    const FS_AvailableRecipeOption option = UBFL_Crafting::evaluateRecipe(*recipe, getStagedQuantities(), sessionState.stationTags);
    if(option.isStationMissing){
        return refusal(T_Crafting_FailReason_StationRequired);
    }
    if(option.state == E_RecipeListState::Missing){
        return refusal(T_Crafting_FailReason_InsufficientStaged);
    }
    if(option.state == E_RecipeListState::ExtraItems){
        return refusal(T_Crafting_FailReason_ExtraIngredients);
    }

    // Everything checked. The craft is running from here; nothing is consumed until it completes.
    sessionState.activeRecipeId = recipeId;
    sessionState.activeCraftSeconds = recipe->craftSeconds;
    pushState();
    services->raiseCosmeticEvent(T_Cosmetic_Event_Craft_Started, GetOwner()->GetActorLocation(), GetOwner());   // other players see the animation (Doc B21 A.4)
    CRAFTING_LOG(Log, this, "%s started crafting %s (%.2f s)", *GetNameSafe(GetOwner()), *recipeId.ToString(), recipe->craftSeconds);

    if(recipe->craftSeconds <= 0.0f){
        completeCraft();
    }else{
        GetWorld()->GetTimerManager().SetTimer(craftTimer, this, &UCraftingComponent::completeCraft, recipe->craftSeconds, false);
    }
    FS_InventoryResult result;
    result.quantity = 1;
    return result;
}

void UCraftingComponent::cancelCraft(){
    if(!isCraftInProgress()){
        return;
    }
    GetWorld()->GetTimerManager().ClearTimer(craftTimer);
    CRAFTING_LOG(Log, this, "%s's craft of %s was cancelled; nothing was consumed", *GetNameSafe(GetOwner()), *sessionState.activeRecipeId.ToString());
    sessionState.activeRecipeId = NAME_None;
    sessionState.activeCraftSeconds = 0.0f;
    pushState();
}

void UCraftingComponent::completeCraft(){
    const UCraftingRecipeSubsystem* recipes = UCraftingRecipeSubsystem::get(this);
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const UCraftingServices* services = UCraftingServices::get(this);
    const UDA_CraftingRecipe* recipe = recipes != nullptr ? recipes->getRecipe(sessionState.activeRecipeId) : nullptr;
    APlayerState* player = getCrafterPlayerState();
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;

    // Damage, then Downed or death, then completion (Doc02 19.9): a player who is Downed or dead by now makes nothing.
    // The same check covers a bag that is gone and a carry that began.
    if(recipe == nullptr || catalog == nullptr || services == nullptr || bag == nullptr || checkPlayerCanCraft().IsValid()){
        cancelCraft();
        return;
    }
    // Still green? A staged stack may have spoiled while the animation played (Section 7).
    const FS_AvailableRecipeOption option = UBFL_Crafting::evaluateRecipe(*recipe, getStagedQuantities(), sessionState.stationTags);
    if(option.state != E_RecipeListState::Craftable){
        cancelCraft();
        return;
    }

    // Work out exactly what will be consumed BEFORE consuming anything (Doc02 §6).
    TArray<FS_ItemRef> refs;
    TArray<int32> amounts;
    for(const FS_CraftingRecipeRequirement& requirement : recipe->requirementList){
        const UDA_ItemDefinition* definition = catalog->getDefinition(requirement.itemId);
        if(definition != nullptr && definition->isInstanced()){
            // A weapon is reserved by its instance id (R02 3.10): take the staged copies one by one (K15).
            int32 found = 0;
            for(const FS_StagedReservation& reservation : bag->getReservations()){
                if(found < requirement.quantity && reservation.instanceId.IsValid() && reservation.itemId == requirement.itemId && reservation.owningStationId == sessionState.stationId){
                    FS_ItemRef ref;
                    ref.itemId = requirement.itemId;
                    ref.instanceId = reservation.instanceId;
                    refs.Add(ref);
                    amounts.Add(1);
                    found++;
                }
            }
            if(found < requirement.quantity){
                cancelCraft();
                return;
            }
        }else{
            FS_ItemRef ref;
            ref.itemId = requirement.itemId;
            refs.Add(ref);
            amounts.Add(requirement.quantity);
        }
    }

    // Everything checked. Consume one recipe's worth of the RESERVED ingredients; the surplus stays staged (R02 3.5).
    for(int32 index = 0; index < refs.Num(); index++){
        const FS_InventoryResult consumed = UBFL_Inventory::consumeStaged(player, sessionState.stationId, refs[index], amounts[index]);
        if(!consumed.succeeded()){
            CRAFTING_LOG(Error, this, "Craft %s: consuming %s failed (%s) after the checks passed", *recipe->recipeId.ToString(), *refs[index].itemId.ToString(), *consumed.failReason.ToString());
        }
    }
    // The output: into the bag, or on the ground next to the player when its type is at the limit. Never destroyed
    // (R02 3.6, Doc02 19.5). An instanced output is a NEW instance at full condition (R02 3.10).
    FS_ItemStack output;
    output.itemId = recipe->outputItemId;
    output.quantity = recipe->outputQuantity;
    const FS_GrantResult granted = UBFL_Inventory::grantOrDrop(player, output);

    const FName craftedRecipeId = recipe->recipeId;
    sessionState.activeRecipeId = NAME_None;
    sessionState.activeCraftSeconds = 0.0f;
    pushState();

    raiseItemCrafted(craftedRecipeId, output.itemId, output.quantity);              // on the server
    clientItemCrafted(craftedRecipeId, output.itemId, output.quantity);             // then local to the crafting player
    services->raiseCosmeticEvent(T_Cosmetic_Event_Craft_Finished, GetOwner()->GetActorLocation(), GetOwner());
    CRAFTING_LOG(Log, this, "%s crafted %s: %s x%d (stored %d, dropped %d, carried %d)", *GetNameSafe(GetOwner()), *craftedRecipeId.ToString(), *output.itemId.ToString(), output.quantity, granted.quantityStored, granted.quantityDropped, granted.quantityCarried);
}

void UCraftingComponent::raiseItemCrafted(FName recipeId, FName outputItemId, int32 outputQuantity) const{
    FS_CraftingItemCraftedMessage message;
    message.crafter = GetOwner();
    message.recipeId = recipeId;
    message.outputItemId = outputItemId;
    message.outputQuantity = outputQuantity;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Crafting_ItemCrafted, message);
}

void UCraftingComponent::clientItemCrafted_Implementation(FName recipeId, FName outputItemId, int32 outputQuantity){
    if(!isServerSide()){
        raiseItemCrafted(recipeId, outputItemId, outputQuantity);   // the host has already raised it for its own player
    }
}
```

And one line in `closeCraftingSession` (Section 5), directly after the server check, so a session that ends takes its running craft with it and nothing is consumed (R02 3.7):

```cpp
    cancelCraft();
```

### CR6.4 — The animation (reference for the R07 and B17 owners)

Crafting plays nothing itself; it says when.

| Who | Learns it from | Does |
| --- | --- | --- |
| The crafting player's own machine | `onCraftingChanged`, then `getSessionState().activeRecipeId` is not `None` | plays the montage of that recipe's `animationTag`, for `activeCraftSeconds`; the bag screen may show the progress |
| Every other machine in range | `Cosmetic.Event.Craft.Started` / `.Finished` (Doc B21) | plays a craft animation on that character. The cosmetic event has no field for the recipe, so others see one generic craft animation (Appendix C) |
| The bag screen | `Crafting.ItemCrafted` and R01's `Inventory.Changed` | the result appears in its own fixed slot in the bag; the crafting area never shows the result (R02 3.6) |

### Verify (Section 6)

Use recipes with `craftSeconds = 0` first, so every row is exact; then repeat the first row with the real times. Bag: 10 sticks, 10 rocks.

| Test | Expect |
| --- | --- |
| Stage 3 sticks and 5 rocks, `requestCraft(Spear)` | accepted; the bag holds 8 sticks, 9 rocks and **1 spear**; staged: **1 stick, 4 rocks**; `Crafting.ItemCrafted` once; the cosmetic stub holds `Craft.Started` then `Craft.Finished` |
| `requestCraft(Spear)` again | refused `InsufficientStaged` (1 stick short); nothing changes |
| Stage 4 sticks and 5 rocks, `requestCraft(Spear)` twice | two spears; 3 rocks stay staged (worked example 1) |
| Stage 3 sticks and 4 rocks, `requestCraft(Arrow)` | 1 stick and 2 rocks remain staged; in the list `Hatchling` is still green and `Arrow` is red (worked example 2) |
| Stage stick, rock and a spear, `requestCraft(Hatchling)` | refused `ExtraIngredients`, even though nothing is missing |
| `requestCraft(StoneSpear)` with its blueprint locked, fully staged, at the table | refused `BlueprintLocked` |
| Unlock it in the stub; fully staged but **not** at a table | refused `StationRequired` |
| At the table (`enterStation`), fully staged | accepted: the old spear is gone, a `StoneSpear` at **full durability** is in the bag |
| Two spears in the bag (durability 40 and 12), stage `Spear` by item id, craft the upgrade | the **12** one was consumed; the 40 one is untouched |
| `requestCraft(NoSuchRecipe)` | `Inventory.FailReason.NotFound` |
| `requestCraft` with nothing staged | `InsufficientStaged` |
| A recipe with `craftSeconds = 2`: `requestCraft`, then at once `requestCraft`, `stageIngredient`, `unstageIngredient` | the first is accepted, `isCraftInProgress()` true; the other three are refused `CraftInProgress`; two seconds later the item is made and the ingredients are consumed **then**, not before |
| The same, and `closeCraftingSession(BagClosed)` after one second | no item; **every** staged unit is available again; nothing was consumed |
| The same, and mark the player Downed (R01's stub flag) after one second | at two seconds the craft is cancelled; nothing consumed; the items are still staged until the session is closed |
| Spears at the bag's per-type limit, craft one more | the new spear lies on the ground next to the player; the log says "dropped 1"; R01 raises `Inventory.GrantDropped` |
| Any `requestCraft` on a client | `Inventory.FailReason.NotServer` |

### Handoff
- **R07** plays the recipe's animation for the owner and a craft animation for others (CR6.4).
- **B21 / R09 / R15**: `Cosmetic.Event.Craft.Started` and `.Finished` are raised at the crafter's location.
- **R05**: a weapon upgrade is an ordinary recipe; nothing in Equipment is involved, because a weapon is not worn (R05 3.4).

---

# SECTION 7 — When a Session Ends

**Goal:** every way a session can end leads to the one function `closeCraftingSession`, and whatever was staged is available again.
**Spec:** R02 3.8 (the whole list), 3.7 (the Downed frame), amendments A89, A102, A103 (S4-F24, S4-F26, S4-F28), A115 (S12-P3, S12-P5); R01 3.2a, 3.6, 3.8; Doc B18 3.7; Doc B20 3.6.
**Depends on:** Section 6.

### CR7.1 — The list of R02 3.8, row by row

| What happens | The session | Who triggers it | Reason |
| --- | --- | --- | --- |
| The player closes the bag | ends; leftovers return automatically (designer-confirmed) | the bag screen, through `serverCloseBag` (Section 8, K7) | `BagClosed` |
| **Taking a hit** | **nothing**: the session, every reservation and the open bag all stay (Index A115, S12-P3) | nobody | |
| The player is Downed | ends; the bag closes | Doc B02, when it enters Downed | `Downed` |
| The player dies | ends | Doc B02. R01's `detachBag` has already released the reservations (S4-F28); safety net: the component's `EndPlay` | `Died` / `PawnRemoved` |
| The player disconnects or leaves | ends | Doc B20 on `Session.PlayerLeft`. R01's `handlePlayerLeft` releases the reservations as well; safety net: `EndPlay` | `PlayerLeft` / `PawnRemoved` |
| A scene starts and locks the input (Doc R17) | ends | Doc R17 | `SceneStarted` |
| The station is destroyed or collapses | ends with a short notice | built in: the station component's `EndPlay` | `StationLost` |
| Something moves the player out of the station's range | ends | Doc B18's lock release (3.7) calls `forceRelease` | `OutOfRange` |
| A hand-craft session | has no range; it ends on the other rows (Index A103, S4-F26) | | |
| A staged stack spoils | the session **stays**; a running craft that used it is cancelled | built in: R01's `onStagedItemLost` | |
| A save by any player | nothing: reservations are never saved and the staged items are still in the bag (Index A102, ST-11) | nobody | |

Items are safe in every row, with or without the caller: a reservation is only a number inside the bag, and R01 clears it on death and on leaving by itself. What the callers add is that the *session* ends cleanly: the station is unlocked, a running craft is cancelled and the screen is told.

### CR7.2 — The two built-in triggers and the safety net

**The crafting component.** Header additions (`UCraftingComponent.h`; `EndPlay` goes with the other overrides):

```cpp
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    // R01 3.2a: a staged stack reached its spoil time and its reservation is gone. Bound while a session is open.
    void onStagedItemLost(const FGuid& stationId, FName itemId);
    void bindStagedItemLost();
    void unbindStagedItemLost();

    FDelegateHandle stagedItemLostHandle;
    TWeakObjectPtr<UInventoryData> boundBag;
```

(add `class UInventoryData;` to the forward declarations of the header.)

Implementation:

```cpp
// ---------- Section 7: ending a session ----------

void UCraftingComponent::EndPlay(const EEndPlayReason::Type EndPlayReason){
    if(isServerSide()){
        // The safety net (K12): the Character is going away and nobody closed the session first.
        // The reservations are released if the bag is still there, and the station is unlocked for the next player.
        closeCraftingSession(E_CraftingCloseReason::PawnRemoved);
    }
    Super::EndPlay(EndPlayReason);
}

void UCraftingComponent::bindStagedItemLost(){
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getCrafterPlayerState());
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    if(bag != nullptr && !stagedItemLostHandle.IsValid()){
        stagedItemLostHandle = bag->onStagedItemLost.AddUObject(this, &UCraftingComponent::onStagedItemLost);
        boundBag = bag;
    }
}

void UCraftingComponent::unbindStagedItemLost(){
    if(boundBag.IsValid() && stagedItemLostHandle.IsValid()){
        boundBag->onStagedItemLost.Remove(stagedItemLostHandle);
    }
    stagedItemLostHandle.Reset();
    boundBag = nullptr;
}

void UCraftingComponent::onStagedItemLost(const FGuid& stationId, FName itemId){
    // The fresh stack became its spoiled item, and R01 dropped the reservation (Index A106, S3-F04). The crafting area
    // simply holds less now. A craft that was counting on that item must not complete.
    const UCraftingRecipeSubsystem* recipes = UCraftingRecipeSubsystem::get(this);
    const UDA_CraftingRecipe* recipe = recipes != nullptr ? recipes->getRecipe(sessionState.activeRecipeId) : nullptr;
    if(recipe != nullptr && recipe->usesItem(itemId)){
        cancelCraft();
    }
    CRAFTING_LOG(Log, this, "%s: staged %s spoiled and left the crafting area", *GetNameSafe(GetOwner()), *itemId.ToString());
}
```

Two one-line edits to code from Section 5:

- In `openCraftingSession`, directly after `sessionState.isOpen = true;` add:

```cpp
    bindStagedItemLost();
```

- In `closeCraftingSession`, directly after the `cancelCraft();` line of Section 6 add:

```cpp
    unbindStagedItemLost();
```

**The station component.** Header additions (`UCraftingStationComponent.h`; `EndPlay` with the other overrides, `forceRelease` with the commands):

```cpp
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // Server. Ends the session of whoever uses the station. Doc B18 calls it with OutOfRange when its lock is released;
    // the component calls it itself with StationLost when the piece goes away.
    void forceRelease(E_CraftingCloseReason reason);
```

Implementation (append to `UCraftingStationComponent.cpp`):

```cpp
void UCraftingStationComponent::forceRelease(E_CraftingCloseReason reason){
    UCraftingComponent* user = currentUser.Get();
    if(user != nullptr){
        user->closeCraftingSession(reason);   // returns the staged items and releases this station
    }
}

void UCraftingStationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason){
    if(GetOwner() != nullptr && GetOwner()->HasAuthority()){
        // Destroyed or collapsed (Doc B01 3.6): the screen closes with a short notice and the staged items return
        // (R02 3.8, Index A103 S4-F24).
        forceRelease(E_CraftingCloseReason::StationLost);
    }
    Super::EndPlay(EndPlayReason);
}
```

### CR7.3 — Library functions for the owners of each state

Additions to `UBFL_Crafting.h` (add `class AActor;` and `class APlayerState;` to the forward declarations):

```cpp
    // ---- for other frameworks ----
    // Query. Works on the server and on the crafter's own machine.
    UFUNCTION(BlueprintPure, Category = "Awaken|Crafting") static bool isSessionOpen(const AActor* crafter);
    // R02 3.8. Server. Ends the crafter's session for that reason. Safe to call when none is open.
    UFUNCTION(BlueprintCallable, Category = "Awaken|Crafting") static void closeSession(AActor* crafter, E_CraftingCloseReason reason);
    // Cleanup for Session.PlayerLeft (Doc B20 3.6): the leaving player's session ends and its materials are returned.
    static void handlePlayerLeft(APlayerState* player);
```

Additions to `UBFL_Crafting.cpp` (add `#include "CraftingFramework/UCraftingComponent.h"` and `#include "GameFramework/PlayerState.h"`):

```cpp
bool UBFL_Crafting::isSessionOpen(const AActor* crafter){
    const UCraftingComponent* crafting = UCraftingComponent::findOn(crafter);
    return crafting != nullptr && crafting->isSessionOpen();
}

void UBFL_Crafting::closeSession(AActor* crafter, E_CraftingCloseReason reason){
    UCraftingComponent* crafting = UCraftingComponent::findOn(crafter);
    if(crafting != nullptr){
        crafting->closeCraftingSession(reason);
    }
}

void UBFL_Crafting::handlePlayerLeft(APlayerState* player){
    UCraftingComponent* crafting = UCraftingComponent::findFor(player);
    if(crafting != nullptr){
        crafting->closeCraftingSession(E_CraftingCloseReason::PlayerLeft);
    }
}
```

The one line each owner adds, on the server:

| Owner | Where | Line |
| --- | --- | --- |
| Doc B02 | entering Downed | `UBFL_Crafting::closeSession(character, E_CraftingCloseReason::Downed);` |
| Doc B02 | a death, **before** `detachBag`, so the message can still name what was staged | `UBFL_Crafting::closeSession(character, E_CraftingCloseReason::Died);` |
| Doc R17 | a scene locks the player's input | `UBFL_Crafting::closeSession(character, E_CraftingCloseReason::SceneStarted);` |
| Doc B20 | `Session.PlayerLeft`, **before** R01's `handlePlayerLeft` | `UBFL_Crafting::handlePlayerLeft(player);` |
| Doc B18 | the station's Exclusive lock is released because the player is out of range | `station->forceRelease(E_CraftingCloseReason::OutOfRange);` |

> **Order with R01 on a death** (Index A103, S4-F28): the reservations are released first and the bag detaches after. If B02 calls `closeSession` first, Crafting releases them; if it does not, R01's `detachBag` does. Either way a bag record never holds a reserved quantity.

### Verify (Section 7)

| Test | Expect |
| --- | --- |
| Stage 3 sticks; apply damage to the player (R01's health stub, or any test damage) | **nothing** changes: the session is open, 3 sticks are still staged |
| Then `UBFL_Crafting::closeSession(character, Downed)` | all sticks available; `Crafting.SessionClosed` with `Stick × 3` and the reason `Downed` |
| A 2-second craft is running; `closeSession(character, Downed)` after one second | no item is made; nothing was consumed; everything is available |
| At a station with 2 rocks staged, destroy the station actor | the session ends with the reason `StationLost`; both rocks are available; no crash |
| At a station, destroy the **player's** pawn | the station's `isInUse()` is false again; the reservations are released; reason `PawnRemoved` |
| At a station, `station->forceRelease(OutOfRange)` | the session ends with `OutOfRange`; the station is free |
| `UBFL_Crafting::handlePlayerLeft(player)` | the session ends with `PlayerLeft` |
| R01's `detachBag` while a session is open (a death without B02's call) | the reservations are released by R01; the pawn's removal then closes the session state; nothing is duplicated or lost |
| A spoilable ingredient is staged and its spoil time passes (move R01's stub clock, then touch the bag) | `getStagedQuantity` of the fresh item is 0; the session is still open; the recipe that used it is red; a craft that was running on it was cancelled |
| A save while 3 sticks are staged (R01's Character snapshot) | the snapshot holds 10 sticks and **no** reservation; after a restore nothing is staged |
| `closeSession` on a character with nothing open | nothing happens |
| Stop Play In Editor while a session is open | no errors |

### Handoff
- **B02, R17, B20, B18**: the one line each from CR7.3. Until you add yours, a Downed player's session stays open on the server (their items are safe; the station stays locked until their pawn is removed).
- **B17**: on `Crafting.SessionClosed` with any reason other than `BagClosed`, close the bag screen; for `StationLost` show the short notice (a text key of Doc R10).

---

# SECTION 8 — Networking: Requests, Replication and Messages

**Goal:** a client can only *ask*; the host validates, changes the state, and the right machines see the result.
**Spec:** R02 §7, §8, §9; Doc02 §6, §8, §19.2, §19.6; Doc B17 3.3, 3.6; Doc B18 3.5.
**Depends on:** Section 7, and the R01 guide's Section 12 (`UInventoryRequestComponent`).

### CR8.1 — Replication audit (R02 §8, Doc02 §19.1)

Walk this table with a listen server and two clients. Every row must be true:

| State | Replicates to | Mechanism | Late joiner |
| --- | --- | --- | --- |
| The session state (open, station id and tags, the running recipe) | **the crafting player only** | `COND_OwnerOnly` on the Character component | not needed: a session is private |
| What is staged | the crafting player only | R01's bag mirror: each entry's `reservedQuantity` | not needed |
| A station's in-use flag and id | **everyone** | two replicated properties on the station component | reads them from the station |
| The craft button list and its colours | nobody | worked out locally from the two rows above (K3) | n/a |
| Recipes | nobody | static data on every machine | n/a |
| The unlocked set | everyone | Doc R06's carrier on the GameState | Doc R06 |
| The craft animation other players see | players in range | the cosmetic channel (Doc B21), unreliable | a late joiner does not need a past animation |

Carrier: none. A crafting session is per player and holds no shared world state (Doc02 §19.1).

### CR8.2 — The requests

A client never mutates anything. It sends **intent**; the server checks, runs the command, and replies to the requester only when it refuses (Doc02 §6, §19.6). The bag screen may update its own picture at once for responsiveness (R02 §8), but the reservation is always the server's, and the replicated bag corrects the picture.

Which checks apply (Doc02 §19.6 asks each framework to say):

| Request | Rate | State (not Downed, Restrained, Arriving, in a scene) | Free hands | The bag | Range |
| --- | --- | --- | --- | --- | --- |
| `serverUseStation` | yes | yes | yes | yes | `stationUseRange` from the pawn's eye, inclusive |
| `serverStageIngredient` | yes | yes | yes | yes | |
| `serverRequestCraft` | yes | yes | yes | yes | the station is the one entered; nothing is sent |
| `serverUnstageIngredient` | yes | | | | |
| `serverCloseBag` | | | | | |

Giving something back and closing are always allowed, like R01's `serverCloseContainer`.

Header additions (`UCraftingComponent.h`, public, then protected):

```cpp
    // ---- requests: the bag screen calls these on the owning client. On the host they run directly ----
    // TEMPORARY entry point (owner: Doc B18): the player pressed interact on a crafting station.
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Crafting") void serverUseStation(AActor* stationActor);
    // One drag or Alt plus click: wholeStack false = one unit. With Shift: wholeStack true (R02 3.3). The server counts (K10).
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Crafting") void serverStageIngredient(const FS_ItemRef& ref, bool wholeStack);
    // A click on a staged item returns one unit; with Shift the whole staged stack (R02 3.3, S12-P4).
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Crafting") void serverUnstageIngredient(const FS_ItemRef& ref, bool wholeStack);
    // A press on a craft button. Only a recipe that is green on the SERVER is made (R02 3.7).
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Crafting") void serverRequestCraft(FName recipeId);
    // The bag screen closed (K7). Ends the session and leaves the station.
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Crafting") void serverCloseBag();

protected:
    // Through R01's gate, so one rate limit covers bag and crafting. False = refused and the requester has been told.
    bool passRequestGate(bool needsHands, bool allowWhenIncapacitated, const TCHAR* operation) const;
    void replyIfRefused(const FGameplayTag& failReason, const TCHAR* operation) const;
```

Implementation (add the includes at the top):

```cpp
#include "CraftingFramework/UCraftingSettings.h"
#include "ItemInventoryFramework/UInventoryRequestComponent.h"
```

```cpp
// ---------- Section 8: requests ----------

bool UCraftingComponent::passRequestGate(bool needsHands, bool allowWhenIncapacitated, const TCHAR* operation) const{
    UInventoryRequestComponent* requests = UInventoryRequestComponent::findFor(getCrafterPlayerState());
    if(requests == nullptr){
        CRAFTING_LOG(Warning, this, "%s refused: the player has no request component", operation);
        return false;
    }
    const FGameplayTag blocked = requests->gate(needsHands, allowWhenIncapacitated);
    if(blocked.IsValid()){
        requests->refuse(blocked, operation);
        return false;
    }
    return true;
}

void UCraftingComponent::replyIfRefused(const FGameplayTag& failReason, const TCHAR* operation) const{
    if(!failReason.IsValid()){
        return;
    }
    UInventoryRequestComponent* requests = UInventoryRequestComponent::findFor(getCrafterPlayerState());
    if(requests != nullptr){
        requests->refuse(failReason, operation);   // logs the reason tag and tells only the requester (Doc02 §6)
    }
}

void UCraftingComponent::serverUseStation_Implementation(AActor* stationActor){
    if(!passRequestGate(true, false, TEXT("useStation"))){
        return;
    }
    UCraftingStationComponent* targetStation = UCraftingStationComponent::findOn(stationActor);
    const APawn* pawn = Cast<APawn>(GetOwner());
    if(targetStation == nullptr || pawn == nullptr){
        replyIfRefused(T_Inventory_FailReason_NotFound, TEXT("useStation"));
        return;
    }
    // Range from the pawn's eye, inclusive: exactly at the limit is allowed (Doc02 §19.6, §19.9).
    if(FVector::Dist(pawn->GetPawnViewLocation(), stationActor->GetActorLocation()) > UCraftingSettings::get()->stationUseRange){
        replyIfRefused(T_Inventory_FailReason_OutOfRange, TEXT("useStation"));
        return;
    }
    replyIfRefused(enterStation(targetStation).failReason, TEXT("useStation"));
}

void UCraftingComponent::serverStageIngredient_Implementation(const FS_ItemRef& ref, bool wholeStack){
    if(!passRequestGate(true, false, TEXT("stageIngredient"))){
        return;
    }
    // The server counts "the whole stack" itself (K10). A weapon is always one copy per request (K16).
    int32 quantity = 1;
    if(wholeStack && !ref.isInstance()){
        const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getCrafterPlayerState());
        const UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
        quantity = bag != nullptr ? FMath::Max(1, bag->getAvailableQuantity(ref.itemId)) : 1;
    }
    replyIfRefused(stageIngredient(ref, quantity).failReason, TEXT("stageIngredient"));
}

void UCraftingComponent::serverUnstageIngredient_Implementation(const FS_ItemRef& ref, bool wholeStack){
    if(!passRequestGate(false, true, TEXT("unstageIngredient"))){
        return;
    }
    const int32 quantity = (wholeStack && !ref.isInstance()) ? FMath::Max(1, getStagedQuantity(ref.itemId)) : 1;
    replyIfRefused(unstageIngredient(ref, quantity).failReason, TEXT("unstageIngredient"));
}

void UCraftingComponent::serverRequestCraft_Implementation(FName recipeId){
    if(!passRequestGate(true, false, TEXT("requestCraft"))){
        return;
    }
    replyIfRefused(requestCraft(recipeId).failReason, TEXT("requestCraft"));
}

void UCraftingComponent::serverCloseBag_Implementation(){
    closeCraftingSession(E_CraftingCloseReason::BagClosed);
}
```

> **Why the RPCs can live on a Character component.** A client may call a server RPC on an actor its connection owns, and a possessed pawn is owned by its PlayerController. No other client can call them.

> **`serverUseStation` is a stand-in.** How a player opens a session at a station belongs to Doc B18 (R02 0.3). When its `RequestInteract` exists, the station's interaction calls `enterStation` on the server after B18's own checks (range, line of sight, the lock), and this request and its range setting are deleted.

### CR8.3 — What the bag screen does (reference for the B17 owner)

| The player | The screen | The server |
| --- | --- | --- |
| presses the inventory key | runs `CanOpenBag`, opens locally. **Nothing is sent** (S12b-D10) | nothing: no session, nothing reserved (R02 3.2) |
| presses interact on a Crafting Table | the interaction reaches `enterStation`; when `getSessionState().hasStation` turns true the screen opens, skipping only the place checks (Doc B17 3.6) | locks the station, replicates its tags to the owner |
| drags an item into the crafting area, or Alt + click (Shift = the whole stack) | `serverStageIngredient(ref, isShiftDown)` | reserves; the first one opens the session |
| clicks a staged item (Shift = the whole staged stack) | `serverUnstageIngredient(ref, isShiftDown)` | releases |
| *(any staged quantity changes)* | on R01's `Inventory.Changed` for the own bag: redraw the staging widget from `getStagedQuantity` and the buttons from `getAvailableRecipes()` | |
| presses a green button | `serverRequestCraft(recipeId)`. White and red buttons cannot be pressed | checks again; starts the craft |
| closes the bag | `serverCloseBag()`, always, also when nothing was staged | ends the session, frees the station |

For the fresh and spoiled stacks that share one bag slot (Doc R01 3.2a, S12b-D4): they are two item ids, so the `ref` of the stack the player dragged is the one that is staged.

### CR8.4 — Message delivery (R02 §7, Doc02 §19.2)

| Message | Payload | Delivery | How it is met |
| --- | --- | --- | --- |
| `Crafting.SessionOpened` | `FS_CraftingSessionOpenedMessage` | **local to the crafting player** | the owning client raises it from `OnRep_sessionState`; the host raises it directly for its own player |
| `Crafting.ItemCrafted` | `FS_CraftingItemCraftedMessage` | **server, then local to the crafting player** | the host raises it in `completeCraft`; `clientItemCrafted` raises it on the owner's machine |
| `Crafting.SessionClosed` | `FS_CraftingSessionClosedMessage` | **local to the crafting player** | `clientSessionClosed` |
| `Cosmetic.Event.Craft.Started` / `.Finished` | the cosmetic event of Doc B21 | **needs the cosmetic channel** | the host raises them through `IBPI_CosmeticEventSink` |

A refusal is not a message: it arrives on `onRequestRefused` of the requester's own R01 request component, with the reason tag the UI turns into text.

### CR8.5 — Leaving, and what is never saved

- **Leaving** (Doc B20 3.6): `UBFL_Crafting::handlePlayerLeft` (CR7.3) ends the session; the pawn's removal does the same if B20 never calls it.
- **Save** (R02 §9): nothing. `UCraftingComponent` is not a save participant, reservations are never saved (R01 3.6), and recipes are data. A save made while a bag is open with items staged writes those items as ordinary bag contents (Index A102, ST-11).
- **Host handover** (Doc B20 3.10): a normal save capture, so every session simply ends with the old host.

### Verify (Section 8)

Listen server + one client (*Play As Listen Server*, 2 players), `AInventoryTestGameMode`. Give the client 10 sticks and 10 rocks from the host (`Awaken.Item.Give Stick 10 p1`).

| Check | Expect |
| --- | --- |
| Client calls `serverStageIngredient(Stick, false)` | on the client: `isSessionOpen()` true, `getStagedQuantity(Stick)` 1, `Crafting.SessionOpened` raised once. On the **host**: the same state for player 1. A second client sees **nothing** about it |
| Client calls `serverStageIngredient(Rock, true)` | all 10 rocks staged; the client never sent a number |
| Client reads `getAvailableRecipes()` | the same list the host gets for that player |
| Client calls `serverRequestCraft(Spear)` with 2 sticks and 1 rock staged | after `craftSeconds` the client's bag shows the spear; `Crafting.ItemCrafted` is raised on the client **and** on the host; nobody else |
| Client calls `serverRequestCraft(Arrow)` while `Arrow` is red on the server | refused `InsufficientStaged`; only the requester is told; no state differs on any machine |
| Client stages a spear as well and requests `Hatchling` | refused `ExtraIngredients` |
| Client calls `serverCloseBag()` | everything is available again on the client; `Crafting.SessionClosed` raised on the client only, with what was returned |
| Client walks to a stub station and calls `serverUseStation(station)` | `hasStation` true on the client; the station shows `isInUse()` on **every** machine |
| The host's player calls `serverUseStation` on the same station | refused `Interaction.FailReason.InUse` |
| Client calls `serverUseStation` from 10 metres away | refused `OutOfRange` |
| Client carrying a heavy item, or marked Downed, sends a stage request | refused `HandsFull` / `NotAllowedInState` |
| Client sends 100 stage requests in one frame | after about `requestsPerSecond` the rest are `RateLimited` (the bucket is shared with the bag requests) |
| The client disconnects with items staged at the station | the station is free again on the host; nothing is lost from the held Character state |
| Client 2 calls a request on client 1's component | impossible: the RPC is dropped |
| Offline (standalone) | the same requests work; the machine is its own host |

### Handoff
- **B17** calls the five requests as in CR8.3 and maps every reason tag through `DT_ReasonText`; add text for the six `Crafting.FailReason.*` tags. `CraftInProgress` can be silent: the button is simply busy.
- **B18** replaces `serverUseStation` with its own interaction (CR8.2).

---

# SECTION 9 — Dev Tools, Tests and Final Acceptance

**Goal:** the closing "Dev Tools and Testing" section that Doc B22 §3.10 requires of every implementation guide.
**Spec:** Doc B22 §3.2 to §3.10 (the ten R02 starter tests of §3.7 are all here).
**Depends on:** Sections 0–8.

### Files

```
Private/CraftingFramework/DevTools/CraftingDevCommands.cpp     (non-shipping only)
Private/CraftingFramework/DevTools/CraftingDebugView.cpp       (non-shipping only)
Private/CraftingFramework/Tests/CraftingSpec.cpp
Content/Awaken/Tests/TestMap_Inventory.umap                    (reuse R01's map)
```

### CR9.1, CR9.2 — Dev commands (Doc B22 §3.2, §3.4)

The rules are the ones the R01 guide follows (B22 §3.2): names start `Awaken.<Domain>.<Verb>`; **host only**; they go **through the owner's public functions**, so the same checks and messages run as in play; each says what happened in `LogAwakenCrafting`; **none saves**; compiled out of Shipping. The optional last argument `pN` picks player N (default 0), as in R01's commands.

`CraftingDevCommands.cpp`

```cpp
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "CraftingFramework/CraftingLog.h"
#include "CraftingFramework/UBFL_CraftingValidation.h"
#include "CraftingFramework/UCraftingComponent.h"
#include "CraftingFramework/UCraftingStationComponent.h"
#include "CraftingFramework/03Interfaces/BPI_ProgressionQuery.h"
#include "CraftingFramework/Stubs/CraftingStubs.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"

namespace{
    bool isPlayerArgument(const FString& argument){
        return argument.Len() > 1 && argument.StartsWith(TEXT("p"), ESearchCase::CaseSensitive) && argument.Mid(1).IsNumeric();
    }

    // Returns null with a log line when not on the host or when the player does not exist.
    APlayerState* resolvePlayer(UWorld* world, const TArray<FString>& arguments){
        if(world == nullptr || !UBFL_Inventory::isServerWorld(world)){
            CRAFTING_LOG(Warning, world, "Host command: refused on a client (the Doc B22 cheat manager forwards it)");
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
            CRAFTING_LOG(Warning, world, "No player %d", playerIndex);
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

    const TCHAR* stateName(E_RecipeListState state){
        return state == E_RecipeListState::Craftable ? TEXT("GREEN") : (state == E_RecipeListState::ExtraItems ? TEXT("WHITE") : TEXT("RED"));
    }

    void logList(UWorld* world, const UCraftingComponent* crafting){
        const TArray<FS_AvailableRecipeOption> list = crafting->getAvailableRecipes();
        CRAFTING_LOG(Log, world, "%d craft button(s):", list.Num());
        for(const FS_AvailableRecipeOption& option : list){
            FString detail;
            for(const FS_CraftingRecipeRequirement& missing : option.missingIngredients){
                detail += FString::Printf(TEXT(" missing %s x%d;"), *missing.itemId.ToString(), missing.quantity);
            }
            if(option.isStationMissing){
                detail += TEXT(" needs its station;");
            }
            for(const FName& extra : option.extraItems){
                detail += FString::Printf(TEXT(" extra %s;"), *extra.ToString());
            }
            CRAFTING_LOG(Log, world, "   %-5s %s (x%d)%s", stateName(option.state), *option.recipeId.ToString(), option.maxCraftableCount, *detail);
        }
    }

    void stageCommand(const TArray<FString>& arguments, UWorld* world){
        UCraftingComponent* crafting = UCraftingComponent::findFor(resolvePlayer(world, arguments));
        const TArray<FString> plain = plainArguments(arguments);
        if(crafting == nullptr || plain.Num() < 1){
            CRAFTING_LOG(Warning, world, "Usage: Awaken.Craft.Stage <itemId> [quantity] [pN]");
            return;
        }
        FS_ItemRef ref;
        ref.itemId = FName(*plain[0]);
        const int32 quantity = plain.Num() > 1 ? FMath::Max(1, FCString::Atoi(*plain[1])) : 1;
        const FS_InventoryResult result = crafting->stageIngredient(ref, quantity);
        CRAFTING_LOG(Log, world, "Stage %s x%d: %s", *plain[0], quantity, result.succeeded() ? TEXT("done") : *result.failReason.ToString());
        logList(world, crafting);
    }

    void unstageCommand(const TArray<FString>& arguments, UWorld* world){
        UCraftingComponent* crafting = UCraftingComponent::findFor(resolvePlayer(world, arguments));
        const TArray<FString> plain = plainArguments(arguments);
        if(crafting == nullptr || plain.Num() < 1){
            CRAFTING_LOG(Warning, world, "Usage: Awaken.Craft.Unstage <itemId> [quantity] [pN]");
            return;
        }
        FS_ItemRef ref;
        ref.itemId = FName(*plain[0]);
        const int32 quantity = plain.Num() > 1 ? FMath::Max(1, FCString::Atoi(*plain[1])) : 1;
        const FS_InventoryResult result = crafting->unstageIngredient(ref, quantity);
        CRAFTING_LOG(Log, world, "Unstage %s x%d: %s", *plain[0], quantity, result.succeeded() ? TEXT("done") : *result.failReason.ToString());
        logList(world, crafting);
    }

    // The situation most likely to break (Doc B22 §3.10): a full crafting area. Everything that can be staged is staged,
    // so the list holds every recipe the bag touches: the cap, the order and the white entries all show at once.
    void stageAllCommand(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = resolvePlayer(world, arguments);
        UCraftingComponent* crafting = UCraftingComponent::findFor(player);
        const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
        if(crafting == nullptr || inventory == nullptr){
            CRAFTING_LOG(Warning, world, "Usage: Awaken.Craft.StageAll [pN]");
            return;
        }
        int32 stagedTypes = 0;
        int32 refusedTypes = 0;
        for(const FS_InventoryEntry& entry : inventory->getBagEntries()){
            if(entry.getAvailableQuantity() < 1){
                continue;
            }
            FS_ItemRef ref;
            ref.itemId = entry.itemId;
            ref.instanceId = entry.instanceId;
            if(crafting->stageIngredient(ref, entry.getAvailableQuantity()).succeeded()){
                stagedTypes++;
            }else{
                refusedTypes++;   // the Axe, the map, the weapon in the hand, a container that holds something
            }
        }
        CRAFTING_LOG(Log, world, "StageAll: %d entries staged, %d refused", stagedTypes, refusedTypes);
        logList(world, crafting);
    }

    void craftCommand(const TArray<FString>& arguments, UWorld* world){
        UCraftingComponent* crafting = UCraftingComponent::findFor(resolvePlayer(world, arguments));
        const TArray<FString> plain = plainArguments(arguments);
        if(crafting == nullptr || plain.Num() < 1){
            CRAFTING_LOG(Warning, world, "Usage: Awaken.Craft.Craft <recipeId> [pN]");
            return;
        }
        const FS_InventoryResult result = crafting->requestCraft(FName(*plain[0]));
        CRAFTING_LOG(Log, world, "Craft %s: %s", *plain[0], result.succeeded() ? TEXT("started") : *result.failReason.ToString());
    }

    void closeCommand(const TArray<FString>& arguments, UWorld* world){
        UCraftingComponent* crafting = UCraftingComponent::findFor(resolvePlayer(world, arguments));
        if(crafting == nullptr){
            CRAFTING_LOG(Warning, world, "Usage: Awaken.Craft.Close [pN]");
            return;
        }
        crafting->closeCraftingSession(E_CraftingCloseReason::BagClosed);
        CRAFTING_LOG(Log, world, "Close: the session and the station were released");
    }

    // TEMPORARY — owner: Doc B01. Spawns a stub station in front of the player and enters it.
    void spawnStationCommand(const TArray<FString>& arguments, UWorld* world){
        const APlayerState* player = resolvePlayer(world, arguments);
        UCraftingComponent* crafting = UCraftingComponent::findFor(player);
        const APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
        const TArray<FString> plain = plainArguments(arguments);
        const FGameplayTag stationTag = plain.Num() > 0 ? FGameplayTag::RequestGameplayTag(FName(*(TEXT("Station.") + plain[0])), false) : FGameplayTag();
        if(crafting == nullptr || pawn == nullptr || !stationTag.IsValid()){
            CRAFTING_LOG(Warning, world, "Usage: Awaken.Craft.SpawnStation <CraftingTable|Forge> [pN]");
            return;
        }
        AStubCraftingStation* stationActor = world->SpawnActor<AStubCraftingStation>(pawn->GetActorLocation() + pawn->GetActorForwardVector() * 150.0f, FRotator::ZeroRotator);
        if(stationActor == nullptr){
            return;
        }
        stationActor->stationComponent->stationTags.AddTag(stationTag);
        const FS_InventoryResult result = crafting->enterStation(stationActor->stationComponent);
        CRAFTING_LOG(Log, world, "SpawnStation %s: %s", *plain[0], result.succeeded() ? TEXT("spawned and entered") : *result.failReason.ToString());
    }

    // TEMPORARY — owner: Doc R06 (its real command is Awaken.Unlock.Grant, Doc B22 §3.4). Sets a tag in the LOCAL stub.
    void stubUnlockCommand(const TArray<FString>& arguments, UWorld* world){
        const UInventoryServices* services = UInventoryServices::get(world);
        UStubProgressionQuery* stub = services != nullptr ? Cast<UStubProgressionQuery>(services->getSeam(UBPI_ProgressionQuery::StaticClass())) : nullptr;
        if(stub == nullptr || arguments.Num() < 1){
            CRAFTING_LOG(Warning, world, "Usage: Awaken.Craft.StubUnlock <tag|all> [0|1]  (does nothing once Doc R06 is registered)");
            return;
        }
        const bool isOn = arguments.Num() < 2 || arguments[1] != TEXT("0");
        if(arguments[0] == TEXT("all")){
            stub->unlockEverything = isOn;
        }else{
            const FGameplayTag tag = FGameplayTag::RequestGameplayTag(FName(*arguments[0]), false);
            if(!tag.IsValid()){
                CRAFTING_LOG(Warning, world, "StubUnlock: unknown tag '%s'", *arguments[0]);
                return;
            }
            if(isOn){
                stub->unlockedTags.AddTag(tag);
            }else{
                stub->unlockedTags.RemoveTag(tag);
            }
        }
        CRAFTING_LOG(Log, world, "StubUnlock %s = %d on this machine", *arguments[0], isOn ? 1 : 0);
    }

    // Read only.
    void listCommand(const TArray<FString>& arguments, UWorld* world){
        const UCraftingComponent* crafting = UCraftingComponent::findFor(resolvePlayer(world, arguments));
        if(crafting == nullptr){
            return;
        }
        const FS_CraftingSessionState state = crafting->getSessionState();
        CRAFTING_LOG(Log, world, "Session %s, station %s, running craft %s", state.isOpen ? TEXT("OPEN") : TEXT("closed"), state.hasStation ? *state.stationTags.ToStringSimple() : TEXT("none"), state.activeRecipeId.IsNone() ? TEXT("none") : *state.activeRecipeId.ToString());
        for(const TPair<FName, int32>& staged : crafting->getStagedQuantities()){
            CRAFTING_LOG(Log, world, "   staged %s x%d", *staged.Key.ToString(), staged.Value);
        }
        logList(world, crafting);
    }

    // Read only. The editor data check of R02 §12.
    void validateCommand(const TArray<FString>& arguments, UWorld* world){
        TArray<FString> errors;
        const int32 errorCount = UBFL_CraftingValidation::validateRecipes(world, errors);
        for(const FString& error : errors){
            CRAFTING_LOG(Error, world, "%s", *error);
        }
        CRAFTING_LOG(Log, world, "Recipe validation: %d error(s)", errorCount);
    }

    FAutoConsoleCommandWithWorldAndArgs cmdStage(TEXT("Awaken.Craft.Stage"), TEXT("<itemId> [quantity] [pN]: stage an ingredient, then print the craft buttons"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&stageCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdUnstage(TEXT("Awaken.Craft.Unstage"), TEXT("<itemId> [quantity] [pN]: return staged units"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&unstageCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdStageAll(TEXT("Awaken.Craft.StageAll"), TEXT("[pN]: stage everything in the bag that can be staged (the full crafting area)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&stageAllCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdCraft(TEXT("Awaken.Craft.Craft"), TEXT("<recipeId> [pN]: press that craft button"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&craftCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdClose(TEXT("Awaken.Craft.Close"), TEXT("[pN]: close the bag: end the session and leave the station"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&closeCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdStation(TEXT("Awaken.Craft.SpawnStation"), TEXT("<CraftingTable|Forge> [pN]: TEMPORARY, spawn a stub station and enter it"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&spawnStationCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdUnlock(TEXT("Awaken.Craft.StubUnlock"), TEXT("<tag|all> [0|1]: TEMPORARY, set an unlock in the local progression stub"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&stubUnlockCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdList(TEXT("Awaken.Craft.List"), TEXT("[pN]: print the session, what is staged and the craft buttons (read)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&listCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdValidate(TEXT("Awaken.Craft.Validate"), TEXT(": check every recipe against the item catalog (read)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&validateCommand));
}

#endif
```

| Command | What it does | Where it runs |
| --- | --- | --- |
| `Awaken.Craft.Stage <itemId> [quantity] [pN]` | stages through `stageIngredient`, then prints the buttons | Host with target |
| `Awaken.Craft.Unstage <itemId> [quantity] [pN]` | returns staged units through `unstageIngredient` | Host with target |
| `Awaken.Craft.StageAll [pN]` | stages everything in the bag that can be staged. **The situation most likely to break**: the longest list, the cap, the order and the white entries at once | Host with target |
| `Awaken.Craft.Craft <recipeId> [pN]` | presses that button through `requestCraft` | Host with target |
| `Awaken.Craft.Close [pN]` | closes the bag: ends the session and leaves the station | Host with target |
| `Awaken.Craft.SpawnStation <CraftingTable\|Forge> [pN]` | TEMPORARY: spawns a stub station and enters it | Host with target |
| `Awaken.Craft.StubUnlock <tag\|all> [0\|1]` | TEMPORARY: sets an unlock in this machine's progression stub | Local |
| `Awaken.Craft.List [pN]` (read) | prints the session, the staged items and every button with its state and what is missing or extra | Host with target |
| `Awaken.Craft.Validate` (read) | runs the recipe data check of R02 §12 | Local |

Useful together with R01's commands: `Awaken.Item.Give Stick 10`, `Awaken.Bag.SetBagless`, `Awaken.Inventory.StubFlag downed 1`; and with Carry's `Awaken.Carry.Give Log 1` to see the `HandsFull` refusal.

### CR9.3 — Debug view `Awaken.Debug.Crafting` (Doc B22 §3.5)

A view draws what the system knows, **locally**, only in non-shipping builds, and never replicates. On a client it shows only the local player's session, because nobody else's replicates.

`CraftingDebugView.cpp`

```cpp
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "CraftingFramework/UCraftingComponent.h"

namespace{
    TAutoConsoleVariable<int32> cvarDebugCrafting(TEXT("Awaken.Debug.Crafting"), 0, TEXT("1 = draw each visible crafting session: station, staged items, running craft and the craft buttons with their states."), ECVF_Default);
    FTSTicker::FDelegateHandle craftingTickerHandle;

    bool drawCraftingDebug(float deltaTime){
        if(GEngine == nullptr){
            return true;
        }
        int32 messageKey = 9500;
        for(const FWorldContext& context : GEngine->GetWorldContexts()){
            const UWorld* world = context.World();
            if(world == nullptr || world->GetGameState() == nullptr){
                continue;
            }
            for(const APlayerState* player : world->GetGameState()->PlayerArray){
                const UCraftingComponent* crafting = UCraftingComponent::findFor(player);
                if(crafting == nullptr){
                    continue;
                }
                const FS_CraftingSessionState state = crafting->getSessionState();
                if(!state.isOpen && !state.hasStation){
                    continue;
                }
                GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::Cyan, FString::Printf(TEXT("[Crafting] %s: session %s, station %s, crafting %s"), *player->GetPlayerName(), state.isOpen ? TEXT("OPEN") : TEXT("not yet"), state.hasStation ? *state.stationTags.ToStringSimple() : TEXT("none"), state.activeRecipeId.IsNone() ? TEXT("nothing") : *state.activeRecipeId.ToString()));
                for(const TPair<FName, int32>& staged : crafting->getStagedQuantities()){
                    GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::White, FString::Printf(TEXT("   staged %s x%d"), *staged.Key.ToString(), staged.Value));
                }
                for(const FS_AvailableRecipeOption& option : crafting->getAvailableRecipes()){
                    const FColor colour = option.state == E_RecipeListState::Craftable ? FColor::Green : (option.state == E_RecipeListState::ExtraItems ? FColor::White : FColor::Red);
                    GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, colour, FString::Printf(TEXT("   [%s] missing %d, extra %d%s"), *option.recipeId.ToString(), option.getMissingCount(), option.extraItems.Num(), option.isStationMissing ? TEXT(", needs station") : TEXT("")));
                }
            }
        }
        return true;
    }

    void onCraftingDebugChanged(IConsoleVariable* variable){
        if(variable->GetInt() != 0 && !craftingTickerHandle.IsValid()){
            craftingTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&drawCraftingDebug), 0.25f);   // only while the view is on (Doc02 §5)
        }else if(variable->GetInt() == 0 && craftingTickerHandle.IsValid()){
            FTSTicker::GetCoreTicker().RemoveTicker(craftingTickerHandle);
            craftingTickerHandle.Reset();
        }
    }

    struct FCraftingDebugRegistrar{
        FCraftingDebugRegistrar(){
            cvarDebugCrafting->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&onCraftingDebugChanged));
        }
    };
    FCraftingDebugRegistrar craftingDebugRegistrar;
}

#endif
```

### CR9.4 — Log lines (Doc B22 §3.6)

Category `LogAwakenCrafting`; every line names the machine role (done by `CRAFTING_LOG`). A Log-level line exists for every state change:

| State change | Where the line is |
| --- | --- |
| Framework started; the recipe catalog loaded | `UCraftingServices::Initialize`, `UCraftingRecipeSubsystem::Initialize` |
| A station was entered | `enterStation` |
| A session opened | `openCraftingSession` |
| A craft started / completed (with where the output went) / was cancelled | `requestCraft` / `completeCraft` / `cancelCraft` |
| A staged item spoiled out of the crafting area | `onStagedItemLost` |
| A session closed, with its reason and how much was returned | `closeCraftingSession` |
| The reservation itself (staged, released, consumed) | R01 (`LogAwakenInventory`); a single stage is `Verbose` here, because a drag per unit would flood the log |
| Every refusal returned to a client | R01's `UInventoryRequestComponent::refuse` (the reason tag, in `LogAwakenInventory`) |
| Dev commands | each command logs its result |

### CR9.5 — Automation specs

Run from *Session Frontend → Automation* filtered by `Awaken.Crafting`, or:

```
UnrealEditor-Cmd.exe <Project>.uproject -ExecCmds="Automation RunTests Awaken.Crafting; Quit" -unattended -nullrhi -log
```

Every test recipe has `craftSeconds = 0`, so a craft completes inside `requestCraft` and each check is exact. The one test that needs a running craft sets its own time. The recipes are the designer's worked example 2, with `Rock` standing for the example's "stone".

`Tests/CraftingSpec.cpp`

```cpp
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NativeGameplayTags.h"
#include "Engine/GameInstance.h"
#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestCharacter.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/Stubs/InventoryStubs.h"
#include "ItemInventoryFramework/03Interfaces/BPI_PlayerStateQuery.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "CraftingFramework/UBFL_Crafting.h"
#include "CraftingFramework/UBFL_CraftingValidation.h"
#include "CraftingFramework/UCraftingComponent.h"
#include "CraftingFramework/UCraftingRecipeSubsystem.h"
#include "CraftingFramework/UCraftingStationComponent.h"
#include "CraftingFramework/UDA_CraftingRecipe.h"
#include "CraftingFramework/02Tags/T_CraftingFramework.h"
#include "CraftingFramework/03Interfaces/BPI_ProgressionQuery.h"
#include "CraftingFramework/Stubs/CraftingStubs.h"

// A blueprint tag that exists only for these tests.
UE_DEFINE_GAMEPLAY_TAG_STATIC(T_Test_Recipe_Unlock, "Progression.Recipe.AutomationTest");

namespace{
    UCraftingRecipeSubsystem* recipesOf(const FInventoryTestFixture& fixture){
        return fixture.world()->GetGameInstance()->GetSubsystem<UCraftingRecipeSubsystem>();
    }

    FS_CraftingRecipeRequirement need(FName itemId, int32 quantity){
        FS_CraftingRecipeRequirement requirement;
        requirement.itemId = itemId;
        requirement.quantity = quantity;
        return requirement;
    }

    // A recipe that exists only in this fixture's recipe catalog. craftSeconds stays 0: it completes at once.
    UDA_CraftingRecipe* addRecipe(const FInventoryTestFixture& fixture, FName recipeId, const TArray<FS_CraftingRecipeRequirement>& requirements, FName outputItemId){
        UCraftingRecipeSubsystem* recipes = recipesOf(fixture);
        UDA_CraftingRecipe* recipe = NewObject<UDA_CraftingRecipe>(recipes);
        recipe->recipeId = recipeId;
        recipe->requirementList = requirements;
        recipe->outputItemId = outputItemId;
        recipes->registerRecipe(recipe);
        return recipe;
    }

    // The items and the three recipes of the designer's worked example 2 (R02 3.5).
    void addExampleContent(FInventoryTestFixture& fixture){
        recipesOf(fixture)->resetForTest();   // only the recipes this test adds
        fixture.addStackable(TEXT("Stick"), 50);
        fixture.addStackable(TEXT("Rock"), 50);
        fixture.addStackable(TEXT("Hatchling"), 50);
        fixture.addStackable(TEXT("Arrow"), 50);
        fixture.addInstanced(TEXT("Spear"), 5, 50.0f);
        fixture.addInstanced(TEXT("StoneSpear"), 5, 80.0f);
        addRecipe(fixture, TEXT("Hatchling"), {need(TEXT("Stick"), 1), need(TEXT("Rock"), 1)}, TEXT("Hatchling"));
        addRecipe(fixture, TEXT("Arrow"), {need(TEXT("Stick"), 2), need(TEXT("Rock"), 2)}, TEXT("Arrow"));
        addRecipe(fixture, TEXT("SpearUpgrade"), {need(TEXT("Spear"), 1), need(TEXT("Stick"), 1)}, TEXT("StoneSpear"));
    }

    // A player with a bag and a character, and that character's crafting component.
    UCraftingComponent* makeCrafter(FInventoryTestFixture& fixture, AInventoryTestPlayerState*& outPlayer, UInventoryData*& outBag){
        outPlayer = fixture.makePlayer();
        fixture.makeCharacterFor(outPlayer);
        outBag = UPlayerInventoryComponent::findFor(outPlayer)->getPersonalInventory();
        return UCraftingComponent::findFor(outPlayer);
    }

    // Puts one spear with that durability into the bag and returns its instance id.
    FGuid giveSpear(const FInventoryTestFixture& fixture, AInventoryTestPlayerState* player, UInventoryData* bag, float durability){
        FS_InventoryEntry spear = UBFL_Inventory::makeFreshInstance(player, *fixture.catalog()->getDefinition(TEXT("Spear")));
        spear.state.durability = durability;
        bag->addInstance(spear);
        return spear.instanceId;
    }

    FS_ItemRef item(FName itemId){
        FS_ItemRef ref;
        ref.itemId = itemId;
        return ref;
    }

    // The button of that recipe, or null when the recipe is not listed.
    const FS_AvailableRecipeOption* findOption(const TArray<FS_AvailableRecipeOption>& list, FName recipeId){
        return list.FindByPredicate([recipeId](const FS_AvailableRecipeOption& option){
            return option.recipeId == recipeId;
        });
    }

    bool hasState(const TArray<FS_AvailableRecipeOption>& list, FName recipeId, E_RecipeListState state){
        const FS_AvailableRecipeOption* option = findOption(list, recipeId);
        return option != nullptr && option->state == state;
    }
}

BEGIN_DEFINE_SPEC(FCraftingSpec, "Awaken.Crafting", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FCraftingSpec)

void FCraftingSpec::Define(){
    // ---- Doc B22 "Craft button states" (tier 1) ----
    Describe("Craft button states", [this](){
        It("1 stick and 1 rock: hatchling green, arrow red, and the server refuses the red one", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 10);
            bag->addStack(TEXT("Rock"), 10);

            TestEqual(TEXT("an empty crafting area lists nothing"), crafting->getAvailableRecipes().Num(), 0);
            crafting->stageIngredient(item(TEXT("Stick")), 1);
            crafting->stageIngredient(item(TEXT("Rock")), 1);
            const TArray<FS_AvailableRecipeOption> list = crafting->getAvailableRecipes();
            TestTrue(TEXT("hatchling is green"), hasState(list, TEXT("Hatchling"), E_RecipeListState::Craftable));
            TestTrue(TEXT("arrow is red"), hasState(list, TEXT("Arrow"), E_RecipeListState::Missing));
            TestTrue(TEXT("spear upgrade is red"), hasState(list, TEXT("SpearUpgrade"), E_RecipeListState::Missing));
            TestTrue(TEXT("green comes first"), list.Num() == 3 && list[0].recipeId == FName(TEXT("Hatchling")));
            TestTrue(TEXT("RequestCraft(arrow) is refused"), crafting->requestCraft(TEXT("Arrow")).failReason == T_Crafting_FailReason_InsufficientStaged);
            TestEqual(TEXT("and nothing was consumed"), bag->getTotalQuantity(TEXT("Stick")), 10);
        });
    });

    // ---- Doc B22 "Craft leftovers" (tier 1) ----
    Describe("Craft leftovers", [this](){
        It("3 sticks and 4 rocks: craft arrow, 1 stick and 2 rocks stay staged, hatchling green, arrow red", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 10);
            bag->addStack(TEXT("Rock"), 10);
            crafting->stageIngredient(item(TEXT("Stick")), 3);
            crafting->stageIngredient(item(TEXT("Rock")), 4);

            TArray<FS_AvailableRecipeOption> list = crafting->getAvailableRecipes();
            TestTrue(TEXT("both are green"), hasState(list, TEXT("Hatchling"), E_RecipeListState::Craftable) && hasState(list, TEXT("Arrow"), E_RecipeListState::Craftable));
            TestTrue(TEXT("craft arrow"), crafting->requestCraft(TEXT("Arrow")).succeeded());
            TestEqual(TEXT("one arrow was made"), bag->getTotalQuantity(TEXT("Arrow")), 1);
            TestEqual(TEXT("one recipe's worth of sticks left the bag"), bag->getTotalQuantity(TEXT("Stick")), 8);
            TestEqual(TEXT("1 stick stays staged"), crafting->getStagedQuantity(TEXT("Stick")), 1);
            TestEqual(TEXT("2 rocks stay staged"), crafting->getStagedQuantity(TEXT("Rock")), 2);
            list = crafting->getAvailableRecipes();
            TestTrue(TEXT("hatchling is still green"), hasState(list, TEXT("Hatchling"), E_RecipeListState::Craftable));
            TestTrue(TEXT("arrow is now red"), hasState(list, TEXT("Arrow"), E_RecipeListState::Missing));
        });
    });

    // ---- Doc B22 "Extra item refusal" (tier 1) ----
    Describe("Extra item refusal", [this](){
        It("stick, rock and spear: hatchling and spear upgrade are white, and the server refuses a white one", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 10);
            bag->addStack(TEXT("Rock"), 10);
            giveSpear(fixture, player, bag, 50.0f);
            crafting->stageIngredient(item(TEXT("Stick")), 1);
            crafting->stageIngredient(item(TEXT("Rock")), 1);
            crafting->stageIngredient(item(TEXT("Spear")), 1);

            const TArray<FS_AvailableRecipeOption> list = crafting->getAvailableRecipes();
            TestTrue(TEXT("hatchling is white"), hasState(list, TEXT("Hatchling"), E_RecipeListState::ExtraItems));
            TestTrue(TEXT("spear upgrade is white"), hasState(list, TEXT("SpearUpgrade"), E_RecipeListState::ExtraItems));
            TestTrue(TEXT("arrow is red: red wins over white"), hasState(list, TEXT("Arrow"), E_RecipeListState::Missing));
            const FS_AvailableRecipeOption* hatchling = findOption(list, TEXT("Hatchling"));
            TestTrue(TEXT("the extra item is named"), hatchling != nullptr && hatchling->extraItems.Contains(FName(TEXT("Spear"))));
            // A client that claims the button was green sends the same request: the server checks again.
            TestTrue(TEXT("refused with ExtraIngredients"), crafting->requestCraft(TEXT("Hatchling")).failReason == T_Crafting_FailReason_ExtraIngredients);
            TestEqual(TEXT("nothing was made"), bag->getTotalQuantity(TEXT("Hatchling")), 0);
        });
    });

    // ---- Doc B22 "Craft list cap and order" (tier 1) ----
    Describe("Craft list cap and order", [this](){
        It("20 red candidates show 15, fewest missing first; 16 green show all 16 and no red", [this](){
            FInventoryTestFixture fixture;
            recipesOf(fixture)->resetForTest();
            fixture.addStackable(TEXT("Stick"), 50);
            fixture.addStackable(TEXT("Rock"), 50);
            fixture.addStackable(TEXT("Thing"), 50);
            // 20 recipes that all use a stick and need 1 to 20 rocks. Registered in reverse to prove the order is by rule.
            for(int32 rocks = 20; rocks >= 1; rocks--){
                addRecipe(fixture, FName(*FString::Printf(TEXT("Red%02d"), rocks)), {need(TEXT("Stick"), 1), need(TEXT("Rock"), rocks)}, TEXT("Thing"));
            }
            TMap<FName, int32> staged;
            staged.Add(TEXT("Stick"), 1);
            TArray<FS_AvailableRecipeOption> list = UBFL_Crafting::buildRecipeList(fixture.world(), staged, FGameplayTagContainer());
            TestEqual(TEXT("the cap is 15"), list.Num(), 15);
            TestTrue(TEXT("fewest missing first"), list[0].recipeId == FName(TEXT("Red01")) && list[14].recipeId == FName(TEXT("Red15")));

            // 16 recipes that need only a stick: all green.
            for(int32 index = 1; index <= 16; index++){
                addRecipe(fixture, FName(*FString::Printf(TEXT("Green%02d"), index)), {need(TEXT("Stick"), 1)}, TEXT("Thing"));
            }
            list = UBFL_Crafting::buildRecipeList(fixture.world(), staged, FGameplayTagContainer());
            TestEqual(TEXT("all 16 green are listed, above the cap"), list.Num(), 16);
            TestFalse(TEXT("and no red is shown"), list.ContainsByPredicate([](const FS_AvailableRecipeOption& option){
                return option.state == E_RecipeListState::Missing;
            }));
        });
    });

    // ---- Doc B22 "Locked blueprint" (tier 1) ----
    Describe("Locked blueprint", [this](){
        It("a locked recipe is never listed, even when fully satisfied, and RequestCraft is refused", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            const_cast<UDA_CraftingRecipe*>(recipesOf(fixture)->getRecipe(TEXT("SpearUpgrade")))->requiredBlueprintTag = T_Test_Recipe_Unlock;
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 10);
            giveSpear(fixture, player, bag, 50.0f);
            crafting->stageIngredient(item(TEXT("Stick")), 1);
            crafting->stageIngredient(item(TEXT("Spear")), 1);

            TestTrue(TEXT("not in the list"), findOption(crafting->getAvailableRecipes(), TEXT("SpearUpgrade")) == nullptr);
            TestTrue(TEXT("refused with BlueprintLocked"), crafting->requestCraft(TEXT("SpearUpgrade")).failReason == T_Crafting_FailReason_BlueprintLocked);

            fixture.stub<UStubProgressionQuery, UBPI_ProgressionQuery>()->unlockedTags.AddTag(T_Test_Recipe_Unlock);
            TestTrue(TEXT("unlocked: listed and green"), hasState(crafting->getAvailableRecipes(), TEXT("SpearUpgrade"), E_RecipeListState::Craftable));
        });
    });

    // ---- Doc B22 "Stage and unstage amounts" (tier 1) ----
    Describe("Stage and unstage amounts", [this](){
        It("staging reserves without removing, opens the session at the first unit, and closing returns everything", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 10);

            TestFalse(TEXT("no session before the first staged ingredient"), crafting->isSessionOpen());
            TestTrue(TEXT("one unit"), crafting->stageIngredient(item(TEXT("Stick")), 1).succeeded());
            TestTrue(TEXT("the session is open"), crafting->isSessionOpen());
            TestEqual(TEXT("staged 1"), crafting->getStagedQuantity(TEXT("Stick")), 1);
            TestEqual(TEXT("the bag still holds all 10"), bag->getTotalQuantity(TEXT("Stick")), 10);
            TestEqual(TEXT("9 are available"), bag->getAvailableQuantity(TEXT("Stick")), 9);
            // "The whole stack" is what the request passes after the server counted the available units.
            TestTrue(TEXT("the rest of the stack"), crafting->stageIngredient(item(TEXT("Stick")), bag->getAvailableQuantity(TEXT("Stick"))).succeeded());
            TestEqual(TEXT("staged 10"), crafting->getStagedQuantity(TEXT("Stick")), 10);
            TestFalse(TEXT("an eleventh is refused"), crafting->stageIngredient(item(TEXT("Stick")), 1).succeeded());

            TestTrue(TEXT("a click returns one unit"), crafting->unstageIngredient(item(TEXT("Stick")), 1).succeeded());
            TestEqual(TEXT("staged 9"), crafting->getStagedQuantity(TEXT("Stick")), 9);
            crafting->unstageIngredient(item(TEXT("Stick")), 9);
            TestEqual(TEXT("staged 0"), crafting->getStagedQuantity(TEXT("Stick")), 0);
            TestTrue(TEXT("an empty crafting area does not end the session"), crafting->isSessionOpen());

            crafting->stageIngredient(item(TEXT("Stick")), 4);
            crafting->closeCraftingSession(E_CraftingCloseReason::BagClosed);
            TestFalse(TEXT("closed"), crafting->isSessionOpen());
            TestEqual(TEXT("everything is available again"), bag->getAvailableQuantity(TEXT("Stick")), 10);
        });
    });

    // ---- Doc B22 "Station recipe" (tier 2: the session, the station component and the rule together) ----
    Describe("Station recipe", [this](){
        It("is red away from the station, green next to it, and the station takes one player at a time", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            const_cast<UDA_CraftingRecipe*>(recipesOf(fixture)->getRecipe(TEXT("Arrow")))->requiredStationTag = T_Station_CraftingTable;
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 10);
            bag->addStack(TEXT("Rock"), 10);
            crafting->stageIngredient(item(TEXT("Stick")), 2);
            crafting->stageIngredient(item(TEXT("Rock")), 2);

            const FS_AvailableRecipeOption* away = findOption(crafting->getAvailableRecipes(), TEXT("Arrow"));
            TestTrue(TEXT("red away from the station, and the card says why"), away != nullptr && away->state == E_RecipeListState::Missing && away->isStationMissing);
            TestTrue(TEXT("refused with StationRequired"), crafting->requestCraft(TEXT("Arrow")).failReason == T_Crafting_FailReason_StationRequired);
            crafting->closeCraftingSession(E_CraftingCloseReason::BagClosed);

            AStubCraftingStation* table = fixture.world()->SpawnActor<AStubCraftingStation>();
            table->stationComponent->stationTags.AddTag(T_Station_CraftingTable);
            TestTrue(TEXT("enter the station"), crafting->enterStation(table->stationComponent).succeeded());
            TestTrue(TEXT("the station is in use"), table->stationComponent->isInUse());
            TestFalse(TEXT("entering starts no session"), crafting->isSessionOpen());

            AInventoryTestPlayerState* otherPlayer = nullptr;
            UInventoryData* otherBag = nullptr;
            UCraftingComponent* other = makeCrafter(fixture, otherPlayer, otherBag);
            TestTrue(TEXT("a second player is refused"), other->enterStation(table->stationComponent).failReason == T_Interaction_FailReason_InUse);

            crafting->stageIngredient(item(TEXT("Stick")), 2);
            crafting->stageIngredient(item(TEXT("Rock")), 2);
            TestTrue(TEXT("green next to the station"), hasState(crafting->getAvailableRecipes(), TEXT("Arrow"), E_RecipeListState::Craftable));
            TestTrue(TEXT("and it can be made"), crafting->requestCraft(TEXT("Arrow")).succeeded());

            table->Destroy();   // the station is destroyed (R02 3.8)
            TestFalse(TEXT("the session ended with the station"), crafting->isSessionOpen());
            TestEqual(TEXT("nothing is still reserved"), bag->getAvailableQuantity(TEXT("Stick")), bag->getTotalQuantity(TEXT("Stick")));
        });
    });

    // ---- Doc B22 "Damage with the bag open" (tier 2: Crafting, R01's reservations and the player-state seam) ----
    Describe("Damage with the bag open", [this](){
        It("a hit changes nothing; Downed closes the session, cancels a running craft and consumes nothing", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            const_cast<UDA_CraftingRecipe*>(recipesOf(fixture)->getRecipe(TEXT("Hatchling")))->craftSeconds = 5.0f;
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 10);
            bag->addStack(TEXT("Rock"), 10);
            crafting->stageIngredient(item(TEXT("Stick")), 1);
            crafting->stageIngredient(item(TEXT("Rock")), 1);

            fixture.services()->damagePlayer(player, 10.0f, FGameplayTag());   // a hit that does not Down the player
            TestTrue(TEXT("the session stays"), crafting->isSessionOpen());
            TestEqual(TEXT("the reservation stays"), crafting->getStagedQuantity(TEXT("Stick")), 1);

            TestTrue(TEXT("a craft starts"), crafting->requestCraft(TEXT("Hatchling")).succeeded());
            TestTrue(TEXT("and is running"), crafting->isCraftInProgress());
            TestTrue(TEXT("a second press is refused"), crafting->requestCraft(TEXT("Hatchling")).failReason == T_Crafting_FailReason_CraftInProgress);
            TestTrue(TEXT("staging is refused while it runs"), crafting->stageIngredient(item(TEXT("Stick")), 1).failReason == T_Crafting_FailReason_CraftInProgress);

            // Health reached zero: Doc B02 marks the player and closes the session.
            fixture.stub<UStubPlayerStateQuery, UBPI_PlayerStateQuery>()->incapacitatedPlayers.Add(player);
            UBFL_Crafting::closeSession(crafting->GetOwner(), E_CraftingCloseReason::Downed);
            TestFalse(TEXT("the bag closed"), crafting->isSessionOpen());
            TestFalse(TEXT("the craft was cancelled"), crafting->isCraftInProgress());
            TestEqual(TEXT("nothing was made"), bag->getTotalQuantity(TEXT("Hatchling")), 0);
            TestEqual(TEXT("nothing was consumed"), bag->getTotalQuantity(TEXT("Stick")), 10);
            TestEqual(TEXT("every reservation was released"), bag->getAvailableQuantity(TEXT("Stick")), 10);
            TestTrue(TEXT("a Downed player cannot stage"), crafting->stageIngredient(item(TEXT("Stick")), 1).failReason == T_Interaction_FailReason_NotAllowedInState);
        });
    });

    // ---- Weapon upgrade, R02 3.10 (tier 2) ----
    Describe("Weapon upgrade", [this](){
        It("uses the most worn spear and makes the new weapon at full condition", [this](){
            FInventoryTestFixture fixture;
            addExampleContent(fixture);
            AInventoryTestPlayerState* player = nullptr;
            UInventoryData* bag = nullptr;
            UCraftingComponent* crafting = makeCrafter(fixture, player, bag);
            bag->addStack(TEXT("Stick"), 10);
            const FGuid goodSpear = giveSpear(fixture, player, bag, 40.0f);
            const FGuid wornSpear = giveSpear(fixture, player, bag, 12.0f);

            crafting->stageIngredient(item(TEXT("Stick")), 1);
            TestTrue(TEXT("stage a spear by its type"), crafting->stageIngredient(item(TEXT("Spear")), 1).succeeded());
            TestTrue(TEXT("the most worn one was staged"), bag->findInstance(wornSpear) != nullptr && bag->findInstance(wornSpear)->reservedQuantity == 1);
            TestTrue(TEXT("craft the upgrade"), crafting->requestCraft(TEXT("SpearUpgrade")).succeeded());
            TestTrue(TEXT("the worn spear is gone"), bag->findInstance(wornSpear) == nullptr);
            TestTrue(TEXT("the good spear is untouched"), bag->findInstance(goodSpear) != nullptr && bag->findInstance(goodSpear)->reservedQuantity == 0);

            const FS_InventoryEntry* upgraded = bag->getEntries().FindByPredicate([](const FS_InventoryEntry& entry){
                return entry.itemId == FName(TEXT("StoneSpear"));
            });
            TestNotNull(TEXT("a stone spear was made"), upgraded);
            if(upgraded != nullptr){
                TestEqual(TEXT("at full condition, not the old one's 12"), upgraded->state.durability, 80.0f);
            }
        });
    });

    // ---- Doc B22 "No firearm recipe" (tier 1) ----
    Describe("No firearm recipe", [this](){
        It("the data check rejects a recipe that makes a firearm, its ammunition, or uses an unknown item", [this](){
            FInventoryTestFixture fixture;
            recipesOf(fixture)->resetForTest();
            fixture.addStackable(TEXT("Scrap"), 50);
            fixture.addStackable(TEXT("Bullet"), 50);
            UDA_ItemDefinition* pistol = fixture.addInstanced(TEXT("Pistol"), 2, 100.0f);
            pistol->weaponType = E_WeaponType::Firearm;
            pistol->ammoItemId = TEXT("Bullet");

            TArray<FString> errors;
            addRecipe(fixture, TEXT("GoodRecipe"), {need(TEXT("Scrap"), 2)}, TEXT("Scrap"));
            TestEqual(TEXT("a normal recipe passes"), UBFL_CraftingValidation::validateRecipes(fixture.world(), errors), 0);

            addRecipe(fixture, TEXT("MakePistol"), {need(TEXT("Scrap"), 5)}, TEXT("Pistol"));
            addRecipe(fixture, TEXT("MakeBullet"), {need(TEXT("Scrap"), 1)}, TEXT("Bullet"));
            addRecipe(fixture, TEXT("Typo"), {need(TEXT("Scrapp"), 1)}, TEXT("Scrap"));
            errors.Reset();
            TestEqual(TEXT("three errors: the firearm, the ammunition, the unknown ingredient"), UBFL_CraftingValidation::validateRecipes(fixture.world(), errors), 3);
        });
    });
}

#endif
```

| Test | Tier | Sets up | Checks |
| --- | --- | --- | --- |
| Craft button states | 1 | 1 stick, 1 rock staged | hatchling green, arrow red; `requestCraft(Arrow)` → `InsufficientStaged` |
| Craft leftovers | 1 | 3 sticks, 4 rocks staged | craft arrow; 1 stick and 2 rocks remain; hatchling green, arrow red |
| Extra item refusal | 1 | stick, rock, spear staged | two white entries; `requestCraft(Hatchling)` → `ExtraIngredients` |
| Craft list cap and order | 1 | 20 red recipes, then 16 green | 15 shown, fewest missing first; all 16 green and no red |
| Locked blueprint | 1 | a gated recipe, fully staged | not listed; `BlueprintLocked`; listed after the unlock |
| Stage and unstage amounts | 1 | 10 sticks | one unit and the whole stack; reserve without removing; the session opens at the first unit; closing returns everything |
| Station recipe | 2 | a stub station, two players | red away, green next to it; the Exclusive lock; the station destroyed ends the session |
| Damage with the bag open | 2 | a 5-second craft, R01's player-state stub | a hit changes nothing; Downed closes, cancels and consumes nothing |
| Weapon upgrade | 2 | two spears of different wear | the most worn is used; the output is at full condition |
| No firearm recipe | 1 | a firearm and its ammunition | the data check reports all three bad recipes |
| Replication and requests | 3 (manual) | CR9.7 | what the client must see |

The remaining R02 row of Doc B22, **"Bag refused"** (a Bagless or carrying player cannot open the screen), is a test of Doc B17's `CanOpenBag`; the server-side half is already inside the rows above and in the Verify table of Section 5.

### CR9.6 — Test map and scenario

Reuse R01's `TestMap_Inventory` (GameMode override `AInventoryTestGameMode`, two PlayerStarts). Nothing has to be placed: the station comes from `Awaken.Craft.SpawnStation CraftingTable`. The **"full crafting area" scenario** is: `Awaken.Item.Give Stick 10`, `Awaken.Item.Give Rock 10`, `Awaken.Item.Give Spear 2`, then `Awaken.Craft.StageAll`, which prints every button with its state.

### CR9.7 — The two-player check (Doc B22 §3.8, §3.10)

Editor *Play As Listen Server*, 2 players (host = player 0, client = player 1), then once offline. Turn on `Awaken.Debug.Crafting 1` on **both** machines. Requests from the client come from the bag screen when Doc B17 exists; until then bind a few test keys or a test widget to the five requests of CR8.2.

1. Host: `Awaken.Item.Give Stick 10 p1`, `Awaken.Item.Give Rock 10 p1`. The client's bag shows them; nothing is staged anywhere.
2. Client: stage one stick (`serverStageIngredient(Stick, false)`). **The client sees** `session OPEN`, `staged Stick x1` and three buttons (the hand recipes that use a stick), all red. **The host sees** the same session for player 1 in its debug view. The host's own player has no session.
3. Client: stage the whole rock stack (`wholeStack = true`). Both views show `Rock x10`; `Hatchling` is green, `Arrow` and `Spear` are red (one stick is not enough).
4. Client: stage one more stick, then press `Spear`. After its `craftSeconds` the client's bag holds a spear; staged is now `Rock x9`; the host's log has the "crafted Spear" line. A third machine would see only the craft animation event.
5. Client: press `Arrow` (red). Refused `InsufficientStaged`; **only the client** is told; nothing changes on either machine.
6. Client: stage the new spear as well and press `Hatchling` after staging a stick: refused `ExtraIngredients`.
7. Client: close the bag (`serverCloseBag`). Everything is available again; the client's view shows no session; `Crafting.SessionClosed` was raised on the client only.
8. Host: `Awaken.Craft.SpawnStation CraftingTable p1`. Both machines show the station as in use and player 1's `station Station.CraftingTable`. Host: `Awaken.Craft.SpawnStation CraftingTable` for itself spawns a second table; then try to enter the **first** one with `serverUseStation`: refused `InUse`.
9. Host: `Awaken.Craft.StubUnlock Progression.Recipe.StoneSpear 1` on the host **and** on the client (the stub is local). Client: stage a spear and a rock at the table: `StoneSpear` is green; press it; the client's bag holds a full-durability stone spear.
10. Host: destroy the first table (select it in the outliner and delete it). The client's session ends with the reason `StationLost` and what was staged is available.
11. Host: `Awaken.Carry.Give Log 1 p1` (Carry guide), then the client tries to stage: refused `HandsFull`.
12. Client: stage three sticks and **disconnect**. On the host nothing is lost from that player's held state and no station stays locked.
13. Repeat steps 2, 4, 7 and 9 **offline** (standalone): same results on the one machine.

### CR9.8 — Stub register (what to delete, and when)

| Stub / temporary item | File | Owner | Remove when |
| --- | --- | --- | --- |
| `UStubProgressionQuery`, the command `Awaken.Craft.StubUnlock` | `Stubs/CraftingStubs.*`, `DevTools/` | Doc R06 Progression | R06 registers an `IBPI_ProgressionQuery` with `registerSeam` (its own command is `Awaken.Unlock.Grant`) |
| `BPI_ProgressionQuery.h` | `03Interfaces/` | Doc R06 | only if R06 or the Building guide declares the same interface elsewhere: then use theirs |
| `AStubCraftingStation`, the command `Awaken.Craft.SpawnStation` | `Stubs/CraftingStubs.*`, `DevTools/` | Doc B01 Building | a Crafting Table piece carries `UCraftingStationComponent`. Keep the stub for the "Station recipe" test if you like |
| The lock inside `UCraftingStationComponent` (`tryLock`, `release`) | `UCraftingStationComponent.*` | Doc B18 | Interaction Core has its own Exclusive lock; the two functions then forward to it |
| `serverUseStation`, the setting `stationUseRange` | `UCraftingComponent.*`, `UCraftingSettings.h` | Doc B18 | the station's interaction goes through `RequestInteract` and calls `enterStation` |
| Duplicate tag definitions `Station.CraftingTable`, `Station.Forge`, `Interaction.FailReason.InUse` | `T_CraftingFramework.*` | Docs B01 / B10, B18 | the owners' tag headers exist |
| `craftingComponent` on `AInventoryTestCharacter` | R01 `Stubs/` | Docs B19, R07 | the real Character carries the component |
| `craftSeconds` on the recipe (K2) | `UDA_CraftingRecipe.h` | Doc R07 | only if the designer wants the time read from the animation asset instead; then the field becomes a cached value written by R07's data check |

### Handoff notes for teammates

| Doc | You must |
| --- | --- |
| **R01** (Item & Inventory) | nothing new: Crafting calls `reserveForStaging`, `releaseStagedItem`, `releaseStagedReservation`, `consumeStaged`, `grantOrDrop` and listens to `onStagedItemLost`. Patch applied by this guide: CR5.7 (the stub Character) |
| **R06** (Progression) | implement `IBPI_ProgressionQuery::isUnlocked` on the progression object and register it; create the `Progression.Recipe.*` tags the recipes name |
| **B01** (Building) | add `UCraftingStationComponent` to the Crafting Table and Forge pieces, set their `stationTags`, call `setStationId(pieceId)`; your own data check: a manual piece cost may not be a heavy item |
| **B18** (Interaction) | the station interaction calls `enterStation`; refuse with `InUse` while `isInUse()`; call `forceRelease(OutOfRange)` when your lock releases; take over the lock and the range (stub register) |
| **B17** (UI) | the five requests and the redraw rules of CR8.3; the button colours, the second cue, the hover card and the empty state from `getAvailableRecipes()`; close the screen on `Crafting.SessionClosed`; `CanOpenBag` refuses Bagless and carrying; text for the six `Crafting.FailReason.*` tags and the keys `UI.Crafting.DropHere` and `UI.Crafting.NeedsStation` |
| **R07** (Animation) | play the recipe's `animationTag` for the owner while `activeRecipeId` is set; a craft animation for others from the cosmetic events; compare each montage length with `craftSeconds` in your data check |
| **B02** (Checkpoint & Respawn) | `UBFL_Crafting::closeSession(character, Downed)` when entering Downed, and `(…, Died)` before `detachBag` |
| **R17** (Cinematic) | `UBFL_Crafting::closeSession(character, SceneStarted)` when a scene locks the input |
| **B20** (Session) | `UBFL_Crafting::handlePlayerLeft(player)` on `Session.PlayerLeft`, before R01's `handlePlayerLeft` |
| **B21 / R09 / R15** | play the effect and sound of `Cosmetic.Event.Craft.Started` / `.Finished` |
| **R05** (Equipment) | nothing: a weapon upgrade is an ordinary recipe (R05 3.4) |
| **B10** (Processing) | nothing: a furnace or sawmill that produces over time is yours (R02 0.2). Only the `Station.*` tag namespace is shared |
| **B19** | add `UCraftingComponent` to the real Character class |

### CR9.9 — Final review: every confirmed R02 rule maps to a task

| R02 rule | Where |
| --- | --- |
| A recipe is a data asset; a new recipe is never new code (3.1) | CR2.1 |
| Recipes are indexed by ingredient type (3.1, review R02-06) | CR2.3, CR3 |
| Inventory and crafting are one screen; opening the bag alone starts no session and reserves nothing (3.2) | CR5, CR8.3 |
| The session starts at the first staged ingredient and ends when the bag closes (3.2, S12-P2) | CR5 `stageIngredient`, CR7 |
| A station opens the same screen with the station recipes included; a station is Exclusive (3.2, §1) | CR4, CR5 `enterStation` |
| Staging wraps `ReserveForStaging`: one unit, or the whole stack with Shift (3.3) | CR5, CR8.2 |
| A click returns one unit, Shift the whole staged stack; wraps `ReleaseStagedItem` (3.3, S12-P4) | CR5, CR8.2 |
| The staged data is the reserved quantity; readers see the available quantity (3.3) | CR5 `getStagedQuantities` (K4) |
| One button per recipe; listed only if it uses a staged item type (3.4, S12-D4, S12-P1) | CR3 |
| Green, white, red; red wins over white (3.4) | CR3 `evaluateRecipe` |
| The cap of 15, the order, greens and whites always listed (3.4, S12-D6, S12-P9) | CR0.3, CR3 `buildRecipeList` |
| A locked blueprint is never listed; a station recipe is red away from the station (3.4, 3.9) | CR3 |
| The hover card data: missing ingredients, the station, extra items, `MaxCraftableCount` (3.4, §6) | CR1.2, CR3 |
| Each press makes exactly one recipe's worth; surplus stays staged; both worked examples (3.5) | CR6, Specs 1 to 3 |
| The output is granted with `GrantOrDrop` and never destroyed (3.6) | CR6 `completeCraft` |
| Crafting is a small animation and nothing more (3.7) | CR6, K2 |
| The server decides; only a recipe that is green on the server is made; the four refusal reasons (3.7, S12-D7) | CR6 `requestCraft` |
| Ingredients are consumed at completion; a press during a craft is ignored; an interrupted craft consumes nothing (3.7) | CR6 |
| Damage, then Downed or death, then completion (3.7, Doc02 19.9) | CR6 rule 7 |
| The list of what ends a session; damage never does (3.8, S12-P3) | CR7 |
| A destroyed or collapsed station closes the session (3.8, S4-F24) | CR7.2 |
| A session is never saved; a save may happen with the bag open (3.8, §9, ST-11) | CR5 design rule, CR8.5 |
| Gating by station and by blueprint; unlocks are shared (3.9) | CR3, CR6, the R06 seam |
| A weapon can be an ingredient, reserved by instance; the upgrade is at full condition; the equipped one cannot be staged; the most worn first (3.10) | CR6 rule 6, R01 guide S7.2 |
| A Bagless player cannot craft; neither can a player carrying a heavy item (3.11, S12-P5) | CR5 `checkPlayerCanCraft` |
| Staging is a mouse interaction; no gamepad at launch (3.12) | no code: the requests do not care what sent them |
| The three messages and their delivery (§7) | CR5, CR6, CR8.4 |
| Server-checked commands; the server is the craft timer (§8) | CR6, CR8.2 |
| Cosmetic events `Craft.Started` / `Craft.Finished` (A89) | CR6 |
| `Session.PlayerLeft` ends the session and returns the materials (A89) | CR7.3 |
| Text is keys, never sentences (A89) | no code here holds player-facing text |
| The Axe, the map and a container that holds something are never ingredients (A92) | R01 guide S7.2 at staging; CR2.4 at data time |
| A recipe names only items without a carry cost (A103, S4-F01) | CR2.4 |
| No recipe makes a firearm or a bullet (A116) | CR2.4, Spec "No firearm recipe" |
| The editor data check: items exist, no duplicate RecipeID (§12) | CR2.1, CR2.3, CR2.4 |

### The closing section every guide must carry (Doc B22 §3.10)

| Part | R02 |
| --- | --- |
| **Commands** | `Awaken.Craft.Stage`, `.Unstage`, `.Craft`, `.Close`, `.SpawnStation`, `.StubUnlock`, `.List` (read), `.Validate` (read), `.StageAll` (the situation most likely to break: a full crafting area) |
| **Debug view** | `Awaken.Debug.Crafting` (each visible session: station, staged items, the running craft, every button in its colour) |
| **Log category** | `LogAwakenCrafting`, role-prefixed, one Log line per state change; refusals log their reason tag through R01's request component |
| **Automated tests** | Craft button states, Craft leftovers, Extra item refusal, Craft list cap and order, Locked blueprint, Stage and unstage amounts, No firearm recipe (tier 1); Station recipe, Damage with the bag open, Weapon upgrade (tier 2); Replication and requests (tier 3, manual two-player) |
| **Two-player check** | CR9.7 |
| **Done when** | the commands run, the tests pass, CR9.7 has been done on a listen server plus one client and once offline |

---

# Appendix A — Files To Create (Doc02 §20.3 layout)

```
Public/CraftingFramework/
    00Enums/E_CraftingFramework.h
    01Structs/S_CraftingFramework.h
    02Tags/T_CraftingFramework.h
    03Interfaces/BPI_ProgressionQuery.h                (outgoing seam)
    04Delegates/D_CraftingFramework.h
    CraftingLog.h   CraftingServices.h   UCraftingSettings.h
    UDA_CraftingRecipe.h   UCraftingRecipeSubsystem.h   UBFL_CraftingValidation.h
    UBFL_Crafting.h   UCraftingStationComponent.h   UCraftingComponent.h
Private/CraftingFramework/
    02Tags/T_CraftingFramework.cpp
    CraftingLog.cpp   CraftingServices.cpp   UCraftingSettings.cpp
    UDA_CraftingRecipe.cpp   UCraftingRecipeSubsystem.cpp   UBFL_CraftingValidation.cpp
    UBFL_Crafting.cpp   UCraftingStationComponent.cpp   UCraftingComponent.cpp
    Stubs/CraftingStubs.h/.cpp
    DevTools/CraftingDevCommands.cpp   CraftingDebugView.cpp
    Tests/CraftingSpec.cpp
Content/Awaken/Recipes/*          (DA_CraftingRecipe instances)
Config/DefaultGame.ini            (the Recipe primary asset type, CR2.2)
```

File of the R01 guide that this guide **patches**: `AInventoryTestCharacter.h/.cpp` (CR5.7). Nothing else in R01 changes: its Section 7 was written for this framework.

> R02 §12 lists `S_CraftingRecipeRequirement.h`, `S_AvailableRecipeOption.h` and `E_RecipeListState.h` as separate files. That list is older than Doc02 §20.3, which asks for one file per category per framework; this guide follows §20.3 and puts the structs in `S_CraftingFramework.h` and the enums in `E_CraftingFramework.h`.

# Appendix B — Tags and data fields to register

- **Messages (owner R02):** `Crafting.SessionOpened`, `Crafting.ItemCrafted`, `Crafting.SessionClosed`. The Tag Registry's `Crafting.*` section lists only three of the fail reasons by name; add the three messages there.
- **Fail reasons (owner R02):** `Crafting.FailReason.StationRequired`, `.BlueprintLocked`, `.InsufficientStaged`; and the three the registry marks PROPOSED: `.BagRequired`, `.CraftInProgress`, `.ExtraIngredients`. No new fail reason was invented by this guide.
- **Cosmetic events (owner R02):** `Cosmetic.Event.Craft.Started`, `Cosmetic.Event.Craft.Finished`.
- **Reused from other owners:** `Station.CraftingTable`, `Station.Forge` (B01 / B10); `Interaction.FailReason.InUse`, `.HandsFull`, `.NotAllowedInState` (B18); `Inventory.FailReason.NotServer`, `.NotFound`, `.OutOfRange`, `.RateLimited`, and everything `reserveForStaging` returns: `.InsufficientQuantity`, `.ItemWielded`, `.Permanent`, `.ContainerNotEmpty`, `.ReservedQuantity`, `.NoBag` (R01); `Progression.Recipe.*` (R06).
- **Text keys (Doc R10, already created in Index A115):** `UI.Crafting.DropHere`, `UI.Crafting.NeedsStation`.
- **New data to log in the Index:** the recipe field `craftSeconds` (K2) and the spelling `recipeId` as an `FName` (K1); the primary asset type `Recipe`; the settings `maxVisibleRecipes` and `stationUseRange`; `isStationMissing` on `S_AvailableRecipeOption` (the spec folds the station into `MissingIngredients`); the struct `S_CraftingSessionState`; the enum `E_CraftingCloseReason`; the payload field `crafter` on all three messages and `outputQuantity` on `Crafting.ItemCrafted`.
- **New names chosen here:** `UCraftingRecipeSubsystem`, `UCraftingStationComponent` (`tryLock`, `release`, `forceRelease`, `setStationId`), `UCraftingServices`, `UBFL_Crafting` (`evaluateRecipe`, `buildRecipeList`, `closeSession`, `handlePlayerLeft`, `isSessionOpen`), `UBFL_CraftingValidation::validateRecipes`, `IBPI_ProgressionQuery`, and on the component `enterStation`, `getStagedQuantities`, `serverUseStation`, `serverStageIngredient`, `serverUnstageIngredient`, `serverRequestCraft`, `serverCloseBag`.

# Appendix C — Open points to confirm with the designer or teammates

| # | Point | Who |
| --- | --- | --- |
| 1 | **K2.** The crafting time is a number on the recipe (`craftSeconds`) that must match the animation's length, checked by Doc R07's data check. The spec says the animation's length *is* the time | You (R02 owner), R07 owner |
| 2 | **K5.** Reds are ordered by the total number of missing units, a missing station counting 1; ties and the order inside green and white are by recipe id. Should a missing station put a recipe last instead? | Designer |
| 3 | **K6.** The station is a component of this framework that Building adds to its table and forge pieces, and it holds the Exclusive lock until Doc B18 has its own | B01 and B18 owners |
| 4 | **K7.** The bag screen tells the server when it closes (`serverCloseBag`). A client that never sends it keeps its own items reserved, and keeps a station locked, until its pawn is removed. Doc B18's out-of-range release covers the station once it exists | B17 and B18 owners |
| 5 | **K13.** While a craft runs, a second press, staging and unstaging are all refused with `CraftInProgress`. The spec says only that a press is ignored | Designer |
| 6 | **K14.** A recipe that lacks both an ingredient and its station is refused with `StationRequired` | Designer |
| 7 | **K16.** Shift on a weapon stages one copy, not every copy of that type | Designer, B17 owner |
| 8 | **Other players see one generic craft animation.** The cosmetic event has four fields and none for the recipe; a per-recipe animation for others needs the optional `DetailTag` that Doc B21 proposes (B21-IG-4) | B21 and R07 owners |
| 9 | **Downed has no safety net.** Until Doc B02 adds its line (CR7.3), a Downed player's session stays open on the server: their items are safe, a running craft is cancelled at its end by the state check, but a station stays locked until the pawn is removed | B02 owner |
| 10 | **The progression stub is local to each machine.** Until Doc R06 registers the real, replicated set, a client colours a gated recipe from its own stub. The server's answer always decides | R06 owner |
| 11 | **A heavy output.** A recipe may make a heavy item (a data warning, not an error): it goes to the hands or lands on the ground, while the bag is still open. Should it be an error instead? | Designer |
| 12 | **An instanced output with a quantity above 1** makes that many separate items; any that do not fit the bag limit drop on the ground | Designer |
| 13 | **Staging rate.** One drag per unit goes through R01's shared request rate limit (`requestsPerSecond`). Fast Alt-clicking can hit it; raise the setting or give staging its own bucket if testing shows it | You, after the first network test |
| 14 | **The animation tag root.** `animationTag` has no tag filter, because no document names the root for context montages yet | R07 owner |
| 15 | **Doc R06 says Equipment calls `IsUnlocked()` for weapon-upgrade gating.** A weapon upgrade is an ordinary recipe (R05 3.4), so that check is the one in this framework, and Equipment has none (the R05 guide notes the same) | R06 and R05 owners |
| 16 | **Doc B01 still describes a "Station Auto-Production extension" of Crafting** for the furnace and sawmill (its §0, §1 and integration table). R02 0.2 gives that to Processing's repeat mode (Doc B10 3.4). Nothing for it is built here | B01 and B10 owners |
| 17 | **Stale text in Doc R02** to tidy at the next edit: §4.1 `OpenCraftingSession(OptionalStationID)` (K9); §6 `S_AvailableRecipeOption` has no separate station flag; §7's payloads lack the added fields; §8 says the server checks "the station range" on every command, while 3.8 says range is released by Doc B18's lock; §12's file list (see Appendix A); the header of 3.8 still says PROPOSED although most of its rows are designer-approved | You (R02 owner) |

# Appendix D — Definition of done for the framework

Ten sections (0 to 9) delivered and reviewed with every task ticked; `Awaken.Craft.Validate` reports zero errors on all recipe content; every test in the Verify tables passes; the ten automation specs pass; CR9.7 done on a listen server plus a client and once offline; R01's own staging tests still pass; every stub in the register has an owner and a removal condition; the closing Dev Tools section is in place.

> **Working method from here:** tell me **"start Section 0"** (or paste the first compiler error of whichever section you built) and we go section by section, fixing as we compile, the same way as the other guides. If a name in 0.4 differs in your real code, send it first and I will adjust the guide before you begin.
