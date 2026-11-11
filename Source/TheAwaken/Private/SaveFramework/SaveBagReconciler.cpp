#include "SaveFramework/SaveBagReconciler.h"

FS_SaveBagDecision FSaveBagReconciler::evaluate(const FGuid& characterId, const FGuid& fileHeldBagId,
											  const TArray<FS_BagRecord>& worldBags, bool worldNewerThanFile){
	FS_SaveBagDecision decision;
	// the file holds no bag: nothing can be duplicated
	if(!characterId.IsValid() || !fileHeldBagId.IsValid()){
		return decision;
	}

	// The duplicate is the world bag with the SAME bagID as the bag the file says this character holds. A bagID is unique, so the owner
	// adds nothing, and requiring it could leave a duplicate in the world. A character owns every bag it ever dropped (Doc R13 3.6, 3.10),
	// so matching by owner would strip a player who later picked up a different bag, although nothing is duplicated.
	auto holdsSameBag = [&fileHeldBagId](const FS_BagRecord& bag){
		return bag.bagID == fileHeldBagId;
	};
	const FS_BagRecord* duplicate = worldBags.FindByPredicate(holdsSameBag);
	// NoConflict: other bags this character dropped earlier stay where they are
	if(!duplicate){
		return decision;
	}

	if(worldNewerThanFile){
		decision.outcome = E_SaveBagOutcome::WorldBagWinsPlayerBagless;   // ST-01 exception: the world already holds that bag
		return decision;
	}

	decision.outcome = E_SaveBagOutcome::SavedStateWinsRemoveWorldBag;
	decision.worldBagToRemove = duplicate->bagID;   // no duplicate bag
	return decision;
}