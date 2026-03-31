//
// spheresvr.H
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
// Precompiled header
//

#ifndef _INC_SPHERESVR_H_
#define _INC_SPHERESVR_H_
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#ifndef SPHERE_SVR
#error SPHERE_SVR must be -defined on compiler command line for common code to work!
#endif
// SPHERE_VERSION should be set

#include "spherecommon.h"	// put slashes this way for LINUX, WIN32 does not care.
#include "sphereproto.h"
#include "cmulmulti.h"
#include "cresourcebase.h"
#include "csectortemplate.h"
#include "caccount.h"
class CClient;
class CAccount;
class CWebPageDef;

enum CLIMODE_TYPE	// What mode is the client to server connection in ? (waiting for input ?)
{
	CLIMODE_PROXY,
	CLIMODE_TELNET_USERNAME,
	CLIMODE_TELNET_PASS,
	CLIMODE_TELNET_READY,	// logged in.

	// setup events ------------------------------------------------

	CLIMODE_SETUP_CONNECTING,
	CLIMODE_SETUP_SERVERS,		// client has received the servers list.
	CLIMODE_SETUP_RELAY,		// client has been relayed to the game server. wait for new login.
	CLIMODE_SETUP_CHARLIST,	// client has the char list and may (play a char, delete a char, create a new char)

	// Capture the user input for this mode. ------------------------------------------
	CLIMODE_NORMAL,		// No targeting going on. we are just walking around etc.

	// asyc events enum here. --------------------------------------------------------
	CLIMODE_DRAG,			// I'm dragging something. (not quite a targeting but similar)
	CLIMODE_DEATH,			// The death menu is up.
	CLIMODE_DYE,			// The dye dialog is up.
	CLIMODE_INPVAL,		// special text input dialog (for setting item attrib)

	// Some sort of general gump dialog ----------------------------------------------
	CLIMODE_DIALOG,		// from RES_Dialog

	// Hard-coded (internal) gumps.
	CLIMODE_DIALOG_ADMIN,
	CLIMODE_DIALOG_EMAIL,	// Force the user to enter their emial address. (if they have not yet done so)
	CLIMODE_DIALOG_GUILD,	// reserved.
	CLIMODE_DIALOG_HAIR_DYE, // Using hair dye

	// Making a selection from a menu. ----------------------------------------------
	CLIMODE_MENU,		// RES_Menu

	// Hard-coded (internal) menus.
	CLIMODE_MENU_SKILL,		// result of some skill. tracking, tinkering, blacksmith, etc.
	CLIMODE_MENU_SKILL_TRACK_SETUP,
	CLIMODE_MENU_SKILL_TRACK,
	CLIMODE_MENU_GM_PAGES,		// show me the gm pages .
	CLIMODE_MENU_EDIT,		// edit the contents of a container.

	// promting for text input.------------------------------------------------------
	// CLIMODE_PROMPT,					// Some sort of text prompt input.
	CLIMODE_PROMPT_NAME_RUNE,
	CLIMODE_PROMPT_NAME_KEY,		// naming a key.
	CLIMODE_PROMPT_NAME_SIGN,		// name a house sign
	CLIMODE_PROMPT_NAME_SHIP,
	CLIMODE_PROMPT_GM_PAGE_TEXT,	// allowed to enter text for page.
	CLIMODE_PROMPT_VENDOR_PRICE,	// What would you like the price to be ?
	CLIMODE_PROMPT_TARG_VERB,		// Send a msg to another player.
	CLIMODE_PROMPT_STONE_NAME,		// prompt for text.
	CLIMODE_PROMPT_STONE_SET_ABBREV,
	CLIMODE_PROMPT_STONE_SET_TITLE,
	CLIMODE_PROMPT_STONE_GRANT_TITLE,

	// Targeting mouse cursor. -------------------------------------------------------------
	CLIMODE_MOUSE_TYPE,	// Greater than this = mouse type targeting.

	// GM targeting command stuff.
	CLIMODE_TARG_OBJ_SET,		// Set some attribute of the item i will show.
	CLIMODE_TARG_OBJ_INFO,		// what item do i want props for ?

	CLIMODE_TARG_UNEXTRACT,		// Break out Multi items
	CLIMODE_TARG_ADDITEM,		// "ADDITEM" command.
	CLIMODE_TARG_LINK,			// "LINK" command
	CLIMODE_TARG_TILE,			// "TILE" command.

	// Normal user stuff. (mouse targeting)
	CLIMODE_TARG_SKILL,				// targeting a skill or spell.
	CLIMODE_TARG_SKILL_MAGERY,
	CLIMODE_TARG_SKILL_HERD_DEST,
	CLIMODE_TARG_SKILL_POISON,
	CLIMODE_TARG_SKILL_PROVOKE,

	CLIMODE_TARG_USE_ITEM,			// target for using the selected item
	CLIMODE_TARG_PET_CMD,			// targetted pet command
	CLIMODE_TARG_PET_STABLE,		// Pick a creature to stable.
	CLIMODE_TARG_REPAIR,		// attempt to repair an item.
	CLIMODE_TARG_STONE_RECRUIT,		// Recruit members for a stone	(mouse select)
	CLIMODE_TARG_PARTY_ADD,

// WESTY MOD (MULTI CONFIRM)
	CLIMODE_PROMPT_MULTI_CONFIRM,
// END WESTY MOD

	CLIMODE_TARG_QTY,
};

#include "cresource.h"
#include "CServRef.h"
#include "cobjbasedef.h"
#include "cObjBase.h"
#include "CWorld.h"
#include "CChat.h"
#include "cclient.h"

///////////////////////////////////////////////

// Text mashers.

extern const char* GetTimeDescFromMinutes( int dwMinutes );

enum SERVTRIG_TYPE
{
	// Events that effect the whole server.
#define CSERVEVENT(a,b,c) SERVTRIG_##a,
#include "cservevents.tbl"
#undef CSERVEVENT
	SERVTRIG_QTY,
};

struct CLog : public CFileText, public CLogBase, public CThreadLockableObj
{
	// Just use CRefPtr to thread lock this.
	// subject matter. (severity level is first 4 bits, LOGL_EVENT)
	// Similar to HRESULT (high bit=critical error, 4bit=reserve, 11bit=facility, 16bits=code)
#define	LOG_GROUP_ACCOUNTS		0x00080
#define	LOG_GROUP_INIT			0x00100
#define	LOG_GROUP_CLIENTS		0x00400
#define LOG_GROUP_GM_PAGE		0x00800	// player gm pages.
#define LOG_GROUP_PLAYER_SPEAK	0x01000	// All that the players say.
#define LOG_GROUP_GM_CMDS		0x02000	// Log all GM commands.
#define	LOG_GROUP_CHEAT			0x04000
#define LOG_GROUP_KILLS			0x08000	// Log player combat results.
#define LOG_GROUP_HTTP			0x10000
#define LOG_GROUP_SAVE			0x20000
#define	LOG_GROUP_DEBUG			0x40000

public:
	CLog();
	~CLog();

