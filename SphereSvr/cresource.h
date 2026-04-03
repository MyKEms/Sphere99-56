//
// CResource.h
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#ifndef _INC_CRESOURCE_H
#define _INC_CRESOURCE_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "CAssoc.h"
#include "cresourcebase.h"
#include "caccountbase.h"
#include "cmulmulti.h"

class CAccount;
class CClient;
class CLogIP;

class CServerDef;
typedef CRefPtr<CServerDef> CServerPtr;
typedef CRefPtr<CServerDef> CServerLock;
class CCharDef;
typedef CRefPtr<CCharDef> CCharDefPtr;
class CItemDef;
typedef CRefPtr<CItemDef> CItemDefPtr;
class CRaceClassDef;
typedef CRefPtr<CRaceClassDef> CRaceClassPtr;

class CItemContainer;
class CItemMessage;
class CItemMap;
class CItemServerGate;

//*************************************************************************

enum BODYPART_TYPE
{
	// The parts of a body that any creature will have.
	BODYPART_HEAD = 0,
	BODYPART_NECK,
	BODYPART_BACK,
	BODYPART_CHEST,	// or thorax
	BODYPART_ARMS,
	BODYPART_HANDS,
	BODYPART_LEGS,
	BODYPART_FEET,
	BODYPART_ARMOR_QTY,	// All the parts that armor will cover.

	// Armor does not cover this.

	BODYPART_LEGS2,	// Alternate set of legs (spider)
	BODYPART_TAIL,	// Dragon, Snake, Alligator, etc. (tail attack?)
	BODYPART_WINGS,	// Dragon, Mongbat, Gargoyle
	BODYPART_CLAWS,	// can't wear any gloves here!
	BODYPART_HOOVES,	// No shoes
	BODYPART_HORNS,	// Bull, Daemon

	BODYPART_STALK,		// Gazer or Corpser
	BODYPART_BRANCH,	// Reaper.
	BODYPART_TRUNK,		// Reaper.
	BODYPART_PSEUDOPOD,	// Slime
	BODYPART_ABDOMEN,	// Spider or insect. asusme throax and chest are the same.

	BODYPART_QTY,
};

#define DAMAGE_GOD			0x0001	// Nothing can block this.
#define DAMAGE_HIT_BLUNT	0x0002	// Physical hit of some sort.
#define DAMAGE_MAGIC		0x0004	// Magic blast of some sort. (we can be immune to magic to some extent)
#define DAMAGE_POISON		0x0008	// Or biological of some sort ? (HARM spell)
#define DAMAGE_FIRE			0x0010	// Fire damage of course.  (Some creatures are immune to fire)
#define DAMAGE_ELECTRIC		0x0020	// lightning.
#define DAMAGE_DRAIN		0x0040	// level drain = negative energy.
#define DAMAGE_GENERAL		0x0080	// All over damage. As apposed to hitting just one point.
#define DAMAGE_ACID			0x0100	// Corrosive will destroy armor.
#define DAMAGE_COLD			0x0200	// Cold freezing damage
#define DAMAGE_HIT_SLASH	0x0400	// sword
#define DAMAGE_HIT_PIERCE	0x0800	// spear.
#define DAMAGE_HOLY			0x1000	// Only does damage to evil

typedef WORD DAMAGE_TYPE;		// describe a type of damage.

class CQuestDef : public CResourceDef
{
	// RES_QUEST
	// Quests are scripted things that run groups of NPC's and players.
	// Players involvement in a quest should be carried in their memory as well.

public:
	// CSCRIPT_CLASS_DEF1(QuestDef);
public:
	static const CScriptProp sm_Props[];

	CGString m_sName;	// List is sorted by name of the quest. This is the TAG_NAME on player?
	int m_iState;		// What is the STATE of the quest.
	DWORD m_dwFlags;	// Arbitrary flags for the quest.
	CUIDRefArray m_Chars;	// Chars involved in the quest. PCs and NPC's

	// These generate Titles that may be listed for the players in Profiles.

public:
	virtual CGString GetName() const
	{
		// ( every CScriptObj must have at least a type name )
		return( m_sName );
	}
	CQuestDef();
	virtual ~CQuestDef()
	{
	}
};

