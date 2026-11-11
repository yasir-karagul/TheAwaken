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