	virtual void SetFilePath( LPCTSTR lpszNewName )
	{
		ASSERT( ! IsFileOpen());
		CFileText::SetFilePath( lpszNewName );
	}
	const char* GetLogDir() const
	{
		return( m_sBaseDir );
	}
	bool OpenLog( const char* pszName = NULL );	// name set previously.

	void Dump( const BYTE* pData, int len );

	virtual int EventStr( LOG_GROUP_TYPE dwGroupMask, LOGL_TYPE level, const char* pszMsg );
	void _cdecl CatchEvent( CGException* pErr, const char* pszCatchContext, ...  );

	bool IsLogged(LOGL_TYPE level) const { throw "not implemented"; }
	bool IsLogged(LOG_GROUP_TYPE dwGroupMask, LOGL_TYPE level) const { throw "not implemented"; }
	bool IsLoggedGroupMask(LOG_GROUP_TYPE dwGroupMask) const { throw "not implemented"; }

	void SetLogLevel(LOGL_TYPE level) { throw "not implemented"; }
	void SetLogGroupMask(LOG_GROUP_TYPE dwGroupMask) { throw "not implemented"; }
	
	LOGL_TYPE GetLogLevel() const { throw "not implemented"; }
	LOG_GROUP_TYPE GetLogGroupMask() const { throw "not implemented"; }

public:
	bool m_fLockOpen;	// resource is locked open ?

protected:
	void EventStrPrint( int iColorType, const char* pMsg );

private:
	int m_iDayStamp;			// last real time stamp. for day transitions
	CGString m_sBaseDir;

	static CGTime sm_prevCatchTick;	// don't flood with these.
};

extern CLog g_Log;		// Log file

#define SPHERE_LOG_TRY_CATCH(desc) catch ( CGException &e )\
	{\
		g_Log.CatchEvent( &e, desc );\
	}\
	catch(...)\
	{\
		g_Log.CatchEvent( NULL, desc );\
	}

#define SPHERE_LOG_TRY_CATCH1(desc,arg) catch ( CGException &e )\
	{\
		g_Log.CatchEvent( &e, desc, arg );\
	}\
	catch(...)\
	{\
		g_Log.CatchEvent( NULL, desc, arg );\
	}

//********************************************************

class CGMPage : public CGObListRec, public CResourceObj
{
	// RES_GMPage
	// Action on account
	// Only one page allowed per CAccount at a time.
public:
	CGMPage( CAccount* pAccount );
	~CGMPage();

	virtual CGString GetName() const;
	CAccountPtr	GetAccount() const;
	CGString GetAccountStatus() const;
	CGString GetReason() const
	{
		return( m_sReason );
	}
	void SetReason( const char* pszReason )
	{
		m_sReason = pszReason;
	}
	CClientPtr FindGMHandler() const
	{
		return( m_pGMClient );
	}
	void ClearGMHandler()
	{
		m_pGMClient = NULL;
	}
	void SetGMHandler( CClient* pClient )
	{
		m_pGMClient = pClient;
	}
	int GetAge() const;

	virtual void s_WriteProps( CScript& s ) const;
	virtual HRESULT s_PropGet( const char* pszKey, CGVariant& vVal, CScriptConsole* pSrc );
	virtual HRESULT s_PropSet( const char* pszKey, CGVariant& vVal );
	//virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script

	DECLARE_LISTREC_REF2(CGMPage);

public:
	// Queue a GM page. (based on account)
	CServTime  m_timePage;		// Time of the last call.
	CPointMap  m_ptOrigin;		// Origin Point of call.

private:
	CAccountPtr m_pAccount;	// The account that created the page. CAccountPtr
	CClientPtr m_pGMClient;	// assigned to an active GM.
	CGString m_sReason;		// Players Description of reason for call.

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CGMPAGEPROP(a,b,c) P_##a,
#include "cgmpageprops.tbl"
#undef CGMPAGEPROP
		P_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];
};

typedef CRefPtr<CGMPage> CGMPagePtr;

struct CMenuItem 	// describe a menu item.
{
public:
	WORD m_id;			// ITEMID_TYPE in base set.
	CGString m_sText;
public:
	bool ParseLine( char* pszArgs, CSphereExpContext& exec );
};

enum CLIENT_BUTTON_TYPE
{
	CLIENT_BUTTON_UNK,
	CLIENT_BUTTON_STATS,
	CLIENT_BUTTON_SKILLS,
	CLIENT_BUTTON_PROFILE,	// 3
	CLIENT_BUTTON_WAR,
	CLIENT_BUTTON_ARROW,	// Quest arrow.
	CLIENT_BUTTON_QTY,
};

enum REGRES_TYPE	// Registration server return values.
{
	REGRES_NOT_ATTEMPTED = 0,			// Have not set this to anything yet.
	REGRES_NOT_REQUIRED,	// This is the list server.
	REGRES_LIST_SERVER_NONE,
	REGRES_LIST_SERVER_UNRESOLVABLE,

	REGRES_FAIL_CREATE_SOCKET,
	REGRES_FAIL_CONNECT,
	REGRES_FAIL_SEND,

	// Code returned from list server.

	REGRES_RET_INVALID,				// registration is bad format.
	REGRES_RET_FAILURE,			// Some other failure decoding the msg.
	REGRES_RET_OK,				// Listed OK
	REGRES_RET_BAD_PASS,		// Password does not match for server.
	REGRES_RET_NAME_UNK,		// Not on the approved name list.
	REGRES_RET_DEBUG_MODE,	// List server is in debug mode. (no listing available)
	REGRES_RET_IP_UNK,

	// Not used yet codes.
	REGRES_RET_IP_INVALID,
	REGRES_RET_IP_NOT_RESPONDING,
	REGRES_RET_IP_BLOCKED,
	REGRES_RET_WEB_INVALID,
	REGRES_RET_WEB_NOT_RESPONDING,
	REGRES_RET_WEB_BLOCKED,
	REGRES_RET_EMAIL_INVALID,
	REGRES_RET_EMAIL_NOT_RESPONDING,
	REGRES_RET_EMAIL_BLOCKED,
	REGRES_RET_STATS_INVALID,	//  Statistics look invalid.

	REGRES_QTY,
};

struct CClientTargModeContext
{
	// Targeting mode or context of a gump dialog on client side
	// ie. when we get a response from this context,
	//  what were we doing?
	CClientTargModeContext() : m_Mode(CLIMODE_NORMAL) {}