enum WEBPAGE_TYPE
{
	WEBPAGE_TEMPLATE,	// Sphere script template.
	WEBPAGE_TEXT,
	WEBPAGE_BMP,
	WEBPAGE_GIF,
	WEBPAGE_JPG,
	WEBPAGE_PNG,
	WEBPAGE_BIN,	// just some other binary format.
	WEBPAGE_QTY,
};

class CWebPageDef : public CResourceTriggered
{
	// RES_WebPage
	// This is a single web page we are generating or serving.

public:
	CWebPageDef( CSphereUID id );
	virtual ~CWebPageDef()
	{
	}

	virtual CGString GetName() const
	{
		return( m_sSrcFilePath );
	}
	CGString GetDstName() const
	{
		return( m_sDstFilePath );
	}
	void SetPageType( WEBPAGE_TYPE iType )
	{
		m_type = iType;
	}
	bool IsMatch( LPCTSTR IsMatchPage ) const;
	HRESULT SetSourceFile( LPCTSTR pszName, CClient* pClientSrc );

	bool ServePagePost( CClient* pClient, LPCTSTR pszURLArgs, TCHAR* pPostData, int iContentLength );
	int ServePageRequest( CClient* pClient, LPCTSTR pszURLArgs, CGTime* pdateLastMod );
	void OnTick( bool fNow );

	static HRESULT ServePage( CClient* pClient, const char* pszReqPage, CGTime* pdateLastMod );

	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CWEBPAGEPROP(a,b,c) P_##a,
#include "cwebpageprops.tbl"
#undef CWEBPAGEPROP
		P_QTY,
	};
	enum M_TYPE_
	{
#define CWEBPAGEMETHOD(a,b,c) M_##a,
#include "cwebpagemethods.tbl"
#undef CWEBPAGEMETHOD
		M_QTY,
	};
	enum T_TYPE_	 // Web page triggers
	{
		// XTRIG_UNKNOWN	= some named trigger not on this list. (Forms?)
#define CWEBPAGEEVENT(a,b,c) T_##a,
		CWEBPAGEEVENT(AAAUNUSED,0,NULL)
#include "cwebpageevents.tbl"
#undef CWEBPAGEEVENT
		T_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];
	static const CScriptMethod sm_Methods[M_QTY+1];
	static const CScriptProp sm_Triggers[T_QTY+1];
	static LPCTSTR const sm_szPageType[];
	static LPCTSTR const sm_szPageExt[];

#ifdef USE_JSCRIPT
#define CWEBPAGEMETHOD(a,b,c) JSCRIPT_METHOD_DEF(a)
#include "cwebpagemethods.tbl"
#undef CWEBPAGEMETHOD
#endif

protected:
	DECLARE_MEM_DYNAMIC;

private:
	static CGString GetErrorBasicText( int iErrorCode );
	static HRESULT ServeGenericError( CClient* pClient, const char* pszPage, int iErrorCode );
	bool GenerateTemplateFile( CSphereExpContext& exec );
	int ServeTemplate( CClient* pClient, CSphereExpContext& exec );
	int ServeFile( CClient* pClient, LPCTSTR pszSrcFile, CGTime* pdateIfModifiedSince );

private:
	WEBPAGE_TYPE m_type;		// What basic format of file is this ? 0=text
	CGString m_sSrcFilePath;	// source template for the generated web page.
	PLEVEL_TYPE m_privlevel;	// What priv level to see this page ?

	// For files that are being translated and updated.
	// or served from some other HTTP server.
	CGString m_sDstFilePath;	// where is the page served from ?
	int  m_iUpdatePeriod;		// How often to update the web page. 0 = never. -1 = decay
	CServTime  m_timeNextUpdate;
};

typedef CRefPtr<CWebPageDef> CWebPagePtr;

class CSpellDef : public CResourceDef	// 1 based spells. See SPELL_*
{
	// RES_Spell
public:
	CSpellDef( SPELL_TYPE id );
	virtual ~CSpellDef()
	{
	}

