#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
// Owners' tag headers that already exist (S13.8: the duplicate definition is removed and the owner's header included).
#include "AbilityActionFramework/02Tags/T_AbilityActionFramework.h"                                         // T_Action_Reload (Doc R04)
#include "StatsFramework/02Tags/T_StatsFramework.h"                                                         // T_Stats_Source_Medicine (Doc R08)
#include "CosmeticEventsEnvironmentPresentationFramework/02Tags/T_CosmeticEventsEnvironmentPresentationFramework.h"   // T_Cosmetic_Event_Interaction_ItemDropped (Doc B21)

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
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_DamageType_Illness);                        // Doc R03
UE_DECLARE_GAMEPLAY_TAG_EXTERN(T_Survival_Meter_Illness);                    // Doc B03
// T_Action_Reload (R04), T_Stats_Source_Medicine (R08) and T_Cosmetic_Event_Interaction_ItemDropped (B21) come from their owners' headers above.
