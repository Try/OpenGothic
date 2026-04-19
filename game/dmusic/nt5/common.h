// Copyright (c) 1998 Microsoft Corporation
#ifndef _COMMON_H_
#define _COMMON_H_


#if (DBG)
#if !defined(DEBUG_LEVEL)
#define DEBUG_LEVEL DEBUGLVL_VERBOSE
#endif
#endif

#include <winerror.h>


#include "portcls.h"
#include "ksdebug.h"
#include <dmusicks.h>       // Ks defines
#include <dmerror.h>        // Error codes
#include <dmdls.h>          // DLS definitions

#include "kernhelp.h"
#include "CSynth.h"
#include "synth.h"
#include "float.h"
#include "muldiv32.h"
#include "SysLink.h"

#endif  //_COMMON_H_
