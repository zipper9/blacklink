#include "stdinc.h"
#include "RWLockWrapper.h"

#ifdef FLYLINKDC_SUPPORT_WIN_XP

#include "RWLockWinXP.h"
#include "RWLockWinDynamic.h"

RWLockWrapper* RWLockWrapper::create()
{
	RWLockWinDynamic* vistaLock = RWLockWinDynamic::create();
	if (vistaLock) return vistaLock;
	return RWLockWinXP::create();
}

#endif
