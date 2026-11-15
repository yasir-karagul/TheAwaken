# Implementation Status — Doc R01 Item & Inventory

Guide: `TheAwaken_DocR01_StepByStep_Implementation_Guide.md` · Module `TheAwaken` (`AWAKEN_API` → `THEAWAKEN_API`)

## Sections
| Section | State |
| --- | --- |
| S0 Scaffolding | done |
| S1 Types, tags, seams | done |
| S2 Item definition, catalog, starter content | done — 25 assets in `/Game/Awaken/Inventory`, `/Game/Awaken/Items`; catalog loads 16 |
| S3 Inventory data | done |
| S4 Registry, bag component, containers, fixture | done |
| S5 Loose items, grantOrDrop, pickup, transfer | done |
| S6 Hotbar and held item | done |
| S7 Craft-staging reservations | done |
| S8 Spoilage, consumables, medicine | done |
| S9 Bag lifecycle, Bagless, starting kit | done |
| S10 Bag-screen data, catalog checks | done — `validateCatalog` = 0 errors on content |
| S11 Save integration | done — against Doc R13's real `IBPI_SaveParticipant` |
| S12 Networking (requests, container views) | done (compiled; see S13.7) |
| S13 Dev tools, tests, test map | done except the client-side half of S13.7 |

## Current task
None. Open: client-side part of the S13.7 two-player check (manual).

## Deviations from the guide (forced by the existing project)
- Owner tags already exist: `Action.Reload` (R04), `Stats.Source.Medicine` (R08), `Cosmetic.Event.Interaction.ItemDropped` (B21) — owners' headers included, not redefined (S13.8 rule).
- Dynamic delegates live at the top of `S_ItemInventoryFramework.h` (the S1.9 fallback).
- S5.7 applied against B21's existing `UCosmeticEventSubsystem` (no `IBPI_CosmeticEventSink` exists).
- S11: Doc R13's `IBPI_SaveParticipant` (FInstancedStruct, `getSaveOwner`, `migrateSnapshotStep`) already exists, so `BPI_SaveParticipantStub.h` and `UStubSaveRegistry` were not created; both participants register with `USaveSubsystem`. `Awaken.Inventory.StubSave/StubLoad` do an in-memory round trip through R13's interface (R13 has no save console command).
- Old TEMPORARY `HeldItemComponent.h/.cpp` replaced by `UHeldItemComponent`; `AwakenCharacter.cpp` include and `GA_Reload::onReloadFinished` (→ `UBFL_Inventory::applyReload`) updated.
- UE 5.8 fixes: `APlayerState::GetActorLocation` is private (called via `AActor`); a `switch` with only `default` is error C4065 (migration step written as `if`).
- Specs mark B21's "DA_EnvironmentSettings_Default is missing" startup errors as expected while that asset is absent (foreign content gap).
- Four B03 tags added to `DefaultGameplayTags.ini` (S1.8 note). `NetCore` added to Build.cs.
- Extra spec `InventorySectionSpec` covers the automatable Verify rows of S1 and S5–S9.

## Blockers / handoffs outside R01 (not done, by design)
- B19: the three inventory components are now on `AAwakenPlayerState` (done after the guide, on request). Still open: nothing in `AAwakenGameMode` calls `UBFL_InventoryBag::startNewCharacter`, so real-game players start Bagless; `heldBagId` on the PlayerState is not set by R01.
- Seams still on stubs: game clock (R12 Time exists), action service (R04 Ability exists), survival, health, carry, player state, equipment, bag actor.
- B21: `DA_EnvironmentSettings_Default` missing; no `DT_CosmeticEvents` row for `ItemDropped`.

## Build / test log
- Baseline (before R01): Editor Development build clean.
- S0–S13: Editor Development build clean, no new warnings.
- Automation `Awaken.Inventory`: 36/36 pass (Data 6, guide specs 5, Sections 15, Slots 8, Save 2).
- PIE listen server + 1 client on `TestMap_Inventory`: steps 1, 2, 3, 5, 8 verified from the host (kits, give, FillBag overflow dropped as loose actor, bind/select/wielded, lazy spoilage, registry 4). Client-perspective rows (owner-only view, client refusals, late joiner) NOT verified: the PIE client window could not be driven reliably.

## Imported FAB assets
None. Placeholder meshes are engine basic shapes (the guide allows any small mesh).

---

# Implementation Status — Doc R05 Equipment

Guide: `TheAwaken_DocR05_StepByStep_Implementation_Guide.md`

