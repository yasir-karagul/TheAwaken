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
UE_DEFINE_GAMEPLAY_TAG(T_DamageType_Illness, "DamageType.Illness");
UE_DEFINE_GAMEPLAY_TAG(T_Survival_Meter_Illness, "Survival.Meter.Illness");