	CLIMODE_TYPE m_Mode;	// Type of async operation under way.

	CSphereUID m_UID;			// The object of interest to apply to the target.
	CSphereUID m_PrvUID;		// The object of interest before this.
	CGString m_sText;		// Text transfered up from client.
	CPointMap m_pt;			// For script targeting,

	// Context of the targeting setup. depends on CLIMODE_TYPE m_Mode
	union
	{
		// CLIMODE_PROXY
		struct
		{
			DWORD m_dwIP;	// Proxy me to this IP.
		} m_tmProxy;

		// CLIMODE_SETUP_CONNECTING
		struct
		{
			DWORD m_dwCryptKey;	// Encryption Key (usually my IP)
			int m_iConnect;		// used for debug only.
		} m_tmSetup;

		// CLIMODE_SETUP_CHARLIST
		CSphereUIDBase m_tmSetupCharList[UO_MAX_CHARS_PER_ACCT];

		// CLIMODE_DIALOG_*
		struct
		{
			CSphereUIDBase m_UID;
			CSphereUIDBase m_ResourceID;	// the gump that is up.
		} m_tmGumpDialog;

		// CLIMODE_DIALOG_ADMIN
		struct
		{
			CSphereUIDBase m_UID;
			BYTE m_iPageNum;		// For long list dialogs. (what page are we on)
			BYTE m_iSortType;
#define ADMIN_CLIENTS_PER_PAGE 8
			DWORD m_Item[UO_MAX_MENU_ITEMS];	// This saves the inrange tracking targets or other context
		} m_tmGumpAdmin;

		// CLIMODE_INPVAL
		struct
		{
			CSphereUIDBase m_UID;
			CSphereUIDBase m_PrvGumpID;	// the gump that was up before this
		} m_tmInpVal;

		// CLIMODE_MENU_*
		// CLIMODE_MENU_SKILL
		// CLIMODE_MENU_GM_PAGES
		struct
		{
			CSphereUIDBase m_UID;
			CSphereUIDBase m_ResourceID;		// What menu is this ?
			DWORD m_Item[UO_MAX_MENU_ITEMS];	// This saves the inrange tracking targets or other context
		} m_tmMenu;	// the menu that is up.

		// CLIMODE_TARG_PET_CMD
		struct
		{
			int	m_iCmd;			// PC_TYPE
			bool m_fAllPets;
		} m_tmPetCmd;	// which pet command am i targeting ?

		// CLIMODE_TARG_CHAR_BANK
		struct
		{
			LAYER_TYPE m_Layer;	// gm command targeting what layer ?
		} m_tmCharBank;

		// CLIMODE_TARG_TILE
		// CLIMODE_TARG_UNEXTRACT
		struct
		{
			CPointMapBase m_ptFirst; // Multi stage targeting.
			int m_Code;
			int m_id;
		} m_tmTile;

		// CLIMODE_TARG_ADDITEM - we have a menu of items to pick from?
		struct
		{
			DWORD m_junk0;
			ITEMID_TYPE m_id;
			int		m_fStatic;
		} m_tmAdd;

		// CLIMODE_TARG_SKILL
		struct
		{
			SKILL_TYPE m_Skill;			// targeting what spell ?
		} m_tmSkillTarg;

		// CLIMODE_TARG_SKILL_MAGERY
		struct
		{
			SPELL_TYPE m_Spell;			// targeting what spell ?
			CREID_TYPE m_SummonID;
			bool m_fSummonPet;
		} m_tmSkillMagery;

		// CLIMODE_TARG_USE_ITEM
		struct
		{
			CGObList* m_pParent;	// the parent of the item being targetted .
		} m_tmUseItem;
	};
};

class CClient : public CGObListRec, public CAccountConsole, public CResourceObj, public CChatClient
{
	// TCP/IP connection to the player or telnet console.
	// Must be scriptable object to do web page scripting.
public:
	DECLARE_LISTREC_REF(CClient);
	CSCRIPT_CLASS_DEF1();
	enum M_TYPE_
	{
#define CCLIENTMETHOD(a,b,c) M_##a,
#include "cclientmethods.tbl"
#undef CCLIENTMETHOD
		M_QTY,
	};
	enum P_TYPE_
	{
#define CCLIENTPROP(a,b,c) P_##a,
#include "cclientprops.tbl"
#undef CCLIENTPROP
		P_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];
	static const CScriptMethod sm_Methods[M_QTY+1];

#ifdef USE_JSCRIPT
#define CCLIENTMETHOD(a,b,c) JSCRIPT_METHOD_DEF(a)
#include "cclientmethods.tbl"
#undef CCLIENTMETHOD
#endif

public:
	CONNECT_TYPE m_ConnectType;	// what sort of a connection is this ?
	CGSocket m_Socket;			// unique socket id number

	CServTime m_timeLogin;		// World clock of login time. "LASTCONNECTTIME"
	CServTime m_timeLastEvent;	// Last time we got event from client.
	CServTime m_timeLastDispatch;	// Last time i processed a message. throttle this.
	CServTime m_timeLastSend;	// Last time i tried to send to the client

	//***************************************
	// Game client stuff.
private:
	CCharPtr m_pChar;			// What char are we playing ?

	// Client last know state stuff.
	CPointMap m_pntSoundRecur;	// Get rid of this in the future !!???
	CSectorEnviron m_Env;		// Last Environment Info Sent. so i don't have to keep resending if it's the same.
	HUE_TYPE m_wHueText;		// selected hue for text that i speak.

	// Sync up with movement.
	bool m_fPaused;			// We paused the client for big download. (remember to unpause it)
	bool m_fUpdateStats;	// update our own status (weight change) when done with the cycle.

	// Walk limiting code. throttling walk speed.
	WORD	m_wWalkCount;		// Make sure we are synced up with client walk count. may be be off by 4
	int		m_iWalkStepCount;	// Count the actual steps . Turning does not count.
	CServTime m_timeWalkStep;	// the last %8 walk step time.

public:
	// GM only stuff.
	CGMPagePtr m_pGMPage;		// Current GM page we are connected to.
	CSphereUID m_Prop_UID;		// The object of /props (used for skills list as well!)

	// Current operation context args for modal async operations..
	// Since we only have one mouse we can only have one mouse context.
	// but we can have multiple Gump Dialogs up in theory?
	CClientTargModeContext m_Targ;

private:
	// Low level data transfer to client.
	XCMD_TYPE m_bin_PrvMsg;	// Last message i decoded.
	int m_bin_msg_len;		// the current message packet to decode. (estimated length)

	// encrypt/decrypt stuff.
	CCrypt		m_Crypt;		// Client to server communications encryption scheme.
	CCompressXOR m_CompressXOR;	// used only in Client 2.0.4 and above.
	static CCompressTree sm_xComp;

