#pragma once
#include "CoreMinimal.h"

namespace AwakenSave{
	/** Bump when a framework-independent rule changes (not for the header layout: that is envelopeVersion). Files newer than this are refused. */
	constexpr int32 schemaVersion = 1;
	constexpr uint32 fileMagic = 0x41575356;   // 'AWSV'
	/** Layout of the envelope and header. Fields are only ever appended, and a reader ignores trailing header bytes. */
	constexpr int32 envelopeVersion = 1;
}