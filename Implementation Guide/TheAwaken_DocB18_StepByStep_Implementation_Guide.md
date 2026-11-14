# Interaction Core Framework: Implementation Guide

Oct 24, 2026 · @Silver · Revised Oct 25, 2026 after a review against Doc B18, B11, Doc02, Doc00 and the Tag Registry (see Revision notes at the end)

## Before you start

This guide builds Doc B18 (Interaction Core) in 12 steps: a client-side focus trace, a prompt, and server-checked requests, holds, locks and noise. It follows Doc B18 and Doc B11 as written. Where those specs and Doc02 disagree on names, Doc02 section 20 wins; the guide says so each time.

**You build**

- Focus: what the player is looking at, one target, local only (B18 3.1)
- The base interface and the three kinds: Press, Hold, Grab (3.2)
- The prompt data handed to the UI (3.3)
- Input through Enhanced Input (3.4)
- Server checks, holds, locks, interruption (3.5 to 3.8)
- The noise hook for enemies (3.9)

**You do not build** doors, windows, containers, Sleep, Save, Harvest, Escape, revive, climbing, or the prompt widget. Those belong to B11, B03, R13, B08, B02, R07 and B17. Your job is the base they plug into.

**Rules every step obeys**

- Clients send intent, the host decides (Doc02 19.6). Nothing is applied partially; the first failed check wins and returns an `Interaction.FailReason.*` tag (Doc02 6).
- Gameplay Messages are local to the machine that sends them. Anything a client needs is re-raised there by an RPC or a replicated flag (Doc02 19.2).
- Hold times are short gameplay timers on real time, not GameTime (Doc02 19.4).
- Ranges, rates, durations and loudness are data. No number in this guide is a design value.
- Content grows through tags, not enum values. Code follows Doc02 20: camelCase, no `b` prefix, `){` on one line, `}` alone on its line.

**What you need from other frameworks**

Call them through tags and interfaces only, never a hard class reference to something not built yet (Doc00 4.2). Until each exists, use the stub in the right column.

| Framework | What you need | In the project today |
| --- | --- | --- |
| Camera (B04 3.1) | Client: `getAimTraceOrigin()` and `getAimTraceDirection()` (the names B04 3.1 gives, camelCased). Host: pawn position plus a stance eye height, never the camera | Folder exists, files empty. Stub: the client pair returns camera location and forward vector; the host eye position is `GetPawnViewLocation()` with a test value for the eye height |
| Carry (B07 3.6) | Is a carry slot occupied | `T_Carry_State_Active` tag on the player's Ability System Component. `UCarryComponent` is a stub, so check the tag |
| Checkpoint (B02) | Is the player Downed, Restrained or Arriving | `T_Player_State_Incapacitated` (in `T_AbilityActionFramework.h`) on the same component. `E_PlayerLifeState` is a temporary enum |
| Stats (R08) | A signal when the player takes damage | `UAS_CoreVitals::currentHPAttribute()` and the component's attribute-change delegate |
| Cinematic (R17) | Scene started signal, "locked into a scene" query | Files empty. Stub: always false |
| AI (B12) | Hearing entry point for noise | Files empty. Stub: log the call |
| UI (B17) | Hold-style setting, prompt rendering | Files empty. Stub: print to screen |
| Session (B20) | `Session.PlayerLeft` | Not declared yet. Call the cleanup by hand in a test |
| Localization (R10) | Refusal tag to text | Built: `ULocalizationService::getReasonText(tag)` |

**The project already has the shells.** The folders `InteractionCoreFramework` and `InteractionFrameworkExtension` exist with empty files in the right places, `UInteractionComponent` is a temporary stub that `AAwakenCharacter` already creates, and the Save Point (`SavePoint.h`) is written to the function shapes in Step 3. This guide fills those files in. It does not create a parallel layout.

Names marked **PLACEHOLDER** in the code are not given in any spec and do not exist in the project yet. Confirm the real name in the owning document before you compile against it.

## Step 1: Fill in the existing folders and files

The module is `TheAwaken` and its class macro is `THEAWAKEN_API`. `EnhancedInput`, `GameplayTags`, `GameplayAbilities` and `GameplayMessageRuntime` are already in `TheAwaken.Build.cs`, so no build change is needed. The files below already exist and are empty unless noted. Open each one and fill it in, so later steps only add code:

```
Public/InteractionCoreFramework/
    00Enums/E_InteractionCoreFramework.h
    01Structs/S_InteractionCoreFramework.h
    02Tags/T_InteractionCoreFramework.h
    03Interfaces/BPI_Interactable.h
    InteractionComponent.h              (temporary stub today)
Private/InteractionCoreFramework/
    02Tags/T_InteractionCoreFramework.cpp
    InteractionComponent.cpp            (temporary stub today)

Public/InteractionFrameworkExtension/
    02Tags/T_InteractionFrameworkExtension.h
    03Interfaces/BPI_InteractableHoldToActivate.h
    03Interfaces/BPI_InteractableToggle.h      (later, Doc B11 pass)
    03Interfaces/BPI_InteractableGrabbable.h   (later, Doc B11 pass)
Private/InteractionFrameworkExtension/
    02Tags/T_InteractionFrameworkExtension.cpp
```

Add these new files in the same style (Doc02 20.3, one file per category):

```
Public/InteractionCoreFramework/
    04Delegates/D_InteractionCoreFramework.h   (only if you add a delegate; otherwise skip)
    DA_InteractionSettings.h
    InteractionLockComponent.h
Private/InteractionCoreFramework/
    InteractionLockComponent.cpp
    DA_InteractionSettings.cpp
    Tests/InteractionCoreTest.cpp              (Step 12)
```

Content assets, made in the editor: `IA_Interact`, `IA_OpenMap`, `IA_Chat`, their entries in a mapping context, and one `DA_InteractionSettings` instance.

- **Keep the stub's class name and constructor.** `AAwakenCharacter` creates `UInteractionComponent` in its constructor and holds it as `interaction`. Fill the same class in place, and delete the word TEMPORARY from both file headers when you finish.
- **The hold interface lives in the Extension folder**, not the Core folder, because it is a Doc B11 file. Build it now since holds (Step 7) cannot work without it. Toggle and Grabbable wait for the B11 pass.
- `InteractionLockComponent` and `DA_InteractionSettings` are not in Doc B18's class table. The designer approved them on 2026-10-24; log them in the Index and amend Doc B18 (see Open points).
- Declare a log category `LogAwakenInteraction` in `InteractionComponent.h`. Doc B22 assigns that name to B11 and B18, and every built framework declares its own (for example `LogAwakenSave`).

## Step 2: Enums, structs and tags

Declare the vocabulary before any logic. Doc B18 writes the prompt field `bHold`; Doc02 20.1 forbids the `b` prefix, so it becomes `isHold`.

**`E_InteractionCoreFramework.h`** (enum values are PascalCase; `E_InteractionLockPolicy::None` is legal on an enum class)

```cpp
UENUM(BlueprintType)
enum class E_InteractionKind : uint8{
    Press,
    Hold,
    Grab
};

UENUM(BlueprintType)
enum class E_InteractionLockPolicy : uint8{
    None,
    Shared,
    Exclusive
};
```

**`S_InteractionCoreFramework.h`** holds every struct. Text is stored as localization keys (`FName`), never sentences (Doc R10 3.3).