	// ??? Since we really only deal with one input at a time we can make this static ?
	CGQueueBytes m_bout;		// CUOCommand out buffer. (to client) (we can build output to multiple clients at the same time)
public:
	CGQueueBytes m_bin;			// CUOEvent in buffer. (from client)

	CCryptVersion m_ProtoVer;	// The version the client reports it is. (protocol desired as opposed to crypt ver)
	int	   m_iClientResourceLevel;	// 0=base, 2=T2A, 3=3rd dawn, 4=Black
	bool   m_fClientVer3d;			// Is this the 3d client ?	
	static BYTE sm_xCompress_Buffer[UO_MAX_EVENT_BUFFER];

private:
	bool OnRxUnk( BYTE* pData, int iLen ); // Receive message from client
	bool OnRxConsoleLoginComplete();
	bool OnRxConsoleLogin();
	bool OnRxConsole( const BYTE* pData, int iLen );
	REGRES_TYPE OnRxAutoServerRegister( const BYTE* pData, int iLen );
	bool OnRxPeerServer( const BYTE* pData, int iLen );
	bool OnRxPing( const BYTE* pData, int iLen );
	bool OnRxWebPageRequest( CWebPageDef* pPage, const char* pszMatch );
	bool OnRxWebPageRequest( BYTE* pRequest, int iLen );

	LOGIN_ERR_TYPE LogIn( CAccount* pAccount );
	LOGIN_ERR_TYPE LogIn( const char* pszName, const char* pPassword );

	bool CheckProtoVersion();
	bool CheckLogIP();

	// Low level message traffic.
	bool xCheckMsgSize( int len );	// check packet.

#ifdef _DEBUG
	void xInit_DeCrypt_FindKey( const BYTE* pCryptData, int len );
#endif

	int  Setup_FillCharList( CUOEventCharDef* pCharList, const CChar* pCharFirst );

	bool CanInstantLogOut() const;
	void Cmd_GM_PageClear();
	void GetAdjustedCharID( const CChar* pChar, CREID_TYPE& id, HUE_TYPE &wHue );
	void GetAdjustedItemID( const CChar* pChar, const CItem* pItem, HUE_TYPE &wHue );

	bool CmdTryVerb( CObjBase* pObj, const char* pszVerb );

	TRIGRET_TYPE Menu_OnSelect( CSphereUID rid, int iSelect, CObjBase* pObj );
	TRIGRET_TYPE Dialog_OnButton( CSphereUID rid, DWORD dwButtonID, CSphereExpArgs& exec );

	LOGIN_ERR_TYPE Login_ServerList( const char* pszAccount, const char* pszPassword );		// Initial login (Login on "loginserver", new format)
	bool Login_Relay( int iServer );			// Relay player to a certain IP
	void Announce( bool fArrive ) const;

	LOGIN_ERR_TYPE Setup_CharListReq( const char* pszAccount, const char* pszPassword, DWORD dwAccount );	// Gameserver login and character listing
	LOGIN_ERR_TYPE Setup_Start( CChar* pChar );	// Send character startup stuff to player
	void Setup_CreateDialog( const CUOEvent* pEvent );	// All the character creation stuff
	DELETE_ERR_TYPE Setup_Delete( int iSlot );			// Deletion of character
	bool Setup_Play( int iSlot );		// After hitting "Play Character" button

	// GM stuff.
public:
	bool OnTarg_Obj_Command( CObjBase* pObj, const TCHAR* pszCommand );
private:
	bool OnTarg_Obj_Info( CObjBase* pObj, const CPointMap& pt, ITEMID_TYPE id );

	bool OnTarg_UnExtract( CObjBase* pObj, const CPointMap& pt ) ;
	bool OnTarg_Stone_Recruit( CChar* pChar );
	bool OnTarg_Item_Add( CObjBase* pObj, const CPointMap& pt ) ;
	bool OnTarg_Item_Link( CObjBase* pObj );
	bool OnTarg_Tile( CObjBase* pObj, const CPointMap& pt );

	// Normal user stuff.
	bool OnTarg_Use_Deed( CItem* pDeed, const CPointMap &pt );
	bool OnTarg_Use_Item( CObjBase* pObj, CPointMap& pt, ITEMID_TYPE id );
	bool OnTarg_Party_Add( CChar* pChar );
// WESTY MOD (MULTI CONFIRM)
	CItemPtr OnTarg_Use_Multi( const CItemDef* pItemDef, const CPointMap& pt, CItemPtr pDeed );
// END WESTY MOD
//	CItemPtr OnTarg_Use_Multi( const CItemDef* pItemDef, const CPointMap& pt, WORD wAttr, HUE_TYPE wHue );

	int OnSkill_AnimalLore( CSphereUID uid, int iTestLevel, bool fTest );
	int OnSkill_Anatomy( CSphereUID uid, int iTestLevel, bool fTest );
	int OnSkill_Forensics( CSphereUID uid, int iTestLevel, bool fTest );
	int OnSkill_EvalInt( CSphereUID uid, int iTestLevel, bool fTest );
	int OnSkill_ArmsLore( CSphereUID uid, int iTestLevel, bool fTest );
	int OnSkill_ItemID( CSphereUID uid, int iTestLevel, bool fTest );
	int OnSkill_TasteID( CSphereUID uid, int iTestLevel, bool fTest );

	bool OnTarg_Skill_Magery( CObjBase* pObj, const CPointMap& pt );
	bool OnTarg_Skill_Herd_Dest( CObjBase* pObj, const CPointMap& pt );
	bool OnTarg_Skill_Poison( CObjBase* pObj );
	bool OnTarg_Skill_Provoke( CObjBase* pObj );
	bool OnTarg_Skill( CObjBase* pObj );

	bool OnTarg_Pet_Command( CObjBase* pObj, const CPointMap& pt );
	bool OnTarg_Pet_Stable( CChar* pCharPet );

