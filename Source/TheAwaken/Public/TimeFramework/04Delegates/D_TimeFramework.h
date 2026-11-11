#pragma once
#include "CoreMinimal.h"

struct FS_TimerHandle;   // defined in S_TimeFramework.h, which includes this file

// Called when a scheduled timer is due. The int64 is the timer's own finish time,
// so an owner can compute its state exactly as of that moment, even during a skip.
DECLARE_DELEGATE_TwoParams(FD_TimeDue, const FS_TimerHandle&, int64);
