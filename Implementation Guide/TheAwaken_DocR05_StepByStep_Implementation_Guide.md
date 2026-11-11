# TheAwaken — Doc R05 Step-by-Step Implementation Guide
## Equipment Framework (armor stack, clothes, armor wear)

Prepared for Yasir · Target: Unreal Engine 5.8 · C++ core with a thin layer of Blueprints
Companion to: *TheAwaken_DocR05_EquipmentFramework_ReworkSpec* (the spec wins on any conflict) and *TheAwaken_DocR01_StepByStep_Implementation_Guide* (this guide builds on the classes that guide created and uses the same format, task ids and rules).

---

## How to use this guide

1. Work top to bottom. Each section ends with **Verify** and **Handoff**. Do not start a section until the previous one compiles and its checks pass.
2. Task ids read `E3.4` = Equipment Section 3, task 4. The `E` keeps them apart from the `S` ids of the R01 guide. Each task cites the R05 clause (or related doc) it implements.
3. **Honesty note:** this code was written against UE 5.8 conventions and against the class and function names of the R01 guide, but **was not compiled**. Expect a few small API or include fixes. After each section, paste the first compiler error (if any) and we fix it together; do not "fix forward" across sections.
4. **This guide assumes the R01 guide's code exists** (at least Sections 0 to 9, 11 and 12). If you renamed something in R01 while building it, the same rename applies here. The exact R01 names this guide calls are listed in 0.4, so you can check them in one pass before you start.
5. Everything marked **PROPOSED** is implemented as written but kept easy to change: every number and every content choice lives in data (Doc00 §2.7).
6. Placeholders are the same as in R01: `AWAKEN_API` is the module export macro and `Awaken` the module name.

**What this framework is, in four sentences.** A player wears a *stack* of armor pieces (up to 14, a data value) that protect the body as a whole, with no body slots. An equipped piece leaves the bag and lives in the Equipment state; it wears down as it absorbs attack damage and disappears when it is used up. A player also wears one outfit of clothes, which gives its own protection and warmth, never wears out, and is changed from the bag. Weapons are **not** part of this framework: a weapon is "equipped" by the hotbar or a direct pick (Doc R01), so nothing here touches them.

---

## 0.1 Code style reminder (Doc02 §20, designer-confirmed)

The full cheat sheet is section 0.1 of the R01 guide. The short version, because every block below follows it:

| Rule | Right | Wrong |
| --- | --- | --- |
| Variables, params, members, functions | `armorStack`, `equipArmor()` | `ArmorStack`, `EquipArmor()` |
| Booleans: no `b` prefix | `isWarm` | `bIsWarm` |
| Enum / struct / interface / delegate / tag | `E_Name`, `FS_Name`, `UBPI_Name` + `IBPI_Name`, `FD_Name`, `T_Equipment_ArmorBroken` | anything else |
| Keyword spacing and braces | `if(x){` … `}else{` … closing `}` alone on its line, braces always | `if (x) {`, `} else {`, `if(x) return;` |
| Indent | 4 spaces | tabs |

Names Unreal forces on us that are new in this guide (Doc02 §20.5): `ForceNetUpdate`, `InitializeDependency`, and `OnRep_armorStack` / `OnRep_wornClothes` (the replication-notify rule). Everything else PascalCase in the code is already on the R01 list (`BeginPlay`, `GetLifetimeReplicatedProps`, `_Implementation`, `IsDataValid`, the macros).

> The spec writes names the old way (`EquipArmor`, `S_EquippedArmorState`). In code they become `equipArmor` and `FS_EquippedArmorState`. Same thing, Doc02 §20 spelling.

---

## 0.2 Decisions this guide makes where the spec is silent

R05 is a short spec and leaves several mechanics open. These are the choices I made so you can build without waiting. Each is small and easy to reverse. **Confirm or override any of them** (they are repeated in Appendix C as questions for the designer); none blocks Section 0.

| # | Open point | Choice in this guide | Why |
| --- | --- | --- | --- |
| E1 | Where equip requests travel | Server RPCs on `UEquipmentComponent` itself (it sits on the PlayerState, which the owning client may call). They reuse R01's request gate, so one rate limit covers both frameworks | No second request component; needs a 3-line patch that makes three R01 helpers public (Section 9) |
| E2 | What happens to stack positions when a piece in the middle leaves | **The stack closes up.** `stackIndex` is always the piece's place in the array (0 = first worn). Pieces above the removed one move down one position | It is a stack: array order *is* stacking order, so "the last piece stacked absorbs the hit first" (R05 amendment) needs no extra field. The other option (stable positions with holes plus an equip counter) is noted in Appendix C |
| E3 | `S_EquippedArmorState` has no item id (R05 §6) | Add `itemId` to the struct | The armor stack replicates to everyone (R05 §8), but other players never receive this player's bag, so without the id they cannot know which mesh to show |
| E4 | How much a hit wears armor down | A piece loses durability equal to **the damage it absorbed**. `durabilityMax` is "how much it can take" (R05 3.2). `durabilityLossPerUse` is not used for armor | Matches "each piece can absorb a limited amount of damage" literally |
| E5 | The arithmetic of R03 step 4 for clothes | Clothes protection is a **flat amount** taken off first; then armor removes up to `aggregatedArmor × (1 − ArmorPenetrationPercent)`, limited by the durability the stack has left | R05: "clothes protection reduces the damage the armor receives"; R03 3.7 gives the armor formula. R03 owns the step; Section 5 ships a reference version so you can test before R03 exists |
| E6 | Who tells Stats (R08) | A seam `IBPI_ArmorStatsSink` with a stub, exactly like the R01 seams. The real R08 registers itself with one line | R08 belongs to a teammate and may not exist yet (Doc00 §4.2) |
| E7 | Who plays the armor-break effect (B21) | A seam `IBPI_CosmeticEventSink` with a stub | Same reason; also lets a test see that the event was raised |
| E8 | Where the armor and clothes numbers live | Five new fields in the empty **Equipment block** of `UDA_ItemDefinition`: `armorValue`, `clothesProtection`, `warmth`, `armorMesh`, `outfitMesh` | R01's guide left that block empty "for R05". The two mesh fields are PROPOSED; Doc R07 owns how they are shown |
| E9 | Old clothes when the new ones come from the bag | Returned to the bag through `grantOrDrop`, so if that type is at its limit they drop at the player's feet | Doc02 §19.5: a full bag means the item drops, never disappears |
| E10 | Taking clothes off without putting others on | **Not offered.** A character always wears something (R01 3.8), and the default clothes are not an item | No rule describes it; adding it later is one small command |
| E11 | A piece breaking on the host | The host raises `Equipment.ArmorBroken` (server only) **and** `Equipment.ArmorUnequipped` (local), clients raise `ArmorUnequipped` from replicated state | Clients cannot hear a server-only message; the mesh must disappear on every machine |
| E12 | Fresh character / Start Again | `resetForNewCharacter()` clears the stack and returns to default clothes. Whoever calls R01's `startNewCharacter` calls it too | R01's function cannot know about R05 |
| E13 | Saved armor of a player whom Doc B06 leaves Bagless at join (the "world bag wins" case, R13 §3.6) | `discardWornArmor()` for B06 to call | The real pieces are in the bag record; the copies in the old Character file must not come back |

---

## 0.3 Section map and dependency order

```
E0 scaffolding ─ E1 types/tags/seams ─ E2 item data ─ E3 component core
E4 equip/unequip armor ─ E5 armor damage ─ E6 clothes ─ E7 bag lifecycle (the R01 seam)
E8 save ─ E9 networking ─ E10 dev tools, tests, acceptance
```

Each section depends only on earlier ones.

## 0.4 What this guide uses from the R01 guide

Check these names against your real R01 code once. If one differs, tell me and I adjust the guide.

| R01 piece | Used for |
| --- | --- |
| `UInventoryServices` (`get`, `registerSeam`, `getSeam`, `isIncapacitated`, `isCarryActive`, `spawnLooseItem`) | the shared seam registry and the player-state checks |
| `IBPI_EquipmentGear`, `FS_GearReleaseResult`, `FS_GearSlotRecord` | the seam R01 calls from `detachBag` / `reattachBag`; **this guide replaces `UStubEquipmentGear`** |
| `UPlayerInventoryComponent` (`findFor`, `hasBag`, `getPersonalInventory`) | the bag |
| `UInventoryData` (`findInstance`, `addInstance`, `removeInstance`, `removeStack`, `getAvailableQuantity`, `canFit`, `isDeathBag`) | moving items between bag and worn state |
| `UBFL_Inventory` (`isServerWorld`, `grantOrDrop`, `getDropLocation`, `makeFreshInstance`) | returning old clothes, tests and dev commands |
| `UItemCatalogSubsystem::getDefinition`, `UDA_ItemDefinition` (`categoryTag`, `itemKind`, `durabilityMax`) | item data |
| `UContainerViewComponent` (`findFor`, `getViewedContainerId`), `UInventoryRegistrySubsystem::findInventory` | clothes taken from a chest |
| `UInventoryRequestComponent` (`findFor`, `gate`, `refuse`, `isContainerReachable`) | request gating (patched to public in Section 9) |
| `IBPI_SaveParticipant`, `E_SaveScope`, `UStubSaveRegistry`, `UBFL_InventorySave::toBytes` / `fromBytes` | save |
| `InventoryLog::roleName`, `FInventoryTestFixture`, `AInventoryTestPlayerState` | logging and tests |
| Tags `T_Item_Category_Armor`, `T_Item_Category_Clothing`, `T_Inventory_FailReason_*`, `T_Interaction_FailReason_BagFull` | categories and refusal reasons |

---

# SECTION 0 — Scaffolding

**Goal:** an empty Equipment framework that compiles and logs.
**Spec:** Doc02 §14, §20.3; Doc B22 §3.6.
**Depends on:** the R01 guide's Sections 0 and 1.

### Files

```
Source/Awaken/Public/EquipmentFramework/
    01Structs/        02Tags/        03Interfaces/        04Delegates/
    EquipmentLog.h
    EquipmentServices.h
    UEquipmentSettings.h
Source/Awaken/Private/EquipmentFramework/
    02Tags/   Stubs/   DevTools/   Tests/
    EquipmentLog.cpp
    EquipmentServices.cpp
    UEquipmentSettings.cpp
```

> There is no `00Enums` folder: this framework needs no enum. Doc02 §20.3 asks for one file per category *that exists*; do not create an empty one.

### E0.1 — Create the folder tree (Doc02 §20.3)

From `Source/<Module>`:

```bash
for dir in 01Structs 02Tags 03Interfaces 04Delegates; do
  mkdir -p "Public/EquipmentFramework/$dir"
done
mkdir -p Private/EquipmentFramework/02Tags Private/EquipmentFramework/Stubs
mkdir -p Private/EquipmentFramework/DevTools Private/EquipmentFramework/Tests
```

No change to `Awaken.Build.cs`: Equipment lives in the same module as Item & Inventory and needs no new dependency.

### E0.2 — Log category (Doc B22 §3.6 names it `LogAwakenEquipment`)

`Public/EquipmentFramework/EquipmentLog.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "ItemInventoryFramework/InventoryLog.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAwakenEquipment, Log, All);

// Every line names the machine role (Host, Client, Offline). Use: EQUIPMENT_LOG(Log, this, "text %d", value);
#define EQUIPMENT_LOG(verbosity, worldContext, format, ...) \
    UE_LOG(LogAwakenEquipment, verbosity, TEXT("[%s] " format), *InventoryLog::roleName(worldContext), ##__VA_ARGS__)
```

`Private/EquipmentFramework/EquipmentLog.cpp`

```cpp
#include "EquipmentFramework/EquipmentLog.h"

DEFINE_LOG_CATEGORY(LogAwakenEquipment);
```

### E0.3 — Project settings (every tuning number in one place)

`Public/EquipmentFramework/UEquipmentSettings.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "UEquipmentSettings.generated.h"

class UDA_ArmorStackLayout;

// Project Settings -> Game -> Awaken Equipment. Every value is tuning data (Doc00 2.7).
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Awaken Equipment"))
class AWAKEN_API UEquipmentSettings : public UDeveloperSettings{
    GENERATED_BODY()

public:
    UEquipmentSettings();

    static const UEquipmentSettings* get(){
        return GetDefault<UEquipmentSettings>();
    }

    // R05 amendment (Q1 to Q21): 14, "a tuning value to be changed in testing".
    UPROPERTY(Config, EditAnywhere, Category = "Armor", meta = (ClampMin = "0")) int32 armorStackCap = 14;
    // Where each stack position shows on the body (R05 3.2). Visual only; read by Doc R07.
    UPROPERTY(Config, EditAnywhere, Category = "Armor") TSoftObjectPtr<UDA_ArmorStackLayout> armorStackLayout;
};
```

`Private/EquipmentFramework/UEquipmentSettings.cpp`

```cpp
#include "EquipmentFramework/UEquipmentSettings.h"

UEquipmentSettings::UEquipmentSettings(){
    CategoryName = TEXT("Game");
}
```

(`UDA_ArmorStackLayout` is created in Section 2; the forward declaration is enough until then.)

### E0.4 — Services subsystem skeleton (startup line)

`Public/EquipmentFramework/EquipmentServices.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EquipmentServices.generated.h"

UCLASS()
class AWAKEN_API UEquipmentServices : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static UEquipmentServices* get(const UObject* worldContext);
};
```

`Private/EquipmentFramework/EquipmentServices.cpp`

```cpp
#include "EquipmentFramework/EquipmentServices.h"
#include "EquipmentFramework/EquipmentLog.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

void UEquipmentServices::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);
    EQUIPMENT_LOG(Log, this, "Equipment framework started");
}

UEquipmentServices* UEquipmentServices::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    if(gameInstance == nullptr){
        return nullptr;
    }
    return gameInstance->GetSubsystem<UEquipmentServices>();
}
```

### Verify
Clean Editor + Development build, zero warnings. The log shows `LogAwakenEquipment: [...] Equipment framework started` when a game instance starts, and *Project Settings → Game → Awaken Equipment* shows `armorStackCap = 14`.

### Handoff
Folder tree, log helper, settings and the services skeleton exist. Nothing is exported to other frameworks yet.

---

# SECTION 1 — Types, Tags, Delegates and Seams

**Goal:** every shared type in one place, plus the two outgoing seams and their stubs.
**Spec:** R05 §6, §7; Doc02 §2, §20.2; Doc00 §4.2.
**Depends on:** Section 0.

### E1.1 — Structs

`Public/EquipmentFramework/01Structs/S_EquipmentFramework.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "S_EquipmentFramework.generated.h"

class APlayerState;

// ---------- Worn state (R05 §6) ----------

// One worn armor piece. stackIndex is its place in the stack: 0 = first worn, the highest = last stacked (E2).
// itemId is the E3 addition: other players need it to draw the piece.
USTRUCT(BlueprintType)
struct FS_EquippedArmorState{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int32 stackIndex = 0;
    UPROPERTY(BlueprintReadOnly) FGuid instanceId;
    UPROPERTY(BlueprintReadOnly) float remainingDurability = 0.0f;
    UPROPERTY(BlueprintReadOnly) FName itemId;
};

// The outfit. An item TYPE id, not an instance: clothes are stackable and carry no state (R05 §6, Index A87).
// An empty itemId means the default clothes, which are built into the character and are not an item (R01 3.8).
USTRUCT(BlueprintType)
struct FS_WornClothesState{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FName itemId;

    bool isDefault() const{
        return itemId.IsNone();
    }
};

// ---------- Query results (R05 §5) ----------

// getAggregatedArmorStats(): cached, rebuilt only on equip, unequip or a break.
USTRUCT(BlueprintType)
struct FS_AggregatedArmorStats{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float armorValue = 0.0f;
    UPROPERTY(BlueprintReadOnly) float warmth = 0.0f;
    UPROPERTY(BlueprintReadOnly) int32 pieceCount = 0;
};

// getClothesProtection(): never changes with damage.
USTRUCT(BlueprintType)
struct FS_ClothesProtection{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float protection = 0.0f;
    UPROPERTY(BlueprintReadOnly) float warmth = 0.0f;
};

// ---------- Command results ----------

USTRUCT(BlueprintType)
struct FS_ArmorDamageResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float absorbedAmount = 0.0f;
    UPROPERTY(BlueprintReadOnly) int32 brokenPieces = 0;
};

// The reference version of Doc R03 3.7 step 4 for a player target (E5).
USTRUCT(BlueprintType)
struct FS_GearAttackResult{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) float damageToHealth = 0.0f;
    UPROPERTY(BlueprintReadOnly) float absorbedByClothes = 0.0f;
    UPROPERTY(BlueprintReadOnly) float absorbedByArmor = 0.0f;
    UPROPERTY(BlueprintReadOnly) int32 brokenPieces = 0;
};

// ---------- Content layout data (R05 3.2, visual only) ----------

USTRUCT(BlueprintType)
struct FS_ArmorStackPosition{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "Equipment.ArmorStack")) FGameplayTag positionTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName socketName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FTransform offset;
};

// ---------- Save record (R05 §9) ----------

USTRUCT()
struct FS_EquipmentSave{
    GENERATED_BODY()

    UPROPERTY() int32 version = 1;
    UPROPERTY() TArray<FS_EquippedArmorState> armorStack;
    UPROPERTY() FS_WornClothesState clothes;
};

// ---------- Message payloads (R05 §7, Doc02 §11) ----------

// Equipment.ArmorEquipped / ArmorUnequipped / ArmorBroken
USTRUCT(BlueprintType)
struct FS_ArmorChangedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<APlayerState> player = nullptr;
    UPROPERTY(BlueprintReadOnly) int32 stackIndex = 0;
    UPROPERTY(BlueprintReadOnly) FGuid instanceId;
    UPROPERTY(BlueprintReadOnly) FName itemId;
};

// Equipment.ClothesChanged
USTRUCT(BlueprintType)
struct FS_ClothesChangedMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<APlayerState> player = nullptr;
    UPROPERTY(BlueprintReadOnly) FName itemId;
};

// Equipment.StatsRecalculated
USTRUCT(BlueprintType)
struct FS_ArmorStatsMessage{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TObjectPtr<APlayerState> player = nullptr;
    UPROPERTY(BlueprintReadOnly) float aggregatedArmorValue = 0.0f;
};
```

> The message payloads carry the player because a Gameplay Message is heard by every listener on the machine; a widget filters by "is this my PlayerState".

### E1.2 — Tags

`Public/EquipmentFramework/02Tags/T_EquipmentFramework.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

// ---- Messages (R05 §7; all five are in the Tag Registry) ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorEquipped);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorUnequipped);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorBroken);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ClothesChanged);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_StatsRecalculated);

// ---- Stack positions (R05 §6: Equipment.ArmorStack.*, visual only). One leaf per position up to the cap of 14 ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position01);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position02);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position03);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position04);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position05);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position06);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position07);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position08);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position09);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position10);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position11);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position12);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position13);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_ArmorStack_Position14);

// ---- Refusal reasons (Doc02 §6). All five are NEW and PROPOSED: add them to the Tag Registry ----
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_FailReason_StackFull);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_FailReason_NotArmor);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_FailReason_NotClothes);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_FailReason_AlreadyWorn);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Equipment_FailReason_InvalidStackIndex);

// ---- Tag OWNED BY ANOTHER FRAMEWORK, referenced here (see the stub register, E10.8) ----
// TEMPORARY duplicate: remove the definition here when Doc B21's header exists and include that header instead.
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Cosmetic_Event_Combat_ArmorBroken);   // Doc B21 A.4 (Index A89)

namespace EquipmentTags{
    // Query. The position tag of a stack index, or an empty tag past the last defined leaf.
    AWAKEN_API FGameplayTag getStackPositionTag(int32 stackIndex);
}
```