	// Commands from client
	void Event_ClientVersion( const char* pData, int Len );
	void Event_Spy( const CUOEvent* pEvent );
	void Event_Target( DWORD context, CSphereUID uid, CPointMap pt, ITEMID_TYPE id );
	void Event_Attack( CSphereUID uid );
	void Event_Skill_Locks( const CUOEvent* pEvent );
	void Event_Skill_Use( SKILL_TYPE x ); // Skill is clicked on the skill list
	void Event_Tips( WORD i); // Tip of the day window
	void Event_Book_Title( CSphereUID uid, const char* pszTitle, const char* pszAuthor );
	void Event_Book_Page( CSphereUID uid, const CUOEvent* pEvent ); // Book window
	void Event_Item_Dye( CSphereUID uid, HUE_TYPE wHue );	// Rehue an item
	void Event_Item_Pickup( CSphereUID uidItem, int amount ); // Client grabs an item
	void Event_Item_Equip( CSphereUID uidItem, LAYER_TYPE layer, CSphereUID uidChar ); // Item is dropped on paperdoll
	void Event_Item_Drop( CSphereUID uidItem, CPointMap pt, CSphereUID uidOn ); // Item is dropped on ground
	void Event_VendorBuy( CSphereUID uidVendor, const CUOEvent* pEvent );
	void Event_SecureTrade( CSphereUID uid, SECURE_TRADE_TYPE action, bool fCheck );
	bool Event_DeathOption( DEATH_MODE_TYPE mode, const CUOEvent* pEvent );
	void Event_Walking( DIR_TYPE dir, bool fRun, BYTE bWalkCount, DWORD dwEchoCode = 0 ); // Player moves
	void Event_CombatMode( bool fWar ); // Only for switching to combat mode
	void Event_MenuChoice( CSphereUID uidItem, DWORD context, WORD select ); // Choice from GMMenu or Itemmenu received
	void Event_PromptResp( const char* pszText, int len );
	void Event_Talk_Common( char* szText ); // PC speech
	void Event_Talk( const char* pszText, HUE_TYPE wHue, TALKMODE_TYPE mode ); // PC speech
	void Event_TalkUNICODE( const CUOEvent* pEvent );
	void Event_SingleClick( CSphereUID uid );
	void Event_SetName( CSphereUID uid, const char* pszCharName );
	void Event_ExtCmd( EXTCMD_TYPE type, const char* pszName );
	bool Event_Command( const char* pszCommand ); // Client entered a '/' command like /ADD
	void Event_GumpInpValRet( const CUOEvent* pEvent );
	void Event_GumpDialogRet( const CUOEvent* pEvent );
	void Event_ToolTip( CSphereUID uid );
	void Event_ExtData( EXTDATA_TYPE type, const CUOExtData* pData, int len );
	void Event_MailMsg( CSphereUID uid1, CSphereUID uid2 );
	void Event_Profile( BYTE fWriteMode, CSphereUID uid, const CUOEvent* pEvent );
	void Event_MapEdit( CSphereUID uid, const CUOEvent* pEvent );
	void Event_BBoardRequest( CSphereUID uid, const CUOEvent* pEvent );
	void Event_ScrollClose( DWORD dwContext );
	void Event_ChatButton(const NCHAR* pszName); // Client's chat button was pressed
	void Event_ChatText( const NCHAR* pszText, int len, CLanguageID lang = 0 ); // Text from a client

public:
	bool Event_DoubleClick( CSphereUID uid, bool fMacro, bool fTestTouch );
	bool Event_VendorSell( CSphereUID uidVendor, int iSellCount, const CUOEventSell* pSellItems );

	// translated commands.
private:
	void Cmd_GM_PageInfo();
	int Cmd_Extract( CScript* pScript, CRectMap &rect, int& zlowest );
public:
	bool Cmd_CreateItem( ITEMID_TYPE id, bool fStatic = false );
	bool Cmd_CreateChar( CREID_TYPE id, SPELL_TYPE iSpell = SPELL_Summon, bool fPet = true );

	void Cmd_GM_PageMenu( int iEntryStart = 0 );
	void Cmd_GM_PageCmd( const char* pCmd );
	void Cmd_GM_PageSelect( int iSelect );
	void Cmd_GM_Page( const char* pszreason); // Help button (Calls GM Call Menus up)

	HRESULT Cmd_Skill_Menu( CSphereUID rid, int iSelect = -1 );
	HRESULT Cmd_Skill_Magery( SPELL_TYPE iSpell, CObjBase* pSrc );
	bool Cmd_Skill_Smith( CItem* pIngots );
	bool Cmd_Skill_Tracking( int track_type = -1, bool fExec = false ); // Fill menu with specified creature types
	bool Cmd_Skill_Inscription();
	bool Cmd_Skill_Alchemy( CItem* pItem );
	bool Cmd_Skill_Cartography( int iLevel );
	void Cmd_Skill_Heal( CItem* pBandages, CChar* pTarg );
	bool Cmd_SecureTrade( CChar* pChar, CItem* pItem );
	HRESULT Cmd_Control( CChar* pChar );

public:

	HRESULT s_PropGet( int index, CGVariant& vVal, CScriptConsole* pSrc );
	HRESULT s_PropSet( int index, CGVariant& vVal );
	HRESULT s_Method( int index, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute script type command on me

	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc );
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script

	// Low level message traffic.
	static int xCompress( BYTE* pOutput, const BYTE* pInput, int inplen );
	static int xDeCompress( BYTE* pOutput, const BYTE* pInput, int inplen );

	void xSendReady( const void *pData, int length ); // We could send the packet now if we wanted to but wait til we have more.
	void xSend( const void *pData, int length ); // Buffering send function
	void xFlush();				// Sends buffered data at once
	void xSendPkt( const CUOCommand* pCmd, int length )
	{
		xSendReady((const void *)( pCmd->m_Raw ), length );
	}
	bool xHasData() const
	{
		return(( m_bin.GetDataQty()) ? true : false );
	}
	bool xProcessClientSetup( CUOEvent* pEvent, int iLen );
	void xFinishProcessMsg( bool fGood );	// Finish Processing a packet
	bool xDispatchMsg();
	bool xRecvData();			// High Level Receive message from client

	// Low level push world data to the client.

	bool addRelay( const CServerDef* pServ );
	bool addLoginErr( LOGIN_ERR_TYPE code );
	void addPause( bool fPause = true );
	void addUpdateStatsFlag()
	{
		m_fUpdateStats |= true;
		addPause();
	}
	bool addDeleteErr( DELETE_ERR_TYPE code );
	void addExtData( EXTDATA_TYPE type, const CUOExtData* pData, int iSize );
	void addSeason( SEASON_TYPE season, bool fNormalCursor = true );
	void addOptions();
	void addObjectRemoveCantSee( CSphereUID uid, const char* pszName = NULL );
	void addObjectRemove( CSphereUID uid );
	void addObjectRemove( const CObjBase* pObj );
	void addRemoveAll( bool fItems, bool fChars );
	void addObjectLight( const CObjBase* pObj, LIGHT_PATTERN iLightType = LIGHT_LARGE );		// Object light level.

	void addItem_OnGround( CItem* pItem ); // Send items (on ground)
	void addItem_Equipped( const CItem* pItem );
	void addItem_InContainer( const CItem* pItem );
	void addItem( CItem* pItem );

	void addOpenGump( const CObjBase* pCont, GUMP_TYPE gump );
	void addContentsTopDown( const CItemContainer* pContainer );
	int  addContents( const CItemContainer* pCont, bool fCorpseEquip = false, bool fCorpseFilter = false, bool fShop = false ); // Send items
	bool addContainerSetup( const CItemContainer* pCont ); // Send Backpack (with items)