```cpp
USTRUCT(BlueprintType)
struct FS_InteractionPrompt{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite)
    FName verbText;
    UPROPERTY(BlueprintReadWrite)
    FName objectName;
    UPROPERTY(BlueprintReadWrite)
    FKey key;
    UPROPERTY(BlueprintReadWrite)
    bool isHold = false;
    UPROPERTY(BlueprintReadWrite)
    float holdProgress = 0.0f;
    UPROPERTY(BlueprintReadWrite)
    FGameplayTag blockedReason;
};

USTRUCT(BlueprintType)
struct FS_InteractionNoise{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite)
    float loudness = 0.0f;
    UPROPERTY(BlueprintReadWrite)
    FGameplayTag noiseTag;
};
```

Also declare the internal enum `E_InteractionHoldCancelReason` in `E_InteractionCoreFramework.h` (`Released`, `OutOfRange`, `NoLineOfSight`, `Damaged`, `Downed`, `InUse`, `Scene`, `Disconnected`; **PROPOSED**, because the spec names a reason in the message but not its values). In `S_InteractionCoreFramework.h`, declare the three message payloads (`FS_InteractionFocusMessage` with `player` and `target`; `FS_InteractionHoldMessage` with `player`, `target` and `cancelReason`, used by `HoldCancelled`; `FS_InteractionCompletedMessage` with `interactionTag` and `instigator`) and one runtime struct, `FS_ActiveHold`, used in Step 7. None of these are saved (B18 section 8).

**Tags** are declared natively, the way every built framework does it: `UE_DECLARE_GAMEPLAY_TAG_EXTERN` in the header (with `#include "NativeGameplayTags.h"`) and `UE_DEFINE_GAMEPLAY_TAG_COMMENT` in the `.cpp`, with a one-line comment. See `T_SaveFramework.cpp` for the style (Doc02 20.2):

```cpp
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Interaction_FailReason_OutOfRange);
UE_DEFINE_GAMEPLAY_TAG_COMMENT(T_Interaction_FailReason_OutOfRange, "Interaction.FailReason.OutOfRange", "The target is farther than the interaction range");
```

Split the tag strings across the two existing tag files:

| File | Tags |
| --- | --- |
| `T_InteractionCoreFramework` | Refusal reasons: `Interaction.FailReason.OutOfRange`, `NoLineOfSight`, `InUse`, `NotAllowedInState`, `HandsFull`, `BagFull`, `EnemyAware`, `Cooldown`, `HostOnly`, `Busy`, `Bagless`. Messages: `Interaction.FocusChanged`, `HoldStarted`, `HoldCancelled` |
| `T_InteractionFrameworkExtension` | Type tags (B11 3.1, B18 section 5, and the Tag Registry for `Revive`): `Interaction.Door`, `Window`, `Fence`, `Furniture`, `Container`, `FireSource`, `Machine`, `Generator`, `WorldObject`, `Throwable`, `Placeable`, `Escape`, `Revive`, `Climb`, `Rope`. Messages: `Interaction.HoldToActivateCompleted`, `ToggleStateChanged`, `GrabBegun`, `GrabEnded` |
| Already defined, do not redefine | `Interaction.Save` (`T_Interaction_Save`, in `T_SaveFramework`), `Player.State.Incapacitated` (`T_AbilityActionFramework`), `Carry.State.Active` (`T_CarryEncumbranceFramework`) |