	bool IsSpellType( WORD wFlags ) const
	{
		return(( m_wFlags & wFlags ) ? true : false );
	}
	virtual CGString GetName() const { return( m_sName ); }
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CSPELLDEFPROP(a,b,c) P_##a,
#include "cspelldefprops.tbl"
#undef CSPELLDEFPROP
		P_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];

	CResourceQtyArray m_Reags;	// What reagents does it take to cast ?
	CResourceQtyArray m_SkillReq;	// What skills/unused reagents does it need to cast.
	ITEMID_TYPE m_idSpell;		// The rune graphic for this.
	ITEMID_TYPE m_idScroll;		// The scroll graphic item for this.

	// When this spell is activated.
	SOUND_TYPE m_sound;			// What noise does it make when done.
	CGString m_sRunes;			// Letter Runes for Words of power.
	ITEMID_TYPE m_idEffect;		// Animation effect ID
	WORD m_wManaUse;			// How much mana does it need. ->m_Reags
	int m_iCastTime;			// In TICKS_PER_SEC.
	CValueCurveDef m_Effect;	// Damage or effect level based on skill of caster.100% magery
	CValueCurveDef m_Duration;	// length of effect. in TICKS_PER_SEC
	CValueCurveDef m_Value;		// How much is this spell effect on embued object worth ?
	CGString m_sTargetPrompt;	// Has a targeting prompt ?

protected:
	DECLARE_MEM_DYNAMIC;
private:
	WORD m_wFlags;
#define SPELLFLAG_DIR_ANIM  0x0001	// Evoke type cast or directed. (animation)
#define SPELLFLAG_TARG_OBJ  0x0002	// Need to target an object or char ?
#define SPELLFLAG_TARG_CHAR 0x0004	// Needs to target a living thing
#define SPELLFLAG_TARG_XYZ  0x0008	// Can just target a location.
#define SPELLFLAG_HARM		0x0010	// The spell is in some way harmfull.
#define SPELLFLAG_FX_BOLT	0x0020	// Effect is a bolt to the target.
#define SPELLFLAG_FX_TARG	0x0040	// Effect is at the target.
#define SPELLFLAG_FIELD		0x0080	// create a field of stuff. (fire,poison,wall)
#define SPELLFLAG_SUMMON	0x0100	// summon a creature.
#define SPELLFLAG_GOOD		0x0200	// The spell is a good spell. u intend to help to receiver.
#define SPELLFLAG_RESIST	0x0400	// Allowed to resist this.
#define SPELLFLAG_DISABLED	0x8000
	CGString m_sName;	// spell name (displayable)
};

typedef const CSpellDef* CSpellDefPtr;
typedef CSpellDef* CSpellDefWPtr;		// I want write access

typedef short STAT_LEVEL;	// the value for Stat or Skill

enum STAT_TYPE	// Standard Character stats. 
{
	// All chars have all stats. (as opposed to skills which not all will have)
	STAT_Str = 0,
	STAT_Int,
	STAT_Dex,

#define STAT_BASE_QTY	3	// STAT_Str to STAT_Dex
	STAT_Health,	// How damaged are we ?
	STAT_Mana,		// How drained of mana are we ?
	STAT_Stam,		// How tired are we ?
	STAT_Food,		// Food level (.5 days food level at normal burn)

#define STAT_REGEN_QTY 4
	STAT_Fame,		// How much hard stuff have u done ? Degrades over time???
	STAT_Karma,		// -10000 to 10000 = how good are you ?

	STAT_MaxHealth,
	STAT_MaxMana,
	STAT_MaxStam,

	// STAT_SpellFail - as a percent.
	// STAT_AttackLo - low attack
	// STAT_AttackHi
	// STAT_AttackSpeed

	STAT_QTY,
};

struct CSkillDef : public CResourceTriggered // For skill def table
{
	// RES_Skill
	// NOTE: This should depend on other skills as well.
	//  lay out the dependancies here. ???
	//  Not all skills are as valuable as others. (and not always linear)

public:
	CSkillDef( SKILL_TYPE iSkill );
	virtual ~CSkillDef()
	{
	}

	LPCTSTR GetSkillKey() const
	{
		return( m_sKey );	// not the same as DEFNAME but can be used as such
	}