	void addPlayerStart( CChar* pChar );
	void addPlayerSee( const CPointMap& pt ); // Send objects the player can now see
	void addPlayerView( const CPointMap& pt );
	void addPlayerWarMode();
	void addPlayerWalkCancel();

	void addCharMove( const CChar* pChar );
	void addChar( const CChar* pChar );
	void addCharName( const CChar* pChar ); // Singleclick text for a character
	void addItemName( CItem* pItem );

	HRESULT addKick( CScriptConsole* pSrc, DWORD dwMinutes );
	void addWeather( WEATHER_TYPE weather = WEATHER_DEFAULT ); // Send new weather to player
	void addLight( int iLight = -1 );
	void addMusic( MIDI_TYPE id );
	void addArrowQuest( int x, int y );
	void addEffect( EFFECT_TYPE motion, ITEMID_TYPE id, const CObjBaseTemplate* pDst, const CObjBaseTemplate* pSrc, BYTE speed = 5, BYTE loop = 1, bool explode = false );
	void addSound( SOUND_TYPE id, const CObjBaseTemplate* pBase = NULL, int iRepeat = 1 );
	void addReSync();
	void addPing( BYTE bCode = 0 );

	void addBark( const char* pText, const CObjBaseTemplate* pSrc, HUE_TYPE wHue = HUE_TEXT_DEF, TALKMODE_TYPE mode = TALKMODE_SAY, FONT_TYPE font = FONT_BOLD );
	void addBarkUNICODE( const NCHAR* pText, const CObjBaseTemplate* pSrc, HUE_TYPE wHue = HUE_TEXT_DEF, TALKMODE_TYPE mode = TALKMODE_SAY, FONT_TYPE font = FONT_BOLD, CLanguageID lang = 0 );
	void addBarkSpeakTable( SPKTAB_TYPE index, const CObjBaseTemplate* pSrc, HUE_TYPE wHue = HUE_TEXT_DEF, TALKMODE_TYPE mode = TALKMODE_SAY, FONT_TYPE font = FONT_BOLD );
	void addObjMessage( const char* pMsg, const CObjBaseTemplate* pSrc, HUE_TYPE wHue = HUE_TEXT_DEF ); // The message when an item is clicked
	void addSysMessage( const char* pMsg, HUE_TYPE wHue = HUE_TEXT_DEF ); // System message (In lower left corner)
	void addSysMessageU( CGVariant& vArgs );

	void addDyeOption( const CObjBase* pBase );
	void addItemDragCancel( BYTE type );
	void addWebLaunch( const char* pMsg ); // Direct client to a web page

	void addPromptConsole( CLIMODE_TYPE mode, const char* pMsg );
	bool addTarget( CLIMODE_TYPE targmode, const char* pMsg, bool fAllowGround = false, bool fCheckCrime=false ); // Send targeting cursor to client
	void addTargetDeed( const CItem* pDeed );
	bool addTargetItems( CLIMODE_TYPE targmode, ITEMID_TYPE id );
	bool addTargetSummon( CLIMODE_TYPE mode, CREID_TYPE id, bool fNoto );
	bool addTargetVerb( const char* pCmd, const char* pArg );

	void addScrollText( const char* pszText );
	void addScrollScript( CResourceLock& s, SCROLL_TYPE type, DWORD dwcontext = 0, const char* pszHeader = NULL );
	void addScrollResource( const char* szResourceName, SCROLL_TYPE type, DWORD dwcontext = 0 );

	void addVendorClose( const CChar* pVendor );
	int  addShopItems( CChar* pVendor, LAYER_TYPE layer );
	bool addShopMenuBuy( CChar* pVendor );
	int  addShopMenuSellFind( CItemContainer* pSearch, CItemContainer* pFrom1, CItemContainer* pFrom2, int iConvertFactor, CUOCommand*& pCur );
	bool addShopMenuSell( CChar* pVendor );
	void addBankOpen( CChar* pChar, LAYER_TYPE layer = LAYER_BANKBOX );

	void addSpellbookOpen( CItem* pBook );
	bool addBookOpen( CItem* pBook );
	void addBookPage( const CItem* pBook, int iPage );
	bool addCharStatWindow( CSphereUID uid, bool fExtra ); // Opens the status window
	void addSkillWindow( CChar* pChar, SKILL_TYPE skill, bool fExtra ); // Opens the skills list
	void addBulletinBoard( const CItemContainer* pBoard );
	bool addBBoardMessage( const CItemContainer* pBoard, BBOARDF_TYPE flag, CSphereUID uidMsg );
	void addCharList3();

	void addToolTip( const CObjBase* pObj, const char* psztext );
	void addMap( CItemMap* pItem );
	void addMapMode( CItemMap* pItem, MAPCMD_TYPE iType, bool fEdit = false );

	void addGumpTextDisp( const CObjBase* pObj, GUMP_TYPE gump, const char* pszName, const char* pszText );
	void addGumpInpVal( bool fcancel, INPVAL_STYLE style,
		DWORD dwmask, const char* ptext1, const char* ptext2, CObjBase* pObj );

	void addContextMenu( CLIMODE_TYPE mode, const CMenuItem* item, int count, CObjBase* pObj = NULL );
	void addItemMenu( CLIMODE_TYPE mode, const CMenuItem* item, int count, CObjBase* pObj = NULL );
	void addGumpDialog( CLIMODE_TYPE mode, CGStringArray& asControls, CGStringArray& asText, int x, int y, CObjBase* pObj );

	bool addGumpDialogProps( CSphereUID uid );
	void addGumpDialogAdmin( int iPage, int iSortType );

	void addFeatureEnable( WORD wFeatureMask );
	void addChatSystemMessage(CHATMSG_TYPE iType, const char* pszName1 = NULL, const char* pszName2 = NULL, CLanguageID lang = 0 );

	virtual CGString GetName() const 
	{
		return CAccountConsole::GetName();
	}
	virtual void DeleteThis();
	CCharPtr GetChar() const
	{
		return( m_pChar );
	}
	virtual CScriptObj* GetAttachedObj() 
	{
#define GET_ATTACHED_CCHAR(p) STATIC_CAST(CChar,(p)->GetAttachedObj())
		return( static_cast <CScriptObj*> ( m_pChar.GetRefObj() ));
	}
	void CharDisconnect();

	virtual void SetMessageColorType( int iMsgColorType );
	virtual bool WriteString( const char* pMsg );		// System message (In lower left corner)
	bool CanSee( const CObjBaseTemplate* pObj ) const;
	bool CanHear( const CObjBaseTemplate* pSrc, TALKMODE_TYPE mode ) const;