**Tag convention (settled by the project).** Save, Session Handshake, Ability & Action and Stats all declare their tags natively, and the project has no `DefaultGameplayTags.ini`. Doc B18 section 11 mentions the ini file, so do not follow that line and do not add an ini. `EnemyAware`, `Cooldown`, `HostOnly` and `Busy` are shared with Doc R11, which is still empty, so they are declared here. `Interaction.FailReason.Bagless` belongs to Doc R13, but nothing declares it yet, so declare it here too and tell the Save owner. The Tag Registry assigns `Interaction.Revive` to Doc B02, `Interaction.Escape` to Doc B02 and B11, and `Interaction.Rope` to Doc B09 and B18 (it is in B18's own tag list, approved in Index A101). None of them is declared yet, so they are declared here. Tell the B02, B09 and B11 owners to use these and not declare their own, because a second definition with the same C++ name will not link. Doc B18 section 11 still names `DefaultGameplayTags.ini`; that line needs amending too (Open points).

The Tag Registry marks several of these PROPOSED. A name that is not yet approved can still be used, but do not rename any of them after the first save format ships (Doc02 19.8).

## Step 3: The interactable interface

Every interactable implements one interface (B18 3.2). Doc B18 names it `IInteractable`; Doc02 20.2 requires the `UBPI_` / `IBPI_` pair, so the file is `BPI_Interactable.h`.

```cpp
UINTERFACE(BlueprintType)
class UBPI_Interactable : public UInterface{
    GENERATED_BODY()
};

class IBPI_Interactable{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FGameplayTag getInteractionTag() const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    E_InteractionKind getInteractionKind() const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool canInteract(AActor* instigator, FGameplayTag& failReason) const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    FS_InteractionPrompt getPrompt(AActor* instigator) const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    void interact(AActor* instigator);
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    E_InteractionLockPolicy getLockPolicy() const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool getNoise(FS_InteractionNoise& noise) const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    int32 getInteractionPriority() const;
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
    bool isInteractableBy(AActor* instigator) const;
};
```

Rules for implementers, which you will enforce in Steps 5 and 6:

- `canInteract` and `getPrompt` are cheap and **read-only** (Doc02 4). The client calls them for the prompt, the server calls `canInteract` again for the request.
- `canInteract` returns a refusal as a tag in `failReason`, never a bare `false`. An object may add its own tags, such as the sleep gate's enemies-nearby reason.
- `interact` runs on the server only and applies state last.
- `getNoise` returns `false` when the object is quiet.

**Hold interface** (`InteractionFrameworkExtension/03Interfaces/BPI_InteractableHoldToActivate.h`, from B11 3.4): `getHoldDuration()` returning seconds, `canActivate(instigator, failReason)` and `onActivationComplete(instigator)`. A Hold object implements both interfaces; its `interact` is not used. The Save Point in `SavePoint.h` already uses exactly these three signatures, so match them.

**Two additions to the spec, approved by the designer on 2026-10-24.** The focus rules need an `InteractionPriority` ("data on the object", B18 3.1), and the death-bag rule needs a way to hide an object from everyone but its owner (3.5, Doc B02 4.3). Neither is in the function table. They are in the interface block above as `getInteractionPriority()` (default 0) and `isInteractableBy(instigator)` (default true). Give the interface class a default `_Implementation` for each, so existing implementers need no change. Log them in the Index and amend Doc B18 (Open points).

**Wiring the Save Point.** `SavePoint.h` has a four-step checklist for when this interface exists: include both interface headers, inherit `IBPI_Interactable` and `IBPI_InteractableHoldToActivate`, rename its five functions to `<name>_Implementation` and mark them `override`, and return `T_Interaction_Save` from `getInteractionTag`. Add `getInteractionKind` (Hold) and `getLockPolicy` (Exclusive) the same way. Do this as the first real customer (Step 12).

## Step 4: Input

One Enhanced Input action drives press, hold and grab (B18 3.4). The object's kind decides what the press means.

1. Create `IA_Interact` as a Digital (bool) action. Map it to **E** in the gameplay mapping context. Players can remap it in Settings. There is no gamepad at launch (D7), but keep the binding in the mapping context so one can be added.
2. Create `IA_OpenMap` (default M) and `IA_Chat`. Both are rebindable data. They are not read in a scene or a loading cover; the mode owner in Doc B17 3.3 decides that by which context is active.
3. Do **not** add `IA_Interact` to the Menu, Dialogue, Scene or Loading contexts. B17 swaps mapping contexts with its input mode, so an open bag screen or a scene removes interaction input without any check in your code (B18 3.4, R17 input lock).
4. Bind two trigger events, not Triggered: **Started** calls `onInteractPressed()` and **Completed/Canceled** calls `onInteractReleased()`. Triggered fires every frame and would flood the server.

Bind from the Character, because `UInteractionComponent` lives there (D5). The controller's `SetupInputComponent` already binds `UActionInputComponent`, but the pawn does not exist yet at that point, so it cannot reach the interaction component. Override `SetupPlayerInputComponent` on `AAwakenCharacter` (an Unreal-forced name, Doc02 20.5) and forward it. Unreal calls it on the locally controlled pawn only, which is what Step 5 needs.

```cpp
void AAwakenCharacter::SetupPlayerInputComponent(UInputComponent* playerInputComponent){
    Super::SetupPlayerInputComponent(playerInputComponent);
    interaction->bindInput(playerInputComponent);
}

void UInteractionComponent::bindInput(UInputComponent* playerInputComponent){
    UEnhancedInputComponent* input = Cast<UEnhancedInputComponent>(playerInputComponent);
    if(input == nullptr || settings == nullptr || settings->interactAction == nullptr){
        UE_LOG(LogAwakenInteraction, Warning, TEXT("bindInput: no Enhanced Input component or no interaction settings, interaction input is not bound"));
        return;
    }
    input->BindAction(settings->interactAction, ETriggerEvent::Started, this, &UInteractionComponent::onInteractPressed);
    input->BindAction(settings->interactAction, ETriggerEvent::Completed, this, &UInteractionComponent::onInteractReleased);
    input->BindAction(settings->interactAction, ETriggerEvent::Canceled, this, &UInteractionComponent::onInteractReleased);
    startFocusTimer();
}
```

`bindInput` also starts the focus timer (Step 5). Unreal calls `SetupPlayerInputComponent` only on the locally controlled pawn, and calls it again after a new possession; starting a timer that already runs just restarts it. `settings` must be loaded before this runs; if your load is asynchronous, call `bindInput` from its completion. Stop the timer and clear the focus in `EndPlay` and when the pawn is unpossessed. In hold-style mode, also send `serverReleaseInteract` at those two points, and when B17 removes `IA_Interact` from the active contexts, so a key release can never be lost and leave the server completing a hold nobody is holding (Open points).

Two project details. Put the mapping context that holds `IA_Interact` in the `defaultMappings` list of `AAwakenPlayerController` (set in the controller Blueprint); it already adds them on the local player. And call the controller's `noteInputActivity()` at the top of `onInteractPressed`, because Doc R12 (R12-IG13) asks every input callback to do so, which keeps a player who only interacts from being marked Away.

**Press-once accessibility.** A hold may be set to "press once instead of hold" (B17 3.13, B18 section 10). Read that setting in `onInteractPressed`. In hold mode, send a release when the key goes up. In press-once mode, never send a release, and treat a second press during the hold as a cancel. The server does not need to know which mode the client uses (see Step 7).

## Step 5: The component and local focus

`UInteractionComponent` already exists as a stub on the Character; fill it in rather than adding a second one. Its data comes from one `DA_InteractionSettings` asset, a `UPrimaryDataAsset` loaded the way `UDA_LocalizationSettings` is (a static `makeAssetId()` and the Asset Manager), so no number lives in code (Doc00 2.7).

| Setting | Used for |
| --- | --- |
| `interactionRange` | Candidate distance in focus and in the server check |
| `sweepRadius` | Thickness of the focus sweep, so thin objects are easy to hit |
| `focusInterval` | Seconds between focus updates |
| `rangeSlack` | Extra server range for movement between press and arrival (**PROPOSED**) |
| `reasonLineSeconds` | How long the refusal line stays up |
| `lockWatchInterval` | Check rate for the range watch (Step 8), active only while a hold runs or a lock is held |
| `allowedWhileCarrying` | Tag container for Step 6, check 5 |
| `interactionChannel` | Collision channel the sweep uses (**PROPOSED**). Interactables Overlap it, solids Block it (Collision setup below) |

The asset also holds `interactAction`, the `IA_Interact` pointer used in Step 4. Do not choose values for any of these. B18 section 10 says range and rate are data and are not chosen. Enter obvious test values and label the asset "TEST ONLY".

**Collision setup.** `SweepMultiByChannel` stops at the first blocking hit. Set every interactable to **Overlap** on `interactionChannel` and let walls and other solids **Block** it. The sweep then returns every interactable in front of the first solid, and rule 3 below has a real choice. If interactables blocked the channel, the sweep would return only the nearest one.

**Focus runs on the owning machine only and is never replicated** (B18 3.1, 3.10). Start the timer from `bindInput` (Step 4), which Unreal calls only on the locally controlled pawn. Do not start it in `BeginPlay`: on a client the possession may not have arrived yet when `BeginPlay` runs, and the timer would never start.

```cpp
void UInteractionComponent::updateFocus(){
    FVector origin = getAimTraceOrigin();
    FVector direction = getAimTraceDirection();
    TArray<FHitResult> hits;
    FCollisionShape shape = FCollisionShape::MakeSphere(settings->sweepRadius);
    FVector end = origin + direction * settings->interactionRange;
    GetWorld()->SweepMultiByChannel(hits, origin, end, FQuat::Identity, settings->interactionChannel, shape);
    AActor* best = pickBest(hits, origin, direction);
    setFocus(best);
}
```

`getAimTraceOrigin()` and `getAimTraceDirection()` are the names Doc B04 3.1 gives, camelCased per Doc02 20.1. B04's files are empty, so until they exist write them as two private functions of this component that return the camera location and forward vector, and swap the bodies for the B04 calls later. Never read the camera component from anywhere else.

`pickBest` applies the B18 3.1 rules in this order:

1. Keep only actors that implement `BPI_Interactable`, return true from `isInteractableBy`, and are within `interactionRange`, measured from the origin to the closest point of the hit's collision, inclusive (Doc02 19.9; **PROPOSED**, because B18 does not say what is measured).
2. Keep only those with a clear line from the origin (a line trace that is not blocked before the target).
3. Choose the one closest to the centre of the view, meaning the smallest angle between `direction` and the direction to the object.
4. Break a tie by the higher `getInteractionPriority()`, then by the shorter distance.

`setFocus` runs on every focus tick. When the target changed, it stores the weak pointer and broadcasts `Interaction.FocusChanged` locally. Whether or not the target changed, it then rebuilds the prompt, because the prompt can change while the player keeps looking at the same object (an enemy turns aware, another player takes the object, the key is remapped):

1. Call `getPrompt` on the target.
2. Stamp what the object cannot know: `key` (the key mapped to `IA_Interact` in the Enhanced Input mapping) and `holdProgress` (from the client's local hold clock, Step 7).
3. Fill `blockedReason` (Prompt rule below).
4. Compare with the stored prompt. If anything differs, store it and broadcast `Interaction.FocusChanged` again. The UI re-reads the prompt through `getFocusedPrompt()`. This adds no new tag (**PROPOSED**; confirm with Doc B17). `holdProgress` only updates at the focus rate, so the widget can animate between updates from the hold duration.

Exactly one object is in focus, and only it shows a prompt.

**Prompt rule.** Fill `blockedReason` from the target's `canInteract`. If that leaves it empty, and the target has an `UInteractionLockComponent` with `isInUse` true and policy `Exclusive`, set `InUse`. Make one exception: while this player's own hold on that target is running, leave it empty (the client knows this from `clientHoldStarted`; `isInUse` is a bare bool and does not say who holds it). The UI shows the prompt only when `blockedReason` is empty (Doc B17, B18 3.3). If the player presses E while the focused target is blocked, the press is sent like any other (Step 6). The server refuses it, and `clientInteractRefused` shows the reason line for `reasonLineSeconds`. The designer-approved behaviour is unchanged: the prompt is hidden and pressing the key shows the reason. Only the route is **PROPOSED**. Sending the press lets the host's check, not a possibly stale client copy, decide, and keeps one code path (Open points).

## Step 6: Requests and the server checks

The client never decides the outcome (B18 3.5, Doc02 19.6). It sends the target; the host checks it.

```cpp
UFUNCTION(Server, Reliable)
void serverRequestInteract(AActor* target);
UFUNCTION(Client, Reliable)
void clientInteractRefused(AActor* target, FGameplayTag reason);
```

Unreal forces the implementation names `serverRequestInteract_Implementation` and `clientInteractRefused_Implementation`. Name them as Doc02 20.5 exceptions in your code review notes. Both RPCs live on a component, and a component's RPCs only route when the component replicates. Call `SetIsReplicatedByDefault(true)` in `UInteractionComponent`'s constructor (check the stub; it may not). The component has no replicated properties of its own.

`onInteractPressed` calls `serverRequestInteract(getFocusedTarget())`. Offline play and the listen-server host go through the same function, so there is one code path (Doc00 5.1). It sends even when the prompt is blocked (Step 5). If a hold is already running, the server handles the request first (Step 7), before the checks below.

Inside the implementation, run **validateRequest** and return at the first failure. The order is fixed by B18 3.5:

| # | Check | Refusal tag |
| --- | --- | --- |
| 1 | Target exists, implements the interface, and `isInteractableBy(player)` is true (a death bag is visible only to its owner, B02 4.3) | none: ignore the request, no prompt existed |
| 2 | Player state allows it: not `Player.State.Incapacitated` (Downed, Restrained, Arriving), not locked into a scene. Exceptions: `Interaction.Escape`, and a teammate acting on a Downed player (`Interaction.Revive`; the reviver must be Healthy or Bagless and not Downed or Restrained) | `NotAllowedInState` |
| 3 | Target within `interactionRange + rangeSlack` of the **host's** eye position (inclusive, Doc02 19.9; measured as in Step 5), with a clear line | `OutOfRange`, `NoLineOfSight` |
| 4 | The target's lock policy allows it (Step 8) | `InUse` |
| 5 | Hands: if a carry slot is occupied, the interaction tag must be in `allowedWhileCarrying` | `HandsFull` |
| 6 | The object's own `canInteract` | the object's tag |

Notes that prevent common mistakes:

- **Use the host's eye position.** Doc B04 3.1 defines it as the pawn position plus a stance eye height. The host never reads a client's camera. Until B04 exists, use `GetPawnViewLocation()` (pawn position plus `BaseEyeHeight`) with a test value. Do not use the camera component: on the host it does not follow a remote client's view.
- **Carrying.** B18 3.5 and section 10 allow only hands-free interactions (doors, switches), plus the ones that use the carried item: blueprint deposit, placing, sending on a rope. Fill `allowedWhileCarrying` with those tags and confirm the list against B07 3.6. Drop and throw are not interactions here.
- **No partial application.** Checks 1 to 6 only read state. `interact` runs after check 6 passes, and so does lock acquisition (Doc02 6).

After the checks pass, branch on `getInteractionKind()`:

```cpp
switch(kind){
    case E_InteractionKind::Press:
        acquireLock(target);
        IBPI_Interactable::Execute_interact(target, GetOwner());
        reportNoise(target);
        releaseIfPressOnly(target);
        break;
    case E_InteractionKind::Hold:
        beginHold(target);
        break;
    case E_InteractionKind::Grab:
        // Doc B11 3.3 owns the grab. Acquire the Exclusive lock and call its beginGrab.
        break;
}
```

`acquireLock` and `releaseIfPressOnly` are small private helpers. Both do nothing when the target has no `UInteractionLockComponent`. `releaseIfPressOnly` frees a Press object's lock right after the use, unless the object keeps it (Step 8).

On refusal the server calls `clientInteractRefused`, and the owning client shows the reason line. The core spec gives no message for this; see Open points.

**Real hooks for checks 2 and 5.** The player's Ability System Component lives on the PlayerState, and `AAwakenCharacter` implements `IAbilitySystemInterface`, so the global lookup works from the pawn. Both states are tags the project already defines:

```cpp
UAbilitySystemComponent* asc = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
bool isIncapacitated = asc != nullptr && asc->HasMatchingGameplayTag(T_Player_State_Incapacitated);
bool isCarrying = asc != nullptr && asc->HasMatchingGameplayTag(T_Carry_State_Active);
```

Read the tags, not `E_PlayerLifeState`, which is a temporary enum that Doc B02 will replace. `T_Carry_State_Active` is what Ability & Action already blocks Sprint and Dodge on; confirm that it also means "a carry slot is occupied" once Doc B07 is built. The scene check stays a stub until Doc R17 exists.

## Step 7: Holds

Holds are Save, Sleep, Escape, Harvest and revive. **The server starts the hold and the server completes it.** The client's progress ring is a display (B18 3.6, section 7).

The runtime record, never saved:

```cpp
USTRUCT()
struct FS_ActiveHold{
    GENERATED_BODY()
    TWeakObjectPtr<AActor> target;
    FGameplayTag interactionTag;
    double endTime = 0.0;
    bool isDue = false;
};
```

A player can run one hold at a time, so the component keeps one `FS_ActiveHold` and one `FTimerHandle`.

**beginHold(target)** on the server:

1. Call `canActivate` once now (B11 3.4). On failure, refuse with the object's tag.
2. Acquire the lock (Step 8). A `None` object has no lock, and the hold still runs.
3. Store the record with `endTime = now + getHoldDuration()` and `isDue = false`. Use world time on the host. It is a short gameplay timer and does not touch GameTime (Doc02 19.4).
4. Start one timer for the duration; its callback is `dueHold()`. Make sure the watch timer of Step 8 is running, because it is what cancels a hold that goes out of range. No tick.
5. Broadcast `Interaction.HoldStarted` on the server, and call `clientHoldStarted(target, durationSeconds)` so the owning client raises it locally for the UI and animation and starts its local hold clock (the source of `holdProgress`).

**dueHold()** runs from the duration timer. It does not complete the hold. It sets `isDue = true` on the record and asks for `finishHold` on the next tick with `GetWorld()->GetTimerManager().SetTimerForNextTick(...)`. The reason: Unreal's timer manager runs before the gameplay tick groups, so a hit applied later in that same frame would always arrive after a hold that completed inside the timer callback, and a lethal hit in the completion frame could never cancel it. Deferring by one tick gives that frame's damage and state changes time to arrive first. The frame in which the duration elapsed is the completion frame for the rules of Step 9, including a hit that lands in that frame before the timer fires (the end-time check in `onDamaged`).

**finishHold()** runs one tick later. It must not complete blindly. It starts with `if(!activeHold.isDue){ return; }` written in the Doc02 20.4 layout, so a stale callback after a cancel does nothing. Then, in the same-frame order of Step 9:

1. Call `shouldCancelForState` (Step 9). If it says cancel, call `cancelHold(reason)` and stop. A disconnect or a scene start has already cancelled the hold through its own handler.
2. Check the target is still valid and, for a locking object, that the lock is still held by this player.
3. Call `canActivate` **again**. A change during the hold, such as an enemy becoming aware during Sleep, still refuses it (B18 3.6).
4. Call `onActivationComplete(instigator)`.
5. Broadcast `Interaction.HoldToActivateCompleted` (server only) with the interaction tag and instigator. Doc B03 (Sleep) and B08 (Harvest) listen to it.
6. Report noise (Step 10), release the lock, clear the record.

**Release from the client.** `serverReleaseInteract()` cancels the hold with reason `Released`. It is sent only in hold-style mode. In press-once mode the client sends a second `serverRequestInteract` instead, and the server treats a request for the target already being held as a cancel. Both styles use the same server code.

**A request while a hold runs** is handled first thing in `serverRequestInteract`, before `validateRequest`, because a cancel must never be refused for state reasons. For the target already being held it cancels with `Released` and returns. For any other target it is ignored and the hold continues (**PROPOSED**). Without this rule `beginHold` would overwrite the single record and timer and leak the first lock.

**Cancelled holds keep no progress.** The next try starts from zero, for revive and Escape too, so letting go of Escape just before it completes restarts it (B18 3.8, Index A110 S2-F08).

**Durations are data.** Escape is 3 seconds and lives on the checkpoint's data asset (B02 4.5). The Save hold, Sleep and Harvest values come from their own owners through `getHoldDuration`. Do not hard-code any of them in this component.

**Each object owns its own cancel conditions** through `canActivate`, which runs at the start and again at completion. Save: refused if the requester is Downed, Restrained or Bagless, leaves, an aware enemy appears, or the Save Point is destroyed; once the capture has happened the save always completes (Doc R13 3.11). Sleep: an aware enemy near the bed (Doc B12 `IsPlayerDetected`). Harvest: the tool rule of Doc B08. This framework only calls `canActivate`; it does not know these rules.

## Step 8: Locking

When two players use one thing, the object's lock policy decides (B18 3.7). The lock is state on the interactable and lives on the host.

| Policy | Behaviour | Typical objects |
| --- | --- | --- |
| `None` | Anyone, any time. The host handles requests one at a time and the first wins | Pickups, doors, toggles, windows, fences |
| `Shared` | Several players at once. The host checks each action separately | Containers (confirmed: two players may look into one chest) |
| `Exclusive` | One player. Others see it as in use | Grabbed furniture, Save Point hold, blueprint deposit, crafting station |

**Build `UInteractionLockComponent`** (approved class) and add it to every actor whose policy is not `None`:

```cpp
class UInteractionLockComponent : public UActorComponent{
    GENERATED_BODY()
public:
    bool canAcquire(AActor* player, E_InteractionLockPolicy policy) const;
    void acquire(AActor* player, E_InteractionLockPolicy policy);
    void release(AActor* player);
    UPROPERTY(ReplicatedUsing = OnRep_isInUse)
    bool isInUse = false;
    UFUNCTION()
    void OnRep_isInUse();
private:
    TArray<TWeakObjectPtr<AActor>> holders;
};
```

- `holders` is server-only and not replicated. `isInUse` is the **only** replicated lock state (B18 3.7, section 7). `OnRep_isInUse` is an Unreal-forced name form (Doc02 20.5). Replicate `isInUse` with `COND_None`, because every player's prompt needs it.
- For `Exclusive`, `canAcquire` fails when `holders` holds a different player. For `Shared`, it always succeeds. Set `isInUse` only for `Exclusive`.
- The carrier rule of Doc02 19.1 applies: this is per-object state, so the actor's own replication is the carrier. No GameState entry is needed.
- `isInUse` is a bare bool, so the holder's own client sees it as in use too. The prompt rule in Step 5 makes an exception for the player's own running hold; nothing else needs the holder's identity on clients.

**Where the component keeps track.** `UInteractionComponent` holds a list of targets its player has locked, so `releaseLocksFor(player)` (B18 section 4) is a loop over that list calling `release`.

**A lock is released when** (B18 3.7):

1. The use ends. A Press object that takes a lock for one call releases it right after `interact`. A station keeps it until its own framework calls `releaseLock`, for example when the bag closes.
2. The player moves out of range. Run one watch timer at `lockWatchInterval` **only while the player has a running hold or holds at least one lock**, and stop it when neither is true (Doc02 5). On each tick, re-run check 3 of Step 6 (inclusive range from the host's eye position, plus the line) for the running hold, and cancel it with `OutOfRange` or `NoLineOfSight` on failure. Re-run the range part for every locked target and release the lock on failure. A hold on a `None` object has no lock, so the hold itself must keep the watch running.
3. The player is Downed, dies, or disconnects.
4. A scene starts (Doc R17).
5. The piece is destroyed or collapses (Doc B01 3.6). Bind to the actor's destroy event inside the lock component.

There is **no idle release** (Index A103, S4-F27). A player in range with the bag open at a station keeps it, because servers are between friends.

**Replication wiring.** In the lock component's constructor call `SetIsReplicatedByDefault(true)`. Add `GetLifetimeReplicatedProps` (an Unreal-forced name, Doc02 20.5) with `DOREPLIFETIME(UInteractionLockComponent, isInUse)`. The owning actor must itself replicate, or the flag never reaches clients.

## Step 9: Interrupting a hold

A hold ends early when any of these happens (B18 3.8). Each one calls `cancelHold(reason)`, which stops the timer, releases the lock, clears the record, and raises `Interaction.HoldCancelled` on the server and on the owning client.

| Trigger | Reason | Notes |
| --- | --- | --- |
| Player lets go of the key | `Released` | Hold-style mode only |
| Key release lost (input context swapped, pawn unpossessed) | `Released` | Hold-style mode. The client sends the release itself (Step 4) |
| Out of range, or line lost | `OutOfRange`, `NoLineOfSight` | Re-checked by the watch timer from Step 8, which runs for every hold, locked or not |
| Player takes damage | `Damaged` | **Not** for `Interaction.Escape`, because a Restrained player cannot be damaged (B02 4.5) |
| Player Downed or dies | `Downed` | Listen for `Checkpoint.PlayerDowned` and the death message |
| Target invalid or locked by another | `InUse` | Check at completion |
| Scene starts (R17) | `Scene` | The scene also releases every lock |
| Player disconnects | `Disconnected` | Handled in Step 11 |
| Revive only: the reviver is Downed or Restrained, dies, leaves or moves out of range | as above | B18 3.8, Index A105 |

**Same-frame order.** One order for every hold and every craft (Doc02 19.9, Index A103 S4-F26): **damage, then Downed or death, then disconnect or scene start, then completion.** In plain terms:

- A hit that does **not** Down or kill the player never cancels a hold that completes in that same frame.
- A hit in an earlier frame cancels normally.
- A bleed-out ending in the same step as a revive completing wins, and the revive does not complete (B02 3.5).

Implement it without a per-frame poll. The hold becomes due when its duration timer fires, and `finishHold` runs one tick later (Step 7). The damage handler checks the record's `isDue` flag and also its end time, because a hit can land earlier in the same frame than the timer that sets the flag:

```cpp
bool UInteractionComponent::isEscapeHold() const{
    return activeHold.interactionTag == T_Interaction_Escape;
}

void UInteractionComponent::onDamaged(){
    if(!activeHold.target.IsValid() || isEscapeHold()){
        return;
    }
    bool isDueNow = activeHold.isDue || GetWorld()->GetTimeSeconds() >= activeHold.endTime;
    if(isDueNow){
        return;
    }
    cancelHold(E_InteractionHoldCancelReason::Damaged);
}

bool UInteractionComponent::shouldCancelForState(E_InteractionHoldCancelReason& reason) const{
    if(isEscapeHold()){
        return false;
    }
    UAbilitySystemComponent* asc = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
    if(asc == nullptr){
        return false;
    }
    bool isDead = asc->GetNumericAttribute(UAS_CoreVitals::currentHPAttribute()) <= 0.0f;
    if(isDead || asc->HasMatchingGameplayTag(T_Player_State_Incapacitated)){
        reason = E_InteractionHoldCancelReason::Downed;
        return true;
    }
    return false;
}
```

Anything that reaches the component between the timer firing and `finishHold` counts as the same frame as completion. A non-lethal hit in that window finds `isDue` true and is ignored, as the spec requires. A non-lethal hit that lands earlier in the same frame, before the timer has fired, finds the end time already reached and is ignored too. Without that second check the hit would cancel a hold that completes in the same frame, which Doc02 19.9 forbids. A lethal hit shows up as the Incapacitated tag (or HP at 0), so `finishHold` cancels through `shouldCancelForState`, and the tag handler below cancels even sooner. A hit in an earlier frame finds `isDue` false and cancels normally. Every Escape exemption goes through `isEscapeHold()`, so the rule lives in one place. The HP check stands in for the death message until Doc B02 provides one.

**Where the signals come from.** Both come from the player's Ability System Component, which `AAwakenCharacter::bindAbilitySystem()` already finds (it runs from `PossessedBy` and `OnRep_PlayerState`). Add one line there, on the server only, `interaction->bindAbilitySystem(abilitySystem)`. It runs again after a respawn, so `bindAbilitySystem` calls `unbindAbilitySystem` first. The component keeps the bound Ability System Component as a `TWeakObjectPtr` (`boundAbilitySystem`) and the two `FDelegateHandle` values (`hpChangedHandle`, `incapacitatedHandle`) for that. Call `unbindAbilitySystem` from `EndPlay` too. The cancel reasons are the internal `E_InteractionHoldCancelReason` values from Step 2.

```cpp
void UInteractionComponent::bindAbilitySystem(UAbilitySystemComponent* abilitySystem){
    unbindAbilitySystem();
    boundAbilitySystem = abilitySystem;
    hpChangedHandle = abilitySystem->GetGameplayAttributeValueChangeDelegate(UAS_CoreVitals::currentHPAttribute()).AddUObject(this, &UInteractionComponent::onHPChanged);
    incapacitatedHandle = abilitySystem->RegisterGameplayTagEvent(T_Player_State_Incapacitated, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &UInteractionComponent::onIncapacitatedChanged);
}

void UInteractionComponent::unbindAbilitySystem(){
    if(boundAbilitySystem.IsValid()){
        boundAbilitySystem->GetGameplayAttributeValueChangeDelegate(UAS_CoreVitals::currentHPAttribute()).Remove(hpChangedHandle);
        boundAbilitySystem->RegisterGameplayTagEvent(T_Player_State_Incapacitated, EGameplayTagEventType::NewOrRemoved).Remove(incapacitatedHandle);
    }
    boundAbilitySystem.Reset();
}

void UInteractionComponent::onHPChanged(const FOnAttributeChangeData& data){
    if(data.NewValue < data.OldValue){
        onDamaged();
    }
}

void UInteractionComponent::onIncapacitatedChanged(const FGameplayTag tag, int32 count){
    E_InteractionHoldCancelReason reason = E_InteractionHoldCancelReason::Downed;
    if(count > 0 && activeHold.target.IsValid() && shouldCancelForState(reason)){
        cancelHold(reason);
    }
}
```

This is the same pattern `AAwakenPlayerState::bindGatingEvents` uses. A Restrained player is Incapacitated by design, so `Escape` is excluded through `isEscapeHold()`. A lethal hit that Downs the player arrives as an HP drop (ignored while the hold is due) and then the tag, so a hold that became due this frame still ends as Downed. There is no death message yet; end the hold and release the locks from `EndPlay` (an Unreal-forced name) so a destroyed pawn never leaves a lock behind.

## Step 10: Noise for enemies

Some interactions are loud: opening a door, running a generator, lighting a fire (B18 3.9). The object says so; the framework reports it.

1. The object returns a loudness and a noise tag from `getNoise`. Both are data on the object, not constants in this framework.
2. After a successful `interact`, or after `onActivationComplete` for a hold, call `reportNoise(target)` on the server.
3. `reportNoise` reads `getNoise`. When it returns true, it raises a hearing stimulus **at the object's location**, with the instigator, through the AI framework's entry point.
4. The call is **server only** and happens after all state changes, so a refused or cancelled use never makes noise.

```cpp
void UInteractionComponent::reportNoise(AActor* target){
    FS_InteractionNoise noise;
    if(!IBPI_Interactable::Execute_getNoise(target, noise)){
        return;
    }
    reportHearingStimulus(target->GetActorLocation(), noise.loudness, noise.noiseTag, GetOwner());
}
```

`reportHearingStimulus` is a **PLACEHOLDER** for Doc B12's hearing entry point. B12 provides the stimulus and decides what an enemy does about it (B12 owns the reaction); you only raise it. Doc B12's amendment A89 confirms it receives this hook. Until B12 is built, make the placeholder log the call so the rest of the chain can be tested.

Two things stay out of this step. Player movement noises are raised by Doc R07, and firearm shots by Doc R03, each through the same B12 hook.

## Step 11: Messages, cleanup and presentation

One mechanism only: the Gameplay Message Subsystem (Doc02 11). A message is local to the machine that sends it, so each row of B18 section 6 needs its own route to the machines that care (Doc02 19.2).

| Message | Raised on | How it reaches the listener |
| --- | --- | --- |
| `Interaction.FocusChanged` | Owning client, in `setFocus` | Already local. UI listens |
| `Interaction.HoldStarted` / `HoldCancelled` | Server, then the owning client | Server broadcasts; `clientHoldStarted` / `clientHoldCancelled` RPC re-raises it on the owner for UI and Animation |
| `Interaction.HoldToActivateCompleted` | Server only | No client route. Doc B03 and B08 listen on the host |
| `Interaction.ToggleStateChanged`, `GrabBegun`, `GrabEnded` | Owned by Doc B11 | Raised from replicated state; not built in this pass |

The in-use flag from Step 8 is the only lock state clients see. Raise nothing else for locks.

**How to send one.** Follow `UPlayerIdentityComponent`: get the `UGameplayMessageSubsystem` from the game instance and call `BroadcastMessage(tag, payload)`. The `GameplayMessageRuntime` module is already a dependency in `TheAwaken.Build.cs`.

**Cleanup on leaving.** Subscribe once, on the host, to `Session.PlayerLeft` (Doc B20 3.6). The message carries a player identity and a reason, not a pawn, so look up that player's pawn and its `UInteractionComponent` first. For the leaving player: cancel the hold with reason `Disconnected`, call `releaseLocksFor(player)`, end shared-chest views and bed occupancy, and let the sleep counter n of N update (B18 amendment A89). Do the same for a scene start from R17, and for Downed or death.

**Cosmetic events.** Doors and similar objects raise `Cosmetic.Event.Interaction.Door` and the other object events of Doc B21 A.4. The object's framework raises them, not this component. The state itself replicates. Other players' views of a door swing come from the replicated state, and the sound comes through the cosmetic channel.

**Text.** Verbs and object names are localization keys. Refusal reasons go through the tag: `ULocalizationService::getReasonText(failReason)` looks the tag up in `DT_ReasonText` (Doc R10 3.9), so interactables return tags and never sentences. Add one row to that table for every new `Interaction.FailReason.*` tag, or the reason line has no text.

## Step 12: Prove it with three test objects, then test it

Do not wait for the real doors and stations. Build three throwaway actors that exercise each path, in a test map, and delete them once Doc B11 objects exist.

| Test actor | Kind | Lock | Purpose |
| --- | --- | --- | --- |
| `ATestPress` | Press | None | Prompt, focus choice, request, noise, `canInteract` refusal |
| `ATestHold` | Hold | Exclusive | Hold start, progress, completion, every interrupt, lock release |
| `ATestShared` | Press | Shared | Two players on one object |

A minimal press object shows the whole contract (a real door adds a toggle state through B11):

```cpp
void ATestPress::interact_Implementation(AActor* instigator){
    isOn = !isOn;
}

bool ATestPress::canInteract_Implementation(AActor* instigator, FGameplayTag& failReason) const{
    if(isBroken){
        failReason = T_Interaction_FailReason_NotAllowedInState;
        return false;
    }
    return true;
}

bool ATestPress::getNoise_Implementation(FS_InteractionNoise& noise) const{
    noise.loudness = testLoudness;
    noise.noiseTag = noiseTag;
    return true;
}
```

Run every case below on a **listen server with two clients**. Offline and host-as-client are the same code path, so they are covered by the same run.

| # | Rule | Expected result |
| --- | --- | --- |
| 1 | Two objects in view | The one nearest the view centre is focused; one prompt only |
| 2 | Equal angle | Higher priority wins, then the nearer one |
| 3 | Object behind a wall | Never focused; a forged request is refused `NoLineOfSight` |
| 4 | Forged request, target far away | Refused `OutOfRange`; nothing changes |
| 5 | Press while Downed or Restrained | Refused `NotAllowedInState`; Escape still works |
| 6 | Press while carrying a heavy item | Refused `HandsFull` except for tags in `allowedWhileCarrying` |
| 7 | Hold released early | Cancelled, lock released, next try starts from zero |
| 8 | Press-once mode | One press runs the hold to the end; a second press cancels |
| 9 | Damage during a hold, earlier frame | Cancelled (not for Escape) |
| 10 | Non-lethal hit in the completion frame | Hold completes |
| 11 | Lethal hit in the completion frame | Cancelled, no completion |
| 12 | Second player on an Exclusive object | Refused `InUse`; the in-use flag shows on both machines |
| 13 | Two players on a Shared object | Both succeed |
| 14 | Lock holder leaves, is Downed, or disconnects | Lock freed immediately |
| 15 | Holder walks out of range | Lock freed within `lockWatchInterval` |
| 16 | Interaction with `getNoise` true | One hearing call at the object's location with the instigator; none on a refusal |
| 17 | Open the bag or start a scene | `IA_Interact` is not read |
| 18 | Escape hold while Restrained, damage attempted, Incapacitated tag present | Not cancelled by damage or the tag; completes |
| 19 | Hold on a `None` object, holder walks out of range | Cancelled `OutOfRange` within `lockWatchInterval`; no lock involved |
| 20 | A second request while a hold runs: same target, then a different target | Same target cancels (press-once); a different target is ignored, the first hold continues and no lock leaks |
| 21 | Keep looking at one object while its state changes (enemy turns aware, another player takes it) | The prompt updates without a focus change; the holder's own prompt keeps its progress on an Exclusive object |
| 22 | Target exactly at `interactionRange + rangeSlack`, then 1 cm farther | Allowed (inclusive, Doc02 19.9), then refused `OutOfRange` |
| 23 | Press while the focused prompt is blocked | The request is sent; the server refuses; the reason line shows for `reasonLineSeconds` |
| 24 | Input context swaps away from `IA_Interact` during a hold-style hold | A release is sent; the hold is cancelled |
| 25 | Non-lethal hit earlier in the completion frame, before the duration timer fires | Hold completes; the hit does not cancel it (end-time check in `onDamaged`) |

Run 10, 11 and 25 first, since they prove the due and finish split of Step 7 and the end-time check of Step 9. They also confirm the Unreal timer order this design assumes, which cannot be checked on paper. When all 25 pass, the framework is done. Then build Doc B11's Toggle and Grabbable interfaces and the first real objects on it.

**Automated tests.** Session Handshake keeps its tests in `Private/SessionHandshakeFramework/Tests/`. Do the same in `Private/InteractionCoreFramework/Tests/InteractionCoreTest.cpp` for the pure rules that need no world: the focus choice order, the six checks in order, the lock policy table and the same-frame order. Cases 3, 4, 12 and 13 above are good first ones. Anything with two machines stays a manual test.

**First real customer.** Wire `ASavePoint` as in Step 3. It exercises a Hold, an Exclusive lock, the hold interface and the Save gate, and needs no other framework.

## Open points

**Settled on 2026-10-24.** The designer approved the working choices below. Log the first four in the Index (Doc00 7.3).

- `getInteractionPriority()` and `isInteractableBy()` are added to the interface (Step 3).
- `UInteractionLockComponent` and `DA_InteractionSettings` are added as classes (Steps 5 and 8).
- A blocked target stays in focus with no prompt, and pressing the key shows the reason line (Step 5).
- In press-once mode, a second press cancels the hold (Steps 4 and 7).
- Tags are declared natively, with no ini file. The built project already works this way (Step 2).

**Spec amendments.** Doc00 7.1 and 1.2 require the owning documents to be amended, not only the Index. Doc B18 needs an AMENDMENT NOTICE for the two interface functions, the two classes, the prompt rule for `InUse`, and the removal of the `DefaultGameplayTags.ini` line in section 11 (Doc00 7.5). Doc B11 needs one for the hold interface signatures, which take `instigator` and `failReason` (Doc02 6, no bare false).

**Still open.** Most of these wait for another framework to be built; until then use the stub from the table in Before you start.

| Point | Working choice in this guide | Who confirms |
| --- | --- | --- |
| No message exists for a refused request, so the reason line has no route | Client RPC to the owning player. Consider a local `Interaction.RequestRefused` message | Doc B18 owner |
| Aim origin and direction (B04 files are empty) | Stub the B04 names `getAimTraceOrigin()` and `getAimTraceDirection()` from the camera, host eye from `GetPawnViewLocation()`, then swap | Doc B04 owner |
| Scene query and scene-start signal (R17 files are empty) | Stub: always false | Doc R17 owner |
| Hearing entry point (B12 files are empty) | Log-only stub | Doc B12 owner |
| Hold-style setting (B17 files are empty) | Read one bool from a stub | Doc B17 owner |
| `Session.PlayerLeft` is not declared yet | Call the cleanup by hand, and also from `EndPlay` | Doc B20 owner |
| Does `Carry.State.Active` mean "a carry slot is occupied"? Which tags go in `allowedWhileCarrying`? | Treat it as hands full. Start with doors, switches, deposit, place, rope send | Doc B07 owner, against B07 3.6 |
| Two small edits to `AAwakenCharacter`: `SetupPlayerInputComponent` and a `bindAbilitySystem` call | Add both | Owner of the class table (Doc B19) |
| `Interaction.FailReason.Bagless` is Doc R13's but nothing declares it | Declared in the Core tag file | Doc R13 owner |
| Hold cancel reasons are not named in the spec | Internal enum `E_InteractionHoldCancelReason`, never saved | Doc B17, if the UI needs them |
| The reason line on a blocked press | The press is sent and the host's refusal shows the line. Designer-approved behaviour unchanged (**PROPOSED** route) | Doc B18 owner |
| A request for another target while a hold runs | Ignored; the hold continues (**PROPOSED**) | Doc B18 owner, designer |
| Prompt refresh while the target stays the same | Re-broadcast `Interaction.FocusChanged`; the UI reads `getFocusedPrompt()`; no new tag (**PROPOSED**) | Doc B17 owner |
| What "range" measures on large objects | Closest point of the hit's collision (**PROPOSED**) | Doc B18 owner, designer |
| Which frame counts as the completion frame | The frame in which the hold's duration elapsed, including a hit that lands in it before the timer fires (end-time check); completion runs one tick later, so the Doc02 19.9 order is reachable with timers | Doc B18 owner |
| A lost key release | B17 broadcasts when `IA_Interact` leaves the active contexts. Until then the release is sent from `EndPlay` and on unpossess | Doc B17 owner |
| `Interaction.Revive` (B02), `Interaction.Escape` (B02, B11) and `Interaction.Rope` (B09, B18) | Declared here; the owners are told to use these | Doc B02, B09 and B11 owners |
| Range, focus rate, slack, sweep radius, lock-watch rate | Not chosen; test values only | Designer, after a grey-box pass |

**Known risk.** Networking a physics-simulated grabbed object responsively is flagged as an open risk in Doc B11 3.3. The host simulates and the client predicts only the target position. Treat the Grab path as its own task, after this framework works.

**What this guide does not cover.** The B11 Toggle and Grabbable interfaces and their objects; Sleep, Save, Harvest, Escape and revive logic (their owners); the prompt widget (B17); and the save format, because nothing here is saved (B18 section 8).

## Appendix: functions and co-op rules

The public surface of `UInteractionComponent` and the lock component, in the Doc02 section 4 form (B18 section 4 lists only the first six).

| Function | Kind | Summary |
| --- | --- | --- |
| `updateFocus()` | Command, timer, owning machine | Works out the focused target (Step 5) |
| `getFocusedTarget()` | Query | The object in focus, or none |
| `getFocusedPrompt()` | Query | The current prompt of the focused object, for the UI (Step 5) |
| `onInteractPressed()` / `onInteractReleased()` | Command, local | Input callbacks (Step 4) |
| `serverRequestInteract(target)` | Command, client to server | Sends the request; the host runs `validateRequest` |
| `serverReleaseInteract()` | Command, client to server | Hold-style release only |
| `clientInteractRefused(target, reason)` | Command, server to client | Shows the reason line |
| `validateRequest(target, failReason)` | Query, server | The six ordered checks (Step 6) |
| `beginHold(target)` / `dueHold()` / `finishHold()` | Command, server | Starts a hold, marks it due when its timer fires, completes it one tick later (Step 7) |
| `shouldCancelForState(reason)` | Query, server | True when the player is Downed or dead, except during an Escape hold (Step 9) |
| `cancelHold(reason)` | Command, server | Stops a hold and releases its lock (Step 9) |
| `clientHoldStarted(target, durationSeconds)` / `clientHoldCancelled(reason)` | Command, server to client | Re-raise the message locally for UI and Animation |
| `releaseLocksFor(player)` / `releaseLock(target)` | Command, server | Frees locks (Step 8) |
| `reportNoise(target)` | Command, server | Hearing hook (Step 10) |
| `bindInput(...)` / `bindAbilitySystem(...)` / `unbindAbilitySystem()` | Command | Wiring done per possession: starts the focus timer, binds and clears the signals |

**Co-op (B18 3.10).** Every player has their own focus and prompt, and nothing about focus is shared. Two players can hold different interactions at once. Only locks and the in-use flag are shared.

**Save (B18 section 8).** Nothing in this framework is saved. Holds, locks and focus are runtime state. An object's own state, such as a door being open, is saved by the framework that owns the object.

## Revision notes (Oct 25, 2026)

Changes made after reviewing this guide against Doc B18, B11, Doc02, Doc00, the Tag Registry and the specs they cite. Everything marked **PROPOSED** here is a working choice and also sits in Open points.

- **Holds:** the range and line watch now runs for every hold, not only for locks (Steps 7, 8, 9). Completion is split into `dueHold()` and `finishHold()` one tick apart, so a lethal hit in the completion frame can cancel it (Steps 7, 9). A request while a hold runs is handled before the checks (Step 7).
- **Prompt:** it is rebuilt on every focus tick, gets its `key` and `holdProgress` stamped, folds in `InUse` with an exception for the holder, and is read by the UI through `getFocusedPrompt()` (Step 5). A blocked press is sent and the host's refusal shows the line (Steps 5, 6).
- **Focus and input:** the focus timer starts from `bindInput`, collision setup is stated (Overlap for interactables), range is measured and inclusive, and lost key releases are covered (Steps 4, 5, 6).
- **Names:** the aim functions use the B04 names, the host eye stub is `GetPawnViewLocation()`, `getOwner()` became `GetOwner()` (Doc02 20.5), and the undefined `owner` is gone.
- **Samples:** the interface block shows the two added functions, `bindAbilitySystem` keeps its delegate handles, the Escape exemption lives in `isEscapeHold()`, `bindInput` logs when it cannot bind, and the component must replicate for its RPCs.
- **Tags and process:** ownership of `Revive`, `Escape` and `Rope` is stated, and the spec amendments Doc00 7.1 requires are listed.
- **Tests:** rows 18 to 25 added.

**Second pass (Oct 25, 2026).** A reviewer's findings were checked against the built project and the specs. Changes: `onDamaged` keeps the end-time check next to `isDue`, so a hit before the timer fires in the completion frame does not cancel the hold (Step 9, test 25). Tag ownership now follows the Tag Registry exactly: `Escape` is B02 and B11, `Rope` is B09 and B18 (Step 2, Open points). The project stub of `UInteractionComponent` was confirmed not to replicate, so the Step 6 replication instruction stands.
