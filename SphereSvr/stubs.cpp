// SphereSvr/stubs.cpp - Stub implementations for SphereSvr layer

#include "stdafx.h"
#include "spheresvr.h"
#include "CWorld.h"
#include "cObjBase.h"
#include "CChat.h"
#include "cresource.h"

// CScriptClass instances for classes that declare CSCRIPT_CLASS_DEF1
// but never have a corresponding IMP that defines the static.
CScriptClass CCharNPC::sm_ScriptClass;
CScriptClass CCharPlayer::sm_ScriptClass;
CScriptClass CClient::sm_ScriptClass;
CScriptClass CContainer::sm_ScriptClass;
CScriptClass CSphereResourceMgr::sm_ScriptClass;
CScriptClass CWorld::sm_ScriptClass;

// CContainer::s_FindMyMethodKey - not implemented via CSCRIPT_CLASS_IMP
int CContainer::s_FindMyMethodKey(LPCTSTR pszKey)
{
	return -1; // STUB: no methods found
}

// CWorld::s_FindMyPropKey
int CWorld::s_FindMyPropKey(LPCTSTR pszKey)
{
	return -1; // STUB: no props found
}