	virtual CGString GetName() const { return( m_sKey ); }
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CSKILLDEFPROP(a,b,c) P_##a,
#include "cskilldefprops.tbl"
#undef CSKILLDEFPROP
		P_QTY,
	};
	enum T_TYPE_
	{
		// All skills may be scripted.
#define CSKILLDEFEVENT(a,b,c) T_##a,
		CSKILLDEFEVENT(AAAUNUSED,CSCRIPTPROP_UNUSED,NULL)
#include "cskilldefevents.tbl"
#undef CSKILLDEFEVENT
		T_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];
	static const CScriptProp sm_Triggers[T_QTY+1];

	CGString m_sTitle;	// title one gets if it's your specialty.
	CGString m_sTargetPrompt;	// targeting prompt. (if needed)

	// success at this skill depends on other things.
	// You will tend toward these stat vals if you use this skill a lot.
	STAT_LEVEL m_Stat[STAT_BASE_QTY];	// STAT_STR, STAT_INT, STAT_DEX
	BYTE m_StatPercent; // BONUS_STATS = % of success depending on stats
	BYTE m_StatBonus[STAT_BASE_QTY]; // % of each stat toward success at skill, total 100

	// Typical params for a skill. (varies by skill level)
	CValueCurveDef m_Delay;		// The base delay for use of the skill. (tenth of seconds)
	CValueCurveDef m_Effect;	// depends on skill. but extent (power) of the effect.

	CValueCurveDef m_AdvRate;	// ADV_RATE defined "skill curve" 0 to 100 skill levels.
	CValueCurveDef m_Values;	// VALUES= influence for items made with 0 to 100 skill levels.

	// Delay before skill complete. modified by skill level of course !
protected:
	DECLARE_MEM_DYNAMIC;
private:
	CGString m_sKey;	// script key word for skill.
};

typedef const CSkillDef* CSkillDefPtr;
typedef CSkillDef* CSkillDefWPtr;		// I want write access

class CProfessionDef : public CResourceLink // For skill def table
{
	// Similar to character class. ie. Ranger, Barbarian
	// RES_Profession
public:
	CSCRIPT_CLASS_DEF2(ProfessionDef);
	enum P_TYPE_
	{
#define CPROFESSIONPROP(a,b,c) P_##a,
#include "cprofessionprops.tbl"
#undef CPROFESSIONPROP
		P_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];

protected:
	DECLARE_MEM_DYNAMIC;
public:
	CGString m_sName;	// The name of this skill class.

	DWORD m_StatSumMax;
	DWORD m_SkillSumMax;

	STAT_LEVEL m_StatMax[STAT_BASE_QTY];	// STAT_QTY
	STAT_LEVEL m_SkillLevelMax[SKILL_QTY];	// SKILL_LEVEL

private:
	void Init();
public:
	virtual CGString GetName() const { return( m_sName ); }

	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );

	CProfessionDef( CSphereUID rid ) : CResourceLink( rid )
	{
		// If there was none defined in scripts.
		Init();
	}
	virtual ~CProfessionDef()
	{
	}
};

typedef CRefPtr<CProfessionDef> CProfessionPtr;
typedef CResLockNameArray<CServerDef> CServArray;	// use CThreadLockPtr

#define FOR_HASH(arr,idx1,idx2) int idx1, idx2; for (int idx1 = 0; idx1 < 5; idx1++)

class CResourceMgr
{
public:
	CVarDefArray m_Const;
	CVarDefArray m_Var;
	CHashArray<CResourceDef> m_ResHash;
	CGString m_sSCPBaseDir;		// if we want to get *.SCP files from elsewhere.
	CGRefArray<CResourceScript> m_ResourceFiles;

public:
	LPCTSTR ResourceGetName(CSphereUID rid) const;
	CResourceFilePtr FindResourceFile(LPCTSTR pszName);
	CResourceFilePtr LoadResourcesAdd(LPCTSTR pszNewName);
	virtual CResourceDefPtr ResourceGetDef(UID_INDEX rid);
	bool LoadResources(CResourceScript* pResScript);
	void LoadResourcesOpen(CResourceScript &script);
	void AddResourceFile(LPCTSTR pszFile);
	bool OpenScriptFind(CScript& s, LPCTSTR pszName);
	CResourceFilePtr GetResourceFile(int iIndex);
	void AddResourceDir(LPCTSTR pszDirName);
	void DeleteResourceFile(LPCTSTR pszFile);
};