`Private/EquipmentFramework/02Tags/T_EquipmentFramework.cpp`

```cpp
#include "EquipmentFramework/02Tags/T_EquipmentFramework.h"
#include "GameplayTagContainer.h"

UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorEquipped, "Equipment.ArmorEquipped");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorUnequipped, "Equipment.ArmorUnequipped");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorBroken, "Equipment.ArmorBroken");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ClothesChanged, "Equipment.ClothesChanged");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_StatsRecalculated, "Equipment.StatsRecalculated");

UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position01, "Equipment.ArmorStack.Position01");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position02, "Equipment.ArmorStack.Position02");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position03, "Equipment.ArmorStack.Position03");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position04, "Equipment.ArmorStack.Position04");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position05, "Equipment.ArmorStack.Position05");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position06, "Equipment.ArmorStack.Position06");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position07, "Equipment.ArmorStack.Position07");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position08, "Equipment.ArmorStack.Position08");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position09, "Equipment.ArmorStack.Position09");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position10, "Equipment.ArmorStack.Position10");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position11, "Equipment.ArmorStack.Position11");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position12, "Equipment.ArmorStack.Position12");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position13, "Equipment.ArmorStack.Position13");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_ArmorStack_Position14, "Equipment.ArmorStack.Position14");

UE_DEFINE_GAMEPLAY_TAG(T_Equipment_FailReason_StackFull, "Equipment.FailReason.StackFull");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_FailReason_NotArmor, "Equipment.FailReason.NotArmor");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_FailReason_NotClothes, "Equipment.FailReason.NotClothes");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_FailReason_AlreadyWorn, "Equipment.FailReason.AlreadyWorn");
UE_DEFINE_GAMEPLAY_TAG(T_Equipment_FailReason_InvalidStackIndex, "Equipment.FailReason.InvalidStackIndex");

UE_DEFINE_GAMEPLAY_TAG(T_Cosmetic_Event_Combat_ArmorBroken, "Cosmetic.Event.Combat.ArmorBroken");

FGameplayTag EquipmentTags::getStackPositionTag(int32 stackIndex){
    if(stackIndex < 0){
        return FGameplayTag();
    }
    const FName tagName(*FString::Printf(TEXT("Equipment.ArmorStack.Position%02d"), stackIndex + 1));
    return FGameplayTag::RequestGameplayTag(tagName, false);   // false = no error when the leaf does not exist
}
```

> If testing raises the cap above 14, add the matching `Position15…` leaves. Protection does not depend on them (R05 3.2: placement is visual only), so a missing leaf only means that piece has no fixed spot to show at; the data check in Section 2 warns about it.

### E1.3 — Delegate

`Public/EquipmentFramework/04Delegates/D_EquipmentFramework.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "D_EquipmentFramework.generated.h"

class APlayerState;

// Raised on every machine that holds the state, after any change to the armor stack, a piece's wear, or the clothes.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FD_EquipmentChanged, APlayerState*, player);
```

> Same caveat as R01's `D_ItemInventoryFramework.h`: if Unreal Header Tool rejected a header that holds only delegates there, apply here whatever fix you settled on for that file.

### E1.4 — Seam interfaces (outgoing)

Equipment calls two frameworks that belong to teammates. Each is reached only through an interface (Doc00 §4.2), one file per interface in `Public/EquipmentFramework/03Interfaces/`.

`BPI_ArmorStatsSink.h` (owner: Doc R08 Stats — `ApplyArmorAggregate`)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "BPI_ArmorStatsSink.generated.h"

class APlayerState;

UINTERFACE(BlueprintType)
class UBPI_ArmorStatsSink : public UInterface{
    GENERATED_BODY()
};

class IBPI_ArmorStatsSink{
    GENERATED_BODY()

public:
    // Doc R08 §5 ApplyArmorAggregate(TotalArmorValue): writes the AggregatedArmor attribute. Server only.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void applyArmorAggregate(APlayerState* player, float totalArmorValue);
};
```

`BPI_CosmeticEventSink.h` (owner: Doc B21 — `RaiseCosmeticEvent`)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "BPI_CosmeticEventSink.generated.h"

UINTERFACE(BlueprintType)
class UBPI_CosmeticEventSink : public UInterface{
    GENERATED_BODY()
};

class IBPI_CosmeticEventSink{
    GENERATED_BODY()

public:
    // Doc B21: an event tag, a location, an instigator and a magnitude. Raised by the host only.
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void raiseCosmeticEvent(FGameplayTag eventTag, FVector location, AActor* instigator, float magnitude);
};
```

> If the teammate who owns B21 already has a cosmetic interface in the project, do not create this one: use theirs and delete `BPI_CosmeticEventSink.h` and its stub.

### E1.5 — Stub implementations

`Private/EquipmentFramework/Stubs/EquipmentStubs.h`

```cpp
// TEMPORARY STUB — owner: see each class below. Delete each class when the real framework lands.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "EquipmentFramework/03Interfaces/BPI_ArmorStatsSink.h"
#include "EquipmentFramework/03Interfaces/BPI_CosmeticEventSink.h"
#include "EquipmentStubs.generated.h"

// TEMPORARY STUB — owner: Doc R08 Stats. Remembers the last value so a test can read it.
UCLASS()
class UStubArmorStatsSink : public UObject, public IBPI_ArmorStatsSink{
    GENERATED_BODY()

public:
    UPROPERTY() float lastArmorValue = 0.0f;
    UPROPERTY() int32 callCount = 0;

    virtual void applyArmorAggregate_Implementation(APlayerState* player, float totalArmorValue) override;
};

// TEMPORARY STUB — owner: Doc B21 Cosmetic Events. Remembers what was raised; plays nothing.
UCLASS()
class UStubCosmeticEventSink : public UObject, public IBPI_CosmeticEventSink{
    GENERATED_BODY()

public:
    UPROPERTY() TArray<FGameplayTag> raisedEvents;

    virtual void raiseCosmeticEvent_Implementation(FGameplayTag eventTag, FVector location, AActor* instigator, float magnitude) override;
};
```

`Private/EquipmentFramework/Stubs/EquipmentStubs.cpp`

```cpp
// TEMPORARY STUB — owner: Doc R08 / Doc B21. Delete when the real frameworks land.
#include "EquipmentFramework/Stubs/EquipmentStubs.h"

void UStubArmorStatsSink::applyArmorAggregate_Implementation(APlayerState* player, float totalArmorValue){
    lastArmorValue = totalArmorValue;
    callCount++;
}

void UStubCosmeticEventSink::raiseCosmeticEvent_Implementation(FGameplayTag eventTag, FVector location, AActor* instigator, float magnitude){
    raisedEvents.Add(eventTag);
}
```

### E1.6 — Fill `UEquipmentServices`

Replace the Section 0 skeleton. Two design points:

- **One seam registry for the whole project.** Equipment does not keep its own map; it registers its stubs in R01's `UInventoryServices`, which is already a generic "interface class → implementation" registry. A teammate replaces a stub the same way as for R01: `UInventoryServices::get(world)->registerSeam(UBPI_ArmorStatsSink::StaticClass(), myRealStats);`
- **This class will be the real `IBPI_EquipmentGear`** that R01 has been calling through a stub. That part is added in Section 7.

`EquipmentServices.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EquipmentServices.generated.h"

class APlayerState;
class UInventoryServices;

UCLASS()
class AWAKEN_API UEquipmentServices : public UGameInstanceSubsystem{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    static UEquipmentServices* get(const UObject* worldContext);

    // ---- facade (Command) ----
    void applyArmorAggregate(APlayerState* player, float totalArmorValue) const;                                        // Doc R08
    void raiseCosmeticEvent(const FGameplayTag& eventTag, const FVector& location, AActor* instigator, float magnitude) const;   // Doc B21

private:
    UInventoryServices* getRegistry() const;
};
```

`EquipmentServices.cpp`

```cpp
#include "EquipmentFramework/EquipmentServices.h"
#include "EquipmentFramework/EquipmentLog.h"
#include "EquipmentFramework/03Interfaces/BPI_ArmorStatsSink.h"
#include "EquipmentFramework/03Interfaces/BPI_CosmeticEventSink.h"
#include "EquipmentFramework/Stubs/EquipmentStubs.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"

void UEquipmentServices::Initialize(FSubsystemCollectionBase& Collection){
    Super::Initialize(Collection);
    // R01's services must exist first: its Initialize registers its own stubs, and Section 7 replaces one of them.
    Collection.InitializeDependency<UInventoryServices>();

    UInventoryServices* registry = getRegistry();
    if(registry == nullptr){
        EQUIPMENT_LOG(Error, this, "Equipment framework: UInventoryServices is missing");
        return;
    }
    // Default each outgoing seam to its stub, unless a real framework registered first.
    if(registry->getSeam(UBPI_ArmorStatsSink::StaticClass()) == nullptr){
        registry->registerSeam(UBPI_ArmorStatsSink::StaticClass(), NewObject<UStubArmorStatsSink>(this));
    }
    if(registry->getSeam(UBPI_CosmeticEventSink::StaticClass()) == nullptr){
        registry->registerSeam(UBPI_CosmeticEventSink::StaticClass(), NewObject<UStubCosmeticEventSink>(this));
    }
    EQUIPMENT_LOG(Log, this, "Equipment framework started");
}

UEquipmentServices* UEquipmentServices::get(const UObject* worldContext){
    UGameInstance* gameInstance = UGameplayStatics::GetGameInstance(worldContext);
    if(gameInstance == nullptr){
        return nullptr;
    }
    return gameInstance->GetSubsystem<UEquipmentServices>();
}

UInventoryServices* UEquipmentServices::getRegistry() const{
    const UGameInstance* gameInstance = GetGameInstance();
    return gameInstance != nullptr ? gameInstance->GetSubsystem<UInventoryServices>() : nullptr;
}

void UEquipmentServices::applyArmorAggregate(APlayerState* player, float totalArmorValue) const{
    const UInventoryServices* registry = getRegistry();
    UObject* sink = registry != nullptr ? registry->getSeam(UBPI_ArmorStatsSink::StaticClass()) : nullptr;
    if(sink != nullptr){
        IBPI_ArmorStatsSink::Execute_applyArmorAggregate(sink, player, totalArmorValue);
    }
}

void UEquipmentServices::raiseCosmeticEvent(const FGameplayTag& eventTag, const FVector& location, AActor* instigator, float magnitude) const{
    const UInventoryServices* registry = getRegistry();
    UObject* sink = registry != nullptr ? registry->getSeam(UBPI_CosmeticEventSink::StaticClass()) : nullptr;
    if(sink != nullptr){
        IBPI_CosmeticEventSink::Execute_raiseCosmeticEvent(sink, eventTag, location, instigator, magnitude);
    }
}
```

### Verify (Section 1)

1. **Compile** Editor + Development. Zero warnings.
2. **Tags:** *Project Settings → Gameplay Tags*, search `Equipment.` — the five messages, fourteen positions and five fail reasons show with *Native* source.
3. **Seams:** put a breakpoint (or a temporary log) after `Initialize`: `getSeam(UBPI_ArmorStatsSink::StaticClass())` is not null.
4. **Default-value test (tier 1):** a fresh `FS_WornClothesState` answers `isDefault() == true`; `EquipmentTags::getStackPositionTag(0)` is `Equipment.ArmorStack.Position01`, index 13 is `Position14`, index 14 is an empty tag.

### Handoff
- Every type, tag and seam interface exists.
- Tell the R08 owner: implement `IBPI_ArmorStatsSink` and register it. Tell the B21 owner: implement `IBPI_CosmeticEventSink` (or give us theirs) and take over the tag `Cosmetic.Event.Combat.ArmorBroken`.

---

# SECTION 2 — Item Data for Armor and Clothes

**Goal:** an armor piece or an outfit is a data asset, never code.
**Spec:** R05 §0.2, §1, 3.2, 3.5, 3.6; R01 §3.1 (the Equipment block); Doc R07 3.12.
**Depends on:** Section 1.

### Files

```
Public/ItemInventoryFramework/UDA_ItemDefinition.h      (patch — you own R01)
Private/ItemInventoryFramework/UDA_ItemDefinition.cpp   (patch)
Public/EquipmentFramework/UDA_ArmorStackLayout.h
Private/EquipmentFramework/UDA_ArmorStackLayout.cpp
Private/ItemInventoryFramework/Tests/InventoryTestFixture.h/.cpp   (extend)
```

### E2.1 — Fill the Equipment block of `UDA_ItemDefinition` (E8)

The R01 guide left this line in the class: `// Equipment block: Doc R05 fills this in later. Nothing here on purpose.` Replace it with:

```cpp
    // ---------- Equipment block (Doc R05). Names PROPOSED, every value data ----------
    // Armor: what one worn piece adds to the aggregated armor value (R05 §1 "ArmorValue").
    // How much damage the piece can absorb before it disappears is durabilityMax (R05 3.2; E4).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (ClampMin = "0")) float armorValue = 0.0f;
    // Clothes: taken off an attack before the armor is asked (R05 3.6; E5). Never wears out.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (ClampMin = "0")) float clothesProtection = 0.0f;
    // Armor and clothes: lowers the cold-exposure Illness source while worn (R05 3.5; Doc B14 3.6). The size is data.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (ClampMin = "0")) float warmth = 0.0f;
    // Armor: the mesh shown at the piece's stack position. Clothes: the outfit mesh. Doc R07 3.12 owns how they are shown.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment") TSoftObjectPtr<UStaticMesh> armorMesh;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment") TSoftObjectPtr<USkeletalMesh> outfitMesh;
```

Add `class USkeletalMesh;` to the forward declarations at the top of the header, and two helpers next to `isWeapon()`:

```cpp
    bool isArmor() const{
        return categoryTag == T_Item_Category_Armor;
    }

    bool isClothing() const{
        return categoryTag == T_Item_Category_Clothing;
    }
```

### E2.2 — Data validation

In `UDA_ItemDefinition::IsDataValid`, before `return result;`:

```cpp
    if(isArmor()){
        if(itemKind != E_ItemKind::Instanced){
            fail(TEXT("Armor must be Instanced: each piece has its own durability (R01 3.2)."));
        }
        if(durabilityMax <= 0.0f){
            fail(TEXT("Armor needs durabilityMax above zero: it is how much damage the piece can absorb (R05 3.2). The unbreakable sentinel is for the starter Axe only."));
        }
        if(armorValue <= 0.0f && warmth <= 0.0f){
            Context.AddWarning(FText::FromString(TEXT("Armor has no armorValue and no warmth, so wearing it does nothing.")));
        }
    }
    if(isClothing()){
        if(itemKind != E_ItemKind::Stackable){
            fail(TEXT("Clothes must be Stackable: they carry no state (R05 3.6, Index A87)."));
        }
        if(durabilityMax != 0.0f){
            fail(TEXT("Clothes never wear out (R05 3.6): durabilityMax must be 0."));
        }
    }
```

### E2.3 — The stack layout asset (R05 3.2: "each position has a predefined place on the body")

`Public/EquipmentFramework/UDA_ArmorStackLayout.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "EquipmentFramework/01Structs/S_EquipmentFramework.h"
#include "UDA_ArmorStackLayout.generated.h"

// Where each stack position shows on the body. Visual only: it never changes protection (R05 3.2).
UCLASS(BlueprintType)
class AWAKEN_API UDA_ArmorStackLayout : public UDataAsset{
    GENERATED_BODY()

public:
    // Entry 0 is where the first piece worn shows, entry 1 the second, and so on.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Layout") TArray<FS_ArmorStackPosition> positions;

    // Query. Null when the layout has no entry for that index.
    const FS_ArmorStackPosition* findPosition(int32 stackIndex) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
```

`Private/EquipmentFramework/UDA_ArmorStackLayout.cpp`

```cpp
#include "EquipmentFramework/UDA_ArmorStackLayout.h"
#include "EquipmentFramework/UEquipmentSettings.h"
#include "EquipmentFramework/02Tags/T_EquipmentFramework.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const FS_ArmorStackPosition* UDA_ArmorStackLayout::findPosition(int32 stackIndex) const{
    return positions.IsValidIndex(stackIndex) ? &positions[stackIndex] : nullptr;
}

#if WITH_EDITOR
EDataValidationResult UDA_ArmorStackLayout::IsDataValid(FDataValidationContext& Context) const{
    EDataValidationResult result = Super::IsDataValid(Context);
    const int32 cap = UEquipmentSettings::get()->armorStackCap;
    if(positions.Num() < cap){
        Context.AddWarning(FText::FromString(FString::Printf(TEXT("The layout has %d positions but armorStackCap is %d. Pieces past the last position have no place to show."), positions.Num(), cap)));
    }
    for(int32 index = 0; index < positions.Num(); index++){
        if(positions[index].positionTag != EquipmentTags::getStackPositionTag(index)){
            Context.AddError(FText::FromString(FString::Printf(TEXT("Entry %d must use the tag Equipment.ArmorStack.Position%02d."), index, index + 1)));
            result = EDataValidationResult::Invalid;
        }
        if(positions[index].socketName.IsNone()){
            Context.AddError(FText::FromString(FString::Printf(TEXT("Entry %d has no socket name."), index)));
            result = EDataValidationResult::Invalid;
        }
    }
    return result;
}
#endif
```

### E2.4 — Author the starter content (Blueprint / editor)

Create these `DA_ItemDefinition` assets in `Content/Awaken/Items/` (the numbers are placeholders for testing, not design):

| Asset | Kind | Category | Fields |
| --- | --- | --- | --- |
| `IronVest` (exists from R01 S2.14) | Instanced | Armor | `durabilityMax = 100`, `armorValue = 10`, `perTypeLimit = 14` |
| `LeatherPad` | Instanced | Armor | `durabilityMax = 30`, `armorValue = 4`, `perTypeLimit = 14` |
| `FurMantle` | Instanced | Armor | `durabilityMax = 40`, `armorValue = 2`, `warmth = 3`, `perTypeLimit = 14` |
| `SoldierVest` | Stackable | Clothing | `clothesProtection = 5`, `perTypeLimit = 3` |
| `BeachOutfit` | Stackable | Clothing | `clothesProtection = 0`, `perTypeLimit = 3` |
| `WoolClothes` | Stackable | Clothing | `clothesProtection = 1`, `warmth = 4`, `perTypeLimit = 3` |

Then:

1. Create one `DA_ArmorStackLayout` in `Content/Awaken/Equipment/` with 14 entries (tags `Position01`…`Position14`). The socket names are whatever the R07 owner puts on the character skeleton; until then use placeholder names such as `armor_01`.
2. Point *Project Settings → Game → Awaken Equipment → armorStackLayout* at it.
3. All six items map to the **Others** page of the bag automatically (R01 3.1a: armor and clothing fall through to Others). Give each an `inventorySlot` on that page.

### E2.5 — Extend the test fixture