	bool Dialog_Setup( CLIMODE_TYPE mode, CSphereUID rid, CObjBase* pObj );
	bool Menu_Setup( CSphereUID ridMenu, CObjBase* pObj, bool fContextMenu = false );

	int OnSkill_Info( SKILL_TYPE skill, CSphereUID uid, int iTestLevel, bool fTest );

	bool Cmd_Use_Item( CItem* pItem, bool fTestTouch );
	void Cmd_EditItem( CObjBase* pObj, int iSelect );

	bool IsConnectTypePacket() const
	{
		// This is a game or login server.
		// m_Crypt.IsInit()
		return( m_ConnectType == CONNECT_CRYPT || m_ConnectType == CONNECT_LOGIN || m_ConnectType == CONNECT_GAME );
	}

	// Stuff I am doing. Started by a command
	CLIMODE_TYPE GetTargMode() const
	{
		return( m_Targ.m_Mode );
	}
	void SetTargMode( CLIMODE_TYPE targmode = CLIMODE_NORMAL, const char* pszPrompt = NULL );
	void ClearTargMode()
	{
		// done with the last mode.
		m_Targ.m_Mode = CLIMODE_NORMAL;
	}

	CClient( SOCKET client );
	~CClient();
};

////////////////////////////////////////////////////////////////////////////////////

class CMailSMTP
{
public:
	static bool IsValidEmailAddressChar(char ch) { throw "not implemented"; }
	static bool IsValidEmailAddressFormat(LPCTSTR pszEmail) { throw "not implemented"; }

public:
	bool SendMail(LPCTSTR pszIp, LPCTSTR pszFrom, LPCTSTR pszTo, CGStringArray* pString) { throw "not implemented"; }
	LPCTSTR GetLastResponse() const { throw "not implemented"; }
};

enum PROFILE_TYPE
{
#define CPROFILEPROP(a,b,c)		PROFILE_##a,
#include "cprofileprops.tbl"
#undef CPROFILEPROP
	PROFILE_QTY,
};

class CProfilerPerfMon
{
public:
	void InitTasks(LPCTSTR lpszFile, int iPropCount, const CScriptPropX* pProps) { }
	void InitTasks(int iPropCount) { }
	int GetTaskStatusDesc(int iProp) { return 0; }
	int GetTaskCurrent() { return 0; }
	void SwitchTask(int type) { }
	void IncTaskCount(PROFILE_TYPE type, int iLenRet) { }
	int SetSampleWindowLen(int iLen) { return 0; }
	int GetSampleWindowLen() { return 0; }
	bool IsProfilingActive() const { return false; }
};

#ifndef _WIN32
typedef CProfilerPerfMon CProfiler;
#endif

enum SERVMODE_TYPE
{
	SERVMODE_ScriptBook,	// Run at Lower Priv Level for IT_EQ_SCRIPT_BOOK
	SERVMODE_RestockAll,	// Major event.
	SERVMODE_Saving,		// Forced save freezes the system.
	SERVMODE_Run,			// Game is up and running
	SERVMODE_ResyncPause,	// paused during resync
	SERVMODE_GarbageCollect,// forced garbage collection.
	SERVMODE_Loading,		// Initial load.
	SERVMODE_ResyncLoad,	// Loading after resync
	SERVMODE_Exiting,		// Closing down
	SERVMODE_Test5,			// Some test modes.
	SERVMODE_Test8,			// Some test modes.
	SERVMODE_QTY,
};

enum SPHEREERR_TYPE
{
	// Error codes that may have meaning to someone.

	SPHEREERR_BAD_SOCKET	= -9,	// can't open tcp/ip socket.
	SPHEREERR_BAD_WORLD	= -8,	// bad world file.
	SPHEREERR_INTERNAL	= -5,	// Major internal check failed.
	SPHEREERR_MULTI_INST	= -4,	// There are multi instances loaded?
	SPHEREERR_BAD_INI		= -3,	// The INI file was corrupt or could not be read.
	SPHEREERR_BAD_CODECRC = -2,	// the code does not match crc!
	SPHEREERR_COMMANDLINE	= -1,   // User commanded exit via command line args ?	

	SPHEREERR_OK = 0,				// Keep running.

	SPHEREERR_CONSOLE_X		= 1,
	SPHEREERR_TIMED_CLOSE		= 2,
	SPHEREERR_CTRLC			= 3,
	SPHEREERR_NTSERVICE_CLOSE = 4,
	SPHEREERR_WINDOW_CLOSE	= 5,
	SPHEREERR_WINDOW_QUIT		= 6,	// some quit message?
	SPHEREERR_COM_UNLOAD		= 7,

};

class CServer : public CServerDef, public CAccountConsole
{
public:
	CSCRIPT_CLASS_DEF2(Server);
	enum M_TYPE_
	{
#define CSERVERMETHOD(a,b,c) M_##a,
#include "cservermethods.tbl"
#undef CSERVERMETHOD
		M_QTY,
	};
	static const CScriptMethod sm_Methods[M_QTY+1];
#ifdef USE_JSCRIPT
#define CSERVERMETHOD(a,b,c) JSCRIPT_METHOD_DEF(a)
#include "cservermethods.tbl"
#undef CSERVERMETHOD
#endif

public:
	SERVMODE_TYPE m_iModeCode;  // Just some error code to return to system.
	SPHEREERR_TYPE  m_iExitFlag;	// identifies who caused the exit. <0 = error
	bool m_fResyncPause;		// Server is temporarily halted so files can be updated.
	DWORD m_dwParentThread;	// The thread we got Init in.
	DWORD m_dwTickCount;	// Last system tick count.

	CGSocket m_SocketMain;	// This is the incoming monitor socket.(might be multiple ports?)
	CGSocket m_SocketGod;	// This is for god clients.
	UINT m_uSizeMsgMax;		// get this from the TCP/IP stack params

	CServTime m_timeShutdown;	// When to perform the shutdowm (g_World.clock)

	int m_nClientsAreGuests;			// How many of the current clients are "guests". Not accurate !
	int m_nClientsAreAdminTelnets;		// how many of my clients are admin consoles ?
	CGObListType<CClient> m_Clients;		// Current list of clients (CClient)

#ifdef _WIN32
	CProfilerPerfMon m_Profile;	// the current active statistical profile.
#else
	CProfiler m_Profile;	// the current active statistical profile.
#endif

	CChat m_Chats;	// keep all the active chats

public:
	bool IsValidBusy() const;
	bool OnTick_Busy();
	SERVMODE_TYPE SetServerMode( SERVMODE_TYPE mode );
	bool TestServerIntegrity();