class CSphereResourceMgr : public CResourceMgr
{
	// Script defined resources (not saved in world file)
public:
	CSphereResourceMgr();
	~CSphereResourceMgr();

	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );
	virtual void s_WriteProps( CScript& s );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script

	bool SaveIni();
	bool LoadIni( bool fTest );
	bool Load( bool fResync );
	void Unload( bool fResync );
	void OnTick( bool fNow );

#if 1
	// Search for a type of resource item.
	int ResourceGetIndex( RES_TYPE restype, LPCTSTR pszName );
	int ResourceGetIndexType( RES_TYPE restype, LPCTSTR pszName );

	CSphereUID ResourceCheckIDType( RES_TYPE restype, LPCTSTR pszName );
	static CSphereUID ResourceGetID( RES_TYPE restype, HASH_INDEX index );
	CSphereUID ResourceGetIDByName( RES_TYPE restype, LPCTSTR pszName );
	CSphereUID ResourceGetIDType( RES_TYPE restype, LPCTSTR pszName )
	{
		// Get a resource of just this index type.
		CSphereUID rid = ResourceGetIDByName( restype, pszName );
		if ( rid.GetResType() != restype )
		{
			rid.InitUID();
			return( rid );
		}
		return( rid );
	}
	CResourceDefPtr ResourceGetDefByName( RES_TYPE restype, LPCTSTR pszName )
	{
		// resolve a name to the actual resource def.
		return( ResourceGetDef( ResourceGetIDByName( restype, pszName )));
	}