`InventoryTestFixture.h`, inside the struct:

```cpp
    UDA_ItemDefinition* addArmor(FName itemId, int32 limit, float armorValue, float durability, float warmth = 0.0f);
    UDA_ItemDefinition* addClothes(FName itemId, int32 limit, float protection, float warmth = 0.0f);
```

`InventoryTestFixture.cpp`:

```cpp
UDA_ItemDefinition* FInventoryTestFixture::addArmor(FName itemId, int32 limit, float armorValue, float durability, float warmth){
    UDA_ItemDefinition* definition = addInstanced(itemId, limit, durability);
    definition->categoryTag = T_Item_Category_Armor;
    definition->armorValue = armorValue;
    definition->warmth = warmth;
    return definition;
}

UDA_ItemDefinition* FInventoryTestFixture::addClothes(FName itemId, int32 limit, float protection, float warmth){
    UDA_ItemDefinition* definition = addStackable(itemId, limit);
    definition->categoryTag = T_Item_Category_Clothing;
    definition->clothesProtection = protection;
    definition->warmth = warmth;
    return definition;
}
```

### Verify (Section 2)

1. Compile. Open `IronVest`: the **Equipment** category shows the five new fields.
2. Run `Awaken.Item.Validate` (R01 S13): zero errors on the six assets.
3. Break one on purpose: set `SoldierVest` to Instanced → the validator reports "Clothes must be Stackable". Set `IronVest.durabilityMax = 0` → "Armor needs durabilityMax above zero". Undo both.
4. Validate the layout asset with 13 entries → the warning about the cap appears; with a wrong tag on entry 3 → an error.

### Handoff
- **R07** reads `armorMesh`, `outfitMesh` and `DA_ArmorStackLayout`. Confirm with the R07 owner that a static mesh on a socket is how an armor piece is shown and that an outfit is one skeletal mesh (R07 3.12 says one outfit per cosmetic mesh).
- New data fields to log in the Index: `armorValue`, `clothesProtection`, `warmth`, `armorMesh`, `outfitMesh`.

---

# SECTION 3 — The Equipment Component (state, replication, queries)

**Goal:** the worn state exists on the PlayerState, replicates to everyone, and answers the two queries from a cache.
**Spec:** R05 §2, §5 (queries), §7, §8; Doc B19 3.2; Doc02 §8, §19.2.
**Depends on:** Sections 0–2.

### Files

```
Public/EquipmentFramework/UEquipmentComponent.h
Private/EquipmentFramework/UEquipmentComponent.cpp
Private/ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h/.cpp   (patch)
```

### Design rules this section enforces

- **The component lives on the PlayerState** (R05 §2, Doc B19 3.2, confirmed 2026-10-18), so worn clothes and armor survive the death of the pawn. The Character only *shows* them.
- **Server only mutates.** Every command begins with the server check. A client holds a replicated copy and nothing else.
- **Replicated to everyone** (`COND_None`, R05 §8 and Doc02 §8): other co-op players must see what a player wears. This is the opposite of the bag, which is owner-only.
- **Messages are local** (Doc02 §19.2). The host raises a message right after it changes the state; a client raises the same message when the replicated state arrives. A late joiner gets the state from replication, never from a message.
- **The aggregate is a cache** (R05 §5): rebuilt only when a piece is added, removed or broken, or the clothes change. Never per frame, and no tick anywhere in this framework.

### E3.1 to E3.6 — `UEquipmentComponent`

`UEquipmentComponent.h` (Section 3 version; later sections add members as marked)

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "EquipmentFramework/01Structs/S_EquipmentFramework.h"
#include "EquipmentFramework/04Delegates/D_EquipmentFramework.h"
#include "ItemInventoryFramework/01Structs/S_ItemInventoryFramework.h"
#include "UEquipmentComponent.generated.h"

class APlayerState;

// R05 §2: the armor stack and the worn clothes of one player. A PlayerState component (Doc B19 3.2).
UCLASS(ClassGroup = (Awaken), meta = (BlueprintSpawnableComponent))
class AWAKEN_API UEquipmentComponent : public UActorComponent{
    GENERATED_BODY()

public:
    UEquipmentComponent();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Query. Null when the component is missing.
    static UEquipmentComponent* findFor(const APlayerState* player);

    // ---- queries (R05 §5) ----
    const TArray<FS_EquippedArmorState>& getArmorStack() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") TArray<FS_EquippedArmorState> getArmorStackCopy() const;
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") FS_WornClothesState getWornClothes() const;
    // Cached: rebuilt only on equip, unequip or a break. Never per frame.
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") FS_AggregatedArmorStats getAggregatedArmorStats() const;
    // Protection and warmth of the worn clothes. Never changes with damage.
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") FS_ClothesProtection getClothesProtection() const;
    // Armor warmth + clothes warmth. For the cold-exposure Illness source (R05 3.5; Doc B14 3.6, Doc B03).
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") float getTotalWarmth() const;
    // The damage the whole stack can still absorb. For the HUD's wear display and for the damage step.
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") float getTotalRemainingDurability() const;
    // The cap for NEW equips (R05 3.2). A stack that already holds more keeps its pieces.
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") int32 getArmorStackCap() const;

    // Raised on every machine after any change (stack, wear, clothes).
    UPROPERTY(BlueprintAssignable, Category = "Awaken|Equipment") FD_EquipmentChanged onEquipmentChanged;

protected:
    // Replicated to everyone (R05 §8, Doc02 §8).
    UPROPERTY(ReplicatedUsing = OnRep_armorStack) TArray<FS_EquippedArmorState> armorStack;
    UPROPERTY(ReplicatedUsing = OnRep_wornClothes) FS_WornClothesState wornClothes;

    UFUNCTION()
    void OnRep_armorStack(const TArray<FS_EquippedArmorState>& previousStack);
    UFUNCTION()
    void OnRep_wornClothes(const FS_WornClothesState& previousClothes);

    bool isServerSide() const;
    APlayerState* getOwnerPlayerState() const;

    // Rebuilds both caches from the item definitions. Runs on every machine.
    void recalculate();
    // Server. Sends the aggregate to Stats (R08) and raises Equipment.StatsRecalculated (server only).
    void pushStats() const;
    // After a change to WHICH pieces or clothes are worn: recalculate, push, replicate now, notify.
    void finishChange();
    // After a change to wear only: replicate now, notify. The aggregate is untouched.
    void finishWearChange();
    void renumberStack();

    void raiseArmorMessage(const FGameplayTag& channel, const FS_EquippedArmorState& piece) const;
    void raiseClothesMessage() const;

    // The bag entry a worn piece turns back into (same instance id, current durability).
    static FS_InventoryEntry makeEntry(const FS_EquippedArmorState& piece);
    static FS_InventoryResult refusal(const FGameplayTag& reason);
    static FS_InventoryResult accepted();

    // Not replicated: every machine rebuilds them from the replicated state.
    FS_AggregatedArmorStats cachedArmorStats;
    FS_ClothesProtection cachedClothes;
};
```

> Commands return R01's `FS_InventoryResult`: success means the `failReason` tag is empty. Equipment does not invent a second result type.

`UEquipmentComponent.cpp`

```cpp
#include "EquipmentFramework/UEquipmentComponent.h"
#include "EquipmentFramework/EquipmentServices.h"
#include "EquipmentFramework/UEquipmentSettings.h"
#include "EquipmentFramework/EquipmentLog.h"
#include "EquipmentFramework/02Tags/T_EquipmentFramework.h"
#include "ItemInventoryFramework/UItemCatalogSubsystem.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "HAL/IConsoleManager.h"
#include "Net/UnrealNetwork.h"

#if !UE_BUILD_SHIPPING
// Tests only: -1 = use the project setting. Lets a test lower the cap while pieces are worn (R05 3.2, S2-F22).
static TAutoConsoleVariable<int32> cvarStackCap(TEXT("Awaken.Equipment.StackCap"), -1, TEXT("Override armorStackCap (tests only). -1 = use the project setting."), ECVF_Cheat);
#endif

// ---------- construction and replication ----------

UEquipmentComponent::UEquipmentComponent(){
    SetIsReplicatedByDefault(true);
}

void UEquipmentComponent::BeginPlay(){
    Super::BeginPlay();
    recalculate();
    if(isServerSide()){
        pushStats();   // a fresh player reports an aggregate of 0
    }
}

void UEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UEquipmentComponent, armorStack);     // COND_None: everyone sees worn armor (R05 §8)
    DOREPLIFETIME(UEquipmentComponent, wornClothes);
}

UEquipmentComponent* UEquipmentComponent::findFor(const APlayerState* player){
    return player != nullptr ? player->FindComponentByClass<UEquipmentComponent>() : nullptr;
}

bool UEquipmentComponent::isServerSide() const{
    return GetOwner() != nullptr && GetOwner()->HasAuthority();
}

APlayerState* UEquipmentComponent::getOwnerPlayerState() const{
    return Cast<APlayerState>(GetOwner());
}

// ---------- queries ----------

const TArray<FS_EquippedArmorState>& UEquipmentComponent::getArmorStack() const{
    return armorStack;
}

TArray<FS_EquippedArmorState> UEquipmentComponent::getArmorStackCopy() const{
    return armorStack;
}

FS_WornClothesState UEquipmentComponent::getWornClothes() const{
    return wornClothes;
}

FS_AggregatedArmorStats UEquipmentComponent::getAggregatedArmorStats() const{
    return cachedArmorStats;
}

FS_ClothesProtection UEquipmentComponent::getClothesProtection() const{
    return cachedClothes;
}

float UEquipmentComponent::getTotalWarmth() const{
    return cachedArmorStats.warmth + cachedClothes.warmth;
}

float UEquipmentComponent::getTotalRemainingDurability() const{
    float total = 0.0f;
    for(const FS_EquippedArmorState& piece : armorStack){
        total += piece.remainingDurability;
    }
    return total;
}

int32 UEquipmentComponent::getArmorStackCap() const{
#if !UE_BUILD_SHIPPING
    const int32 overrideCap = cvarStackCap.GetValueOnGameThread();
    if(overrideCap >= 0){
        return overrideCap;
    }
#endif
    return UEquipmentSettings::get()->armorStackCap;
}

// ---------- the cache ----------

void UEquipmentComponent::recalculate(){
    cachedArmorStats = FS_AggregatedArmorStats();
    cachedClothes = FS_ClothesProtection();
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    if(catalog == nullptr){
        return;
    }
    // The stack protects the player as a whole: the values simply add up (R05 3.2). Position plays no part.
    for(const FS_EquippedArmorState& piece : armorStack){
        const UDA_ItemDefinition* definition = catalog->getDefinition(piece.itemId);
        if(definition != nullptr){
            cachedArmorStats.armorValue += definition->armorValue;
            cachedArmorStats.warmth += definition->warmth;
        }
        cachedArmorStats.pieceCount++;
    }
    if(!wornClothes.isDefault()){
        const UDA_ItemDefinition* clothes = catalog->getDefinition(wornClothes.itemId);
        if(clothes != nullptr){
            cachedClothes.protection = clothes->clothesProtection;
            cachedClothes.warmth = clothes->warmth;
        }
    }
}

void UEquipmentComponent::pushStats() const{
    APlayerState* player = getOwnerPlayerState();
    const UEquipmentServices* services = UEquipmentServices::get(this);
    if(services != nullptr){
        services->applyArmorAggregate(player, cachedArmorStats.armorValue);   // Doc R08 ApplyArmorAggregate
    }
    FS_ArmorStatsMessage message;
    message.player = player;
    message.aggregatedArmorValue = cachedArmorStats.armorValue;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Equipment_StatsRecalculated, message);   // server only (R05 §7)
}

void UEquipmentComponent::finishChange(){
    recalculate();
    if(isServerSide()){
        pushStats();
        GetOwner()->ForceNetUpdate();   // a PlayerState replicates slowly by default; worn gear should not lag a second behind
    }
    onEquipmentChanged.Broadcast(getOwnerPlayerState());
}

void UEquipmentComponent::finishWearChange(){
    if(isServerSide()){
        GetOwner()->ForceNetUpdate();
    }
    onEquipmentChanged.Broadcast(getOwnerPlayerState());
}

void UEquipmentComponent::renumberStack(){
    for(int32 index = 0; index < armorStack.Num(); index++){
        armorStack[index].stackIndex = index;   // E2: the stack closes up
    }
}

// ---------- messages (R05 §7, Doc02 §19.2: local on every machine) ----------

void UEquipmentComponent::raiseArmorMessage(const FGameplayTag& channel, const FS_EquippedArmorState& piece) const{
    FS_ArmorChangedMessage message;
    message.player = getOwnerPlayerState();
    message.stackIndex = piece.stackIndex;
    message.instanceId = piece.instanceId;
    message.itemId = piece.itemId;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(channel, message);
}

void UEquipmentComponent::raiseClothesMessage() const{
    FS_ClothesChangedMessage message;
    message.player = getOwnerPlayerState();
    message.itemId = wornClothes.itemId;
    UGameplayMessageSubsystem::Get(this).BroadcastMessage(T_Equipment_ClothesChanged, message);
}

// A client learns of a change only here. The previous value lets it tell what was added and what left.
void UEquipmentComponent::OnRep_armorStack(const TArray<FS_EquippedArmorState>& previousStack){
    recalculate();
    for(const FS_EquippedArmorState& before : previousStack){
        const bool stillWorn = armorStack.ContainsByPredicate([&before](const FS_EquippedArmorState& piece){
            return piece.instanceId == before.instanceId;
        });
        if(!stillWorn){
            raiseArmorMessage(T_Equipment_ArmorUnequipped, before);   // unequipped, released or broken: the mesh goes
        }
    }
    for(const FS_EquippedArmorState& piece : armorStack){
        const bool wasWorn = previousStack.ContainsByPredicate([&piece](const FS_EquippedArmorState& before){
            return before.instanceId == piece.instanceId;
        });
        if(!wasWorn){
            raiseArmorMessage(T_Equipment_ArmorEquipped, piece);
        }
    }
    onEquipmentChanged.Broadcast(getOwnerPlayerState());   // also covers a wear-only change
}

void UEquipmentComponent::OnRep_wornClothes(const FS_WornClothesState& previousClothes){
    recalculate();
    if(previousClothes.itemId != wornClothes.itemId){
        raiseClothesMessage();
    }
    onEquipmentChanged.Broadcast(getOwnerPlayerState());
}

// ---------- small helpers ----------

FS_InventoryEntry UEquipmentComponent::makeEntry(const FS_EquippedArmorState& piece){
    FS_InventoryEntry entry;
    entry.itemId = piece.itemId;
    entry.instanceId = piece.instanceId;
    entry.totalQuantity = 1;
    entry.state.durability = piece.remainingDurability;
    return entry;
}

FS_InventoryResult UEquipmentComponent::refusal(const FGameplayTag& reason){
    FS_InventoryResult result;
    result.failReason = reason;
    return result;
}

FS_InventoryResult UEquipmentComponent::accepted(){
    FS_InventoryResult result;
    result.quantity = 1;
    return result;
}
```

> **Why `OnRep_armorStack` takes the previous value.** Unreal passes the old array when the notify function has one parameter of the property's type. Comparing old and new by `instanceId` is what turns one replicated array into the two local messages R05 §7 asks for. A late joiner arrives with an empty "previous", so it raises `ArmorEquipped` for every piece and the visuals build themselves; no message was ever sent to it.

### E3.7 — Put the component on the PlayerState

In the R01 stub `AInventoryTestPlayerState` (owner: Doc B19; the real PlayerState gets the same line):

```cpp
// header
class UEquipmentComponent;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UEquipmentComponent> equipmentComponent;

// constructor (.cpp), with #include "EquipmentFramework/UEquipmentComponent.h"
    equipmentComponent = CreateDefaultSubobject<UEquipmentComponent>(TEXT("Equipment"));
```

### E3.8 — Blueprint exposure (BP)

The five queries are `BlueprintPure` and `onEquipmentChanged` is `BlueprintAssignable`. In a test Blueprint: *Get Player State → Get Component by Class (EquipmentComponent) → bind `onEquipmentChanged`* and print `getArmorStackCopy` length.

### Verify (Section 3)

1. Compile. Play standalone with `AInventoryTestGameMode`: no errors; the stats stub received one call with `0` (breakpoint in `UStubArmorStatsSink::applyArmorAggregate_Implementation`, or wait for the dev commands of Section 10).
2. `getArmorStackCap()` returns 14; after `Awaken.Equipment.StackCap 3` in the console it returns 3; after `-1` it returns 14 again.
3. Nothing can be equipped yet; that is Section 4.

### Handoff
- `UEquipmentComponent::findFor(playerState)` is how every other framework reaches worn state.
- **B19** owner: add `UEquipmentComponent` to the real PlayerState class.

---

# SECTION 4 — Equip and Unequip Armor

**Goal:** an armor piece moves between the bag and the stack, never in both, never lost.
**Spec:** R05 3.2, 3.3, §5 (`EquipArmor`, `UnequipArmor`); R01 3.8 (Bagless); Doc02 §6.
**Depends on:** Section 3.

### Design rules

- **Validate everything, then mutate once** (Doc02 §6). A refusal changes nothing and names its reason with a tag, never free text (R05 §5: "reason keys go through Doc B18, Doc R10 and the Tag Registry").
- **An equipped piece leaves the bag** (R05 3.3). It is removed from the Personal Inventory and held in the stack, so it is never counted twice. Its instance id and its durability travel with it.
- **A Bagless player cannot equip** (R05 3.3; R01 3.8). No bag, no armor.
- **The cap applies to new equips only** (R05 3.2, S2-F22). Nothing is ever taken off because the cap was lowered.

### E4.1, E4.2 — The two commands

Header additions (`UEquipmentComponent.h`, public):

```cpp
    // ---- commands (server only; each returns an explained result) ----
    // R05 §5 EquipArmor: bag -> next free position of the stack. Refused at the cap (S2-F22).
    FS_InventoryResult equipArmor(const FGuid& instanceId);
    // R05 §5 UnequipArmor: stack -> bag. Refused when the bag is at its limit, so nothing is lost (S2-F22).
    FS_InventoryResult unequipArmor(int32 stackIndex);
```

Implementation (append to `UEquipmentComponent.cpp`; add the two includes at the top):

```cpp
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
```

```cpp
// ---------- Section 4: armor commands ----------