	void SetExitFlag( SPHEREERR_TYPE iFlag );
	void SetShutdownTime( int iMinutes );
	bool IsLoading() const
	{
		return( m_iModeCode > SERVMODE_Run || m_fResyncPause );
	}
	void SetSignals();

	bool SocketsInit(); // Initialize sockets
	bool SocketsInit( CGSocket& socket, int iPort );
	void SocketsReceive();
	CClientPtr SocketsAccept( CGSocket& socket, bool fGod );
	void SocketsFlush();
	void SocketsClose();

	bool Load();

	virtual bool WriteString( const char* pMsg );
	void Event_PrintClient( const char* pszMsg ) const;
	void Event_PrintPercent( SERVTRIG_TYPE type, long iCount, long iTotal );

	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script
	virtual HRESULT s_Method( int iProp, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc );
	virtual void s_WriteProps( CScript& s );

	CString GetStatusString( BYTE iIndex = 0 ) const;
	CString GetStatusStringRegister( bool fFirst ) const;
	int GetAgeHours() const;

	bool OnConsoleCmd( CGString& sText, CScriptConsole* pSrc );

	void OnTick();

private:
	void ListServers( CStreamText* pConsole ) const;

public:
	void ListClients( CScriptConsole* pClient ) const;
	void ListGMPages( CStreamText* pConsole ) const;

	CClientPtr FindClientAccount( const CAccount* pAccount ) const;
	CClientPtr GetClientHead() const
	{
		return( m_Clients.GetHead());
	}

	void SetResyncPause( bool fPause, CScriptConsole* pSrc );
	HRESULT CommandLineArg( TCHAR* pArg );

	virtual CGString GetName() const 
	{ 
		return( CServerDef::GetName()); 
	}
	virtual int GetPrivLevel() const
	{
		if ( m_iModeCode == SERVMODE_ScriptBook )
			return( PLEVEL_GM );
		return( PLEVEL_Admin );
	}

	CString GetModeDescription() const;
	virtual void OnTriggerEvent( SERVTRIG_TYPE type, DWORD dwArg1=0, DWORD dwArg2=0 );

	CServer();
	~CServer();
};

extern CServer g_Serv;	// current state stuff not saved.

class CBackTask : public CSphereThread
{
	// Do polling ad other stuff in a background thread.
	// Check the creation of the back task.
public:
	CBackTask()
	{
		m_iTotalPolledAccounts = 0;
		m_iTotalPolledClients = 0;
	}
	void CreateThread();
	void CheckStuckThread();

public:
	CGString m_sRegisterResult;			// result of last register attempt.
	CGString m_sRegisterResult2;
	int		m_iTotalPolledClients;
	int		m_iTotalPolledAccounts;

private:
	static THREAD_ENTRY_RET _cdecl EntryProc( void* lpThreadParameter );
	void EntryTask();
	bool RegisterServer( bool fFirst, CSocketNamedAddr& addr, CGString& sResult );
	void PollServers();

private:
	CServTime m_timeNextRegister;	// only register once every 2 hours or so.
	CServTime m_timeNextRegister2;
	CServTime m_timeNextPoll;		// Only poll listed CServerDef once every 10 minutes or so.
	CServTime m_timeNextMail;		// Time to check for outgoing mail ?
};

extern CBackTask g_BackTask;

class CServTask : public CSphereThread
{
	// The main world thread. (handles all client connections)
public:
	CServTask()
	{
	}
	void CreateThread();
	void CheckStuckThread();
public:
	static THREAD_ENTRY_RET _cdecl EntryProc( void* lpThreadParameter );
private:
	CServTime m_timeRestart;
	CServTime m_timeWarn;
	CServTime m_timePrev;
};

extern CServTask g_ServTask;

struct CMainTask : public CSphereThread
{
	// This thread is only used in NT.
	// It inits and decides to exit.
	// It checks the status of the server.
	// It gives ticks to the GUI window.
	static THREAD_ENTRY_RET _cdecl EntryProc( void* lpThreadParameter );
	void CreateThread();
};

extern CMainTask g_MainTask;

class CServConsole : public CScriptConsole
{
	// The interface to the main console.
public:
	CServConsole()
	{
		m_fCommandReadyFlag = false;
	}

	virtual CGString GetName() const	// name of the console.
	{
		return( "Console" );
	}
	virtual int GetPrivLevel() const // What privs do i have ? PLEVEL_TYPE Higher = more.
	{
		return PLEVEL_Admin;
	}

	virtual void SetMessageColorType( int iMsgColorType );
	virtual bool WriteString( const char* pszMsg );

	bool IsCommandReady() const
	{
		return m_fCommandReadyFlag;
	}
	bool SendCommand( CGString sCmd )
	{
		if ( m_fCommandReadyFlag )
			return false;
		m_sCommand = sCmd;
		m_fCommandReadyFlag = true;
		return true;
	}
	CGString GetCommand()
	{
		CGString sTmp = m_sCommand;
		m_sCommand.Empty();
		m_fCommandReadyFlag = false;
		return sTmp;
	}

#ifdef _WIN32
	bool Init( HINSTANCE hInstance, LPSTR lpCmdLinel, int nCmdShow );
#else
	bool Init( LPSTR lpCmdLinel, int nCmdShow );
#endif
	void Exit();
	bool OnTick( int iWaitmSec );
	void OnTriggerEvent( SERVTRIG_TYPE iType, DWORD dwArg1, DWORD dwArg2 );

protected:
	CGString m_sCommand;		// local console input.
	CGString m_sConsoleText;	// current console text being typed.
	bool m_fCommandReadyFlag;	// interlocking flag for moving between tasks.
	bool m_fConsoleTextReadyFlag;
};

extern CServConsole g_ServConsole;

#if defined(_WIN32) && ! defined(_LIB)
class CSphereService : public CNTService
{
public:
	int MainEntryPoint( int argc, char* argv[] );
	const char* GetServiceName(int iType) const;
};
extern CSphereService g_NTService;
#endif

//////////////////////////////////////////////////////////////

extern const char* g_szServerDescription;
extern const char* const g_Stat_Name[STAT_QTY+1];
extern const CPointMap g_pntLBThrone; // This is origin for degree, sextant minute coordinates
extern const CScriptPropX g_ProfileProps[PROFILE_QTY+1];

extern SPHEREERR_TYPE Sphere_InitServer( int argc, char *argv[] );
extern SPHEREERR_TYPE Sphere_OnTick();
extern void Sphere_ExitServer();
extern SPHEREERR_TYPE Sphere_MainEntryPoint( int argc, char *argv[] );

extern "C"
{
	extern void globalstartsymbol();
	extern void globalendsymbol();
	extern const BYTE globalstartdata;
	extern const BYTE globalenddata;
}

#endif	// _INC_SPHERESVR_H_