#endif

	bool ResourceDump( const char* pArg );

	bool ResourceTest( LPCTSTR pszFilename );
	bool ResourceTestSort( LPCTSTR pszFilename );
	bool ResourceTestSkills();
	bool ResourceTestItemDupes();
	bool ResourceTestItemMuls();
	bool ResourceTestCharAnims();

	virtual bool LoadScriptSection( CScript& s );
	virtual CResourceDefPtr ResourceGetDef( UID_INDEX rid );
	virtual const CScript* SetScriptContext( const CScript* pScriptContext );
	virtual CResourceObjPtr FindUID( UID_INDEX rid );

	static LPCTSTR GetResourceBlockName( RES_TYPE restype );

	// Specialized resource accessors.

	bool CanUsePrivVerb( const CScriptObj* pObjTarg, LPCTSTR pszCmd, CScriptConsole* pSrc ) const;
	PLEVEL_TYPE GetPrivCommandLevel( LPCTSTR pszCmd ) const;

	static STAT_TYPE FindStatKey( LPCTSTR pszKey, bool fAllowDigit );
	bool IsObscene( LPCTSTR pszText ) const;
	CWebPagePtr FindWebPage( const char* pszPath ) const;

	bool SetLogIPBlock( LPCTSTR pszIP, LPCTSTR pszBlock, CScriptConsole* pSrc );

	CServerPtr Server_GetDef( int index );

	CLogIPPtr FindLogIP( const CGSocket& socket )
	{
		return m_LogIP.FindLogIP( socket );
	}

	CSpellDefPtr GetSpellDef( SPELL_TYPE index ) const
	{
		if ( ! index || ! m_SpellDefs.IsValidIndex(index))
			return( NULL );
		return( m_SpellDefs[index] );
	}

	LPCTSTR GetSkillKey( SKILL_TYPE index ) const
	{
		return( m_SkillIndexDefs[index]->GetSkillKey());
	}
	CSkillDefPtr GetSkillDef( SKILL_TYPE index ) const
	{
		return( m_SkillIndexDefs[index] );
	}
	CSkillDefWPtr GetSkillDefW( SKILL_TYPE index )
	{
		return( index < SKILL_QTY ? m_SkillIndexDefs[index] : NULL );
	}
	CSkillDefPtr FindSkillDef( LPCTSTR pszKey ) const
	{
		// Find the skill name in the alpha sorted list.
		// RETURN: SKILL_NONE = error.
		int i = m_SkillNameDefs.FindKey( pszKey );
		if ( i < 0 )
			return( NULL );
		return( m_SkillNameDefs.ConstElementAt(i));
	}
	SKILL_TYPE FindSkillKey( LPCTSTR pszKey, bool fAllowDigit ) const;

	int GetSpellEffect( SPELL_TYPE spell, int iSkillval ) const;

	LPCTSTR GetRune( TCHAR ch ) const
	{
		ch = toupper(ch) - 'A';
		if ( ! m_Runes.IsValidIndex(ch))
			return( _TEXT("?"));
		return( m_Runes.ConstElementAt(ch));
	}
	LPCTSTR GetNotoTitle( int iLevel ) const
	{
		if ( ! m_NotoTitles.IsValidIndex(iLevel))
		{
			return( _TEXT(""));
		}
		else
		{
			return m_NotoTitles.ConstElementAt(iLevel);
		}
	}

	const CMulMultiType* GetMultiItemDefs( ITEMID_TYPE itemid );
	CCharDefPtr FindCharDef( CREID_TYPE id );
	CItemDefPtr FindItemDef( ITEMID_TYPE id );

	bool CanRunBackTask() const;
	bool IsConsoleCmd( TCHAR ch ) const;
	void SetWorldStatics( LPCTSTR pszFileName );

	CPointMap GetRegionPoint( LPCTSTR pCmd ) const; // Decode a teleport location number into X/Y/Z

	int Calc_MaxCarryWeight( const CChar* pChar ) const;
	int Calc_DropStamWhileMoving( CChar* pChar, int iWeightLoadPercent );
	int Calc_WalkThroughChar( CChar* pCharMove, CChar* pCharObstacle );
	int Calc_CombatAttackSpeed( CChar* pChar, CItem* pWeapon );
	int Calc_CombatChanceToHit( CChar* pChar, SKILL_TYPE skill, CChar* pCharTarg, CItem* pWeapon );
	int Calc_CombatDamage( CChar* pChar, CItem* pWeapon, CChar* pCharTarg );
	int Calc_CharDamageTake( CChar* pChar, CItem* pWeapon, CChar* pCharAttacker, int iDamage, DAMAGE_TYPE DamageType, BODYPART_TYPE LocationHit );
	int Calc_ItemDamageTake( CItem* pItem, CItem* pWeapon, CChar* pCharAttacker, int iDamage, DAMAGE_TYPE DamageType, BODYPART_TYPE LocationHit );
	bool Calc_SkillCheck( int iSkillLevel, int iDifficulty );
	int  Calc_StealingItem( CChar* pCharThief, CItem* pItem, CChar* pCharMark );
	bool Calc_CrimeSeen( CChar* pCharThief, CChar* pCharViewer, SKILL_TYPE SkillToSee, bool fBonus );
	int Calc_FameKill( CChar* pKill );
	int Calc_FameScale( int iFame, int iFameChange );
	int Calc_KarmaKill( CChar* pKill, NOTO_TYPE NotoThem );
	int Calc_KarmaScale( int iKarma, int iKarmaChange );

private:
	void LoadChangedFiles();

	CSphereUID ResourceGetNewID( RES_TYPE restype, LPCTSTR pszName, CVarDefPtr& pVarNum );

public:
	static LPCTSTR const sm_szResourceBlocks[RES_QTY+1];
	static const CAssocReg sm_PropsAssoc[];

	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CRESOURCEPROP(a,b,c,d,e) P_##a,
#include "cresourceprops.tbl"
#undef CRESOURCEPROP
		P_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];

	CServTime m_timePeriodic;	// When to perform the next periodic update

	// Begin INI file options.

	// Server control features.
	int  m_iPollServers;		// background polling of peer servers. (minutes)
	CGString m_sMainLogServerDir;	// This is the main log server Directory. Will list any server that polls.
	int  m_iMapCacheTime;		// Time in sec to keep unused map data.
	int	 m_iSectorSleepMask;	// The mask for how long sectors will sleep.
	CSocketNamedAddr m_EMailGateway;	// Where to forward mail?
	CSocketNamedAddr m_RegisterServer;	// SPHERE_MAIN_SERVER,2593
	CSocketNamedAddr m_RegisterServer2; // alternate registry server.
	int m_iServerValidListTime;	// How long is a server valid for after list request.

	CGString m_sWorldBaseDir;	// "e:\spheresvr\worldsave\" = world files go here.
	CGString m_sWorldStatics;	// World statics file (read last) (items not saved)
	CGString m_sAcctBaseDir;	// Where do the account files go/come from ?
	CGString m_sSCPInBoxDir;	// Take files from here and replace existing files. NOT USED.

	bool m_fSecure;				// Secure mode. (will trap exceptions)
	int  m_iFreezeRestartTime;	// # seconds before restarting.
