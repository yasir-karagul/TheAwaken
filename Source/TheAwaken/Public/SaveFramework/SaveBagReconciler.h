#pragma once
#include "CoreMinimal.h"
#include "SaveFramework/00Enums/E_SaveFramework.h"
#include "SaveFramework/01Structs/S_SaveFramework.h"

class THEAWAKEN_API FSaveBagReconciler{
public:
	/**
	 * @param characterId          characterID from the Character file header
	 * @param fileHeldBagId        heldBagID from the header: the bag the file says the player holds (invalid = none). A bagID is unique, so the owner is not compared.
	 * @param worldBags            bag records of the running world (Doc B02's registry, via IBPI_SaveBagProvider)
	 * @param worldNewerThanFile  file counter < world counter, same worldID. A held state is never older than the world: pass false.
	 */
	static FS_SaveBagDecision evaluate(const FGuid& characterId, const FGuid& fileHeldBagId,
									 const TArray<FS_BagRecord>& worldBags, bool worldNewerThanFile);
};