FS_InventoryResult UEquipmentComponent::equipArmor(const FGuid& instanceId){
    if(!isServerSide()){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    APlayerState* player = getOwnerPlayerState();
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    if(inventory == nullptr || catalog == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    if(!inventory->hasBag()){
        return refusal(T_Inventory_FailReason_NoBag);   // a Bagless player cannot equip armor (R05 3.3, R01 3.8)
    }
    UInventoryData* bag = inventory->getPersonalInventory();
    const FS_InventoryEntry* entry = bag->findInstance(instanceId);
    if(entry == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);
    }
    const UDA_ItemDefinition* definition = catalog->getDefinition(entry->itemId);
    if(definition == nullptr || !definition->isArmor()){
        return refusal(T_Equipment_FailReason_NotArmor);
    }
    if(entry->reservedQuantity > 0){
        return refusal(T_Inventory_FailReason_ReservedQuantity);   // it is staged for a craft (R01 3.6)
    }
    if(armorStack.Num() >= getArmorStackCap()){
        return refusal(T_Equipment_FailReason_StackFull);   // refused with a short reason (R05 3.2)
    }

    // Everything checked. Copy what we need first: removing the entry invalidates the pointer.
    FS_EquippedArmorState piece;
    piece.stackIndex = armorStack.Num();   // the next free position
    piece.instanceId = entry->instanceId;
    piece.itemId = entry->itemId;
    piece.remainingDurability = entry->state.durability;

    const FS_InventoryResult removed = bag->removeInstance(instanceId);
    if(!removed.succeeded()){
        return removed;
    }
    armorStack.Add(piece);
    finishChange();
    raiseArmorMessage(T_Equipment_ArmorEquipped, piece);
    EQUIPMENT_LOG(Log, this, "Equipped %s at position %d (durability %.1f), aggregate armor %.1f", *piece.itemId.ToString(), piece.stackIndex, piece.remainingDurability, cachedArmorStats.armorValue);
    return accepted();
}

FS_InventoryResult UEquipmentComponent::unequipArmor(int32 stackIndex){
    if(!isServerSide()){
        return refusal(T_Inventory_FailReason_NotServer);
    }
    if(!armorStack.IsValidIndex(stackIndex)){
        return refusal(T_Equipment_FailReason_InvalidStackIndex);
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getOwnerPlayerState());
    if(inventory == nullptr || !inventory->hasBag()){
        return refusal(T_Inventory_FailReason_NoBag);
    }
    UInventoryData* bag = inventory->getPersonalInventory();
    const FS_EquippedArmorState piece = armorStack[stackIndex];
    FS_ItemStack single;
    single.itemId = piece.itemId;
    single.quantity = 1;
    if(bag->canFit(single) < 1){
        return refusal(T_Interaction_FailReason_BagFull);   // refused, the piece stays worn: nothing is lost (R05 §5)
    }

    const FS_InventoryResult added = bag->addInstance(makeEntry(piece));
    if(!added.succeeded()){
        return added;
    }
    armorStack.RemoveAt(stackIndex);
    renumberStack();
    finishChange();
    raiseArmorMessage(T_Equipment_ArmorUnequipped, piece);
    EQUIPMENT_LOG(Log, this, "Unequipped %s from position %d, aggregate armor %.1f", *piece.itemId.ToString(), stackIndex, cachedArmorStats.armorValue);
    return accepted();
}
```

> **Who checks Downed, Restrained and carrying?** Not these functions. Like R01's library functions they are the *rule*; the *request* layer of Section 9 checks the player's state and the rate limit before it calls them. A dev command or another server system may call them directly.

### Verify (Section 4)

Until Section 10 adds the commands, use a small test or a temporary `BeginPlay` snippet on the host.

| Test | Expect |
| --- | --- |
| Give one `IronVest`, `equipArmor(instanceId)` | accepted; the bag holds **0** vests, the stack holds 1 at `stackIndex 0` with durability 100; aggregate 10; the stats stub got 10 |
| Equip a second `IronVest` and a `LeatherPad` | stack indices 0, 1, 2; aggregate 24 |
| `unequipArmor(1)` | the piece returns to the bag with the **same instance id and durability**; the `LeatherPad` moves from index 2 to **1** (E2); aggregate 14 |
| Equip an item that is a Spear | `Equipment.FailReason.NotArmor`; bag and stack unchanged |
| Equip the same instance id twice | second call `Inventory.FailReason.NotFound` (it is no longer in the bag) |
| `Awaken.Equipment.StackCap 1`, wearing 2, equip a third | `Equipment.FailReason.StackFull`; **both worn pieces stay**; the third is still in the bag |
| Bag at the per-type limit for vests, `unequipArmor` of a vest | `Interaction.FailReason.BagFull`; the piece stays worn |
| Player has no bag (`Awaken.Bag.SetBagless`) and `equipArmor` | `Inventory.FailReason.NoBag` |
| Any command on a client | `Inventory.FailReason.NotServer`; nothing changes |

### Handoff
- `equipArmor` and `unequipArmor` are the only ways a piece enters or leaves the stack by the player's choice. Sections 5 and 7 add the two other exits: breaking, and the bag being detached.

---

# SECTION 5 — Armor Takes Damage

**Goal:** worn armor wears down as it absorbs attack damage, top piece first, and a used-up piece disappears.
**Spec:** R05 3.2, §5 (`ApplyDamageToArmor`), §7 (`ArmorBroken`), amendments Q1–Q21 and A92; Doc R03 3.7 step 4, 3.9; Doc B21 A.4.
**Depends on:** Section 4.

### The rules, in order

1. **Attack damage only** (R05 A92, R03 3.9). Melee, Projectile and Explosion reach armor. Hunger, thirst, illness, poison, drowning, burning, falling and debris never do, and never wear it. *That decision is Combat's*: Doc R03 calls `applyDamageToArmor` only at step 4 and only for attack damage. Equipment does not look at the damage type.
2. **The last piece stacked absorbs first** (R05 amendment). With E2 that is the highest `stackIndex`.
3. **A piece absorbs until it has nothing left, then disappears** (R05 3.2). What it could not absorb passes to the piece under it.
4. **A broken piece is destroyed.** It does not return to the bag.
5. **Clothes come first and never wear out** (R05 3.6 and amendment: "clothes protection reduces the damage the armor receives").

### The contract with Doc R03 (step 4 of its damage order)

R03 owns the arithmetic of a hit. What it needs from Equipment is two numbers and one command:

| R03 needs | From |
| --- | --- |
| The target's clothes protection | `getClothesProtection().protection` |
| The target's armor value | the `AggregatedArmor` attribute of Doc R08 (Equipment pushes it there), or `getAggregatedArmorStats().armorValue` |
| To wear the armor down | `applyDamageToArmor(amountTheArmorAbsorbed)`, which returns how much was really absorbed |

Decision **E5** fixes how the three combine. Worked example: a hit of **30** after Block, weapon `ArmorPenetrationPercent` **0.5**, target wears `SoldierVest` (clothes 5) and one `IronVest` (armor 10, durability 100):

```
after clothes   = max(0, 30 − 5)            = 25
armor reduction = min(25, 10 × (1 − 0.5))   = 5      ← R03 3.7: armor × (1 − penetration)
absorbed        = applyDamageToArmor(5)     = 5      ← the vest goes from 100 to 95
to health       = 25 − 5                    = 20     ← then R03 applies MinimumAttackDamage and its steps 5 to 9
```

If the stack has less durability left than the reduction, the armor absorbs only what it has, and the rest goes to health. `UBFL_Equipment::applyAttackToGear` below is this example as code. It is a **reference** so you can test and so the R03 owner has an exact contract; R03 may call it for a player target or do the same arithmetic itself.

### E5.1 — `applyDamageToArmor`

Header addition (public):

```cpp
    // R05 §5 ApplyDamageToArmor. Server only. Called by Doc R03 at step 4, for attack damage only.
    // damageAmount is what the armor is asked to absorb. Returns what it really absorbed and how many pieces broke.
    FS_ArmorDamageResult applyDamageToArmor(float damageAmount);
```

Implementation:

```cpp
// ---------- Section 5: armor wear ----------

FS_ArmorDamageResult UEquipmentComponent::applyDamageToArmor(float damageAmount){
    FS_ArmorDamageResult result;
    if(!isServerSide() || damageAmount <= 0.0f || armorStack.Num() == 0){
        return result;
    }
    float remaining = damageAmount;
    TArray<FS_EquippedArmorState> brokenPieces;
    // The last piece stacked absorbs the hit first (R05 amendment). What it cannot take passes down.
    for(int32 index = armorStack.Num() - 1; index >= 0 && remaining > 0.0f; index--){
        const float absorbed = FMath::Min(armorStack[index].remainingDurability, remaining);
        armorStack[index].remainingDurability -= absorbed;
        remaining -= absorbed;
        result.absorbedAmount += absorbed;
        if(armorStack[index].remainingDurability <= 0.0f){
            brokenPieces.Add(armorStack[index]);   // copied before it leaves the array
            armorStack.RemoveAt(index);
        }
    }
    result.brokenPieces = brokenPieces.Num();

    if(brokenPieces.Num() == 0){
        finishWearChange();   // wear only: the aggregate has not changed, so it is not rebuilt (R05 §5)
        return result;
    }

    renumberStack();
    finishChange();           // a piece broke: rebuild the cache and tell Stats
    APlayerState* player = getOwnerPlayerState();
    APawn* pawn = player != nullptr ? player->GetPawn() : nullptr;
    const UEquipmentServices* services = UEquipmentServices::get(this);
    for(const FS_EquippedArmorState& piece : brokenPieces){
        raiseArmorMessage(T_Equipment_ArmorBroken, piece);        // server only (R05 §7)
        raiseArmorMessage(T_Equipment_ArmorUnequipped, piece);    // local on the host; clients raise it from replication (E11)
        if(services != nullptr && pawn != nullptr){
            // The effect and the sound travel on the cosmetic channel (R05 A89, Doc B21 A.4).
            services->raiseCosmeticEvent(T_Cosmetic_Event_Combat_ArmorBroken, pawn->GetActorLocation(), pawn, 1.0f);
        }
        EQUIPMENT_LOG(Log, this, "Armor %s broke and is destroyed, aggregate armor %.1f", *piece.itemId.ToString(), cachedArmorStats.armorValue);
    }
    return result;
}
```

(add `#include "GameFramework/Pawn.h"` at the top of the file.)

### E5.2 — The function library (what other frameworks call)

Teammates should not need to know the component. One small library gives them player-level functions.

`Public/EquipmentFramework/UBFL_Equipment.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EquipmentFramework/01Structs/S_EquipmentFramework.h"
#include "UBFL_Equipment.generated.h"

class APlayerState;

UCLASS()
class AWAKEN_API UBFL_Equipment : public UBlueprintFunctionLibrary{
    GENERATED_BODY()

public:
    // ---- queries. A player without the component answers zeros ----
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") static FS_AggregatedArmorStats getAggregatedArmorStats(const APlayerState* player);
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") static FS_ClothesProtection getClothesProtection(const APlayerState* player);
    // For Doc B14 / Doc B03: how warm the player's armor and clothes are. The size of the effect is their data.
    UFUNCTION(BlueprintPure, Category = "Awaken|Equipment") static float getTotalWarmth(const APlayerState* player);

    // ---- commands, server only ----
    // Doc R03 step 4: wear the armor down by what it absorbed.
    static FS_ArmorDamageResult applyDamageToArmor(APlayerState* player, float damageAmount);
    // REFERENCE version of Doc R03 3.7 step 4 for a player target (E5). damage is the attack damage after Block.
    static FS_GearAttackResult applyAttackToGear(APlayerState* player, float damage, float armorPenetrationPercent);
};
```

`Private/EquipmentFramework/UBFL_Equipment.cpp`

```cpp
#include "EquipmentFramework/UBFL_Equipment.h"
#include "EquipmentFramework/UEquipmentComponent.h"
#include "GameFramework/PlayerState.h"

FS_AggregatedArmorStats UBFL_Equipment::getAggregatedArmorStats(const APlayerState* player){
    const UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
    return equipment != nullptr ? equipment->getAggregatedArmorStats() : FS_AggregatedArmorStats();
}

FS_ClothesProtection UBFL_Equipment::getClothesProtection(const APlayerState* player){
    const UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
    return equipment != nullptr ? equipment->getClothesProtection() : FS_ClothesProtection();
}

float UBFL_Equipment::getTotalWarmth(const APlayerState* player){
    const UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
    return equipment != nullptr ? equipment->getTotalWarmth() : 0.0f;
}

FS_ArmorDamageResult UBFL_Equipment::applyDamageToArmor(APlayerState* player, float damageAmount){
    UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
    return equipment != nullptr ? equipment->applyDamageToArmor(damageAmount) : FS_ArmorDamageResult();
}

FS_GearAttackResult UBFL_Equipment::applyAttackToGear(APlayerState* player, float damage, float armorPenetrationPercent){
    FS_GearAttackResult result;
    result.damageToHealth = FMath::Max(0.0f, damage);
    UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
    if(equipment == nullptr || damage <= 0.0f){
        return result;
    }
    // 1. Clothes first: they reduce the damage the armor receives, and never wear out (R05 3.6 and amendment).
    const float afterClothes = FMath::Max(0.0f, damage - equipment->getClothesProtection().protection);
    result.absorbedByClothes = damage - afterClothes;

    // 2. Armor: up to armor x (1 - penetration) (Doc R03 3.7), and never more than the stack can still take.
    const float penetration = FMath::Clamp(armorPenetrationPercent, 0.0f, 1.0f);
    const float armorReduction = FMath::Min(afterClothes, equipment->getAggregatedArmorStats().armorValue * (1.0f - penetration));
    const FS_ArmorDamageResult worn = equipment->applyDamageToArmor(armorReduction);
    result.absorbedByArmor = worn.absorbedAmount;
    result.brokenPieces = worn.brokenPieces;

    // 3. The rest goes on to health. Never below zero. MinimumAttackDamage is Doc R03's rule, applied by R03 after this.
    result.damageToHealth = FMath::Max(0.0f, afterClothes - worn.absorbedAmount);
    return result;
}
```

### Verify (Section 5)

| Test | Expect |
| --- | --- |
| Wear `IronVest` (100) then `LeatherPad` (30). `applyDamageToArmor(20)` | absorbed 20; the **LeatherPad** (last stacked) is at 10, the vest untouched; no message except `onEquipmentChanged`; aggregate unchanged and the stats stub **not** called again |
| Then `applyDamageToArmor(25)` | absorbed 25; the pad took 10 and **broke**, the vest took 15 (now 85); stack size 1; aggregate 10; stats stub got 10; `Equipment.ArmorBroken` raised once; the cosmetic stub holds `Cosmetic.Event.Combat.ArmorBroken`; the pad is **not** in the bag |
| Then `applyDamageToArmor(500)` | absorbed **85** (all that was left); stack empty; aggregate 0 |
| `applyDamageToArmor(10)` with an empty stack, or with `-5` | absorbed 0, nothing changes |
| `applyAttackToGear(30, 0.5)` wearing `SoldierVest` + one fresh `IronVest` | `absorbedByClothes 5`, `absorbedByArmor 5`, `damageToHealth 20`; vest at 95 |
| `applyAttackToGear(3, 0)` wearing `SoldierVest` (clothes 5) | `damageToHealth 0`, never negative; armor untouched (R03 then raises it to `MinimumAttackDamage`) |
| `applyAttackToGear(30, 1.0)` (full penetration) | armor absorbs 0 and does not wear |
| Clothes after 100 hits | same protection as before: clothes never wear out |

### Handoff
- **R03** calls `UBFL_Equipment::applyDamageToArmor` (or `applyAttackToGear`) at step 4, **for attack damage only**, and applies `MinimumAttackDamage` afterwards. Confirm E4 and E5 with the R03 owner; if R03 prefers another arithmetic, only `applyAttackToGear` changes.
- **R08** receives the aggregate through `IBPI_ArmorStatsSink` on every equip, unequip and break.
- **B21 / R15 / R09** play the break effect and sound from `Cosmetic.Event.Combat.ArmorBroken`.

---

# SECTION 6 — Clothes

**Goal:** the player wears one outfit, changes it from the bag, and never loses the old one.
**Spec:** R05 3.6, §5 (`EquipClothes`, `GetClothesProtection`), amendments Q1–Q21, A87, A110, A115; R01 3.8; Doc B17 3.6.
**Depends on:** Section 5.

### The rules

- **One outfit**, not a top and a bottom (R05 A87). A new one replaces the old one.
- **Clothes are stackable items with no state** (Index A87). Wearing one takes **one unit** out of its stack (S2-F12). The worn state stores the item *type* id.
- **Default clothes are not an item** (R01 3.8, Q-S2-09). A new character wears them; in code that is an empty `itemId`. Replacing the default clothes returns nothing to the bag.
- **The old clothes go back to the bag** (R05 §5). If their type is at its limit they drop at the player's feet (E9).
- **Taken from a chest instead of the bag:** the player switches and the old clothes are dropped on the ground (R05 amendment).
- **A Bagless player cannot change clothes** and keeps what they wore (R05 amendment). Clothes are **not** part of the bag: they stay on the player through death (R01 3.8, Doc B02).
- **Clothes never wear out** and the HUD shows nothing for them (R05 3.6, A115). The bag's Others page shows spare clothes with a *wear* hover button (Doc B17 3.6).

### E6.1, E6.2 — The two commands

Header additions (public):

```cpp
    // R05 §5 EquipClothes: one unit leaves the bag's stack; the previous outfit returns to the bag.
    FS_InventoryResult equipClothes(FName itemId);
    // R05 amendment: clothes taken from the container the player has open. The previous outfit drops on the ground.
    FS_InventoryResult equipClothesFromContainer(FName itemId);

protected:
    // Shared checks of the two commands. Empty tag = allowed.
    FGameplayTag checkClothesChange(FName itemId) const;
    void applyClothesChange(FName itemId);
```

Implementation (add the includes at the top of the file):

```cpp
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UContainerViewComponent.h"
#include "ItemInventoryFramework/UInventoryRegistrySubsystem.h"
#include "ItemInventoryFramework/InventoryServices.h"
```

```cpp
// ---------- Section 6: clothes ----------

FGameplayTag UEquipmentComponent::checkClothesChange(FName itemId) const{
    if(!isServerSide()){
        return T_Inventory_FailReason_NotServer;
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getOwnerPlayerState());
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    if(inventory == nullptr || catalog == nullptr){
        return T_Inventory_FailReason_NotFound;
    }
    if(!inventory->hasBag()){
        return T_Inventory_FailReason_NoBag;   // a Bagless player cannot change clothes (R05 amendment)
    }
    const UDA_ItemDefinition* definition = catalog->getDefinition(itemId);
    if(definition == nullptr || !definition->isClothing()){
        return T_Equipment_FailReason_NotClothes;
    }
    if(wornClothes.itemId == itemId){
        return T_Equipment_FailReason_AlreadyWorn;
    }
    return FGameplayTag();
}

void UEquipmentComponent::applyClothesChange(FName itemId){
    wornClothes.itemId = itemId;
    finishChange();          // clothes carry warmth, so the cache is rebuilt; the armor aggregate is simply re-sent
    raiseClothesMessage();
}

FS_InventoryResult UEquipmentComponent::equipClothes(FName itemId){
    const FGameplayTag blocked = checkClothesChange(itemId);
    if(blocked.IsValid()){
        return refusal(blocked);
    }
    APlayerState* player = getOwnerPlayerState();
    UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
    if(bag->getAvailableQuantity(itemId) < 1){
        return refusal(T_Inventory_FailReason_InsufficientQuantity);   // none in the bag, or all of them staged
    }

    const FS_InventoryResult removed = bag->removeStack(itemId, 1);   // one unit leaves the stack (S2-F12)
    if(!removed.succeeded()){
        return removed;
    }
    const FName previous = wornClothes.itemId;
    applyClothesChange(itemId);
    if(!previous.IsNone()){
        // Back to the bag; if that type is at its limit it drops at the player's feet (Doc02 §19.5, E9).
        FS_ItemStack returned;
        returned.itemId = previous;
        returned.quantity = 1;
        UBFL_Inventory::grantOrDrop(player, returned);
    }
    EQUIPMENT_LOG(Log, this, "Clothes changed from %s to %s", previous.IsNone() ? TEXT("default") : *previous.ToString(), *itemId.ToString());
    return accepted();
}

FS_InventoryResult UEquipmentComponent::equipClothesFromContainer(FName itemId){
    const FGameplayTag blocked = checkClothesChange(itemId);
    if(blocked.IsValid()){
        return refusal(blocked);
    }
    APlayerState* player = getOwnerPlayerState();
    const UContainerViewComponent* view = UContainerViewComponent::findFor(player);
    const UInventoryRegistrySubsystem* registry = UInventoryRegistrySubsystem::get(this);
    UInventoryData* source = (view != nullptr && registry != nullptr) ? registry->findInventory(view->getViewedContainerId()) : nullptr;
    if(source == nullptr){
        return refusal(T_Inventory_FailReason_NotFound);   // no container is open
    }
    if(source->isDeathBag()){
        return refusal(T_Inventory_FailReason_DeathBagLocked);
    }
    if(source->getAvailableQuantity(itemId) < 1){
        return refusal(T_Inventory_FailReason_InsufficientQuantity);
    }

    const FS_InventoryResult removed = source->removeStack(itemId, 1);
    if(!removed.succeeded()){
        return removed;
    }
    const FName previous = wornClothes.itemId;
    applyClothesChange(itemId);
    if(!previous.IsNone()){
        // "The old clothes are dropped on the ground" (R05 amendment). Dropped, never destroyed.
        FS_InventoryEntry loose;
        loose.itemId = previous;
        loose.totalQuantity = 1;
        const UInventoryServices* services = UInventoryServices::get(this);
        if(services != nullptr){
            services->spawnLooseItem(player, loose, UBFL_Inventory::getDropLocation(player));
        }
    }
    EQUIPMENT_LOG(Log, this, "Clothes changed from %s to %s (taken from a container, old ones dropped)", previous.IsNone() ? TEXT("default") : *previous.ToString(), *itemId.ToString());
    return accepted();
}
```

> `getClothesProtection()` already works: Section 3's `recalculate()` reads `clothesProtection` and `warmth` from the worn item's definition. There is nothing to "unequip": see E10.

### E6.3 — Warmth (R05 3.5)

Nothing to build beyond `getTotalWarmth()`. The spec fixes only the direction: warm armor **and** warm clothes lower the cold-exposure Illness source, and the amount is data (R05 3.5 and amendment; Doc B14 3.6; Doc B03). Equipment supplies the number; Weather or Survival Tick decides what it does with it.

### Verify (Section 6)

| Test | Expect |
| --- | --- |
| New player | `getWornClothes().isDefault()` true; protection 0, warmth 0 |
| Bag holds 2 `SoldierVest`; `equipClothes(SoldierVest)` | accepted; bag holds **1**; worn = `SoldierVest`; protection 5; `Equipment.ClothesChanged` raised; **nothing** returned to the bag (the default clothes are not an item) |
| Then `equipClothes(WoolClothes)` with 1 in the bag | worn = `WoolClothes`; bag holds 0 `WoolClothes` and **2** `SoldierVest` (the old one came back); `getTotalWarmth()` 4 |
| `SoldierVest` at its per-type limit in the bag, change away from a worn `SoldierVest` | the returned one **drops as a loose item** at the player's feet; `Inventory.GrantDropped` raised by R01 |
| `equipClothes` of the outfit already worn | `Equipment.FailReason.AlreadyWorn`; bag unchanged |
| `equipClothes(IronVest)` | `Equipment.FailReason.NotClothes` |
| `equipClothes` of clothes not in the bag | `Inventory.FailReason.InsufficientQuantity` |
| Bagless player | `Inventory.FailReason.NoBag`; still wears what they wore |
| Chest open with a `BeachOutfit`; `equipClothesFromContainer(BeachOutfit)` | the chest loses 1; worn = `BeachOutfit`; the old outfit lies on the ground as a loose item; the bag is unchanged |
| `equipClothesFromContainer` with no chest open | `Inventory.FailReason.NotFound` |

### Handoff
- **B17** puts the *wear* hover button on clothes on the Others page and calls the request of Section 9.
- **R07** swaps the outfit mesh on `Equipment.ClothesChanged` (an empty `itemId` means the default outfit) and shows it on the owner's own body in first person as well (R05 3.6, Doc R07 3.13).
- **B14 / B03** read `UBFL_Equipment::getTotalWarmth(player)` for the cold-exposure source.

---

# SECTION 7 — Bag Lifecycle: the Seam R01 Calls

**Goal:** when the bag is left at a death, worn armor goes with it; when the bag is recovered, the armor is back on. Replace R01's `UStubEquipmentGear` with the real thing.
**Spec:** R05 3.3, §5 (`ReleaseEquippedGear`); R01 3.8; Doc B02 3.2, 3.6, amendment Q1–Q21 ("recovered gear re-equips automatically in the slots it occupied"); Doc R13 §3.6.
**Depends on:** Section 6, and the R01 guide's Section 9.

### What R01 already does, and what it expects from us

R01's `UBFL_InventoryBag::detachBag` and `reattachBag` are written and call the seam `IBPI_EquipmentGear` (R01 guide S1.10, S9). Read these two contracts carefully, because R01's code depends on them exactly:

| Seam function | R01 calls it | What we must do | What we must **not** do |
| --- | --- | --- | --- |
| `releaseEquippedGear(player)` | inside `detachBag`, step 2 | Empty the stack. Return every worn piece as a full bag entry (`gearEntries`) and its position (`gearSlots`). Raise the normal changed messages, so Stats and the visuals update (R05 3.3) | Put the entries into the bag. **R01 does that itself** with `addInstanceForced`, which ignores limits so nothing is lost |
| `reEquipGear(player, gear)` | inside `reattachBag`, after the bag is attached again | Take each listed instance out of the bag and put it back on, in the order it was worn | Apply the cap. These pieces were already worn (R05 3.2: the cap is for new equips only) |

After a release a Bagless player has **no armor protection** (R05 3.3, S2-F03): the aggregate pushed to Stats is 0. **Clothes are untouched** by both functions: they stay on the player (R01 3.8, Doc B02).

### E7.1 — The component side

Header additions (public):

```cpp
    // R05 §5 ReleaseEquippedGear. Server. Empties the stack and returns the pieces; the caller (R01) stores them in the bag record.
    FS_GearReleaseResult releaseEquippedGear();
    // Server. Puts recovered pieces back on, in the order they were worn. The cap is not applied.
    bool reEquipGear(const TArray<FS_GearSlotRecord>& gear);
    // Server. Destroys the worn armor without returning it anywhere (E13: Doc B06's "world bag wins" case).
    void discardWornArmor();
    // Server. Fresh character / Start Again: no armor, default clothes (E12).
    void resetForNewCharacter();
```

Implementation:

```cpp
// ---------- Section 7: bag lifecycle ----------

FS_GearReleaseResult UEquipmentComponent::releaseEquippedGear(){
    FS_GearReleaseResult result;
    if(!isServerSide()){
        result.succeeded = false;
        return result;
    }
    const TArray<FS_EquippedArmorState> released = armorStack;
    for(const FS_EquippedArmorState& piece : released){
        result.gearEntries.Add(makeEntry(piece));   // same instance id, current durability
        FS_GearSlotRecord record;
        record.instanceId = piece.instanceId;
        record.stackIndex = piece.stackIndex;
        result.gearSlots.Add(record);
    }
    if(released.Num() == 0){
        return result;   // nothing worn: nothing to announce
    }
    armorStack.Reset();
    finishChange();      // aggregate is now 0 and Stats is told: a Bagless player has no armor protection (S2-F03)
    for(const FS_EquippedArmorState& piece : released){
        raiseArmorMessage(T_Equipment_ArmorUnequipped, piece);
    }
    EQUIPMENT_LOG(Log, this, "Released %d worn armor pieces to the bag record", released.Num());
    return result;
}

bool UEquipmentComponent::reEquipGear(const TArray<FS_GearSlotRecord>& gear){
    if(!isServerSide()){
        return false;
    }
    const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(getOwnerPlayerState());
    UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
    if(bag == nullptr){
        return false;
    }
    // "In the slots it occupied" (Doc B02): re-adding in ascending position rebuilds the same stack.
    TArray<FS_GearSlotRecord> ordered = gear;
    ordered.Sort([](const FS_GearSlotRecord& left, const FS_GearSlotRecord& right){
        return left.stackIndex < right.stackIndex;
    });
    TArray<FS_EquippedArmorState> restored;
    for(const FS_GearSlotRecord& record : ordered){
        const FS_InventoryEntry* entry = bag->findInstance(record.instanceId);
        if(entry == nullptr){
            EQUIPMENT_LOG(Warning, this, "Re-equip skipped: instance %s is not in the recovered bag", *record.instanceId.ToString());
            continue;
        }
        FS_EquippedArmorState piece;
        piece.stackIndex = armorStack.Num();
        piece.instanceId = entry->instanceId;
        piece.itemId = entry->itemId;
        piece.remainingDurability = entry->state.durability;
        if(!bag->removeInstance(record.instanceId).succeeded()){
            continue;   // it stays in the bag; nothing is lost
        }
        armorStack.Add(piece);
        restored.Add(piece);
    }
    if(restored.Num() > 0){
        finishChange();
        for(const FS_EquippedArmorState& piece : restored){
            raiseArmorMessage(T_Equipment_ArmorEquipped, piece);
        }
    }
    EQUIPMENT_LOG(Log, this, "Re-equipped %d of %d recovered armor pieces", restored.Num(), gear.Num());
    return true;
}

void UEquipmentComponent::discardWornArmor(){
    if(!isServerSide() || armorStack.Num() == 0){
        return;
    }
    const TArray<FS_EquippedArmorState> discarded = armorStack;
    armorStack.Reset();
    finishChange();
    for(const FS_EquippedArmorState& piece : discarded){
        raiseArmorMessage(T_Equipment_ArmorUnequipped, piece);
    }
    EQUIPMENT_LOG(Log, this, "Discarded %d worn armor pieces", discarded.Num());
}

void UEquipmentComponent::resetForNewCharacter(){
    if(!isServerSide()){
        return;
    }
    discardWornArmor();
    if(!wornClothes.isDefault()){
        wornClothes = FS_WornClothesState();   // a player at game start wears the default clothes (R05 amendment)
        finishChange();
        raiseClothesMessage();
    }
}
```

### E7.2 — `UEquipmentServices` becomes the real `IBPI_EquipmentGear`

`EquipmentServices.h`: add the include, the base class and the two overrides.

```cpp
#include "ItemInventoryFramework/03Interfaces/BPI_EquipmentGear.h"

class AWAKEN_API UEquipmentServices : public UGameInstanceSubsystem, public IBPI_EquipmentGear{
    GENERATED_BODY()

public:
    // ---- IBPI_EquipmentGear: the seam Doc R01 calls from detachBag / reattachBag ----
    virtual FS_GearReleaseResult releaseEquippedGear_Implementation(APlayerState* player) override;
    virtual bool reEquipGear_Implementation(APlayerState* player, const TArray<FS_GearSlotRecord>& gear) override;
```

`EquipmentServices.cpp`: in `Initialize`, after the two stub registrations and before the log line:

```cpp
    // Replace R01's UStubEquipmentGear with the real implementation (R01 guide, stub register).
    registry->registerSeam(UBPI_EquipmentGear::StaticClass(), this);
```

and the two functions (add `#include "EquipmentFramework/UEquipmentComponent.h"`):

```cpp
FS_GearReleaseResult UEquipmentServices::releaseEquippedGear_Implementation(APlayerState* player){
    UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
    if(equipment == nullptr){
        return FS_GearReleaseResult();   // no component: nothing worn, and that is a success
    }
    return equipment->releaseEquippedGear();
}

bool UEquipmentServices::reEquipGear_Implementation(APlayerState* player, const TArray<FS_GearSlotRecord>& gear){
    UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
    return equipment != nullptr && equipment->reEquipGear(gear);
}
```

Now delete `UStubEquipmentGear` from R01's `InventoryStubs.h/.cpp` and its `registerSeam` line in `UInventoryServices::Initialize`, and tick it off R01's stub register (R01 guide S13.8).

### E7.3 — One patch to R01's `detachBag` (a bug the stub was hiding)

In the R01 guide's `detachBag`, step 2 reads:

```cpp
    const FS_GearReleaseResult gear = services->releaseEquippedGear(player);
    if(shouldFailStep(2) || !gear.succeeded){
```

With the stub that was harmless. With the real seam, the test `Awaken.Inventory.FailBagStep 2` would release the armor **first** and then report a failure, and the released pieces would be lost. Make the injected failure happen *before* the call:

```cpp
    // Step 2: worn armor goes back to the bag record (R05 3.3).
    FS_GearReleaseResult gear;
    gear.succeeded = false;
    if(!shouldFailStep(2)){
        gear = services->releaseEquippedGear(player);
    }
    if(!gear.succeeded){
        IBPI_BagActorService::Execute_deleteBag(bagService, bagId);   // the record/actor created in step 1 are deleted
        result.failReason = T_Inventory_FailReason_GearReleaseFailed;
        return result;
    }
```

Our `releaseEquippedGear` cannot fail on the server, so in real play step 2 always succeeds and R01's "nothing below can fail" still holds.

### E7.4 — Fresh character

Wherever `UBFL_InventoryBag::startNewCharacter(player, newCharacterId)` is called (Doc B05 / R13; in R01's dev commands too), call this right after it:

```cpp
    UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
    if(equipment != nullptr){
        equipment->resetForNewCharacter();
    }
```

### Verify (Section 7)

| Test | Expect |
| --- | --- |
| Startup log | R01's line still says its seams are registered; a breakpoint shows `getSeam(UBPI_EquipmentGear::StaticClass())` is the `UEquipmentServices` object, not a stub |
| Wear 3 pieces (one damaged to 60) and `SoldierVest`; `Awaken.Bag.DropHere` | stack **empty**; aggregate 0 and the stats stub got 0; three `ArmorUnequipped`; the bag's inventory (now a death bag in the registry) holds the three instances with their durabilities; **clothes still `SoldierVest`** |
| While Bagless, `equipArmor` | `Inventory.FailReason.NoBag` |
| `Awaken.Bag.Recover` | the same three instance ids are worn again, **same order**, the damaged one still at 60; the bag no longer holds them; aggregate restored; three `ArmorEquipped` |
| Cap lowered to 1 while the bag lies in the world, then recover | all three are worn again (the cap is for new equips only); a fourth equip is refused `StackFull` |
| `Awaken.Inventory.FailBagStep 2`, then `Awaken.Bag.DropHere` | `GearReleaseFailed`; the player **still wears all the armor** and still has the bag (this is the E7.3 patch) |
| `resetForNewCharacter()` | stack empty, default clothes, aggregate 0 |

**Two-player check:** host and one client, the client wearing two pieces. Host runs `Awaken.Bag.DropHere p1`: **both** machines see the client's armor disappear at once. `Awaken.Bag.Recover p1`: both see it return at the same positions.

### Handoff
- **B02** needs nothing new: it already calls R01's `detachBag` / `reattachBag`, which now reach the real Equipment.
- **R13**: the bag record's gear list (`FS_BagContents.gear`) is what `reEquipGear` consumes; worn clothes are never part of a bag record (R13 §3.6).
- **B05 / R13**: call `resetForNewCharacter()` next to `startNewCharacter` (E7.4).

---

# SECTION 8 — Save Integration

**Goal:** the armor stack (with each piece's remaining durability) and the worn clothes survive save and reload.
**Spec:** R05 §9; Doc00 §9; Doc R13 §3.3, §3.5, §3.6, §3.11; Doc B06 §3.6 (P5).
**Depends on:** Section 7, and the R01 guide's Section 11 (the stub participant contract and the byte helpers).

### What is and is not saved

| Saved (Character file) | Not saved |
| --- | --- |
| Each worn piece: item id, instance id, position, remaining durability | The cached aggregate (rebuilt on load) |
| The worn clothes (item type id; empty = default) | Anything about weapons (R01 owns the hotbar; the held item is never saved) |

Rules that come from other documents and need no code here, but explain what you will see in testing:

- **Per-player data, saved when that player is saved** (R05 §9). One save writes every connected player (R13 §3.3). A change made after the last save is lost on reload, including a clothes change.
- **A Bagless player is never captured** (R13 §3.3, Index A85/A93): their previous Character file is kept. So a file never holds "no armor because the bag is lying in the world".
- **A player who was not connected at the last save** comes back wearing what their own last file says (R05 §9, S2-F26).
- **Armor inside a bag record** is saved by R01 with the bag's inventory, in the World file. It is not in this snapshot, so it cannot be duplicated.

### E8.1 — Make the component a save participant

Header changes:

```cpp
#include "ItemInventoryFramework/03Interfaces/BPI_SaveParticipantStub.h"

class AWAKEN_API UEquipmentComponent : public UActorComponent, public IBPI_SaveParticipant{
    GENERATED_BODY()

public:
    inline static constexpr int32 currentSnapshotVersion = 1;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    // ---- IBPI_SaveParticipant (Doc R13) ----
    virtual FName getSaveKey_Implementation() override;
    virtual E_SaveScope getSaveScope_Implementation() override;
    virtual int32 getSnapshotVersion_Implementation() override;
    virtual TArray<uint8> getSnapshotForSave_Implementation() override;
    virtual bool restoreFromSnapshot_Implementation(const TArray<uint8>& snapshot) override;

protected:
    // Chained migration, one step per version. False when a step is missing or the file is newer (R13 §3.11).
    static bool migrateSave(FS_EquipmentSave& record);
```

Implementation (add the includes):

```cpp
#include "ItemInventoryFramework/UBFL_InventorySave.h"
#include "ItemInventoryFramework/Stubs/StubSaveRegistry.h"
```

```cpp
// ---------- Section 8: save ----------

FName UEquipmentComponent::getSaveKey_Implementation(){
    return TEXT("Equipment.Character");
}

E_SaveScope UEquipmentComponent::getSaveScope_Implementation(){
    return E_SaveScope::Character;
}

int32 UEquipmentComponent::getSnapshotVersion_Implementation(){
    return currentSnapshotVersion;
}

// Query: never mutates.
TArray<uint8> UEquipmentComponent::getSnapshotForSave_Implementation(){
    FS_EquipmentSave record;
    record.version = currentSnapshotVersion;
    record.armorStack = armorStack;   // includes each piece's remaining durability (R05 §9)
    record.clothes = wornClothes;
    return UBFL_InventorySave::toBytes(FS_EquipmentSave::StaticStruct(), &record);
}

bool UEquipmentComponent::migrateSave(FS_EquipmentSave& record){
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

bool UEquipmentComponent::restoreFromSnapshot_Implementation(const TArray<uint8>& snapshot){
    if(!isServerSide()){
        return false;
    }
    FS_EquipmentSave record;
    if(!UBFL_InventorySave::fromBytes(FS_EquipmentSave::StaticStruct(), &record, snapshot) || !migrateSave(record)){
        return false;   // refused; R13 shows the reason and a failed load never overwrites a file
    }
    const UItemCatalogSubsystem* catalog = UItemCatalogSubsystem::get(this);
    const TArray<FS_EquippedArmorState> previousStack = armorStack;
    const FName previousClothes = wornClothes.itemId;

    armorStack.Reset();
    for(const FS_EquippedArmorState& saved : record.armorStack){
        const UDA_ItemDefinition* definition = catalog != nullptr ? catalog->getDefinition(saved.itemId) : nullptr;
        if(definition == nullptr || !definition->isArmor() || !saved.instanceId.IsValid() || saved.remainingDurability <= 0.0f){
            // R13 §3.11: skip the entry, one log line, the load continues. Never crash.
            EQUIPMENT_LOG(Warning, this, "Skipped saved armor %s: the item no longer exists or is not armor", *saved.itemId.ToString());
            continue;
        }
        armorStack.Add(saved);   // the cap is NOT applied: these pieces were already worn (R05 3.2)
    }
    renumberStack();

    wornClothes = FS_WornClothesState();
    if(!record.clothes.isDefault()){
        const UDA_ItemDefinition* clothes = catalog != nullptr ? catalog->getDefinition(record.clothes.itemId) : nullptr;
        if(clothes != nullptr && clothes->isClothing()){
            wornClothes = record.clothes;
        }else{
            EQUIPMENT_LOG(Warning, this, "Saved clothes %s no longer exist: the default clothes are worn", *record.clothes.itemId.ToString());
        }
    }

    finishChange();
    for(const FS_EquippedArmorState& piece : previousStack){
        raiseArmorMessage(T_Equipment_ArmorUnequipped, piece);
    }
    for(const FS_EquippedArmorState& piece : armorStack){
        raiseArmorMessage(T_Equipment_ArmorEquipped, piece);
    }
    if(previousClothes != wornClothes.itemId){
        raiseClothesMessage();
    }
    EQUIPMENT_LOG(Log, this, "Restored %d armor pieces and clothes %s", armorStack.Num(), wornClothes.isDefault() ? TEXT("default") : *wornClothes.itemId.ToString());
    return true;
}

void UEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason){
    if(isServerSide()){
        UStubSaveRegistry* saveRegistry = UStubSaveRegistry::get(this);
        if(saveRegistry != nullptr){
            saveRegistry->unregisterParticipant(this);
        }
    }
    Super::EndPlay(EndPlayReason);
}
```

And in `BeginPlay`, inside the `if(isServerSide()){` block:

```cpp
        UStubSaveRegistry* saveRegistry = UStubSaveRegistry::get(this);
        if(saveRegistry != nullptr){
            saveRegistry->registerParticipant(this);   // when R13 lands, replace with its registration call
        }
```

### E8.2 — Restore order (Doc B06 §3.6): what R05 needs

| B06 step | R05 piece | Requirement |
| --- | --- | --- |
| Player P4 | bag reconciliation | Decides whether this player stays Bagless because a newer world holds their bag (R13 §3.6, "the world bag wins") |
| Player P5 | `UEquipmentComponent::restoreFromSnapshot` | After P4. No order against the other P5 restores: Equipment reads nothing from them (B06: "none reads another while restoring"). The item catalog must be loaded, which it is from game-instance start |
| Player P5, the P4 exception | `discardWornArmor()` | When P4 left the player Bagless, B06 calls this after our restore (or does not restore the armor at all): the real pieces are in the bag record, so the copies from the old file must not come back (E13). **Clothes are still restored** |

### Verify (Section 8): the round trip

`Tests/EquipmentSaveSpec.cpp`, tier 2:

1. Build a player wearing `IronVest` (durability 100), `LeatherPad` (damaged to 12) and `WoolClothes`, with one more `IronVest` in the bag.
2. Capture the Equipment bytes **and** R01's Character bytes. Then `discardWornArmor()`, change clothes, and clear the bag, to prove the restore really writes.
3. Restore both. Compare: two pieces, same order, **same instance ids**, durabilities 100 and 12, clothes `WoolClothes`, aggregate 14, warmth 4, and the stats stub received 14.
4. The bag holds exactly one `IronVest` and the worn ones are **not** in it: nothing is counted twice.
5. Unknown item: capture, then unregister `LeatherPad` from the test catalog (or patch the id in the record) and restore: one warning line, the vest is restored at index 0, the load continues.
6. A record with `version = 99`: `restoreFromSnapshot` returns false and the worn state is unchanged.
7. Restore with `armorStackCap` lowered to 1: both pieces are still restored.

Doc B22's starter test "R13 Save round trip" lists "equipped gear and worn clothes" in its equal-list; this spec is R05's half of it.

### Handoff
- **R13** replaces the stub interface and registry and stores the blob under the key `Equipment.Character`.
- **B06** calls `discardWornArmor()` in the P4 exception (E8.2).
- **B20**: the 15-minute hold of a dropped player keeps the PlayerState, so the worn state is held with it; nothing to do here.

---

# SECTION 9 — Networking: Requests, Replication and Messages

**Goal:** a client can only *ask*; the host validates, changes the state, and every machine sees the result.
**Spec:** R05 §7, §8; Doc02 §6, §8, §19.2, §19.6; Doc B07 3.6; Doc B17 3.6.
**Depends on:** Section 8, and the R01 guide's Section 12 (`UInventoryRequestComponent`).

### E9.1 — Replication audit (R05 §8, Doc02 §19.1)

Walk this table with a listen server and two clients. Every row must be true:

| State | Replicates to | Mechanism | Late joiner gets it from |
| --- | --- | --- | --- |
| Armor stack (item id, instance id, position, remaining durability) | **everyone** | `COND_None` on the PlayerState component | replicated data, then `OnRep_armorStack` raises the local messages |
| Worn clothes (item type id) | **everyone** | `COND_None` | replicated data, then `OnRep_wornClothes` |
| Cached aggregate, cached clothes protection | nobody | rebuilt locally from the replicated state | n/a |
| The `AggregatedArmor` attribute | Doc R08's choice | Stats owns it | Stats |

> Other players learn *what* a player wears and how worn it is. They never learn the player's bag: that is still owner-only (R01 §7). This is exactly why `itemId` had to be added to the struct (E3).

### E9.2 — Patch R01: three helpers become public (E1)

Equipment reuses R01's request gate, so a player has **one** rate limit for bag and equipment requests together and the "Downed / carrying" checks are written once. In `UInventoryRequestComponent.h` move these three declarations from `protected:` to `public:` (nothing else changes):

```cpp
    // Rate limit + player state + hands. Empty tag = allowed (Doc02 §19.6).
    FGameplayTag gate(bool needsHands, bool allowWhenIncapacitated);
    bool isContainerReachable(APlayerState* player, const FGuid& containerId) const;
    void refuse(const FGameplayTag& reason, const TCHAR* operation);
```

### E9.3 — The requests

A client never mutates anything. It sends **intent**; the server checks rate, state and hands, runs the command, and replies to the requester only when it refuses (Doc02 §6, §19.6).

Which checks apply to Equipment's requests (Doc02 §19.6 asks each framework to say):

| Check | Applies | Reason tag |
| --- | --- | --- |
| Rate limit | all three | `Inventory.FailReason.RateLimited` |
| Downed, Restrained, Arriving, scene lock | all three | `Interaction.FailReason.NotAllowedInState` |
| Carrying a heavy item | all three | `Interaction.FailReason.HandsFull` (Doc B07 3.6: equipping is refused while carrying) |
| Bagless | all three | `Inventory.FailReason.NoBag` (inside the commands) |
| Range and line of sight | only clothes taken from a container | `Inventory.FailReason.OutOfRange` |

Header additions (public, then protected):

```cpp
    // ---- requests: the UI calls these on the owning client. On the host they run directly ----
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Equipment") void serverEquipArmor(FGuid instanceId);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Equipment") void serverUnequipArmor(int32 stackIndex);
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Awaken|Equipment") void serverEquipClothes(FName itemId, bool fromOpenContainer);

protected:
    // Rate, state and hands, through R01's gate. False = refused and the requester has been told.
    bool passRequestGate(const TCHAR* operation) const;
    void replyIfRefused(const FS_InventoryResult& result, const TCHAR* operation) const;
```

Implementation (add `#include "ItemInventoryFramework/UInventoryRequestComponent.h"`):

```cpp
// ---------- Section 9: requests ----------

bool UEquipmentComponent::passRequestGate(const TCHAR* operation) const{
    UInventoryRequestComponent* requests = UInventoryRequestComponent::findFor(getOwnerPlayerState());
    if(requests == nullptr){
        EQUIPMENT_LOG(Warning, this, "%s refused: the player has no request component", operation);
        return false;
    }
    const FGameplayTag blocked = requests->gate(true, false);   // needs hands; not while Downed or Restrained
    if(blocked.IsValid()){
        requests->refuse(blocked, operation);
        return false;
    }
    return true;
}

void UEquipmentComponent::replyIfRefused(const FS_InventoryResult& result, const TCHAR* operation) const{
    if(result.succeeded()){
        return;
    }
    UInventoryRequestComponent* requests = UInventoryRequestComponent::findFor(getOwnerPlayerState());
    if(requests != nullptr){
        requests->refuse(result.failReason, operation);   // logs the reason tag and tells only the requester (Doc02 §6)
    }
}

void UEquipmentComponent::serverEquipArmor_Implementation(FGuid instanceId){
    if(!passRequestGate(TEXT("equipArmor"))){
        return;
    }
    replyIfRefused(equipArmor(instanceId), TEXT("equipArmor"));
}

void UEquipmentComponent::serverUnequipArmor_Implementation(int32 stackIndex){
    if(!passRequestGate(TEXT("unequipArmor"))){
        return;
    }
    replyIfRefused(unequipArmor(stackIndex), TEXT("unequipArmor"));
}

void UEquipmentComponent::serverEquipClothes_Implementation(FName itemId, bool fromOpenContainer){
    if(!passRequestGate(TEXT("equipClothes"))){
        return;
    }
    if(!fromOpenContainer){
        replyIfRefused(equipClothes(itemId), TEXT("equipClothes"));
        return;
    }
    // Taken from a chest: the chest must still be in reach (Doc02 §19.6; range is inclusive, Doc02 §19.9).
    APlayerState* player = getOwnerPlayerState();
    const UContainerViewComponent* view = UContainerViewComponent::findFor(player);
    UInventoryRequestComponent* requests = UInventoryRequestComponent::findFor(player);
    if(view == nullptr || !requests->isContainerReachable(player, view->getViewedContainerId())){
        requests->refuse(T_Inventory_FailReason_OutOfRange, TEXT("equipClothes"));
        return;
    }
    replyIfRefused(equipClothesFromContainer(itemId), TEXT("equipClothes"));
}
```

> **Why the RPCs can live on a PlayerState component.** A client may call a server RPC on an actor its connection owns. A PlayerState is owned by its PlayerController, so the owning client can call these, and no other client can. If the two-player check below logs *"No owning connection for actor"*, your real PlayerState is not owned by the controller; tell me, and we move the three RPCs onto R01's `UInventoryRequestComponent` (a ten-minute change).

### E9.4 — Message delivery (R05 §7, Doc02 §19.2)

| Message | Payload | Delivery | How it is met |
| --- | --- | --- | --- |
| `Equipment.ArmorEquipped` / `Equipment.ArmorUnequipped` | `FS_ArmorChangedMessage` | **local on every machine** | the host raises it in the command; a client raises it from `OnRep_armorStack` |
| `Equipment.ClothesChanged` | `FS_ClothesChangedMessage` | **local on every machine** | host in the command; client from `OnRep_wornClothes` |
| `Equipment.ArmorBroken` | `FS_ArmorChangedMessage` | **server only**; the effect and sound **need the cosmetic channel** | host in `applyDamageToArmor`, plus `Cosmetic.Event.Combat.ArmorBroken` through the B21 seam |
| `Equipment.StatsRecalculated` | `FS_ArmorStatsMessage` | **server only** | host in `pushStats` |

Notes for the people who listen:

- **UI (B17), HUD "external armor":** listen to `ArmorEquipped` / `ArmorUnequipped` for the piece list and bind `onEquipmentChanged` for the wear, because a hit changes durability without adding or removing a piece. Filter by "is this my PlayerState": the messages fire for **every** player on the machine.
- **Animation (R07):** on either armor message, rebuild that player's armor meshes from `getArmorStackCopy()` and `DA_ArmorStackLayout`. Rebuild the whole list rather than one position, because the stack closes up when a piece leaves (E2).
- A client cannot tell "broken" from "taken off": both arrive as `ArmorUnequipped`. The break *effect* reaches it on the cosmetic channel.

### E9.5 — When a player leaves (Doc B20)

Nothing to release: Equipment holds no reservations and no open views. The worn state stays on the PlayerState, and the host's 15-minute hold of a dropped player (Doc B20 3.8) keeps it with the rest of the Character state.

### Verify (Section 9)

Listen server + one client (*Play As Listen Server*, 2 players), `AInventoryTestGameMode`:

| Check | Expect |
| --- | --- |
| Client calls `serverEquipArmor` for a vest in its bag | the client's stack shows it; **the host and a second client see it too**; the client's bag loses the vest and only the client sees that |
| Client calls `serverEquipArmor` with a made-up instance id | refused `NotFound`; only the requester gets `clientRefused`; no state differs on any machine |
| Client at the cap equips one more | refused `StackFull`, only the requester is told |
| Host hits the client with `Awaken.Equipment.Hit 20 p1` (Section 10) | durability drops on **all** machines; no armor message; the client's `onEquipmentChanged` fires |
| A hit that breaks the top piece | all machines raise `ArmorUnequipped` for it; only the host logs `ArmorBroken`; the cosmetic stub on the host holds the event |
| Client changes clothes | everyone's copy of that player's `wornClothes` changes; each machine raises `ClothesChanged` once |
| Client carrying a heavy item (stub flag) requests any of the three | refused `HandsFull` |
| Client Downed (stub flag) | refused `NotAllowedInState` |
| Client sends 100 equip requests in one frame | after about `requestsPerSecond`, the rest are `RateLimited` (the bucket is shared with the bag requests) |
| A **late joiner** connects while another player wears 3 pieces and `WoolClothes` | it sees all of it from replicated data: three `ArmorEquipped` and one `ClothesChanged` are raised locally on the new machine |
| Client 2 calls `serverEquipArmor` on client 1's component | impossible: the RPC is dropped, client 2 does not own that PlayerState |
| Offline (standalone) | the same requests work; the machine is its own host |

### Handoff
- **B17** calls `serverEquipArmor`, `serverUnequipArmor`, `serverEquipClothes` and maps every reason tag through `DT_ReasonText`; add text for the five `Equipment.FailReason.*` tags.
- **R07** and **B17** listen as described in E9.4.

---

# SECTION 10 — Dev Tools, Tests and Final Acceptance

**Goal:** the closing "Dev Tools and Testing" section that Doc B22 §3.10 requires of every implementation guide.
**Spec:** Doc B22 §3.2 to §3.10.
**Depends on:** Sections 0–9.

### Files

```
Private/EquipmentFramework/DevTools/EquipmentDevCommands.cpp     (non-shipping only)
Private/EquipmentFramework/DevTools/EquipmentDebugView.cpp       (non-shipping only)
Private/EquipmentFramework/Tests/
    EquipmentSpec.cpp   EquipmentSaveSpec.cpp
Content/Awaken/Tests/TestMap_Inventory.umap                      (reuse R01's map)
```

### E10.1, E10.2 — Dev commands (Doc B22 §3.2, §3.4)

The rules are the ones the R01 guide follows (B22 §3.2): names start `Awaken.<Domain>.<Verb>`; **host only**; they go **through the owner's public functions**, so the same checks and messages run as in play; each says what happened in `LogAwakenEquipment`; **none saves**; compiled out of Shipping. The optional last argument `pN` picks player N (default 0), as in R01's commands.

`EquipmentDevCommands.cpp`

```cpp
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "EquipmentFramework/UEquipmentComponent.h"
#include "EquipmentFramework/UBFL_Equipment.h"
#include "EquipmentFramework/EquipmentLog.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"

namespace{
    bool isPlayerArgument(const FString& argument){
        return argument.Len() > 1 && argument.StartsWith(TEXT("p"), ESearchCase::CaseSensitive) && argument.Mid(1).IsNumeric();
    }

    // Returns null with a log line when not on the host or when the player does not exist.
    APlayerState* resolvePlayer(UWorld* world, const TArray<FString>& arguments){
        if(world == nullptr || !UBFL_Inventory::isServerWorld(world)){
            EQUIPMENT_LOG(Warning, world, "Host command: refused on a client (the Doc B22 cheat manager forwards it)");
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
            EQUIPMENT_LOG(Warning, world, "No player %d", playerIndex);
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

    // The first copy of an instanced item in the player's bag, or an invalid id.
    FGuid findFirstInBag(const APlayerState* player, FName itemId){
        const UPlayerInventoryComponent* inventory = UPlayerInventoryComponent::findFor(player);
        const UInventoryData* bag = inventory != nullptr ? inventory->getPersonalInventory() : nullptr;
        if(bag != nullptr){
            for(const FS_InventoryEntry& entry : bag->getEntries()){
                if(entry.itemId == itemId && entry.isInstanced()){
                    return entry.instanceId;
                }
            }
        }
        return FGuid();
    }

    void equipCommand(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = resolvePlayer(world, arguments);
        UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
        const TArray<FString> plain = plainArguments(arguments);
        if(equipment == nullptr || plain.Num() < 1){
            EQUIPMENT_LOG(Warning, world, "Usage: Awaken.Equipment.Equip <itemId> [pN]");
            return;
        }
        const FGuid instanceId = findFirstInBag(player, FName(*plain[0]));
        if(!instanceId.IsValid()){
            EQUIPMENT_LOG(Log, world, "Equip: no %s in the bag. Use Awaken.Item.Give %s 1 first", *plain[0], *plain[0]);
            return;
        }
        const FS_InventoryResult result = equipment->equipArmor(instanceId);
        EQUIPMENT_LOG(Log, world, "Equip %s: %s", *plain[0], result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    void unequipCommand(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = resolvePlayer(world, arguments);
        UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
        const TArray<FString> plain = plainArguments(arguments);
        if(equipment == nullptr || plain.Num() < 1){
            EQUIPMENT_LOG(Warning, world, "Usage: Awaken.Equipment.Unequip <stackIndex> [pN]");
            return;
        }
        const FS_InventoryResult result = equipment->unequipArmor(FCString::Atoi(*plain[0]));
        EQUIPMENT_LOG(Log, world, "Unequip position %s: %s", *plain[0], result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    // The situation most likely to break (Doc B22 §3.10): a full stack. Adds things, so it is not safe to repeat.
    void fillStackCommand(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = resolvePlayer(world, arguments);
        UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
        const TArray<FString> plain = plainArguments(arguments);
        if(equipment == nullptr || plain.Num() < 1){
            EQUIPMENT_LOG(Warning, world, "Usage: Awaken.Equipment.FillStack <itemId> [pN]");
            return;
        }
        const FName itemId(*plain[0]);
        FS_ItemStack single;
        single.itemId = itemId;
        single.quantity = 1;
        int32 equipped = 0;
        while(equipment->getArmorStack().Num() < equipment->getArmorStackCap()){
            UBFL_Inventory::grantOrDrop(player, single);   // through R01, so a full bag drops it (Doc02 §19.5)
            const FGuid instanceId = findFirstInBag(player, itemId);
            if(!instanceId.IsValid() || !equipment->equipArmor(instanceId).succeeded()){
                break;
            }
            equipped++;
        }
        // One more on purpose, to show the refusal at the cap.
        UBFL_Inventory::grantOrDrop(player, single);
        const FS_InventoryResult extra = equipment->equipArmor(findFirstInBag(player, itemId));
        EQUIPMENT_LOG(Log, world, "FillStack %s: equipped %d, stack %d of %d, one more -> %s", *plain[0], equipped, equipment->getArmorStack().Num(), equipment->getArmorStackCap(), extra.succeeded() ? TEXT("accepted (unexpected)") : *extra.failReason.ToString());
    }

    void hitCommand(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = resolvePlayer(world, arguments);
        const TArray<FString> plain = plainArguments(arguments);
        if(player == nullptr || plain.Num() < 1){
            EQUIPMENT_LOG(Warning, world, "Usage: Awaken.Equipment.Hit <damage> [penetration 0..1] [pN]");
            return;
        }
        const float damage = FCString::Atof(*plain[0]);
        const float penetration = plain.Num() > 1 ? FCString::Atof(*plain[1]) : 0.0f;
        // The reference step 4 only: health is NOT changed here. That is Doc R03's pipeline.
        const FS_GearAttackResult result = UBFL_Equipment::applyAttackToGear(player, damage, penetration);
        EQUIPMENT_LOG(Log, world, "Hit %.1f (penetration %.2f): clothes took %.1f, armor took %.1f, %d piece(s) broke, %.1f would reach health", damage, penetration, result.absorbedByClothes, result.absorbedByArmor, result.brokenPieces, result.damageToHealth);
    }

    void wearCommand(const TArray<FString>& arguments, UWorld* world){
        APlayerState* player = resolvePlayer(world, arguments);
        UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
        const TArray<FString> plain = plainArguments(arguments);
        if(equipment == nullptr || plain.Num() < 1){
            EQUIPMENT_LOG(Warning, world, "Usage: Awaken.Equipment.Wear <clothesItemId> [pN]");
            return;
        }
        const FS_InventoryResult result = equipment->equipClothes(FName(*plain[0]));
        EQUIPMENT_LOG(Log, world, "Wear %s: %s", *plain[0], result.succeeded() ? TEXT("done") : *result.failReason.ToString());
    }

    // Read only. Runs on any machine: it prints the replicated state this machine holds.
    void dumpCommand(const TArray<FString>& arguments, UWorld* world){
        const AGameStateBase* gameState = world != nullptr ? world->GetGameState() : nullptr;
        if(gameState == nullptr){
            return;
        }
        for(const APlayerState* player : gameState->PlayerArray){
            const UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            if(equipment == nullptr){
                continue;
            }
            const FS_AggregatedArmorStats stats = equipment->getAggregatedArmorStats();
            const FS_WornClothesState clothes = equipment->getWornClothes();
            EQUIPMENT_LOG(Log, world, "%s: %d piece(s), armor %.1f, durability left %.1f, clothes %s (protection %.1f), warmth %.1f", *player->GetPlayerName(), stats.pieceCount, stats.armorValue, equipment->getTotalRemainingDurability(), clothes.isDefault() ? TEXT("default") : *clothes.itemId.ToString(), equipment->getClothesProtection().protection, equipment->getTotalWarmth());
            for(const FS_EquippedArmorState& piece : equipment->getArmorStack()){
                EQUIPMENT_LOG(Log, world, "   [%d] %s durability %.1f %s", piece.stackIndex, *piece.itemId.ToString(), piece.remainingDurability, *piece.instanceId.ToString().Left(8));
            }
        }
    }

    FAutoConsoleCommandWithWorldAndArgs cmdEquip(TEXT("Awaken.Equipment.Equip"), TEXT("<itemId> [pN]: equip the first copy of that armor in the bag"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&equipCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdUnequip(TEXT("Awaken.Equipment.Unequip"), TEXT("<stackIndex> [pN]: return a worn piece to the bag"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&unequipCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdFill(TEXT("Awaken.Equipment.FillStack"), TEXT("<itemId> [pN]: grant and equip up to the cap, then try one more (the full-stack situation)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&fillStackCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdHit(TEXT("Awaken.Equipment.Hit"), TEXT("<damage> [penetration] [pN]: run the reference armor step on the player's gear (health is not changed)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&hitCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdWear(TEXT("Awaken.Equipment.Wear"), TEXT("<clothesItemId> [pN]: put on clothes from the bag"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&wearCommand));
    FAutoConsoleCommandWithWorldAndArgs cmdDump(TEXT("Awaken.Equipment.Dump"), TEXT(": print every player's worn armor and clothes as this machine sees them (read)"), FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&dumpCommand));
}

#endif
```

| Command | What it does | Where it runs |
| --- | --- | --- |
| `Awaken.Equipment.Equip <itemId> [pN]` | equips the first copy of that armor found in the bag, through `equipArmor` | Host with target |
| `Awaken.Equipment.Unequip <stackIndex> [pN]` | returns a worn piece to the bag, through `unequipArmor` | Host with target |
| `Awaken.Equipment.FillStack <itemId> [pN]` | grants and equips pieces up to the cap, then tries one more and logs the refusal. **The situation most likely to break** | Host with target |
| `Awaken.Equipment.Hit <damage> [penetration] [pN]` | runs the reference armor step on the gear and logs who absorbed what. Health is not changed | Host with target |
| `Awaken.Equipment.Wear <itemId> [pN]` | puts on clothes from the bag, through `equipClothes` | Host with target |
| `Awaken.Equipment.Dump` (read) | prints every player's worn state as this machine sees it; run it on the client to check replication | Local |
| `Awaken.Equipment.StackCap <n>` (cvar) | overrides the cap for tests; `-1` returns to the project setting | Local to the host |

Useful together with R01's commands: `Awaken.Item.Give IronVest 3`, `Awaken.Bag.DropHere`, `Awaken.Bag.Recover`, and the cvar `Awaken.Inventory.FailBagStep`.

### E10.3 — Debug view `Awaken.Debug.Equipment` (Doc B22 §3.5)

A view draws what the system knows, **locally**, only in non-shipping builds, and never replicates.

`EquipmentDebugView.cpp`

```cpp
#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING

#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "EquipmentFramework/UEquipmentComponent.h"

namespace{
    TAutoConsoleVariable<int32> cvarDebugEquipment(TEXT("Awaken.Debug.Equipment"), 0, TEXT("1 = draw each player's armor stack, wear, aggregate, clothes and warmth on screen."), ECVF_Default);
    FTSTicker::FDelegateHandle equipmentTickerHandle;

    bool drawEquipmentDebug(float deltaTime){
        if(GEngine == nullptr){
            return true;
        }
        int32 messageKey = 9300;
        for(const FWorldContext& context : GEngine->GetWorldContexts()){
            const UWorld* world = context.World();
            if(world == nullptr || world->GetGameState() == nullptr){
                continue;
            }
            for(const APlayerState* player : world->GetGameState()->PlayerArray){
                const UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
                if(equipment == nullptr){
                    continue;
                }
                const FS_AggregatedArmorStats stats = equipment->getAggregatedArmorStats();
                const FS_WornClothesState clothes = equipment->getWornClothes();
                GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::Green, FString::Printf(TEXT("[Equipment] %s: %d/%d pieces, armor %.1f, clothes %s (%.1f), warmth %.1f"), *player->GetPlayerName(), stats.pieceCount, equipment->getArmorStackCap(), stats.armorValue, clothes.isDefault() ? TEXT("default") : *clothes.itemId.ToString(), equipment->getClothesProtection().protection, equipment->getTotalWarmth()));
                // Drawn top of the stack first: that is the order the pieces absorb a hit in.
                for(int32 index = equipment->getArmorStack().Num() - 1; index >= 0; index--){
                    const FS_EquippedArmorState& piece = equipment->getArmorStack()[index];
                    GEngine->AddOnScreenDebugMessage(messageKey++, 0.3f, FColor::White, FString::Printf(TEXT("   [%d] %s durability %.1f"), piece.stackIndex, *piece.itemId.ToString(), piece.remainingDurability));
                }
            }
        }
        return true;
    }

    void onEquipmentDebugChanged(IConsoleVariable* variable){
        if(variable->GetInt() != 0 && !equipmentTickerHandle.IsValid()){
            equipmentTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&drawEquipmentDebug), 0.25f);   // only while the view is on (Doc02 §5)
        }else if(variable->GetInt() == 0 && equipmentTickerHandle.IsValid()){
            FTSTicker::GetCoreTicker().RemoveTicker(equipmentTickerHandle);
            equipmentTickerHandle.Reset();
        }
    }

    struct FEquipmentDebugRegistrar{
        FEquipmentDebugRegistrar(){
            cvarDebugEquipment->SetOnChangedCallback(FConsoleVariableDelegate::CreateStatic(&onEquipmentDebugChanged));
        }
    };
    FEquipmentDebugRegistrar equipmentDebugRegistrar;
}

#endif
```

### E10.4 — Log lines (Doc B22 §3.6)

Category `LogAwakenEquipment`; every line names the machine role (done by `EQUIPMENT_LOG`). A Log-level line exists for every state change:

| State change | Where the line is |
| --- | --- |
| Framework started | `UEquipmentServices::Initialize` |
| Armor equipped / unequipped | `equipArmor` / `unequipArmor` |
| Armor broke | `applyDamageToArmor` |
| Clothes changed | `equipClothes` / `equipClothesFromContainer` |
| Gear released to the bag / re-equipped / discarded | `releaseEquippedGear` / `reEquipGear` / `discardWornArmor` |
| Restored from a save (and each skipped entry, as a Warning) | `restoreFromSnapshot` |
| Every refusal returned to a client | R01's `UInventoryRequestComponent::refuse` (the reason tag is logged in `LogAwakenInventory`) |
| Dev commands | each command logs its result |

A hit that only wears armor is **not** logged at Log level (it repeats in combat); add a `Verbose` line in `applyDamageToArmor` if you want to watch it.

### E10.5 — Automation specs

Run from *Session Frontend → Automation* filtered by `Awaken.Equipment`, or:

```
UnrealEditor-Cmd.exe <Project>.uproject -ExecCmds="Automation RunTests Awaken.Equipment; Quit" -unattended -nullrhi -log
```

`Tests/EquipmentSpec.cpp`

```cpp
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/IConsoleManager.h"
#include "ItemInventoryFramework/Tests/InventoryTestFixture.h"
#include "ItemInventoryFramework/Stubs/AInventoryTestPlayerState.h"
#include "ItemInventoryFramework/UBFL_Inventory.h"
#include "ItemInventoryFramework/UBFL_InventoryBag.h"
#include "ItemInventoryFramework/UPlayerInventoryComponent.h"
#include "ItemInventoryFramework/UInventoryData.h"
#include "ItemInventoryFramework/UDA_ItemDefinition.h"
#include "ItemInventoryFramework/InventoryServices.h"
#include "ItemInventoryFramework/02Tags/T_ItemInventoryFramework.h"
#include "EquipmentFramework/UEquipmentComponent.h"
#include "EquipmentFramework/UBFL_Equipment.h"
#include "EquipmentFramework/02Tags/T_EquipmentFramework.h"
#include "EquipmentFramework/03Interfaces/BPI_ArmorStatsSink.h"
#include "EquipmentFramework/03Interfaces/BPI_CosmeticEventSink.h"
#include "EquipmentFramework/Stubs/EquipmentStubs.h"

namespace{
    // Puts a fresh copy of an armor item into the player's bag and returns its instance id.
    FGuid giveArmor(AInventoryTestPlayerState* player, const UDA_ItemDefinition& definition){
        const FS_InventoryEntry entry = UBFL_Inventory::makeFreshInstance(player, definition);
        UPlayerInventoryComponent::findFor(player)->getPersonalInventory()->addInstance(entry);
        return entry.instanceId;
    }

    void setStackCap(int32 cap){
        IConsoleVariable* variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Awaken.Equipment.StackCap"));
        if(variable != nullptr){
            variable->Set(cap);
        }
    }
}

BEGIN_DEFINE_SPEC(FEquipmentSpec, "Awaken.Equipment", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
END_DEFINE_SPEC(FEquipmentSpec)

void FEquipmentSpec::Define(){
    // ---- Spec 1 — the armor stack (tier 1) ----
    Describe("Armor stack", [this](){
        It("moves a piece out of the bag, refuses at the cap and never loses a piece", [this](){
            FInventoryTestFixture fixture;
            const UDA_ItemDefinition* vest = fixture.addArmor(TEXT("IronVest"), 2, 10.0f, 100.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            const UStubArmorStatsSink* stats = Cast<UStubArmorStatsSink>(fixture.services()->getSeam(UBPI_ArmorStatsSink::StaticClass()));

            const FGuid first = giveArmor(player, *vest);
            const FGuid second = giveArmor(player, *vest);
            TestTrue(TEXT("equip first"), equipment->equipArmor(first).succeeded());
            TestTrue(TEXT("equip second"), equipment->equipArmor(second).succeeded());
            TestEqual(TEXT("the bag holds none: worn is not in the bag"), bag->getInstanceCount(TEXT("IronVest")), 0);
            TestEqual(TEXT("two worn"), equipment->getArmorStack().Num(), 2);
            TestEqual(TEXT("aggregate is the sum"), equipment->getAggregatedArmorStats().armorValue, 20.0f);
            TestEqual(TEXT("Stats was told"), stats->lastArmorValue, 20.0f);

            setStackCap(2);
            const FGuid third = giveArmor(player, *vest);
            TestTrue(TEXT("refused at the cap"), equipment->equipArmor(third).failReason == T_Equipment_FailReason_StackFull);
            TestTrue(TEXT("the refused piece is still in the bag"), bag->findInstance(third) != nullptr);
            setStackCap(1);
            TestEqual(TEXT("lowering the cap takes nothing off"), equipment->getArmorStack().Num(), 2);
            setStackCap(-1);

            TestTrue(TEXT("unequip position 0"), equipment->unequipArmor(0).succeeded());
            TestTrue(TEXT("the same instance is back in the bag"), bag->findInstance(first) != nullptr);
            TestTrue(TEXT("the stack closed up"), equipment->getArmorStack()[0].instanceId == second && equipment->getArmorStack()[0].stackIndex == 0);
            // The bag now holds two vests, which is the per-type limit of this test item.
            TestTrue(TEXT("refused when the bag is at its limit"), equipment->unequipArmor(0).failReason == T_Interaction_FailReason_BagFull);
            TestEqual(TEXT("so the piece stays worn"), equipment->getArmorStack().Num(), 1);
        });
    });

    // ---- Spec 2 — armor wear and the reference damage step (tier 1) ----
    Describe("Armor damage", [this](){
        It("wears the last piece first, passes the rest down and destroys a used-up piece", [this](){
            FInventoryTestFixture fixture;
            const UDA_ItemDefinition* vest = fixture.addArmor(TEXT("IronVest"), 5, 10.0f, 100.0f);
            const UDA_ItemDefinition* pad = fixture.addArmor(TEXT("LeatherPad"), 5, 4.0f, 30.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            const UStubCosmeticEventSink* cosmetics = Cast<UStubCosmeticEventSink>(fixture.services()->getSeam(UBPI_CosmeticEventSink::StaticClass()));
            const FGuid vestId = giveArmor(player, *vest);
            const FGuid padId = giveArmor(player, *pad);
            equipment->equipArmor(vestId);
            equipment->equipArmor(padId);

            TestEqual(TEXT("absorbs 20"), equipment->applyDamageToArmor(20.0f).absorbedAmount, 20.0f);
            TestEqual(TEXT("the pad, stacked last, took it"), equipment->getArmorStack()[1].remainingDurability, 10.0f);
            TestEqual(TEXT("the vest is untouched"), equipment->getArmorStack()[0].remainingDurability, 100.0f);

            const FS_ArmorDamageResult second = equipment->applyDamageToArmor(25.0f);
            TestEqual(TEXT("absorbs 25 across two pieces"), second.absorbedAmount, 25.0f);
            TestEqual(TEXT("one piece broke"), second.brokenPieces, 1);
            TestEqual(TEXT("one piece left"), equipment->getArmorStack().Num(), 1);
            TestEqual(TEXT("the vest took the remaining 15"), equipment->getArmorStack()[0].remainingDurability, 85.0f);
            TestEqual(TEXT("aggregate dropped"), equipment->getAggregatedArmorStats().armorValue, 10.0f);
            TestTrue(TEXT("a broken piece is destroyed, not returned"), bag->findInstance(padId) == nullptr);
            TestTrue(TEXT("the break went to the cosmetic channel"), cosmetics->raisedEvents.Contains(T_Cosmetic_Event_Combat_ArmorBroken));

            TestEqual(TEXT("absorbs only what is left"), equipment->applyDamageToArmor(500.0f).absorbedAmount, 85.0f);
            TestEqual(TEXT("the stack is empty"), equipment->getArmorStack().Num(), 0);
        });
        It("clothes first, then armor times one minus penetration, never below zero", [this](){
            FInventoryTestFixture fixture;
            const UDA_ItemDefinition* vest = fixture.addArmor(TEXT("IronVest"), 5, 10.0f, 100.0f);
            fixture.addClothes(TEXT("SoldierVest"), 3, 5.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            UPlayerInventoryComponent::findFor(player)->getPersonalInventory()->addStack(TEXT("SoldierVest"), 1);
            equipment->equipClothes(TEXT("SoldierVest"));
            equipment->equipArmor(giveArmor(player, *vest));

            const FS_GearAttackResult hit = UBFL_Equipment::applyAttackToGear(player, 30.0f, 0.5f);
            TestEqual(TEXT("clothes took 5"), hit.absorbedByClothes, 5.0f);
            TestEqual(TEXT("armor took 10 x 0.5"), hit.absorbedByArmor, 5.0f);
            TestEqual(TEXT("20 goes on to health"), hit.damageToHealth, 20.0f);
            TestEqual(TEXT("the vest wore by what it absorbed"), equipment->getArmorStack()[0].remainingDurability, 95.0f);

            const FS_GearAttackResult weak = UBFL_Equipment::applyAttackToGear(player, 3.0f, 0.0f);
            TestEqual(TEXT("never below zero"), weak.damageToHealth, 0.0f);
            TestEqual(TEXT("clothes never wear out"), equipment->getClothesProtection().protection, 5.0f);
        });
    });

    // ---- Spec 3 — clothes (tier 1) ----
    Describe("Clothes", [this](){
        It("takes one unit from the stack and returns the old outfit to the bag", [this](){
            FInventoryTestFixture fixture;
            fixture.addClothes(TEXT("SoldierVest"), 3, 5.0f);
            fixture.addClothes(TEXT("WoolClothes"), 3, 1.0f, 4.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            UInventoryData* bag = UPlayerInventoryComponent::findFor(player)->getPersonalInventory();
            bag->addStack(TEXT("SoldierVest"), 2);
            bag->addStack(TEXT("WoolClothes"), 1);

            TestTrue(TEXT("a new player wears the default clothes"), equipment->getWornClothes().isDefault());
            TestTrue(TEXT("wear"), equipment->equipClothes(TEXT("SoldierVest")).succeeded());
            TestEqual(TEXT("one unit left the stack"), bag->getTotalQuantity(TEXT("SoldierVest")), 1);
            TestTrue(TEXT("the same outfit again is refused"), equipment->equipClothes(TEXT("SoldierVest")).failReason == T_Equipment_FailReason_AlreadyWorn);

            TestTrue(TEXT("change"), equipment->equipClothes(TEXT("WoolClothes")).succeeded());
            TestEqual(TEXT("the old outfit came back"), bag->getTotalQuantity(TEXT("SoldierVest")), 2);
            TestEqual(TEXT("none of the new one left in the bag"), bag->getTotalQuantity(TEXT("WoolClothes")), 0);
            TestEqual(TEXT("warmth follows the clothes"), equipment->getTotalWarmth(), 4.0f);
        });
    });

    // ---- Spec 4 — death bag and recovery (tier 2: several frameworks' pieces together) ----
    Describe("Bag lifecycle", [this](){
        It("armor leaves with the bag and returns in the same order; clothes stay on", [this](){
            FInventoryTestFixture fixture;
            const UDA_ItemDefinition* vest = fixture.addArmor(TEXT("IronVest"), 5, 10.0f, 100.0f);
            const UDA_ItemDefinition* pad = fixture.addArmor(TEXT("LeatherPad"), 5, 4.0f, 30.0f);
            fixture.addClothes(TEXT("SoldierVest"), 3, 5.0f);
            AInventoryTestPlayerState* player = fixture.makePlayer();
            UEquipmentComponent* equipment = UEquipmentComponent::findFor(player);
            const UStubArmorStatsSink* stats = Cast<UStubArmorStatsSink>(fixture.services()->getSeam(UBPI_ArmorStatsSink::StaticClass()));
            UPlayerInventoryComponent::findFor(player)->getPersonalInventory()->addStack(TEXT("SoldierVest"), 1);
            equipment->equipClothes(TEXT("SoldierVest"));
            const FGuid vestId = giveArmor(player, *vest);
            const FGuid padId = giveArmor(player, *pad);
            equipment->equipArmor(vestId);
            equipment->equipArmor(padId);
            equipment->applyDamageToArmor(10.0f);   // the pad is now at 20

            const FS_BagDetachResult detached = UBFL_InventoryBag::detachBag(player, FVector::ZeroVector);
            TestTrue(TEXT("detached"), detached.succeeded());
            TestEqual(TEXT("no armor while Bagless"), equipment->getArmorStack().Num(), 0);
            TestEqual(TEXT("Stats was told zero"), stats->lastArmorValue, 0.0f);
            TestTrue(TEXT("clothes stay on the player"), equipment->getWornClothes().itemId == FName(TEXT("SoldierVest")));
            TestTrue(TEXT("a Bagless player cannot equip"), equipment->equipArmor(vestId).failReason == T_Inventory_FailReason_NoBag);

            TestTrue(TEXT("recovered"), UBFL_InventoryBag::reattachBag(player, detached.bagId).succeeded());
            TestEqual(TEXT("both pieces are worn again"), equipment->getArmorStack().Num(), 2);
            TestTrue(TEXT("in the positions they occupied"), equipment->getArmorStack()[0].instanceId == vestId && equipment->getArmorStack()[1].instanceId == padId);
            TestEqual(TEXT("durability travelled with the piece"), equipment->getArmorStack()[1].remainingDurability, 20.0f);
            TestEqual(TEXT("aggregate restored"), equipment->getAggregatedArmorStats().armorValue, 14.0f);
        });
    });
}

#endif
```

`Tests/EquipmentSaveSpec.cpp` (tier 2): the seven steps of Section 8's Verify, written in the same style.

| Test | Tier | Sets up | Checks |
| --- | --- | --- | --- |
| Armor stack | 1 | two vests, the cap override | worn is not in the bag; cap refusal; lowering the cap removes nothing; unequip keeps the instance; bag-full refusal |
| Armor damage | 1 | vest + pad; clothes + vest | last piece first, spill-over, break destroys, cosmetic event; the reference step 4 numbers |
| Clothes | 1 | two outfits in the bag | one unit leaves the stack; the old outfit returns; default clothes; warmth |
| Bag lifecycle | 2 | armor, clothes, R01's detach and reattach | no armor while Bagless; same pieces and order after recovery; clothes untouched |
| Save round trip | 2 | Section 8 Verify | ids, order, durability, clothes equal; nothing counted twice; skip-unknown; newer version refused |
| Replication and requests | 3 (manual) | E10.7 | what the client must see |

### E10.6 — Test map and scenario

Reuse R01's `TestMap_Inventory` (GameMode override `AInventoryTestGameMode`, two PlayerStarts, the chest). Put one `BeachOutfit` in the chest for the "clothes from a container" check. The **"full stack" scenario** is `Awaken.Equipment.FillStack IronVest`, which ends by logging `Equipment.FailReason.StackFull`.

### E10.7 — The two-player check (Doc B22 §3.8, §3.10)

Editor *Play As Listen Server*, 2 players (host = player 0, client = player 1), then once offline. Turn on `Awaken.Debug.Equipment 1` on **both** machines.

1. Host: `Awaken.Item.Give IronVest 2 p1`, then `Awaken.Equipment.Equip IronVest p1` twice. **The client sees** two pieces on itself in its debug view; **the host sees the same two pieces** for player 1. The host's own stack is empty.
2. Client: unequip position 0 through the request (`serverUnequipArmor`, from a test widget or key). Both machines show one piece at position 0; the vest is back in the **client's** bag and the host never sees that bag.
3. Host: `Awaken.Equipment.Hit 20 p1`. Durability drops by 10 on **both** machines (armor 10, no penetration); no piece is added or removed.
4. Host: run `Awaken.Equipment.Hit 20 p1` nine more times (each hit wears the vest by 10, because armor absorbs at most its armor value per hit). On the last one the piece disappears on **both** machines, only the host logs `ArmorBroken`, and the broken vest is **not** in the client's bag.
5. Host: `Awaken.Item.Give WoolClothes 1 p1`, `Awaken.Equipment.Wear WoolClothes p1`. Both machines show `WoolClothes` for player 1 and warmth 4.
6. Host: `Awaken.Equipment.FillStack IronVest p1`. The client's stack shows 14 of 14 on both machines; the log ends with `StackFull`.
7. Host: `Awaken.Bag.DropHere p1`. The client's armor vanishes on **both** machines at once; its clothes stay. `Awaken.Bag.Recover p1`: all 14 return in order.
8. A **late joiner** connects now: it sees player 1's 14 pieces and `WoolClothes` from replicated data.
9. Client tries to equip while the host has set the "carrying" stub flag: refused `HandsFull`, and **only the client** is told.
10. Repeat steps 1, 3, 5 and 7 **offline** (standalone): same results on the one machine.

### E10.8 — Stub register (what to delete, and when)

| Stub / temporary item | File | Owner | Remove when |
| --- | --- | --- | --- |
| `UStubArmorStatsSink` | `Stubs/EquipmentStubs.*` | Doc R08 Stats | R08 registers an `IBPI_ArmorStatsSink` with `registerSeam` |
| `UStubCosmeticEventSink`, `BPI_CosmeticEventSink.h` | `Stubs/`, `03Interfaces/` | Doc B21 | B21's own raise function or interface exists |
| Duplicate tag definition `Cosmetic.Event.Combat.ArmorBroken` | `T_EquipmentFramework.*` | Doc B21 | B21's tag header exists |
| `UStubSaveRegistry` use in `BeginPlay` / `EndPlay` | `UEquipmentComponent.cpp` | Doc R13 | R13's participant registry exists |
| `equipmentComponent` on `AInventoryTestPlayerState` | R01 `Stubs/` | Doc B19 | the real PlayerState carries the component |
| `applyAttackToGear` (reference step 4) | `UBFL_Equipment.*` | Doc R03 | R03's damage pipeline exists and you both agree whether it keeps calling this function |
| **Removed by this guide:** `UStubEquipmentGear` | R01 `Stubs/InventoryStubs.*` | Doc R05 (us) | done in E7.2 |

### Handoff notes for teammates

| Doc | You must |
| --- | --- |
| **R03** (Combat) | at step 4, for **attack damage only**, read clothes protection and the armor value and call `UBFL_Equipment::applyDamageToArmor` (or call `applyAttackToGear` and use its `damageToHealth`); apply `MinimumAttackDamage` after it; never call either for body or environment damage |
| **R08** (Stats) | implement `IBPI_ArmorStatsSink::applyArmorAggregate` (write the `AggregatedArmor` attribute) and register it; optionally listen to `Equipment.StatsRecalculated` |
| **R07** (Animation) | show armor meshes from `getArmorStackCopy()` + `DA_ArmorStackLayout` + each item's `armorMesh`; swap the outfit from `Equipment.ClothesChanged` + `outfitMesh` (empty id = default outfit); rebuild the whole armor list on any armor message |
| **B17** (UI) | HUD "external armor": pieces and wear from the armor messages and `onEquipmentChanged`; bag Others page: armor with its wear, clothes with a *wear* hover button calling `serverEquipClothes`; add text for the five `Equipment.FailReason.*` tags |
| **B21 / R15 / R09** | implement the cosmetic seam; play the break effect and sound for `Cosmetic.Event.Combat.ArmorBroken` |
| **B14 / B03** | read `UBFL_Equipment::getTotalWarmth(player)` for the cold-exposure Illness source; the size of the effect is your data |
| **B02** | nothing new: `detachBag` / `reattachBag` now reach the real Equipment |
| **B06** | restore Equipment in P5; call `discardWornArmor()` for a player left Bagless by P4 |
| **B05 / R13** | call `resetForNewCharacter()` next to `startNewCharacter`; store the blob under `Equipment.Character` |
| **B19** | add `UEquipmentComponent` to the real PlayerState |

### E10.9 — Final review: every confirmed R05 rule maps to a task

| R05 rule | Where |
| --- | --- |
| Weapons use no equipment slot; held item comes from R01's `getHeldItem` (§0.1, 3.1) | no code here, on purpose |
| `UEquipmentComponent` on the PlayerState; the Character shows it (§2) | E3 |
| Armor is a stack, no body slots, protects as a whole; position is visual only (3.2) | E3 `recalculate`, E2.3 layout |
| Cap 14, tuning data; refused past the cap; lowering the cap removes nothing (3.2, S2-F22) | E0.3, E4, E7, E8 |
| Armor wears out and disappears; how much it takes is item data (3.2) | E2.1, E5 |
| The last piece stacked absorbs the hit first (amendment) | E5 |
| Equipping moves the item out of the bag; never counted twice (3.3) | E4 |
| `ReleaseEquippedGear` inside `DetachBag`; a Bagless player has no armor and cannot equip (3.3, S2-F03) | E7, E4 |
| Recovered gear re-equips in the slots it occupied (Doc B02) | E7 |
| Weapon upgrades are crafting recipes (3.4) | no code here: Doc R02 3.10 |
| Warm armor and warm clothes reduce cold exposure; the amount is data (3.5, amendment) | E2.1 `warmth`, E3 `getTotalWarmth`, E6.3 |
| One outfit; clothes give protection, never wear out, no HUD element (3.6, A87) | E6 |
| Clothes are stackable; one unit leaves the stack; type id, not instance (S2-F12) | E1.1, E6 |
| Default clothes are built in, not an item (Q-S2-09) | E1.1 `isDefault`, E6 |
| Clothes protection reduces the damage the armor receives (amendment) | E5 reference step |
| Bagless: cannot change clothes, keeps what they wore (amendment) | E6, E7 |
| Clothes taken from another inventory: switch, old ones dropped (amendment) | E6 |
| Armor and clothes protect against attack damage only (A92) | E5 rule 1 (R03 decides when to call) |
| Messages and their delivery (§7); cosmetic event for the break (A89) | E3, E5, E9.4 |
| Server-authoritative commands; worn state replicates to everyone (§8) | E3, E9 |
| Save: stack with durability, and clothes; per-player (§9) | E8 |

### The closing section every guide must carry (Doc B22 §3.10)

| Part | R05 |
| --- | --- |
| **Commands** | `Awaken.Equipment.Equip`, `.Unequip`, `.Hit`, `.Wear`, `.Dump` (read), `.FillStack` (the situation most likely to break: a full stack), `.StackCap` (cvar) |
| **Debug view** | `Awaken.Debug.Equipment` (each player's pieces top-first, wear, aggregate, cap, clothes, warmth) |
| **Log category** | `LogAwakenEquipment`, role-prefixed, one Log line per state change; refusals log their reason tag through R01's request component |
| **Automated tests** | Armor stack (tier 1), Armor damage (tier 1), Clothes (tier 1), Bag lifecycle (tier 2), Save round trip (tier 2), Replication and requests (tier 3, manual two-player) |
| **Two-player check** | E10.7 |
| **Done when** | the commands run, the tests pass, E10.7 has been done on a listen server plus one client and once offline |

---

# Appendix A — Files To Create (Doc02 §20.3 layout)

```
Public/EquipmentFramework/
    01Structs/S_EquipmentFramework.h
    02Tags/T_EquipmentFramework.h
    03Interfaces/BPI_ArmorStatsSink.h  BPI_CosmeticEventSink.h          (outgoing seams)
    04Delegates/D_EquipmentFramework.h
    EquipmentLog.h   EquipmentServices.h   UEquipmentSettings.h
    UDA_ArmorStackLayout.h   UEquipmentComponent.h   UBFL_Equipment.h
Private/EquipmentFramework/
    02Tags/T_EquipmentFramework.cpp
    EquipmentLog.cpp   EquipmentServices.cpp   UEquipmentSettings.cpp
    UDA_ArmorStackLayout.cpp   UEquipmentComponent.cpp   UBFL_Equipment.cpp
    Stubs/EquipmentStubs.h/.cpp
    DevTools/EquipmentDevCommands.cpp   EquipmentDebugView.cpp
    Tests/EquipmentSpec.cpp   EquipmentSaveSpec.cpp
Content/Awaken/Items/*            (armor and clothes DA_ItemDefinition instances)
Content/Awaken/Equipment/DA_ArmorStackLayout
```

Files of the R01 guide that this guide **patches**: `UDA_ItemDefinition.h/.cpp` (E2.1, E2.2), `InventoryTestFixture.h/.cpp` (E2.5), `AInventoryTestPlayerState.h/.cpp` (E3.7), `InventoryStubs.h/.cpp` and `InventoryServices.cpp` (E7.2, stub removed), `UBFL_InventoryBag.cpp` (E7.3), `UInventoryRequestComponent.h` (E9.2).

> R05 §11 lists `S_EquippedArmorState.h` and `S_WornClothesState.h` as separate files. That list is older than Doc02 §20.3, which asks for one struct file per framework; this guide follows §20.3 and puts both structs in `S_EquipmentFramework.h`.

# Appendix B — Tags and data fields to register

- **Already in the Tag Registry (owner R05):** `Equipment.ArmorEquipped`, `Equipment.ArmorUnequipped`, `Equipment.ArmorBroken`, `Equipment.ClothesChanged`, `Equipment.StatsRecalculated`, `Equipment.ArmorStack.*`, `Cosmetic.Event.Combat.ArmorBroken` (owner B21).
- **New leaves:** `Equipment.ArmorStack.Position01` to `Position14`.
- **New, PROPOSED:** `Equipment.FailReason.StackFull`, `.NotArmor`, `.NotClothes`, `.AlreadyWorn`, `.InvalidStackIndex`.
- **Reused from other owners:** `Inventory.FailReason.NoBag`, `.NotFound`, `.NotServer`, `.ReservedQuantity`, `.InsufficientQuantity`, `.DeathBagLocked`, `.OutOfRange`, `.RateLimited` (R01); `Interaction.FailReason.BagFull`, `.HandsFull`, `.NotAllowedInState` (B18).
- **New data fields to log in the Index:** `armorValue`, `clothesProtection`, `warmth`, `armorMesh`, `outfitMesh` on `DA_ItemDefinition`; `itemId` on `S_EquippedArmorState` (E3); the settings `armorStackCap` and `armorStackLayout`; the asset type `DA_ArmorStackLayout`; the save key `Equipment.Character`.

# Appendix C — Open points to confirm with the designer or teammates

| # | Point | Who |
| --- | --- | --- |
| 1 | **E2.** When a piece in the middle of the stack leaves, the pieces above move down one position, so their place on the body changes. The alternative is that every piece keeps its place and the next equip fills the hole; that needs an extra "stacked order" number to know which piece is hit first | Designer |
| 2 | **E4 / E5.** Armor loses durability equal to the damage it absorbed; clothes protection is a flat amount taken off before the armor. `DurabilityLossPerUse` is unused for armor | Designer, R03 owner |
| 3 | **How the player equips and unequips armor in the UI.** Doc B17 3.6 names a *wear* hover button for clothes only. This guide provides the requests (`serverEquipArmor`, `serverUnequipArmor`); B17 needs a rule for where the buttons are | Designer, B17 owner |
| 4 | **E10.** There is no way to take clothes off and return to the default outfit | Designer |
| 5 | **Clothes picked up from the ground** go to the bag like any item and are worn from there. Only clothes taken from an open container use the "old ones are dropped" rule | Designer |
| 6 | **Doc B22 §3.7**, starter test "R03 Damage pipeline order" says "Armor, then clothes, then health". Doc R03 3.7 and Doc R05 say clothes first. This guide follows R03 and R05; the B22 row should be reworded | B22 owner |
| 7 | **NPC armor.** Doc B12 lists Equipment as a hard dependency for "NPC armor/weapon equip state", but Doc R08 says an NPC's armor value comes from its `DA_NPCProfile`. This guide covers **players only** (the component is on the PlayerState) | B12 / R08 owners |
| 8 | **Doc R06** says Equipment calls `IsUnlocked()` for weapon-upgrade recipe gating. Weapon upgrades are ordinary crafting recipes (R05 3.4), so that check is made by Crafting (Doc R02), and Equipment has no code for it | R06 / R02 owners |
| 9 | **E8.** The two mesh fields and the layout asset are PROPOSED; Doc R07 owns how armor and outfits are shown | R07 owner |
| 10 | **Warmth.** One number per item, summed over armor and clothes. How much it lowers cold exposure is data of Doc B14 / B03 | B14 / B03 owners |
| 11 | **Stale text in Doc R05** to clean up at the next edit: the first amendment notice still says "Sections 4.1 … and 4.3 … remain open" (both are resolved); §5 says "the one open item in Section 4" (resolved in A115); §11 file list (see Appendix A) | You (R05 owner) |
| 12 | **E13.** A player whom Doc B06 leaves Bagless at join must not get the armor of their older Character file back | B06 / R13 owners |

# Appendix D — Definition of done for the framework

Eleven sections (0 to 10) delivered and reviewed with every task ticked; `Awaken.Item.Validate` reports zero errors on all armor and clothes content and the layout asset; every test in the Verify tables passes; E10.7 done on a listen server plus a client and once offline; R01's `UStubEquipmentGear` is deleted and R01's own bag tests still pass with the real seam; every stub in the register has an owner and a removal condition; the closing Dev Tools section is in place.

> **Working method from here:** tell me **"start Section 0"** (or paste the first compiler error of whichever section you built) and we go section by section, fixing as we compile, the same way as R01. If a name in 0.4 differs in your real R01 code, send it first and I will adjust the guide before you begin.