#define DEBUGF_NPC_EMOTE		0x0001
#define DEBUGF_ADVANCE_STATS	0x0002
#define DEBUGF_MOTIVATION		0x0004	// display motication level debug messages.
#define DEBUGF_SKILL_TIME		0x0010	// Show the amount of time til skill exec.
#define DEBUGF_WALKCODES		0x0080	// try the new walk code checking stuff.
	WORD m_wDebugFlags;			// DEBUG In game effects to turn on and off.
	CLanguageID m_LangDef;		// default language for the server.

	// Decay
	int  m_iDecay_Item;			// Base decay time in minutes.
	int  m_iDecay_CorpsePlayer;	// Time for a playercorpse to decay (mins).
	int  m_iDecay_CorpseNPC;	// Time for a NPC corpse to decay.

	// Save
	int  m_iSavePeriod;			// Minutes between saves.
	int  m_iSaveBackupLevels;	// How many backup levels.
	int  m_iSaveBackgroundTime;	// Speed of the background save in minutes.
	bool m_fSaveGarbageCollect;	// Always force a full garbage collection.
	bool m_fSaveInBackground;	// Do background save stuff.

	// Account
	bool m_fRequireEmail;		// Valid Email required to leave GUEST mode.
	bool m_fArriveDepartMsg;    // General switch to turn on/off arrival/depart messages.
	int  m_iGuestsMax;			// Allow guests who have no accounts ?
	int  m_iMinCharDeleteTime;	// How old must a char be ? (minutes)
	int  m_iMaxCharsPerAccount;	// MAX_CHARS_PER_ACCT

	int  m_iDeadSocketTime;
	int  m_iClientsPerIPMax;
	int  m_iClientsMax;			// Maximum (FD_SETSIZE) open connections to server
	int  m_iClientLingerTime;	// How long logged out clients linger in seconds.
	bool m_fLocalIPAdmin;		// The local ip is the admin ?
	CCryptVersion m_ClientVerMin;

	CServerPtr m_pServAccountMaster;	// Verify my accounts through here.

	// Magic
	int  m_iPreCastTime;		// Precasting, must be targeting in this time. (seconds)
	bool m_fReagentsRequired;
	bool m_fWordsOfPowerPlayer; // Words of Power for players
	bool m_fWordsOfPowerStaff;	// Words of Power for staff
	bool m_fEquippedCast;		// Allow casting while equipped.
	bool m_fReagentLossFail;	// ??? Lose reags when failed.
	int m_iMagicUnlockDoor;  // 1 in N chance of magic unlock working on doors -- 0 means never
	// ITEMID_TYPE m_iSpell_Teleport_Effect_Player;
	// SOUND_TYPE m_iSpell_Teleport_Sound_Player;
	ITEMID_TYPE m_iSpell_Teleport_Effect_Staff;
	SOUND_TYPE m_iSpell_Teleport_Sound_Staff;

	// In Game Effects
	bool m_fAllowKeyLockdown;	// lock keys in houses ?
	int	 m_iLightDungeon;
	int  m_iLightDay;		// Outdoor light level.
	int  m_iLightNight;		// Outdoor light level.
	int  m_iGameMinuteLength;	// Length of the game world minute in real world (TICKS_PER_SEC) seconds.
	int	 m_iGameTimeOffset;		// Base Game time in game minutes. 12*60 = noon.
	bool m_fNoWeather;			// Turn off all weather.
	bool m_fCharTags;			// Put [NPC] tags over chars.
	bool m_fFlipDroppedItems;	// Flip dropped items.
	bool m_fMonsterFight;	// Will creatures fight amoung themselves.
	bool m_fMonsterFear;	// will they run away if hurt ?
	int	 m_iBankIMax;	// Maximum number of items allowed in bank.
	int  m_iBankWMax;	// Maximum weight in WEIGHT_UNITS stones allowed in bank.
	int  m_iVendorMaxSell;		// Max things a vendor will sell in one shot.
	int  m_iMaxCharComplexity;		// How many chars per sector.
	int  m_iMaxItemComplexity;		// How many items per meter.
	bool m_fPlayerGhostSounds;	// Do player ghosts make a ghostly sound?
	bool m_fAutoNewbieKeys;		// Are house and boat keys newbied automatically?
	int  m_iStamRunningPenalty;		// Weight penalty for running (+N% of max carry weight)
	int  m_iStaminaLossAtWeight;	// %Weight at which characters begin to lose stamina
	int  m_iHitpointPercentOnRez;// How many hitpoints do they get when they are rez'd?
	int  m_iMaxBaseSkill;		// Maximum value for base skills at char creation
	int  m_iTrainSkillPercent;	// How much can NPC's train up to ?
	int	 m_iTrainSkillMax;
	int	 m_iMountHeight;		// The height at which a mounted person clears ceilings.
	int  m_iArcheryMinDist;		// archery does not work if too close ?
	bool m_fCharTitles;			// display char titles in the profile window

	// Criminal/Karma
	bool m_fGuardsInstantKill;	// Will guards kill instantly or follow normal combat rules?
	int	 m_iGuardLingerTime;	// How long do guards linger about.
	int  m_iSnoopCriminal;		// 1 in # chance of getting criminalflagged when succesfully snooping.
	int  m_iMurderMinCount;		// amount of murders before we get title.
	int	 m_iMurderDecayTime;	// (minutes) Roll murder counts off this often.
	bool m_fHelpingCriminalsIsACrime;// If I help (rez, heal, etc) a criminal, do I become one too?
	bool m_fLootingIsACrime;	// Looting a blue corpse is bad.
	int  m_iCriminalTimer;		// How many minutes are criminals flagged for?
	int	 m_iPlayerKarmaNeutral;	// How much bad karma makes a player neutral?
	int	 m_iPlayerKarmaEvil;

	// End INI file options.

	CResourceScript m_scpIni;	// Keep this around so we can link to it.

	CServArray m_Servers; // CServerDef s list. we act like the login server with this.
	CResNameSortArray<CResourceNamed> m_HelpDefs;	// Help on these commands.
	CLogIPArray m_LogIP;	// Keep track of these IP numbers

	// static definition stuff from *TABLE.SCP mostly.
	CGRefArray<CStartLoc> m_StartDefs; // Start points list
	CValueCurveDef m_StatAdv[STAT_BASE_QTY]; // "skill curve"
	CGTypedArray<CPointMapBase,CPointMapBase&> m_MoonGates;	// The array of moongates.

	CHashArray<CWebPageDef> m_WebPages;	// These can be linked back to the script.
	CRaceClassPtr m_DefaultRaceClass; // ( RES_RaceClass ); default.

	BYTE m_void;	// place holder.

private:
	CStringSortArray m_Obscene;			// Bad Names/Words etc.
	CGStringArray m_NotoTitles;	// Noto titles.
	CGStringArray m_Runes;		// Words of power. (A-Z)

	CHashArray<CMulMultiType> m_MultiDefs;	// read from the MUL files. Cached here on demand.

	CResNameSortArray<CSkillDef> m_SkillNameDefs;	// Name sorted
	CGRefArray<CSkillDef> m_SkillIndexDefs;	// Defined Skills indexed by number
	CGRefArray<CSpellDef> m_SpellDefs;	// Defined Spells

	CStringSortArray m_PrivCommands[PLEVEL_QTY];	// what command are allowed for a priv level?
};

extern CSphereResourceMgr g_Cfg;

inline LPCTSTR CSphereResourceMgr::GetResourceBlockName( RES_TYPE restype )	// static
{
	// Get the name of the class type.
	if ( restype < 0 || restype >= RES_QTY )
		restype = RES_UNKNOWN;
	return( sm_szResourceBlocks[restype] );
}

#endif	// _INC_CRESOURCE_H
