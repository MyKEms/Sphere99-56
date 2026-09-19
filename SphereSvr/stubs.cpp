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

// CContainer's method table is registered manually because it has no property
// table; keep the lookup in sync with the table used by CScript dispatch.
int CContainer::s_FindMyMethodKey(LPCTSTR pszKey)
{
	return pszKey ? s_FindKeyInTable(pszKey, CContainer::sm_Methods) : -1;
}

// CWorld's property table is likewise registered manually.
int CWorld::s_FindMyPropKey(LPCTSTR pszKey)
{
	return pszKey ? s_FindKeyInTable(pszKey, CWorld::sm_Props) : -1;
}