## Sections
| Section | State |
| --- | --- |
| E0 Scaffolding | done |
| E1 Types, tags, seams | done |
| E2 Item data, layout asset, content | done — 5 new items + IronVest updated, `DA_ArmorStackLayout` (14 positions) in `/Game/Awaken/Equipment`, set in settings |
| E3 Component core | done — replaces the old placeholder `UEquipmentComponent`, so `AAwakenPlayerState` carries the real one |
| E4 Equip / unequip armor | done |
| E5 Armor damage, `UBFL_Equipment` | done |
| E6 Clothes | done |
| E7 Bag lifecycle seam | done — `UStubEquipmentGear` deleted from R01; test GameMode calls `resetForNewCharacter` |
| E8 Save | done — against Doc R13's real interface, key `Equipment.Character` |
| E9 Requests | done (E9.2 patch was already in R01) |
| E10 Dev tools, specs | done except the client-side half of E10.7 |

## Deviations (forced by the existing project)
- `BPI_CosmeticEventSink` and its stub not created: Doc B21's `UCosmeticEventSubsystem` exists (E1.4/E10.8 rule); `raiseCosmeticEvent` calls it. `Cosmetic.Event.Combat.ArmorBroken` comes from B21's header.
- `FD_EquipmentChanged` lives in `S_EquipmentFramework.h` (UHT delegate-only-header fix, as in R01).
- `UDA_ArmorStackLayout` created with Section 0 (UHT needs the class for the settings' soft pointer).
- E8 implemented on R13's `IBPI_SaveParticipant` (FInstancedStruct), registered with `USaveSubsystem`; migration step written as `if` (C4065).
- Extra spec `EquipmentSectionSpec` covers the automatable Verify rows of E1, E3–E7.
- E10.6 "a BeachOutfit in the chest": container contents are runtime data, so it is covered by the E6 test instead of map content.

## Open / handoffs outside R05
- Stats seam still the stub: R08 has `UStatsLibrary::applyArmorAggregate` but no `IBPI_ArmorStatsSink` registration.
- R03 must call `applyDamageToArmor` / `applyAttackToGear` at damage step 4.
- E2 Verify 4 (layout validator warnings) not run as a test.

## Build / test log
- E0–E10: Editor Development build clean, no new warnings.
- Automation `Awaken.Equipment` 12/12 and `Awaken.Inventory` 36/36 pass (48 total).
- Offline (standalone `-game`, TestMap_Inventory): E10.7 steps 1, 3, 5, 6, 7 verified from the log. Listen-server / client-side rows NOT verified.

## Imported FAB assets
None.

---

# Implementation Status — Doc B07 Carry & Encumbrance

Guide: `TheAwaken_DocB07_StepByStep_Implementation_Guide.md` · Folder `CarryEncumbranceFramework` (the project's existing folder; the guide says `CarryFramework`)

## Sections
| Section | State |
| --- | --- |
| C0 Scaffolding | done — `LogAwakenCarry`, `UCarrySettings` (Project Settings → Game → Awaken Carry), `UCarryServices`. GameplayAbilities was already a dependency |
| C1 Types, tags, seams | done — `IBPI_CarryableActor`, `IBPI_ThrowImpactSink` + stubs |
| C2 Item data | done — `throwImpactDamage` field, IsDataValid carry checks, fixture `addHeavy`; content: Log, Stone, Trunk heavy; Rock created as the ordinary ingredient |
| C3 Component core | done — replaces the TEMPORARY `CarryComponent.h/.cpp` (now `UCarryComponent.h/.cpp`); already on `AAwakenCharacter`, added to `AInventoryTestCharacter` |
| C4 Begin a carry, R01 seam | done — `UCarryServices` is the real `IBPI_CarryService`; `UStubCarryService` and the `carrying` half of `StubFlag` deleted |
| C5 End a carry, cost sourcing | done — `endCarry`, `consumeCarriedForCost`, `UBFL_Carry` |
| C6 Throw pipeline | done — `AThrownItemActor`, `UBFL_Carry::throwEntry` |
| C7 Interrupts, refusal list | done — `interruptCarry`, swim + pawn-removed safety nets; R01 hotbar patch (C7.5) |
| C8 Actor carry | done — `beginCarryActor` |
| C9 Networking | done (compiled) — `serverDropCarried` / `serverThrowCarried` through R01's gate; client-side rows not verified |
| C10 Dev tools, tests | done except C10.7 (two-player) and the C2.3 skeleton sockets |

## Deviations from the guide (forced by the existing project)
- Folder/file names follow the project: `CarryEncumbranceFramework/`, `E_/S_/T_CarryEncumbranceFramework.*`; `FD_CarryChanged` lives in `S_CarryEncumbranceFramework.h` (UHT delegate-only-header fix, as R01/R05); `D_CarryEncumbranceFramework.h` just includes it.
- Not redefined, owners' headers included: `Action.Dodge/Sprint/Block/Reload/WeaponCharge` (R04), `Cosmetic.Event.Carry.Picked/Dropped` (B21).
- No `IBPI_CosmeticEventSink` exists: cosmetic events go through B21's `UCosmeticEventSubsystem` (as R05 did).
- UE 5.8 has no `AddReplicatedLooseGameplayTag`: `Carry.State.Active` is set with `SetLooseGameplayTagCount(…, TagOnly)` (idempotent, replicated).
- Dev-command / debug-view helpers are inside file-named namespaces (unity-build collision rule from the 2026-11-14 maintenance).
- Stone is now heavy (guide C2.3); its bag-page slot moved to the new ordinary `Rock`. R01's `InventorySlotSpec` "slot data check" now uses Rock; R01's S5 grant test uses the real component instead of the deleted stub.
- `AStubCarryableActor` shows a small cube (no collision) so `CarryDummy` is visible; `UStubThrowImpactSink.letItemLand` added for the "do not land" test.
- Extra spec rows beyond the guide's six: defaults, seam registration, Downed refusal, SlotCount cvar, five actions cancelled once, spoil time kept through the hands, pawn-removed safety net, impact sink "do not land", carried actor destroyed.

## Open / handoffs outside B07
- C2.3: sockets `carry_01` / `carry_02` on the character skeleton not added (editor task; without them the mesh draws at the mesh root).
- C10.6: no water volume added to `TestMap_Inventory` (swim net untested live).
- C10.7 two-player check (listen server + client, late joiner, disconnect) NOT done.
- Seams still stubbed: `IBPI_ThrowImpactSink` (R03), `IBPI_ActionService` (R04, from R01). One-line `interruptCarry` calls still to add by B02 (Downed/Death/Restrained), R07 (BeginSwim), R12 (Sleep), B20 (`handlePlayerLeft`).
- Input (R04/B17): drop/throw keys not mapped to `serverDropCarried(-1)` / `serverThrowCarried(-1, cameraForward)`.

## Build / test log
- C0–C10: Editor Development build clean, no new warnings.
- Automation `Awaken.Carry` 10/10, `Awaken.Inventory` 36/36, `Awaken.Equipment` 12/12 (58 total).
- Offline standalone `-game` on TestMap_Inventory: Give Log 3 (2 carried, 1 loose), Drop, Throw (landed as a loose log), FillHands → HandsFull, Interrupt Downed (2 dropped at the feet), Give Trunk (both slots), Drop, CarryDummy → Throw refused CannotThrow → Drop, Give Stick (bag), `Awaken.Item.Validate` 0 errors — all verified from the log.

---

# Implementation Status — Doc R02 Crafting

Guide: `TheAwaken_DocR02_StepByStep_Implementation_Guide.md` · Folder `CraftingFramework`

## Sections
| Section | State |
| --- | --- |
| CR0 Scaffolding | done — `LogAwakenCrafting`, `UCraftingSettings` (Project Settings → Game → Awaken Crafting), `UCraftingServices` |
| CR1 Types, tags, seam | done — `IBPI_ProgressionQuery`; default is the real R06 bridge (see deviations) |
| CR2 Recipe data, catalog, data check | done — `UDA_CraftingRecipe`, `UCraftingRecipeSubsystem`, `UBFL_CraftingValidation`; `Recipe` asset type in DefaultGame.ini; content: 4 recipes in `/Game/Awaken/Recipes`, new items `Hatchling` (Materials slot M4) and `StoneSpear` (new Weapons slot W9); `Awaken.Craft.Validate` 0 errors |
| CR3 Craft button list | done — `UBFL_Crafting::evaluateRecipe` / `buildRecipeList` |
| CR4 Station component | done — `UCraftingStationComponent`, `AStubCraftingStation` |
| CR5 Crafting component, session, staging | done — replaces the TEMPORARY `CraftingComponent.h/.cpp` (now `UCraftingComponent.h/.cpp`); already on `AAwakenCharacter`, added to `AInventoryTestCharacter` (CR5.7) |
| CR6 Request / complete a craft | done |
| CR7 Ending a session | done — pawn and station `EndPlay` safety nets, `UBFL_Crafting::closeSession` / `handlePlayerLeft` |
| CR8 Networking | done (compiled) — five server requests through R01's gate; client-side rows not verified |
| CR9 Dev tools, tests | done except CR9.7 (two-player) |

## Deviations from the guide (forced by the existing project)
- Doc R06 Progression exists: `UCraftingServices` registers `UCraftingProgressionBridge` (asks `UProgressionComponent` on the GameState) instead of the stub. `UStubProgressionQuery` stays for tests; `Awaken.Craft.StubUnlock` switches the local machine to the stub (for TestMap_Inventory, whose GameState has no Progression component) and logs a warning.
- Not redefined, owners' headers included: `Interaction.FailReason.InUse` (B18), `Cosmetic.Event.Craft.Started/Finished` (B21). `Station.CraftingTable` / `Station.Forge` are still TEMPORARY duplicates here (no B01/B10 tag header declares them).
- No `IBPI_CosmeticEventSink` exists: cosmetic events go through B21's `UCosmeticEventSubsystem` (as R05/B07).
- B18's `UInteractionLockComponent` exists: if a station's actor also carries one, `tryLock` / `release` mirror the hold into it (holder = the Character, as B18 uses).
- `FD_CraftingChanged` lives in `S_CraftingFramework.h` (UHT delegate-only-header fix); `D_CraftingFramework.h` just includes it.
- `UCraftingComponent::EndPlay` only stops the timer on `EndPlayInEditor` / `Quit` (world teardown) instead of running the full close; destroy / removal still closes with `PawnRemoved`.
- Dev-command and debug-view helpers are in file-named namespaces (unity-build rule).
- Content: `Hatchling` duplicated from Stick (stackable material), `StoneSpear` from Spear (weapon). StoneSpear needed a 200×100 slot; none was free on the Weapons page, so slot W9 was added to `DA_Page_Weapons`. `Progression.Recipe.StoneSpear` added to DefaultGameplayTags.ini (owner R06).
- Extra spec rows beyond the guide's ten: Defaults (Section 1), Station lifecycle (id kept, forceRelease OutOfRange, pawn removed), Bag refused (NotFound, InsufficientStaged, BagRequired).

## Open / handoffs outside R02
- CR9.7 two-player check (listen server + client, refusals told only to the requester, disconnect frees the station) NOT done.
- One-line `closeSession` calls still to add by B02 (Downed / Died before `detachBag`), R17 (SceneStarted), B20 (`handlePlayerLeft` before R01's), B18 (`forceRelease(OutOfRange)` on lock release).
- B18 should replace `serverUseStation` + `stationUseRange` with its own interaction calling `enterStation`.
- B01: no Crafting Table / Forge piece carries `UCraftingStationComponent` yet; B17: no bag screen calls the five requests.
- R07: no montage behind `animationTag`; `craftSeconds` (K2) is the only crafting time.
- Appendix C open points (K2, K5, K7, K13, K14, K16, heavy / multi-copy outputs) not confirmed with the designer.

## Build / test log
- CR0–CR9: Editor Development build clean, no compiler warnings.
- Automation `Awaken.Crafting` 13/13, `Awaken.Inventory` 36/36, `Awaken.Equipment` 12/12, `Awaken.Carry` 10/10 (71 total). (First run: R01's slot data check failed on StoneSpear's too-small slot; fixed by slot W9.)
- Offline standalone `-game` on TestMap_Inventory, verified from the log: catalog 4 recipes, Validate 0 errors; stage sticks (3 red) → rocks (Arrow/Hatchling/Spear green) → Craft Spear started, second press and a stage refused CraftInProgress, spear granted after 1.5 s; StageAll with spears → 3 white, Craft Hatchling refused ExtraIngredients, Close returned 3 types; SpawnStation CraftingTable entered, StoneSpear refused BlueprintLocked, after StubUnlock listed green, crafted after 2 s.

## Imported FAB assets
None. The stub station shows the engine cube.

---

## Maintenance — 2026-11-14 (project failed to build on open; Rider showed 58 issues)
- **Build failure, cause 1 (code):** a unity-build name collision. `TimeCheatCommands.cpp`, `WeatherCheatCommands.cpp` and `EquipmentDevCommands.cpp` each define `report` / `cmdSet` / `cmdDump` / `dumpCommand` in an anonymous namespace; when UBT's adaptive unity build regrouped files they landed in one translation unit (C2084/C2374). Fix: each file's anonymous namespace is nested in a file-named namespace. Verified by compiling the whole module as a single unity file (`-StressTestUnity -DisableAdaptiveUnity`).
- **Build failure, cause 2 (environment):** the machine runs out of commit memory (16 GB RAM + ~11 GB page file) with Unreal Editor + Rider open; UBA reports "Failed to create shared memory" and Windows killed `cl.exe` (low-virtual-memory event). Not a code issue; enlarging the page file is recommended (user action).
- **Rider "Toolset, Environment" (58 × MSB4236):** Visual Studio Community 2026 lacked its .NET SDK component, so its MSBuild had no .NET SDK resolver and the engine's C# programs could not load. Fixed by adding `Microsoft.NetCore.Component.SDK` with the VS Installer; the solution now loads 60/60 projects. Remaining 3 items are NuGet vulnerability notices in Epic's engine tool programs (not game code).
- **Tests:** `Awaken.Inventory` 36/36 and `Awaken.Equipment` 12/12 pass; live standalone run on TestMap_Inventory exercising both systems is clean.
