//
// CObjBase.h
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#ifndef _INC_COBJBASE_H
#define _INC_COBJBASE_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "cobjbasetemplate.h"
#include "cobjbasedef.h"
#include "spheresvr.h"

enum MEMORY_TYPE
{
	// IT_EQ_MEMORY_OBJ
	// Types of memory a CChar has about a game object. (m_wHue)
	MEMORY_NONE			= 0,
	MEMORY_SAWCRIME		= 0x0001,	// I saw them commit a crime or i was attacked criminally. I can call the guards on them. the crime may not have been against me.
	MEMORY_IPET			= 0x0002,	// I am a pet. (this link is my master or spawner) (never time out)
	MEMORY_FIGHT		= 0x0004,	// Active fight going on now. may not have done any damage. and they may not know they are fighting me.
	MEMORY_IAGGRESSOR	= 0x0008,	// I was the agressor here. (good or evil)
	MEMORY_HARMEDBY		= 0x0010,	// I was harmed by them. (but they may have been retaliating)
	MEMORY_IRRITATEDBY	= 0x0020,	// I saw them snoop from me or someone.
	MEMORY_SPEAK		= 0x0040,	// We spoke about something at some point. (or was tamed) (NPC_MEM_ACT_TYPE)
	MEMORY_AGGREIVED	= 0x0080,	// I was attacked and was the inocent party here !
	MEMORY_GUARD		= 0x0100,	// Guard this item (never time out)
	MEMORY_ISPAWNED		= 0x0200,	// I am spawned from this item. (never time out) ----- get rid of this -----
	MEMORY_GUILD		= 0x0400,	// This is my guild stone. (never time out) only have 1
	MEMORY_TOWN			= 0x0800,	// This is my town stone. (never time out) only have 1
	MEMORY_FOLLOW		= 0x1000,	// I am following this Object (never time out) (make this just a script)
	MEMORY_WAR_TARG		= 0x2000,	// This is one of my current war targets.
	MEMORY_FRIEND		= 0x4000,	// They can command me but not release me. (not primary blame)
// WESTY MOD (MULTI CONFIRM)
	MEMORY_MULTIPREVIEW = 0x8000,	// They placed a multi, this will time out to redeed
// END WESTY MOD
	OLDMEMORY_MURDERDECAY	= 0x4000,	// Next time a murder count gets dropped
};

enum NPC_MEM_ACT_TYPE	// A simgle primary memory about the object.
{
	// related to MEMORY_SPEAK
	NPC_MEM_ACT_NONE = 0,		// we spoke about something non-specific,
	NPC_MEM_ACT_SPEAK_TRAIN,	// I am speaking about training. Waiting for money
	NPC_MEM_ACT_SPEAK_HIRE,		// I am speaking about being hired. Waiting for money
	NPC_MEM_ACT_FIRSTSPEAK,		// I attempted (or could have) to speak to player. but have had no response.
	NPC_MEM_ACT_TAMED,			// I was tamed by this person previously.
	NPC_MEM_ACT_IGNORE,			// I looted or looked at and discarded this item (ignore it)
};

class CObjBase : public CObjBaseTemplate
{
	// All Instances of CItem or CChar have these base attributes.

public:
	CObjBase( UID_INDEX dwUIDMask );
	virtual ~CObjBase();

	virtual bool OnTick() = 0;
	virtual int FixWeirdness() = 0;
	virtual int GetWeight( void ) const = 0;
	virtual long GetMakeValue() = 0;
	virtual bool IsResourceMatch( CSphereUID rid, DWORD dwArg );
	virtual TRIGRET_TYPE OnTrigger( LPCTSTR pszTrigName, CScriptExecContext& exec ) = 0;

	virtual int IsWeird() const;
	virtual void DeleteThis();
	void SetChangerSrc( CScriptConsole* pSrc );
	int NameStrip( TCHAR* pszNameOut, LPCTSTR pszNameInp, int iMaxLen );

	// Accessors

	CRefPtr<CObjBaseDef> Base_GetDef() const
	{
		return( m_BaseRef );
	}
	virtual CGString GetName() const	// resolve ambiguity w/CScriptObj
	{
		return( CObjBaseTemplate::GetName());
	}
	virtual CGString GetResourceName() const
	{
		return Base_GetDef()->GetResourceName();
	}

	// Hue
	void SetHue( HUE_TYPE wHue )
	{
		m_wHue = wHue;
	}
	HUE_TYPE GetHue() const
	{
		return( m_wHue );
	}

protected:
	WORD GetHueAlt() const
	{
		// DEBUG_CHECK( IsItemEquipped());
		// IT_EQ_MEMORY_OBJ = MEMORY_TYPE mask
		// IT_EQ_SCRIPT_BOOK = previous book script time.
		// IT_EQ_VENDOR_BOX = restock time.
		return( m_wHue );
	}
	void SetHueAlt( HUE_TYPE wHue )
	{
		m_wHue = wHue;
	}

public:
	// Timer
	virtual void SetTimeout( int iDelayInTicks );
	bool IsTimerSet() const
	{
		return( m_timeout.IsTimeValid());
	}
	int GetTimerDiff() const	
	{
		// How long till this will expire ?
		// return: < 0 = in the past ( m_timeout - CServTime::GetCurrentTime() )
		return( m_timeout.GetTimeDiff());
	}
	bool IsTimerExpired() const
	{
		return( GetTimerDiff() <= 0 );
	}
	int GetTimerAdjusted() const
	{
		// RETURN: time in seconds from now.
		if ( ! IsTimerSet())
			return( -1 );
		int iDiffInTicks = GetTimerDiff();
		if ( iDiffInTicks < 0 )
			return( 0 );
		return( iDiffInTicks / TICKS_PER_SEC );
	}
	int GetTimerAdjustedD() const
	{
		// RETURN: time in TICKS_PER_SEC from now.
		if ( ! IsTimerSet())
			return( -1 );
		int iDiffInTicks = GetTimerDiff();
		if ( iDiffInTicks < 0 )
			return( 0 );
		return( iDiffInTicks );
	}

public:
	// Location
	virtual bool MoveTo( CPointMap pt ) = 0;	// Move to a location at top level.

	virtual void MoveNear( CPointMap pt, 
		int iSteps = 0, 
		CAN_TYPE wCan = ( CAN_C_WALK | CAN_C_CLIMB | CAN_C_INDOORS ));
	virtual bool MoveNearObj( const CObjBaseTemplate* pObj, 
		int iSteps = 0, 
		CAN_TYPE wCan = ( CAN_C_WALK | CAN_C_CLIMB | CAN_C_INDOORS ));

	bool SetNamePool( LPCTSTR pszName );

	void Sound( SOUND_TYPE id, int iRepeat = 1 ) const; // Play sound effect from this location.
	void Effect( EFFECT_TYPE motion, ITEMID_TYPE id, const CObjBase* pSource = NULL, BYTE bspeedseconds = 5, BYTE bloop = 1, bool fexplode = false ) const;

	void s_WriteSafe( CScript& s );

	virtual void s_Serialize( CGFile& a );	// binary
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script
	virtual void s_WriteProps( CScript& s );

	void Emote( LPCTSTR pText, CClient* pClientExclude = NULL, bool fPossessive = false );

	virtual void Speak( LPCTSTR pText, HUE_TYPE wHue = HUE_TEXT_DEF, TALKMODE_TYPE mode = TALKMODE_SAY, FONT_TYPE font = FONT_NORMAL );
	virtual void SpeakUTF8( LPCTSTR pText, HUE_TYPE wHue= HUE_TEXT_DEF, TALKMODE_TYPE mode= TALKMODE_SAY, FONT_TYPE font= FONT_NORMAL, CLanguageID lang = 0 );

	void RemoveFromView( CClient* pClientExclude = NULL );	// remove this item from all clients.
	void UpdateCanSee( const CUOCommand* pCmd, int iLen, CClient* pClientExclude = NULL ) const;
	void UpdateObjMessage( LPCTSTR pTextThem, LPCTSTR pTextYou, CClient* pClientExclude, HUE_TYPE wHue, TALKMODE_TYPE mode ) const;

	TRIGRET_TYPE OnHearTrigger( CResourceLink* pLink, CSphereExpArgs& exec );

	bool IsContainer( void ) const;
	void UpdateX()
	{
		RemoveFromView();
		Update();
	}

	virtual void Update( const CClient* pClientExclude = NULL )	// send this new item to clients.
	= 0;
	virtual void Flip( LPCTSTR pCmd = NULL )
	= 0;
	virtual int OnGetHit( int iDmg, CChar* pSrc, DAMAGE_TYPE uType = DAMAGE_HIT_BLUNT )
	= 0;
	virtual int OnTakeDamage( int iDmg, CChar* pSrc, DAMAGE_TYPE uType = DAMAGE_HIT_BLUNT )
	= 0;
	virtual bool OnSpellEffect( SPELL_TYPE spell, CChar* pCharSrc, int iSkillLevel, CItem* pSourceItem )
	= 0;

protected:
	virtual void DupeCopy( const CObjBase* pObj );

public:
	DECLARE_LISTREC_REF(CObjBase)
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define COBJBASEPROP(a,b,c) P_##a,
#include "cobjbaseprops.tbl"
#undef COBJBASEPROP
		P_QTY,
	};
	enum M_TYPE_
	{
#define COBJBASEMETHOD(a,b,c,d) M_##a,
#include "cobjbasemethods.tbl"
#undef COBJBASEMETHOD
		M_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];
	static const CScriptMethod sm_Methods[M_QTY+1];
#ifdef USE_JSCRIPT
#define COBJBASEMETHOD(a,b,c,d) JSCRIPT_METHOD_DEF(a)
#include "cobjbasemethods.tbl"
#undef COBJBASEMETHOD
#endif

	CServTime  m_timeCreate;		// When was i created ?
	CSphereUID m_uidChanger;		// UID of account that created this. or Last GM (Account) to have modified this.

	CResourceRefArray m_Events;		// When events are attached to this?
	CVarDefArray m_TagDefs;			// attach extra tags here. (this that dont apply to all)

	static int  sm_iCount;		// how many total objects in the world ?
	static bool sm_fDeleteReal;	// Delete for real. not just place in "to be deleted" list

protected:
	CRefPtr<CObjBaseDef> m_BaseRef;	// Pointer to the resource that describes this type.

private:
	CServTime m_timeout;		// when does this rot away ? or other action. 0 = never, else system time
	HUE_TYPE m_wHue;		// Hue or skin color. (WORD w/High 2 bits reserved)
};

enum STONEALIGN_TYPE // Types of Guild/Town stones
{
	STONEALIGN_STANDARD = 0,
	STONEALIGN_ORDER,
	STONEALIGN_CHAOS,
};

enum ITC_TYPE	// Item Template commands
{
	ITC_BUY,
	ITC_CONTAINER,
	ITC_ITEM,
	ITC_ITEMNEWBIE,
	ITC_SELL,
	ITC_QTY,
};

enum ATTR_TYPE
{
	// Attribute flags.
	ATTR_IDENTIFIED		=0x0001,	// This is the identified name. ???
	ATTR_DECAY			=0x0002,	// Timer currently set to decay.
	ATTR_NEWBIE			=0x0004,	// Not lost on death or sellable ?
	ATTR_MOVE_ALWAYS	=0x0008,	// Always movable (else Default as stored in client) (even if MUL says not movalble) NEVER DECAYS !
	ATTR_MOVE_NEVER		=0x0010,	// Never movable (else Default as stored in client) NEVER DECAYS !
	ATTR_MAGIC			=0x0020,	// DON'T SET THIS WHILE WORN! This item is magic as apposed to marked or markable.
	ATTR_OWNED			=0x0040,	// This is owned by the town. You need to steal it. NEVER DECAYS !
	ATTR_INVIS			=0x0080,	// Sphere hidden item (to GM's or owners?)
	ATTR_CURSED			=0x0100,
	ATTR_CURSED2		=0x0200,	// cursed damned unholy
	ATTR_BLESSED		=0x0400,
	ATTR_BLESSED2		=0x0800,	// blessed sacred holy
	ATTR_FORSALE		=0x1000,	// For sale on a vendor.
	ATTR_STOLEN			=0x2000,	// The item is hot. m_uidLink = previous owner.
	ATTR_CAN_DECAY		=0x4000,	// This item can decay. but it would seem that it would not (ATTR_MOVE_NEVER etc)
	ATTR_STATIC			=0x8000,	// WorldForge merge marker. (not used) Don't save this object!
};

class CItem : public CObjBase
{
	// RES_WorldItem
public:
	DECLARE_LISTREC_TYPE(CItem)
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CITEMPROP(a,b,c) P_##a,
#include "citemprops.tbl"
#undef CITEMPROP
		P_QTY,
	};
	enum M_TYPE_
	{
#define CITEMMETHOD(a,b,c) M_##a,
#include "citemmethods.tbl"
#undef CITEMMETHOD
		M_QTY,
	};

	static const CScriptProp sm_Props[P_QTY+1];
	static const CScriptMethod sm_Methods[M_QTY+1];
	static LPCTSTR const sm_szTemplateTable[ITC_QTY+1];
	static LPCTSTR const sm_szAttrNames[];	// desc ATTR_TYPE bits.

#ifdef USE_JSCRIPT
#define CITEMMETHOD(a,b,c) JSCRIPT_METHOD_DEF(a)
#include "citemmethods.tbl"
#undef CITEMMETHOD
#endif

private:
	WORD m_wDispIndex;		// The current display type. ITEMID_TYPE
	WORD m_amount;		// Amount of items in pile. 64K max (or corpse type)
	IT_TYPE m_type;		// What does this item do when dclicked ? 
	WORD m_AttrMask;		// ATTR_TYPE Attribute flags.

	// TAG_CRAFTEDBY=uid Maker of this item
public:
	// Type specific info. IT_TYPE
	// NOTE: If this link is set but not valid -> then delete the whole object !
	CSphereUID m_uidLink;		// Linked to this other object in the world. (owned, key, etc)

	union // 4(more1) + 4(more2) + 5(morep) = 13 bytes
	{
		// IT_NORMAL
		struct	// used only to save and restore all this junk.
		{
			DWORD m_more1;
			DWORD m_more2;
			CPointMapBase m_morep;
		} m_itNormal;

		// IT_CONTAINER
		// IT_CONTAINER_LOCKED
		// IT_DOOR
		// IT_DOOR_OPEN
		// IT_DOOR_LOCKED
		// IT_SHIP_SIDE
		// IT_SHIP_SIDE_LOCKED
		// IT_SHIP_PLANK
		// IT_SHIP_HOLD
		// IT_SHIP_HOLD_LOCK
		struct	// IsTypeLockable()
		{
			CSphereUIDBase m_lockUID;		// more1=the lock code. normally this is the same as the uid (magic lock=non UID)
			DWORD m_lock_complexity;	// more2=0-1000 = How hard to pick or magic unlock. (conflict with door ?)
			WORD  m_TrapType;			// morex = poison or explosion. (NOT USED YET)
		} m_itContainer;

		// IT_SHIP_TILLER
		// IT_KEY
		// IT_SIGN_GUMP
		struct
		{
			CSphereUIDBase m_lockUID;		// more1 = the lock code. Normally this is the UID, except if uidLink is set.
		} m_itKey;

		// IT_EQ_BANK_BOX
		struct
		{
			DWORD m_Check_Amount;		// more1=Current amount of gold in account..
			DWORD m_Check_Restock;		// more2= amount to restock the bank account to
			CPointMapBase m_pntOpen;	// morep=point we are standing on when opened bank box.
		} m_itEqBankBox;

		// IT_EQ_VENDOR_BOX
		struct
		{
			DWORD m_junk1;
			DWORD m_junk2;
			CPointMapBase m_pntOpen;	// morep=point we are standing on when opened vendor box.
		} m_itEqVendorBox;

		// IT_GAME_BOARD
		struct
		{
			int m_GameType;		// more1=0=chess, 1=checkers, 2= no pieces.
		} m_itGameBoard;

		// IT_WAND
		// IT_WEAPON_*
		// IT_SHOVEL
		struct
		{
			WORD m_Hits_Cur;		// more1l=eqiv to quality of the item (armor/weapon).
			WORD m_Hits_Max;		// more1h=can only be repaired up to this level.
			int  m_spellcharges;	// more2=for a wand etc.
			WORD m_spell;			// morex=SPELL_TYPE = The magic spell cast on this. (daemons breath)(boots of strength) etc
			WORD m_spelllevel;		// morey=level of the spell. (0-1000)
			BYTE m_poison_skill;	// morez=0-100 = Is the weapon poisoned ?
		} m_itWeapon;

		// IT_ARMOR
		// IT_ARMOR_LEATHER
		// IT_SHIELD:
		// IT_CLOTHING
		// IT_JEWELRY
		struct
		{
			WORD m_Hits_Cur;		// more1l= eqiv to quality of the item (armor/weapon).
			WORD m_Hits_Max;		// more1h= can only be repaired up to this level.
			int  m_spellcharges;	// more2 = ? spell charges ? not sure how used here..
			WORD m_spell;			// morex = SPELL_TYPE = The magic spell cast on this. (daemons breath)(boots of strength) etc
			WORD m_spelllevel;		// morey=level of the spell. (0-1000)
			BYTE m_junk;	// morez=0-100 = Is the weapon poisoned ?
		} m_itArmor;

		// IT_SPELL = a magic spell effect. (might be equipped)
		// IT_FIRE
		// IT_SCROLL
		// IT_COMM_CRYSTAL
		// IT_CAMPFIRE
		// IT_LAVA
		struct
		{
			short m_PolyStr;	// more1l=polymorph effect of this. (on strength)
			short m_PolyDex;	// more1h=polymorph effect of this. (on dex)
			int  m_spellcharges; // more2=not sure how used here..
			WORD m_spell;		// morex=SPELL_TYPE = The magic spell cast on this. (daemons breath)(boots of strength) etc
			WORD m_spelllevel;	// morey=0-1000=level of the spell.
			BYTE m_pattern;		// morez = light pattern - CAN_I_LIGHT LIGHT_QTY
		} m_itSpell;

		// IT_SPELLBOOK
		struct	// Spellbook extra spells.
		{
			BYTE m_spells[14];	// more1=Mask of avail spells for spell book.
		} m_itSpellbook;

		// IT_POTION
		struct
		{
			SPELL_TYPE m_Type;		// more1 = potion effect type
			DWORD m_skillquality;	// more2 = 0-1000 Strength of the resulting spell.
			WORD m_tick;			// morex = countdown to explode purple.
		} m_itPotion;

		// IT_MAP
		struct
		{
			WORD m_top;			// more1l=in world coords.
			WORD m_left;		// more1h=
			WORD m_bottom;		// more2l=
			WORD m_right;		// more2h=
			WORD m_junk3;
			WORD m_junk4;
			BYTE m_fPinsGlued;	// morez=pins are glued in place. Cannot be moved.
		} m_itMap;

		// IT_FRUIT
		// IT_FOOD
		// IT_FOOD_RAW
		// IT_MEAT_RAW
		struct
		{
			ITEMID_TYPE m_cook_id;		// more1=Cooks into this. (only if raw)
			CREID_TYPE m_MeatType;		// more2= Meat from what type of creature ?
			WORD m_spell;				// morex=SPELL_TYPE = The magic spell cast on this. ( effect of eating.)
			WORD m_spelllevel;			// morey=level of the spell. (0-1000)
			BYTE m_poison_skill;		// morez=0-100 = Is poisoned ?
		} m_itFood;

		// IT_CORPSE
		struct	// might just be a sleeping person as well
		{
			CServTimeBase	m_timeDeath;	// more1=Death time of corpse object. 0=sleeping or carved
			CSphereUIDBase m_uidKiller;	// more2=Who killed this corpse, carved or looted it last. sleep=self.
			CREID_TYPE	m_BaseID;		// morex,morey=The true type of the creature who's corpse this is.
			BYTE		m_facing_dir;	// morez=corpse dir. 0x80 = on face. DIR_TYPE
			// m_amount = the body type.
			// m_uidLink = the creatures ghost.
		} m_itCorpse;

		// IT_LIGHT_LIT
		// IT_LIGHT_OUT
		// IT_WINDOW
		struct
		{
			// CAN_I_LIGHT may be set for others as well..ie.Moon gate conflict
			DWORD   m_junk1;
			DWORD	m_junk2;
			WORD	m_junk3;
			WORD	m_charges;	// morey = how long will the torch last ?
			BYTE	m_pattern;	// morez = light rotation pattern (LIGHT_PATTERN)
		} m_itLight;

		// IT_EQ_TRADE_WINDOW
		struct
		{
			DWORD	m_junk1;
			DWORD	m_junk2;
			DWORD	m_junk3;
			BYTE	m_fCheck;	// morez=Check box for trade window.
			// m_uidLink = my trade partner
		} m_itEqTradeWindow;

		// IT_SPAWN_ITEM
		struct
		{
			CSphereUIDBase m_ItemID;	// more1=The ITEMID_* or template for items)
			DWORD	m_pile;			// more2=The max # of items to spawn per interval.  If this is 0, spawn up to the total amount
			WORD	m_TimeLoMin;	// morex=Lo time in minutes.
			WORD	m_TimeHiMin;	// morey=
			BYTE	m_DistMax;		// morez=How far from this will it spawn?
		} m_itSpawnItem;

		// IT_SPAWN_CHAR
		struct
		{
			CSphereUIDBase m_CharID;	// more1=CREID_*,  or (SPAWNTYPE_*,
			DWORD	m_current;		// more2=The current spawned from here. m_amount = the max count.
			WORD	m_TimeLoMin;	// morex=Lo time in minutes.
			WORD	m_TimeHiMin;	// morey=
			BYTE	m_DistMax;		// morez=How far from this will they wander ?
		} m_itSpawnChar;

		// IT_EXPLOSION
		struct
		{
			DWORD	m_junk1;
			DWORD	m_junk2;
			WORD	m_iDamage;		// morex = damage of the explosion
			WORD	m_wFlags;		// morey = DAMAGE_TYPE = fire,magic,etc
			BYTE	m_iDist;		// morez = distance range of damage.
		} m_itExplode;	// Make this asyncronous.

		// IT_BOOK
		// IT_MESSAGE
		// IT_EQ_MESSAGE
		// IT_EQ_DIALOG - RES_Dialog or RES_Menu
		struct
		{
			CSphereUIDBase m_ResID;			// more1 = preconfigured book id from RES_Book 
		} m_itBook;

		// IT_EQ_SCRIPT_BOOK
		struct
		{
			CSphereUIDBase m_ResID;	// more1= preconfigured book id from RES_Book or 0 = local
			DWORD   m_junk2;			// more2= WAS Time date stamp for the book/message creation.
			WORD	m_ExecPage;			// morex= what page of the script are we on ?
			WORD	m_ExecOffset;		// morey= offset on the page. 0 = not running (yet).
			BYTE	m_iPriorityLevel;	// morez = How much % they want to do this. (NPC)
		} m_itScriptBook;

		// IT_DEED
		struct
		{
			ITEMID_TYPE m_Type;		// more1 = deed for what multi, item or template ?
			DWORD		m_dwKeyCode;	// more2 = previous key code. (dry docked ship)
		} m_itDeed;

		// IT_CROPS
		// IT_FOLIAGE - the leaves of a tree normally.
		struct
		{
			int m_Respawn_Sec;		// more1 = plant respawn time in seconds. (for faster growth plants)
			ITEMID_TYPE m_ReapFruitID;	// more2 = What is the fruit of this plant.
			WORD m_ReapStages;		// morex = how many more stages of this to go til ripe.
		} m_itCrop;

		// IT_TREE
		// ? IT_ROCK
		// ? IT_WATER
		// ? IT_GRASS
		struct	// Natural resources. tend to be statics.
		{
			CSphereUIDBase m_rid_res;	// more1 = base resource type. RES_RegionResource
		} m_itResource;

		// IT_FIGURINE
		// IT_EQ_HORSE
		struct
		{
			CREID_TYPE m_ID;	// more1 = What sort of creature will this turn into.
			CSphereUIDBase m_uidChar;	// more2 = If stored by the stables. (offline creature)
		} m_itFigurine;

		// IT_RUNE
		struct
		{
			int m_Strength;			// more1 = How many uses til a rune will wear out ?
			DWORD m_junk2;
			CPointMapBase m_pntMark;	// morep = rune marked to a location or a teleport ?
		} m_itRune;

		// IT_TELEPAD
		// IT_MOONGATE
		struct
		{
			int m_fPlayerOnly;		// more1 = The gate is player only. (no npcs, xcept pets)
			int m_fQuiet;			// more2 = The gate/telepad makes no noise.
			CPointMapBase m_pntMark;	// morep = marked to a location or a teleport ?
		} m_itTelepad;

		// IT_EQ_MEMORY_OBJ
		struct
		{
			// m_wHue = memory type mask.
			WORD m_Arg1;		// more1l = MEMORY_SPEAK NPC_MEM_ACT_TYPE (m_Action) What sort of action is this memory about ? (1=training, 2=hire, etc)
			WORD m_Arg2;		// more1h = MEMORY_SPEAK SKILL_TYPE = training a skill ? 
			CServTimeBase m_timeStart;	// more2 = When did the fight start or action take place ?
			CPointMapBase m_ptStart;	// morep = Location the memory occured.
			// m_uidLink = what is this memory linked to. (must be valid)
		} m_itEqMemory;

		// IT_MULTI
		// IT_SHIP
		struct
		{
			CSphereUIDBase m_uidCreator;	// more1 = who created/owns this house or ship ?
			BYTE m_fSail;		// more2.b1 = ? speed ?
			BYTE m_fAnchored;
			BYTE m_DirMove;		// DIR_TYPE
			BYTE m_DirFace;
			// uidLink = my IT_SHIP_TILLER or IT_SIGN_GUMP,
		} m_itShip;

		// IT_PORTCULIS
		// IT_PORT_LOCKED
		struct
		{
			int m_z1;			// more1 = The down z height.
			int m_z2;			// more2 = The up z height.
		} m_itPortculis;

		// IT_ADVANCE_GATE
		struct
		{
			CREID_TYPE m_Type;		// more1 = What char template to use.
		} m_itAdvanceGate;

		// IT_BEE_HIVE
		struct
		{
			int m_honeycount;		// more1 = How much honey has accumulated here.
		} m_itBeeHive;

		// IT_LOOM
		struct
		{
			ITEMID_TYPE m_ClothID;	// more1 = the cloth type currenctly loaded here.
			int m_ClothQty;			// more2 = IS the loom loaded with cloth ?
		} m_itLoom;

		// IT_ARCHERY_BUTTE
		struct
		{
			ITEMID_TYPE m_AmmoType;	// more1 = arrow or bolt currently stuck in it.
			int m_AmmoCount;		// more2 = how many arrows or bolts ?
		} m_itArcheryButte;

		// IT_CANNON_MUZZLE
		struct
		{
			DWORD m_junk1;
			DWORD m_Load;			// more2 = Is the cannon loaded ? Mask = 1=powder, 2=shot
		} m_itCannon;

		// IT_EQ_MURDER_COUNT
		struct
		{
			DWORD m_Decay_Balance;	// more1 = For the murder flag, how much time is left ?
		} m_itEqMurderCount;

		// IT_ITEM_STONE
		struct
		{
			ITEMID_TYPE m_ItemID;	// more1= generate this item or template.
			int m_iPrice;			// more2= ??? gold to purchase / sellback. (vending machine)
			WORD m_wRegenTime;		// morex=regen time in seconds. 0 = no regen required.
			WORD m_wAmount;			// morey=Total amount to deliver. 0 = infinite, 0xFFFF=none left
		} m_itItemStone;

		// IT_EQ_STUCK
		struct
		{
			// LINK = what are we stuck to ? STATF_Immobile
		} m_itEqStuck;

		// IT_WEB
		struct
		{
			DWORD m_Hits_Cur;	// more1 = how much damage the web can take.
		} m_itWeb;

		// IT_DREAM_GATE
		struct
		{
			int m_index;	// more1 = how much damage the web can take.
		} m_itDreamGate;

		// IT_TRAP
		// IT_TRAP_ACTIVE
		// IT_TRAP_INACTIVE
		struct
		{
			ITEMID_TYPE m_AnimID;	// more1 = What does a trap do when triggered. 0=just use the next id.
			int	m_Damage;			// more2 = Base damage for a trap.
			WORD m_wAnimSec;		// morex = How long to animate as a dangerous trap.
			WORD m_wResetSec;		// morey = How long to sit idle til reset.
			BYTE m_fPeriodic;		// morez = Does the trap just cycle from active to inactive ?
		} m_itTrap;

		// IT_ANIM_ACTIVE
		struct
		{
			// NOTE: This is slightly dangerous to use as it will overwrite more1 and more2
			ITEMID_TYPE m_PrevID;	// more1 = What to turn back into after the animation.
			IT_TYPE m_PrevType;	// more2 = Any item that will animate.	??? Get rid of this !!
		} m_itAnim;

		// IT_SWITCH
		struct
		{
			ITEMID_TYPE m_SwitchID;	// more1 = the next state of this switch.
			DWORD		m_junk2;
			WORD		m_fStep;	// morex = can we just step on this to activate ?
			WORD		m_wDelay;	// morey = delay this how long before activation.
			// uidLink = the item to use when this item is thrown or used.
		} m_itSwitch;

		// IT_SOUND
		struct
		{
			DWORD	m_Sound;	// more1 = SOUND_TYPE, RES_SOUND
			int		m_Repeat;	// more2 =
		} m_itSound;

		// IT_STONE_GUILD
		// IT_STONE_TOWN
		// IT_STONE_ROOM
		struct
		{
			STONEALIGN_TYPE m_iAlign;	// more1=Neutral, chaos, order.
			int m_iAccountGold;			// more2=How much gold has been dropped on me?
			// ATTR_OWNED = auto promote to member.
		} m_itStone;

		// IT_LEATHER
		// IT_FEATHER
		// IT_FUR
		// IT_WOOL
		// IT_BLOOD
		// IT_BONE
		struct
		{
			int m_junk1;
			CREID_TYPE m_creid;	// more2=the source creature id. (CREID_TYPE)
		} m_itSkin;

	};	// IT_QTY

protected:
	bool SetBase( CItemDef* pItemDef );
	virtual int FixWeirdness();

public:
	virtual bool OnTick();
	virtual void OnHear( LPCTSTR pszCmd, CChar* pSrc )
	{
		// This should never be called directly. Normal items cannot hear. IT_SHIP and IT_COMM_CRYSTAL
	}

	CItemDefPtr Item_GetDef() const
	{
		return( REF_CAST( CItemDef, Base_GetDef()));
	}

	ITEMID_TYPE GetID() const
	{
		CItemDefPtr pItemDef = Item_GetDef();
		ASSERT(pItemDef);
		return( pItemDef->GetID());
	}

	bool SetBaseID( ITEMID_TYPE id );
	bool SetID( ITEMID_TYPE id );
	ITEMID_TYPE GetDispID() const
	{
		// This is what the item looks like.
		// May not be the same as the item that defines it's type.
		// DEBUG_CHECK( CItem::IsValidDispID( m_wDispIndex ));
		return((ITEMID_TYPE) m_wDispIndex );
	}
	bool IsSameDispID( ITEMID_TYPE id ) const	// account for flipped types ?
	{
		CItemDefPtr pItemDef = Item_GetDef();
		ASSERT(pItemDef);
		return( pItemDef->IsSameDispID( id ));
	}
	void SetDispID( ITEMID_TYPE id );
	void SetAnim( ITEMID_TYPE id, int iTime );

	virtual void DeleteThis();
	virtual int IsWeird() const;
	void SetDisconnected();

	void SetAttr( WORD wAttrMask )
	{
		m_AttrMask |= wAttrMask;
	}
	void ClrAttr( WORD wAttrMask )
	{
		m_AttrMask &= ~wAttrMask;
	}
	bool IsAttr( WORD wAttrMask ) const	// ATTR_DECAY
	{
		return(( m_AttrMask & wAttrMask ) ? true : false );
	}
	WORD GetAttr() const { return m_AttrMask; }

	int  GetDecayTime() const;
	void SetDecayTime( int iTime = 0 );
	SOUND_TYPE GetDropSound( const CObjBase* pObjOn ) const;
	bool IsTopLevelMultiLocked() const;
	bool IsMovableType() const;
	bool IsMovable() const;
	virtual int GetVisibleDistance() const
	{
		return( SPHEREMAP_VIEW_SIZE );
	}

	bool  IsStackableException() const;
	bool  IsStackable( const CItem* pItem ) const;
	bool  IsSameType( const CObjBase* pObj ) const;
	bool  Stack( CItem* pItem );
	int ConsumeAmount( int iQty = 1, bool fTest = false );

	CREID_TYPE GetCorpseType() const
	{
		return( (CREID_TYPE) GetAmount());	// What does the corpse look like ?
	}
	void  SetCorpseType( CREID_TYPE id )
	{
		DEBUG_CHECK( GetDispID() == ITEMID_CORPSE );
		m_amount = id;	// m_corpse_DispID
	}
	void  SetAmount( int amount );
	void  SetAmountUpdate( int amount );
	int	  GetAmount() const { return( m_amount ); }

	LPCTSTR GetNameFull( bool fIdentified ) const;
	virtual CGString GetName() const;	// allowed to be default name.
	virtual bool SetName( LPCTSTR pszName );

	virtual long GetMakeValue()
	{
		CItemDefPtr pItemDef = Item_GetDef();
		ASSERT(pItemDef);
		return pItemDef->GetMakeValue(0) * GetAmount();
	}
	virtual int GetWeight() const
	{
		CItemDefPtr pItemDef = Item_GetDef();
		ASSERT(pItemDef);
		int iWeight = pItemDef->GetWeight() * GetAmount();
		DEBUG_CHECK( iWeight >= 0 );
		return( iWeight );
	}

	void SetTimeout( int iDelay );

	virtual void OnMoveFrom()	// Moving from current location.
	{
	}
	virtual void OnMoveTo() // Put item on the ground.
	{
	}
	virtual bool MoveTo( CPointMap pt ); // Put item on the ground here.
	bool MoveToDecay( const CPointMap& pt, int iDecayTime )
	{
		SetDecayTime( iDecayTime );
		return MoveTo( pt );
	}
	bool MoveToCheck( const CPointMap& pt, CScriptConsole* pSrcMover = NULL );
	virtual bool MoveNearObj( const CObjBaseTemplate* pItem, int iSteps = 0, CAN_TYPE wCan = CAN_C_WALK | CAN_C_CLIMB | CAN_C_INDOORS);

	CObjBasePtr GetContainer() const
	{
		// What is this CItem contained in ?
		// Container should be a CChar or CItemContainer
		return( PTR_CAST(CObjBase,GetParent()));
	}
	CObjBasePtr GetTopLevelObj( void ) const
	{
		// recursively get the item that is at "top" level.
		CObjBasePtr pObj = GetContainer();
		if ( pObj == NULL )
			return( const_cast <CItem*>(this) );
		return( pObj->GetTopLevelObj());
	}

	virtual void  Update( const CClient* pClientExclude = NULL );		// send this new item to everyone.
	void  Flip( LPCTSTR pCmd = NULL );
	HRESULT LoadSetContainer( CSphereUID uid, LAYER_TYPE layer );

	void WriteUOX( CScript& s, int index );

	void v_GetMore1( CGVariant& vVal );
	void v_GetMore2( CGVariant& vVal );

	virtual void s_Serialize( CGFile& a );	// binary
	virtual void s_WriteProps( CScript& s );
	virtual bool s_LoadProps( CScript& s ); // Load an item from script
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script

private:
	virtual TRIGRET_TYPE OnTrigger( LPCTSTR pszTrigName, CScriptExecContext& exec );
public:
	TRIGRET_TYPE OnTrigger( CItemDef::T_TYPE_ trigger, CScriptExecContext& exec )
	{
		ASSERT( trigger < CItemDef::T_QTY );
		return( OnTrigger( MAKEINTRESOURCE(trigger), exec ));
	}

	int GetBlessCurseLevel() const;
	bool SetBlessCurse( int iLevel, CChar* pCharSrc );

	// Item type specific stuff.
	bool IsType( IT_TYPE type ) const
	{
		return( m_type == type );
	}
	IT_TYPE GetType() const
	{
		return( m_type );
	}
	CItemPtr SetType( IT_TYPE type );
	bool IsTypeLit() const
	{
		// has m_pattern arg
		switch(m_type)
		{
		case IT_SPELL: // a magic spell effect. (might be equipped)
		case IT_FIRE:
		case IT_LIGHT_LIT:
		case IT_CAMPFIRE:
		case IT_LAVA:
		case IT_WINDOW:
			return( true );
		}
		return( false );
	}
	bool IsTypeBook() const
	{
		switch( m_type )
		{
		// case IT_EQ_DIALOG:
		case IT_BOOK:
		case IT_MESSAGE:
		case IT_EQ_SCRIPT_BOOK:
		case IT_EQ_MESSAGE:
			return( true );
		}
		return( false );
	}
	bool IsTypeArmor() const
	{
		return( CItemDef::IsTypeArmor(m_type));
	}
	bool IsTypeWeapon() const
	{
		return( CItemDef::IsTypeWeapon(m_type));
	}
	bool IsTypeArmorWeapon() const
	{
		// Armor or weapon.
		if ( IsTypeArmor())
			return( true );
		return( IsTypeWeapon());
	}
	bool IsTypeLocked() const
	{
		switch ( m_type )
		{
		case IT_SHIP_SIDE_LOCKED:
		case IT_CONTAINER_LOCKED:
		case IT_SHIP_HOLD_LOCK:
		case IT_DOOR_LOCKED:
			return( true );
		}
		return(false);
	}
	bool IsTypeLockable() const
	{
		switch( m_type )
		{
		case IT_CONTAINER:
		case IT_DOOR:
		case IT_DOOR_OPEN:
		case IT_SHIP_SIDE:
		case IT_SHIP_PLANK:
		case IT_SHIP_HOLD:
			return( true );
		}
		return( IsTypeLocked() );
	}
	bool IsTypeSpellable() const
	{
		// m_itSpell
		switch( m_type )
		{
		case IT_SCROLL:
		case IT_SPELL:
		case IT_FIRE:
			return( true );
		}
		return( IsTypeArmorWeapon());
	}

	bool IsResourceMatch( CSphereUID rid, DWORD dwArg );

	bool IsValidLockLink( CItem* pItemLock ) const;
	bool IsValidLockUID() const;
	bool IsKeyLockFit( DWORD dwLockUID ) const
	{
		DEBUG_CHECK( IsType( IT_KEY ));
		return( m_itKey.m_lockUID == dwLockUID );
	}

	void ConvertBolttoCloth();
	SPELL_TYPE GetScrollSpell() const;
	bool IsSpellInBook( SPELL_TYPE spell ) const;
	int  AddSpellbookScroll( CItem* pItem );
	int AddSpellbookSpell( SPELL_TYPE spell, bool fUpdate );

	int	GetDoorOffset() const;
	bool IsDoorOpen() const;
	bool Use_Door( bool fJustOpen );
	bool Use_Portculis();
	SOUND_TYPE Use_Music( bool fWell ) const;

	bool SetMagicLock( CChar* pCharSrc, int iSkillLevel );
	void SetSwitchState();
	void SetTrapState( IT_TYPE state, ITEMID_TYPE id, int iTimeSec );
	int Use_Trap();
	bool Use_Light();
	int Use_LockPick( CChar* pCharSrc, bool fTest, bool fFail );
	LPCTSTR Use_SpyGlass( CChar* pUser ) const;
	LPCTSTR Use_Sextant( CPointMap pntCoords ) const;

	bool IsBookWritable() const
	{
		DEBUG_CHECK( IsTypeBook());
		return( (UID_INDEX) m_itBook.m_ResID == 0 );
	}
	bool IsBookSystem() const	// stored in RES_Book
	{
		DEBUG_CHECK( IsTypeBook());
		return( m_itBook.m_ResID.GetResType() == RES_Book );
	}

	bool OnExplosion();
	virtual bool OnSpellEffect( SPELL_TYPE spell, CChar* pCharSrc, int iSkillLevel, CItem* pSourceItem );
	virtual int OnGetHit( int iDmg, CChar* pSrc, DAMAGE_TYPE uType = DAMAGE_HIT_BLUNT );
	virtual int OnTakeDamage( int iDmg, CChar* pSrc, DAMAGE_TYPE uType = DAMAGE_HIT_BLUNT );

	int Armor_GetRepairPercent() const;
	LPCTSTR Armor_GetRepairDesc() const;
	bool Armor_IsRepairable() const;
	int Armor_GetDefense( bool fAverage ) const;
	int Armor_GetDefenseOrAttack( bool fAverage ) const;
	int Weapon_GetAttack( bool fAverage ) const;
	SKILL_TYPE Weapon_GetSkill() const;

	void Spawn_OnTick( bool fExec );
	void Spawn_KillChildren();
	CCharDefPtr Spawn_SetTrackID();
	void Spawn_GenerateItem( CResourceDef* pDef );
	void Spawn_GenerateChar( CResourceDef* pDef );
	CResourceDefPtr Spawn_FixDef();
	int Spawn_GetName( TCHAR* pszOut ) const;

	bool IsMemoryTypes( WORD wType ) const
	{
		// wType = MEMORY_FIGHT
		if ( ! IsType( IT_EQ_MEMORY_OBJ ))
			return( false );
		return(( GetHueAlt() & wType ) ? true : false );
	}

	bool Ship_Plank( bool fOpen );

	void Plant_SetTimer();
	bool Plant_OnTick();
	void Plant_CropReset();
	bool Plant_Use( CChar* pChar );

	virtual void DupeCopy( const CItem* pItem );
	CItemPtr UnStackSplit( int amount, CChar* pCharSrc = NULL );

	static CItemPtr CreateBase( ITEMID_TYPE id );

	static CItemPtr CreateDupeItem( const CItem* pItem );
	static CItemPtr CreateScript( ITEMID_TYPE id, CChar* pSrc );
	static CItemPtr CreateTemplate( ITEMID_TYPE id, CObjBase* pCont, CChar* pSrc );
	static CItemPtr CreateHeader( CGVariant& vArgs, CObjBase* pCont, CChar* pSrc, bool fDupeCheck );

protected:
	CItem( ITEMID_TYPE id, CItemDef* pItemDef );	// only created via CreateBase()
public:
	virtual ~CItem();
};

class CItemVendable : public CItem
{
	// Any item that can be sold and has value.
protected:
	DECLARE_MEM_DYNAMIC;

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CITEMVENDPROP(a,b,c) P_##a,
#include "citemvendprops.tbl"
#undef CITEMVENDPROP
		P_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];

private:
	WORD m_wQuality;	// {0 100} quality. - can mean diff things to diff items.
	long m_lPrice;		// The price of this item if on a vendor. (allow random (but remembered) pluctuations)

public:
	WORD	GetQuality() const { return m_wQuality; }
	void	SetQuality(WORD wQuality );

	void SetPlayerVendorPrice( int dwVal );
	long GetBasePrice();
	long GetVendorPrice( int iConvertFactor );

	bool  IsValidSaleItem( bool fBuyFromVendor ) const;
	bool  IsValidNPCSaleItem() const;

	void	Restock( bool fSellToPlayers );

	virtual long GetMakeValue();
	virtual void DupeCopy( const CItem* pItem );

	virtual void s_Serialize( CGFile& a );	// binary
	virtual void s_WriteProps( CScript& s );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );

	CItemVendable( ITEMID_TYPE id, CItemDef* pItemDef );
	~CItemVendable();
};

typedef CRefPtr<CItemVendable> CItemVendablePtr;

class CContainer : public CGObListType<CItem>	// This class contains a list of items but may or may not be an item itself.
{
	// This could be part of a CItemContainer or CChar
private:
	int	m_totalweight;	// weight of all the items it has. (1/WEIGHT_UNITS pound)
public:
	CSCRIPT_CLASS_DEF1();
	enum M_TYPE_
	{
#define CCONTAINERMETHOD(a,b,c) M_##a,
#include "ccontainermethods.tbl"
#undef CCONTAINERMETHOD
		M_QTY
	};
	static const CScriptMethod sm_Methods[M_QTY+1];
#ifdef USE_JSCRIPT
#define CCONTAINERMETHOD(a,b,c) JSCRIPT_METHOD_DEF(a)
#include "ccontainermethods.tbl"
#undef CCONTAINERMETHOD
#endif

protected:
	virtual void OnRemoveOb( CGObListRec* pObRec );	// Override this = called when removed from list.
	void ContentAddPrivate( CItemPtr pItem );

	void s_WriteContent( CScript& s ) const;
	void s_SerializeContent( CGFile& a ) const;	// binary
	HRESULT s_MethodContainer( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc );

	virtual long GetMakeValue();

public:

	bool ContentFindKeyFor( CItem* pLocked ) const;
	// bool IsItemInside( CItem* pItem ) const;

	CItemPtr ContentFindRandom( void ) const;
	void ContentsDump( const CPointMap& pt, WORD wAttr = 0 );
	void ContentsTransfer( CItemContainer* pCont, bool fNoNewbie );
	void ContentAttrMod( WORD wAttr, bool fSet );

	// For resource usage and gold.
	CItemPtr ContentFind( CSphereUID rid, DWORD dwArg = 0, int iDecendLevels = 255 ) const;
	int ContentCount( CSphereUID rid, DWORD dwArg = 0 );
	int ContentCountAll() const;
	virtual int ContentConsume( CSphereUID rid, int iQty = 1, bool fTest = false, DWORD dwArg = 0 );

	int ResourceConsume( const CResourceQtyArray* pResources, int iReplicationQty, bool fTest = false, DWORD dwArg = 0 );
	int ResourceConsumePart( const CResourceQtyArray* pResources, int iReplicationQty, int iFailPercent, bool fTest = false );

	int	GetTotalWeight( void ) const
	{
		DEBUG_CHECK( m_totalweight >= 0 );
		return( m_totalweight );
	}
	int FixWeight();

	virtual void OnWeightChange( int iChange );
	virtual void ContentAdd( CItemPtr pItem ) = 0;

	CContainer( void )
	{
		m_totalweight = 0;
	}
	~CContainer()
	{
		DeleteAll(); // call this early so the virtuals will work.
	}
};

class CItemContainer : public CItemVendable, public CContainer
{
	// This item has other items inside it.
protected:
	DECLARE_MEM_DYNAMIC;
public:
	CSCRIPT_CLASS_DEF2(ItemContainer);
	enum M_TYPE_
	{
#define CITEMCONTAINERMETHOD(a,b,c)	M_##a,
#include "citemcontainermethods.tbl"
#undef CITEMCONTAINERMETHOD
		M_QTY,
	};
	static const CScriptMethod sm_Methods[M_QTY+1];
#ifdef USE_JSCRIPT
#define CITEMCONTAINERMETHOD(a,b,c) JSCRIPT_METHOD_DEF(a)
#include "citemcontainermethods.tbl"
#undef CITEMCONTAINERMETHOD
#endif

public:
	// bool m_fTinkerTrapped;	// magic trap is diff.
public:
	bool IsWeighed() const
	{
		if ( IsType(IT_EQ_BANK_BOX ))
			return false;
		if ( IsType(IT_EQ_VENDOR_BOX))
			return false;
		if ( IsAttr(ATTR_MAGIC))	// magic containers have no weight.
			return false;
		return( true );
	}
	bool IsSearchable() const
	{
		if ( IsType(IT_EQ_BANK_BOX ))
			return false;
		if ( IsType(IT_EQ_VENDOR_BOX))
			return false;
		if ( IsType(IT_CONTAINER_LOCKED))
			return( false );
		return( true );
	}

	bool IsItemInside(const CItem* pItem) const;
	bool CanContainerHold(const CItem* pItem, CChar* pCharMsg );

	virtual void s_Serialize( CGFile& a );	// binary
	virtual void s_WriteProps( CScript& s );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script

	virtual long GetMakeValue()
	{
		return( CItem::GetMakeValue() + CContainer::GetMakeValue());
	}
	virtual int GetWeight() const
	{	// true weight == container item + contents.
		if ( ! IsWeighed())
			return 0;
		return( CItem::GetWeight() + CContainer::GetTotalWeight());
	}
	void OnWeightChange( int iChange );

	void ContentAdd( CItemPtr pItem );
	void ContentAdd( CItemPtr pItem, POINT pt );
protected:
	virtual void OnRemoveOb( CGObListRec* pObRec );	// Override this = called when removed from list.
public:
	void Trade_Status( bool fCheck, bool fSetStabilizeTimer = false );
	void Trade_Delete();

	void MakeKey( CScriptConsole* pSrc );
	void SetKeyRing();
	void Game_Create();
	void Restock();
	bool OnTick();

	int GetRestockTimeSeconds() const
	{
		// Vendor or Bank boxes.
		return( GetHueAlt());
	}
	void SetRestockTimeSeconds( int iSeconds )
	{
		SetHueAlt(iSeconds);
	}

	virtual void DupeCopy( const CItem* pItem );

	POINT GetRandContainerLoc() const;

	void OnOpenEvent( CChar* pCharOpener, const CObjBaseTemplate* pObjTop );

	virtual void DeleteThis()
	{
		if ( IsType( IT_EQ_TRADE_WINDOW ))
		{
			Trade_Delete();
		}
		DeleteAll();	// get rid of my contents first to protect against weight calc errors.
		CItemVendable::DeleteThis();
	}

public:
	CItemContainer( ITEMID_TYPE id, CItemDef* pItemDef );
	virtual ~CItemContainer()
	{
	}
};

typedef CRefPtr<CItemContainer> CItemContainerPtr;

class CItemCorpse : public CItemContainer
{
	// IT_CORPSE
	// A corpse is a special type of item.
protected:
	DECLARE_MEM_DYNAMIC;
public:
	bool	 IsPlayerDecayed() const;	// anyone can loot.
	bool	 IsPlayerCorpse() const;
	CCharPtr IsCorpseSleeping() const;

	virtual long GetMakeValue()
	{
		// GetAmount is used abnormally here.
		return( 1 + CContainer::GetMakeValue());
	}
	virtual int GetWeight() const
	{
		// GetAmount is used abnormally here.
		// true weight == container item + contents.
		return( 1 + CContainer::GetTotalWeight());
	}
	CItemCorpse( ITEMID_TYPE id, CItemDef* pItemDef ) : CItemContainer( id, pItemDef )
	{
	}
	virtual ~CItemCorpse()
	{
	}
};

typedef CRefPtr<CItemCorpse> CItemCorpsePtr;

typedef short MOTIVE_LEVEL;	// -100 = extreme hatred for target, 0 = neutral, 100 = love target.

enum STONESTATUS_TYPE // Priv level for a char CItemMemory
{
	STONESTATUS_CANDIDATE = 0,
	STONESTATUS_MEMBER,
	STONESTATUS_MASTER,
	STONESTATUS_BANISHED,	// This char is banished from the town/guild.
	STONESTATUS_ACCEPTED,	// The candidate has been accepted. But they have not dclicked on the stone yet.
	STONESTATUS_RESIGNED,	// We have resigned but not timed out yet.
	STONESTATUS_ENEMY_OLD,	// OBSOLETE
	STONESTATUS_REMOVE,	
};

class CItemStone;
typedef CRefPtr<CItemStone> CItemStonePtr;

class CItemMemory : public CItemVendable
{
	// IT_EQ_MEMORY
	// m_itEqMemory.m_timeStart = last time this was modified.
	// m_itEqMemory.m_ptStart = location point of start of memory.
	// GetHueAlt = memory type flags. MEMORY_FIGHT
	// m_wQuality = love level for target. (MOTIVE_LEVEL)
	// 
	// For Guild/Town:
	// TAG_LOYALTO=uid of who i'm loyal to.
	// TAG_TITLE=What is my title in the guild?
	// m_wQuality = Temporary space to calculate votes for me.
	// m_lPrice = how much i still owe to the guild or have surplus (Normally negative).

protected:
	DECLARE_MEM_DYNAMIC;
public:
	WORD SetMemoryTypes( WORD wType )	// For memory type objects.
	{
		// wType = MEMORY_TYPE - MEMORY_FIGHT etc
		DEBUG_CHECK( IsType( IT_EQ_MEMORY_OBJ ));
		SetHueAlt( wType );
		return( wType );
	}
	WORD GetMemoryTypes() const
	{
		DEBUG_CHECK( IsType( IT_EQ_MEMORY_OBJ ));
		return( GetHueAlt());	// MEMORY_FIGHT
	}
	virtual int FixWeirdness();

	// If this is NPC memory
	MOTIVE_LEVEL GetMotivation() const
	{
		// -100 hatred, 100=extreme love
		return( (MOTIVE_LEVEL) GetQuality());
	}
	void ChangeMotivation( MOTIVE_LEVEL iChange );

	// If this is a guild/town memory
	CItemStonePtr Guild_GetLink();
	bool Guild_IsAbbrevOn() const
	{
		return( m_itEqMemory.m_Arg1 );
	}
	void Guild_SetAbbrev( bool fAbbrevShow )
	{
		m_itEqMemory.m_Arg1 = fAbbrevShow;
	}
	WORD Guild_GetVotes() const
	{
		return GetQuality();
	}
	void Guild_SetVotes( WORD wVotes )
	{
		SetQuality( wVotes );
	}

	int Guild_SetLoyalTo( CSphereUID uid )
	{
		// Some other place checks to see if this is a valid member.
		return m_TagDefs.SetKeyVar("LoyalTo", CGVariant( (UID_INDEX) uid ));
	}
	CSphereUID Guild_GetLoyalTo() const
	{
		return (UID_INDEX) m_TagDefs.FindKeyVar("LoyalTo");
	}
	int Guild_SetTitle( LPCTSTR pszTitle )
	{
		return m_TagDefs.SetKeyStr("Title",pszTitle);
	}
	CGString Guild_GetTitle() const
	{
		return m_TagDefs.FindKeyStr("Title");
	}

	virtual void DeleteThis();	// delete myself from the list and system.

	CItemMemory( ITEMID_TYPE id, CItemDef* pItemDef );
	virtual ~CItemMemory();
};

typedef CRefPtr<CItemMemory> CItemMemoryPtr;

class CItemMulti : public CItem
{
	// IT_MULTI IT_SHIP
	// A ship or house etc.
protected:
	DECLARE_MEM_DYNAMIC;

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CITEMMULTIPROP(a,b,c) P_##a,
#include "citemmultiprops.tbl"
#undef CITEMMULTIPROP
		P_QTY,	
	};
	enum M_TYPE_
	{
#define CITEMMULTIMETHOD(a,b,c) M_##a,
#include "citemmultimethods.tbl"
#undef CITEMMULTIMETHOD
		M_QTY,
	};	
	static const CScriptProp sm_Props[P_QTY+1];
	static const CScriptMethod sm_Methods[M_QTY+1];

#ifdef USE_JSCRIPT
#define CITEMMULTIMETHOD(a,b,c) JSCRIPT_METHOD_DEF(a)
#include "citemmultimethods.tbl"
#undef CITEMMULTIMETHOD
#endif

private:
	CRegionComplexPtr m_pRegion;		// we own this region.

	// BELOW HERE SUPPORT FOR BAN SUPRORT
	CUIDRefArray m_Bans;
	CUIDRefArray m_CoOwners;
	CUIDRefArray m_Friends;
//	CUIDRefArray m_LockDowns;			// All items, including containers
//	CUIDRefArray m_ContainerLockDowns;	// Just containers
//	CUIDRefArray m_OwnerChests;			// 1 per owner, not counted toward total item count

	// DWORD m_dwDeed;	// deed this was made from (only used for placement confirmation, no need to save this)

private:
	void Multi_Delete();
	void Multi_Region_UnRealize( bool fRetestChars );
	bool Multi_Region_Realize();
	void Multi_Region_Delete();

	CItemPtr Multi_FindItemType( IT_TYPE type ) const;
	CItemPtr Multi_FindItemComponent( int iComp ) const;

	bool Multi_IsPartOf( const CItem* pItem ) const;
	int Multi_IsComponentOf( const CItem* pItem, const CItemDefMulti* pMultiDef ) const;

	CItemDefMultiPtr Multi_GetDef() const
	{
		return( REF_CAST(CItemDefMulti, Base_GetDef()));
	}
	bool Multi_CreateComponent( ITEMID_TYPE id, int dx, int dy, int dz, DWORD dwKeyCode );
	int Multi_GetMaxDist() const;

	CItemPtr Multi_GetSign();	// or Tiller
	int Ship_GetFaceOffset() const
	{
		return( GetID()& 3 );
	}
	int  Ship_ListObjs( CObjBasePtr* ppObjList );
	bool Ship_CanMoveTo( const CPointMap& pt ) const;
	bool Ship_SetMoveDir( DIR_TYPE dir );
	HRESULT Ship_MoveDelta( CPointMapBase pdelta );
	HRESULT Ship_Face( DIR_TYPE dir );
	HRESULT Ship_Move( DIR_TYPE dir );
	bool Ship_OnMoveTick( void );

public:
	virtual int GetVisibleDistance() const
	{
		return( SPHEREMAP_VIEW_RADAR );	// much farther than normal item.
	}

	virtual void DeleteThis();
	virtual bool OnTick();
	virtual bool MoveTo( CPointMap pt ); // Put item on the ground here.
	virtual void OnMoveFrom();	// Moving from current location.
	void OnHearRegion( LPCTSTR pszCmd, CChar* pSrc );

	void Multi_Create( CChar* pChar, DWORD dwKeyCode );

	virtual void s_Serialize( CGFile& a );	// binary
	virtual void s_WriteProps( CScript& s );
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script
	virtual void DupeCopy( const CItem* pItem );

	void Ship_Stop( void );
	bool Multi_DeedConvert( CChar* pChar, CItemMemory* pMemory );

	CItemMulti( ITEMID_TYPE id, CItemDef* pItemDef );
	virtual ~CItemMulti();
};

typedef CRefPtr<CItemMulti> CItemMultiPtr;

class CItemMap : public CItemVendable
{
	// IT_MAP
	// NOTE: This may be used to guide a ship or even a walking path ?
public:
	DECLARE_MEM_DYNAMIC;
	CSCRIPT_CLASS_DEF1();
	enum M_TYPE_
	{
#define CITEMMAPMETHOD(a,b,c) M_##a,
#include "citemmapmethods.tbl"
#undef CITEMMAPMETHOD
		M_QTY,
	};
	static const CScriptProp sm_Props[2];
	static const CScriptMethod sm_Methods[M_QTY+1];

#ifdef USE_JSCRIPT
#define CITEMMAPMETHOD(a,b,c) JSCRIPT_METHOD_DEF(a)
#include "citemmapmethods.tbl"
#undef CITEMMAPMETHOD
#endif

public:
	enum
	{
		MAX_PINS = 128,
	};

	bool m_fPlotMode;	// should really be per-client based but oh well.
	CGTypedArray<CPointMap,CPointMap&> m_Pins;

public:
	virtual void s_Serialize( CGFile& a );	// binary
	virtual void s_WriteProps( CScript& s );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script
	virtual void DupeCopy( const CItem* pItem );

	CItemMap( ITEMID_TYPE id, CItemDef* pItemDef ) : CItemVendable( id, pItemDef )
	{
		m_fPlotMode = false;
	}
	virtual ~CItemMap()
	{
	}
};

typedef CRefPtr<CItemMap> CItemMapPtr;

class CItemCommCrystal : public CItemVendable
{
	// STATF_COMM_CRYSTAL and IT_COMM_CRYSTAL
	// What speech blocks does it like ?
protected:
	DECLARE_MEM_DYNAMIC;
	CResourceRefArray m_Speech;	// Speech fragment list (other stuff we know)
public:
	CSCRIPT_CLASS_DEF1();
	static const CScriptProp sm_Props[2];
public:
	virtual void OnMoveFrom();
	virtual bool MoveTo( CPointMap pt );

	virtual void OnHear( LPCTSTR pszCmd, CChar* pSrc );
	virtual void s_Serialize( CGFile& a );	// binary
	virtual void s_WriteProps( CScript& s );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual void DupeCopy( const CItem* pItem );

	CItemCommCrystal( ITEMID_TYPE id, CItemDef* pItemDef ) : CItemVendable( id, pItemDef )
	{
	}
	virtual ~CItemCommCrystal()
	{
	}
};

enum STONEDISP_TYPE	// Hard coded Menus
{
	STONEDISP_NONE = 0,
	STONEDISP_ROSTER,
	STONEDISP_CANDIDATES,
	STONEDISP_FEALTY,
	STONEDISP_ACCEPTCANDIDATE,
	STONEDISP_REFUSECANDIDATE,
	STONEDISP_DISMISSMEMBER,
	STONEDISP_VIEWCHARTER,
	STONEDISP_SETCHARTER,
	STONEDISP_VIEWENEMYS,
	STONEDISP_VIEWTHREATS,
	STONEDISP_DECLAREWAR,
	STONEDISP_DECLAREPEACE,
	STONEDISP_GRANTTITLE,
	STONEDISP_VIEWBANISHED,
	STONEDISP_BANISHMEMBER,
};

class CItemStone : public CItem
{
	// IT_STONE_GUILD
	// IT_STONE_TOWN
	// ATTR_OWNED = auto promote to member.

public:
	CItemStone( ITEMID_TYPE id, CItemDef* pItemDef );
	virtual ~CItemStone();

	virtual void s_Serialize( CGFile& a );	// binary
	virtual void s_WriteProps( CScript& s );
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script

	LPCTSTR GetTypeName() const;
	bool IsUniqueName( LPCTSTR pName );

	CCharPtr Member_GetMaster() const;
	CItemMemoryPtr Member_GetMasterMemory() const;
	CItemMemoryPtr Member_FindIndex( int i );
	bool Member_IsMaster( CChar* pChar ) const;
	bool Member_IsMember( CChar* pChar ) const;
	bool Member_AddRecruit( CChar* pChar, bool fMember );
	void Member_ElectMaster();

	// Simple accessors.
	STONEALIGN_TYPE Align_GetType() const { return m_itStone.m_iAlign; }
	void Align_SetType(STONEALIGN_TYPE iAlign) { m_itStone.m_iAlign = iAlign; }
	LPCTSTR Align_GetName() const;
	bool Align_IsSameType( const CItemStone* pStone) const
	{
		if ( pStone == NULL )
			return( false );
		return( Align_GetType() != STONEALIGN_STANDARD &&
			Align_GetType() == pStone->Align_GetType());
	}
	bool War_IsAtWarWith( const CItemStone* pStone ) const;

	LPCTSTR Abbrev_Get() const { return( m_sAbbrev ); }
	void Abbrev_Set( LPCTSTR pAbbrev ) { m_sAbbrev = pAbbrev; }

	virtual void DeleteThis();

	int FixWeirdness();

	// Manage menus/gumps
	bool OnPromptResp( CClient* pClient, CLIMODE_TYPE TargMode, LPCTSTR pszText, CGString& sMsg );
	bool OnDialogButton( CClient* pClient, STONEDISP_TYPE type, CSphereExpArgs& resp );
	void Use_Item( CClient* pClient );

private:
	CResNameSortArray<CItemStone>* GetStoneArray() const;

	void UpdateTownName();
	virtual bool SetName( LPCTSTR pszName );
	virtual bool MoveTo( CPointMap pt );

	MEMORY_TYPE GetMemoryType() const;

	LPCTSTR Charter_Get(int iLine) const { ASSERT(iLine<COUNTOF(m_sCharter)); return(  m_sCharter[iLine] ); }
	void Charter_Set( int iLine, LPCTSTR pCharter ) { ASSERT(iLine<COUNTOF(m_sCharter)); m_sCharter[iLine] = pCharter; }
	LPCTSTR GetWebPageURL() const { return( m_sWebPageURL ); }
	void SetWebPage( LPCTSTR pWebPage ) { m_sWebPageURL = pWebPage; }

	bool ValidateMemberArray( CUIDRefArray& Array );

	bool War_CanWarWith( CItemStone* pEnemyStone ) const;
	bool War_Undeclare( CItemStone* pStone );
	bool War_Declare( CItemStone* pStone );

	void War_TheyDeclarePeace( CItemStone* pEnemyStone, bool fForcePeace );
	bool War_WeDeclareWar(CItemStone* pEnemyStone);
	void War_WeDeclarePeace(CSphereUID uidEnemy, bool fForcePeace = false);
	void War_AnnounceWar( const CItemStone* pEnemyStone, bool fWeDeclare, bool fWar );

	void AddMember( CSphereUID uid, STONESTATUS_TYPE StatusLevel, CGString sTitle, CSphereUID uidLoyalTo, bool fArg1, bool fArg2 );

	// Client interaction.
	bool IsInMenu( STONEDISP_TYPE iStoneMenu, const CItemStone* pOtherStone ) const;
	bool IsInMenu( STONEDISP_TYPE iStoneMenu, const CItemMemory* pMember ) const;
	int  addStoneListSetup( STONEDISP_TYPE iStoneMenu, CGStringArray* pasText );
	void addStoneList( CClient* pClient, STONEDISP_TYPE iStoneMenu );
	void addStoneSetViewCharter( CClient* pClient, STONEDISP_TYPE iStoneMenu );
	void addStoneDialog( CClient* pClient, STONEDISP_TYPE menuid );

	void SetupMenu( CClient* pClient, bool fMasterFunc=false );

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CITEMSTONEPROP(a,b,c) P_##a,
#include "citemstoneprops.tbl"
#undef CITEMSTONEPROP
		P_QTY,
	};
	enum M_TYPE_
	{
#define CITEMSTONEMETHOD(a,b,c) M_##a,
#include "citemstonemethods.tbl"
#undef CITEMSTONEMETHOD
		M_QTY,
	};
	static const CScriptMethod sm_Methods[M_QTY+1];
	static const CScriptProp sm_Props[P_QTY+1];

#ifdef USE_JSCRIPT
#define CITEMSTONEMETHOD(a,b,c) JSCRIPT_METHOD_DEF(a)
#include "citemstonemethods.tbl"
#undef CITEMSTONEMETHOD
#endif

	CUIDRefArray m_AtWar;			// Guilds we are actually at war with.
	CUIDRefArray m_WeDeclared;		// Guilds we declared war on
	CUIDRefArray m_TheyDeclared;	// Guilds declared on us

	CUIDRefArray m_Members;			// Members/Resigns from this guild 
	CUIDRefArray m_Candidates;		// Candidates from this guild 
	CUIDRefArray m_Bans;			// Bans from this guild 

	CSphereUID m_uidMaster;			// The master member.

protected:
	DECLARE_MEM_DYNAMIC;

private:
	CGString m_sCharter[6];
	CGString m_sWebPageURL;
	CGString m_sAbbrev;
	int		 m_iDailyDues;	// Real world daily dues in gp.
};

class CItemMessage : public CItemVendable	// A message for a bboard or book text.
{
	// IT_BOOK, IT_MESSAGE, IT_EQ_MESSAGE, IT_EQ_SCRIPT_BOOK = can be written into.
	// IT_EQ_DIALOG = cannot really be written to.
	// the name is the title for the message. (ITEMID_BBOARD_MSG)
protected:
	DECLARE_MEM_DYNAMIC;
private:
	CGStringArray m_sBodyLines;	// The main body of the text for bboard message or book.
public:
	CGString m_sAuthor;					// Should just have author name !

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CITEMMESSAGEPROP(a,b,c) P_##a,
#include "citemmessageprops.tbl"
#undef CITEMMESSAGEPROP
		P_QTY,
	};
	enum M_TYPE_
	{
#define CITEMMESSAGEMETHOD(a,b,c) M_##a,
#include "citemmessagemethods.tbl"
#undef CITEMMESSAGEMETHOD
		M_QTY,
	};
	static const CScriptMethod sm_Methods[M_QTY+1];
	static const CScriptProp sm_Props[P_QTY+1];
#ifdef USE_JSCRIPT
#define CITEMMESSAGEMETHOD(a,b,c) JSCRIPT_METHOD_DEF(a)
#include "citemmessagemethods.tbl"
#undef CITEMMESSAGEMETHOD
#endif

public:
	WORD GetScriptTimeout() const
	{
		return( GetHueAlt());
	}
	void SetScriptTimeout( WORD wTimeInTenths )
	{
		SetHueAlt( wTimeInTenths );
	}

	virtual void s_Serialize( CGFile& a );	// binary
	virtual void s_WriteProps( CScript& s );
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script

	int GetPageCount() const
	{
		return( m_sBodyLines.GetSize());
	}
	LPCTSTR GetPageText( int iPage ) const
	{
		if ( iPage >= m_sBodyLines.GetSize())
			return( NULL );
		return( m_sBodyLines.ConstElementAt(iPage));
	}
	void SetPageText( int iPage, LPCTSTR pszText );
	void AddPageText( LPCTSTR pszText )
	{
		m_sBodyLines.Add( CGString( pszText ));
	}

	virtual void DupeCopy( const CItem* pItem );
	bool LoadSystemPages();
	void UnLoadSystemPages()
	{
		m_sAuthor.Empty();
		m_sBodyLines.RemoveAll();
	}

	CItemMessage( ITEMID_TYPE id, CItemDef* pItemDef );
	virtual ~CItemMessage();
};

typedef CRefPtr<CItemMessage> CItemMessagePtr;

class CItemServerGate : public CItem
{
	// IT_DREAM_GATE
protected:
	DECLARE_MEM_DYNAMIC;
	CGString m_sServerName;	// Link to the other server by name.
public:
	CSCRIPT_CLASS_DEF1();
	static const CScriptProp sm_Props[3];
public:
	CServerPtr GetServerRef() const;
	HRESULT SetServerLink( LPCTSTR pszName );
	LPCTSTR GetServerLink() const
	{
		return( m_sServerName );	// MEMORY_FIGHT
	}

	virtual int FixWeirdness();
	virtual void s_Serialize( CGFile& a );	// binary
	virtual void s_WriteProps( CScript& s );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );

	CItemServerGate( ITEMID_TYPE id, CItemDef* pItemDef ) : CItem( ITEMID_MEMORY, pItemDef )
	{
	}
	virtual ~CItemServerGate()
	{
	}
};

enum NPCBRAIN_TYPE	// General AI type.
{
	NPCBRAIN_NONE = 0,	// 0 = This should never really happen.
	NPCBRAIN_ANIMAL,	// 1 = can be tamed.
	NPCBRAIN_HUMAN,		// 2 = generic human.
	NPCBRAIN_HEALER,	// 3 = can res.
	NPCBRAIN_GUARD,		// 4 = inside cities
	NPCBRAIN_BANKER,	// 5 = can open your bank box for you
	NPCBRAIN_VENDOR,	// 6 = will sell from vendor boxes.
	NPCBRAIN_BEGGAR,	// 7 = begs.
	NPCBRAIN_STABLE,	// 8 = will store your animals for you.
	NPCBRAIN_THIEF,		// 9 = should try to steal ?
	NPCBRAIN_MONSTER,	// 10 = not tamable. normally evil.
	NPCBRAIN_BESERK,	// 11 = attack closest (blades, vortex)
	NPCBRAIN_UNDEAD,	// 12 = disapears in the light.
	NPCBRAIN_DRAGON,	// 13 = we can breath fire. may be tamable ? hirable ?
	NPCBRAIN_VENDOR_OFFDUTY,	// 14 = "Sorry i'm not working right now. come back when my shop is open.
	NPCBRAIN_CRIER,		// 15 = will speak periodically.
	NPCBRAIN_CONJURED,	// 16 = elemental or other conjured creature.
	NPCBRAIN_WEATHERMAN,	// 17 = spreads rain were they go. (invis and can walk thru walls)
	NPCBRAIN_QTY,
};

struct CCharNPC : public CMemDynamic
{
	// This is basically the unique "brains" for any NPC character.
protected:
	DECLARE_MEM_DYNAMIC;
public:
	// Stuff that is specific to an NPC character instance (not an NPC type see CCharDef for that).
	// Any NPC AI stuff will go here.
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CCHARNPCPROP(a,b,c) P_##a,
#include "ccharnpcprops.tbl"
#undef CCHARNPCPROP
		P_QTY,
	};
	enum M_TYPE
	{
#define CCHARNPCMETHOD(a,b,c) M_##a,
#include "ccharnpcmethods.tbl"
#undef CCHARNPCMETHOD
		M_QTY,
	};
	static const CScriptMethod sm_Methods[M_QTY+1];
	static const CScriptProp sm_Props[P_QTY+1];
#ifdef USE_JSCRIPT
#define CCHARNPCMETHOD(a,b,c) JSCRIPT_METHOD_DEF(a)
#include "ccharnpcmethods.tbl"
#undef CCHARNPCMETHOD
#endif

	NPCBRAIN_TYPE m_Brain;		// For NPCs: Number of the assigned basic AI block
	WORD	m_Home_Dist_Wander;	// Distance to allow to "wander".
	MOTIVE_LEVEL m_Act_Motivation;	// 0-100 (100=very greatly) how bad do i want to do the current action.

	// We respond to what we here with this.
	// NOTE: This does not mean that i speak in response.
	CResourceRefArray m_Speech;	// Speech fragment list (other stuff we know)
	CResourceQty m_Need;	// What items might i need/Desire ? (coded as resource scripts) ex "10 gold,20 logs" etc.

public:
	void s_SerializeNPC( CChar* pChar, CGFile& a );	// binary
	void s_WriteNPC( CChar* pChar, CScript& s );
	HRESULT s_PropGetNPC( CChar* pChar, int iProp, CGVariant& vVal );
	HRESULT s_PropSetNPC( CChar* pChar, int iProp, CGVariant& vVal );

	bool IsVendor() const
	{
		return( m_Brain == NPCBRAIN_HEALER || m_Brain == NPCBRAIN_STABLE || m_Brain == NPCBRAIN_VENDOR );
	}

	CCharNPC( CChar* pChar, NPCBRAIN_TYPE NPCBrain );
	~CCharNPC();
};

#define IS_SKILL_BASE(sk) ((sk)>= SKILL_First && (sk)< SKILL_QTY )

struct CCharPlayer : public CMemDynamic
{
	// Stuff that is specific to a player character.
protected:
	DECLARE_MEM_DYNAMIC;
private:
	BYTE m_SkillLock[SKILL_QTY];	// SKILLLOCK_TYPE List of skill lock states for this player character
	CProfessionPtr m_Profession;	// RES_Profession CProfessionDef What skill class group have we selected.
public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_	// Player char.
	{
#define CCHARPLAYERPROP(a,b,c) P_##a,
#include "ccharplayerprops.tbl"
#undef CCHARPLAYERPROP
		P_QTY,
	};
	enum M_TYPE_
	{
#define CCHARPLAYERMETHOD(a,b,c) M_##a,
#include "ccharplayermethods.tbl"
#undef CCHARPLAYERMETHOD
		M_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];
	static const CScriptMethod sm_Methods[M_QTY+1];
#ifdef USE_JSCRIPT
#define CCHARPLAYERMETHOD(a,b,c) JSCRIPT_METHOD_DEF(a)
#include "ccharplayermethods.tbl"
#undef CCHARPLAYERMETHOD
#endif

	CServTime m_timeLastUsed;	// Time the player char was last used.
	WORD m_wMurders;		// Murder count.
	WORD m_wDeaths;			// How many times have i died ?

public:
	SKILLLOCK_TYPE Skill_GetLock( SKILL_TYPE skill ) const
	{
		ASSERT( skill < COUNTOF(m_SkillLock));
		return((SKILLLOCK_TYPE) m_SkillLock[skill] );
	}
	void Skill_SetLock( SKILL_TYPE skill, SKILLLOCK_TYPE state )
	{
		ASSERT( skill < COUNTOF(m_SkillLock));
		m_SkillLock[skill] = state;
	}

	void s_SerializePlayer( CChar* pChar, CGFile& a );	// binary
	void s_WritePlayer( CChar* pChar, CScript& s );
	HRESULT s_PropGetPlayer( CChar* pChar, int iProp, CGVariant& vVal );
	HRESULT s_PropSetPlayer( CChar* pChar, int iProp, CGVariant& vVal );

	HRESULT SetProfession( CChar* pChar, CSphereUID rid );
	CProfessionPtr GetProfession() const;

	CCharPlayer( CChar* pChar );
	~CCharPlayer();
};

enum WAR_SWING_TYPE	// m_Act_War_Swing_State
{
	WAR_SWING_INVALID = -1,
	WAR_SWING_EQUIPPING = 0,	// we are recoiling our weapon.
	WAR_SWING_READY,			// we can swing at any time.
	WAR_SWING_SWINGING,			// we are swinging our weapon.
};

class CPartyDef;

struct CCharActState
{
	// What action is the CChar taking now?

	SKILL_TYPE	m_SkillCurrent;		// Currently using a skill. Could be combat skill.
	CSphereUID	m_Targ;			// Current combat/action target
	CSphereUID	m_TargPrv;		// Targeted bottle for alchemy or previous beg target.
	SKILL_LEVEL	m_Difficulty;	// -1 = fail skill. (0-100) for skill advance calc.
	CPointMap	m_pt;			// Moving to this location. or location of forge we are working on.

	union	// arg specific to the action type.(m_Act.m_SkillCurrent)
	{
		struct
		{
			DWORD m_Arg1;	// "ACTARG1"
			DWORD m_Arg2;	// "ACTARG2"
			DWORD m_Arg3;	// "ACTARG3"
		} m_atUnk;

		// SKILL_MAGERY
		struct
		{
			SPELL_TYPE	m_Spell;		// ACTARG1=Currently casting spell.
			CREID_TYPE	m_SummonID;		// ACTARG2=A sub arg of the skill. (summoned type ?)
			bool m_fSummonPet;			// ACTARG3=
		} m_atMagery;

		// SKILL_MUSICIANSHIP
		// SKILL_ENTICEMENT
		// SKILL_PROVOCATION
		// SKILL_PEACEMAKING
		struct
		{
			int m_iMusicDifficulty;		// ACTARG1=Base music diff, (whole thing won't work if this fails)
		} m_atMusician;

		// SKILL_ALCHEMY
		// SKILL_BLACKSMITHING
		// SKILL_BOWCRAFT
		// SKILL_CARPENTRY
		// SKILL_CARTOGRAPHY:
		// SKILL_INSCRIPTION
		// SKILL_TAILORING:
		// SKILL_TINKERING,

		struct	// creation type skill.
		{
			ITEMID_TYPE m_ItemID;		// ACTARG1=Making this item.
			WORD m_Stroke_Count;		// ACTARG2=For smithing, tinkering, etc. all requiring multi strokes.
			WORD m_Amount;				// How many of this item are we making?
		} m_atCreate;

		// SKILL_LUMBERJACKING,
		// SKILL_MINING
		// SKILL_FISHING
		struct
		{
			DWORD m_junk1;
			WORD m_Stroke_Count;		// all requiring multi strokes.
		} m_atResource;

		// SKILL_TAMING
		// SKILL_MEDITATION
		struct
		{
			DWORD m_junk1;
			WORD m_Stroke_Count;		// all requiring multi strokes.
		} m_atTaming;

		// SKILL_SWORDSMANSHIP
		// SKILL_MACEFIGHTING,
		// SKILL_FENCING,
		// SKILL_WRESTLING
		struct
		{
			WAR_SWING_TYPE m_War_Swing_State;	// We are in the war mode swing.
			// m_Act.m_Targ = who are we currently attacking?
		} m_atFight;

		// SKILL_TRACKING
		struct
		{
			DIR_TYPE	m_PrvDir; // Previous direction of tracking target, used for when to notify player
		} m_atTracking;

		// SKILL_CARTOGRAPHY
		struct
		{
			int	m_Dist;
		} m_atCartography;

		// NPCACT_FOLLOW_TARG
		struct
		{
			int	m_DistMin;		// Try to force this distance.
			int	m_DistMax;		// Try to force this distance.
			// m_Act.m_Targ = what am i folloiwng ?
		} m_atFollowTarg;

		// NPCACT_RIDDEN
		struct
		{
			CSphereUIDBase m_FigurineUID;	// This creature is being ridden by this object link. IT_FIGURINE IT_EQ_HORSE
		} m_atRidden;

		// NPCACT_TALK
		// NPCACT_TALK_FOLLOW
		struct
		{
			int	 m_HearUnknown;	// Speaking NPC has no idea what u're saying.
			int  m_WaitCount;	// How long have i been waiting (xN sec)
			// m_Act.m_Targ = who am i talking to ?
		} m_atTalk;

		// NPCACT_FLEE
		// m_Act.m_Targ = who am i fleeing from ?
		struct
		{
			int	 m_iStepsMax;	// how long should it take to get there.
			int	 m_iStepsCurrent;	// how long has it taken ?
		} m_atFlee;

		// NPCACT_LOOTING
		// m_Act.m_Targ = what am i looting ? (Corpse)
		// m_Act.m_TargPrv = targets parent
		// m_Act.m_pt = location of the target.
		struct
		{
			int	 m_iDistEstimate;	// how long should it take to get there.
			int	 m_iDistCurrent;	// how long has it taken ?
		} m_atLooting;

		// NPCACT_TRAINING
		// m_Act.m_Targ = what am i training on
		// m_Act.m_TargPrv = weapon
		//
	};

public:
	void Init()
	{
		m_SkillCurrent = SKILL_NONE;
		m_atUnk.m_Arg1 = 0;
		m_atUnk.m_Arg2 = 0;
		m_atUnk.m_Arg3 = 0;
	}
};

class CChar : public CObjBase, public CContainer, public CAccountConsole
{
	// RES_WorldChar
public:
	DECLARE_LISTREC_TYPE(CChar)
private:
	// Spell type effects.
#define STATF_INVUL			0x00000001	// Invulnerability
#define STATF_DEAD			0x00000002
#define STATF_Freeze		0x00000004	// Paralyzed. (spell)
#define STATF_Invisible		0x00000008	// Invisible (spell).
#define STATF_Sleeping		0x00000010	// You look like a corpse ?
#define STATF_War			0x00000020	// War mode on ?
#define STATF_Reactive		0x00000040	// have reactive armor on.
#define STATF_Poisoned		0x00000080	// Poison level is in the poison object
#define STATF_NightSight	0x00000100	// All is light to you
#define STATF_Reflection	0x00000200	// Magic reflect on.
#define STATF_Polymorph		0x00000400	// We have polymorphed to another form.
#define STATF_Incognito		0x00000800	// Dont show skill titles
#define STATF_SpiritSpeak	0x00001000	// I can hear ghosts clearly.
#define STATF_Insubstantial	0x00002000	// Ghost has not manifest. or GM hidden
#define STATF_Squelch		0x00004000	// This player cannot speak normally.
#define STATF_COMM_CRYSTAL	0x00008000	// I have a IT_COMM_CRYSTAL or listening item on me.
#define STATF_HasShield		0x00010000	// Using a shield
#define STATF_Script_Play	0x00020000	// Playing a Script. (book script)
#define STATF_Stone			0x00040000	// turned to stone.
#define STATF_Immobile		0x00080000	// Non modile. (but i can hit!)
#define STATF_Fly			0x00100000	// Flying or running ? (anim)
#define STATF_RespawnNPC	0x00200000	// This NPC is a plot NPC that must respawn later.
#define STATF_Hallucinating	0x00400000	// eat 'shrooms or bad food.
#define STATF_Hidden		0x00800000	// Hidden (non-magical)
#define STATF_InDoors		0x01000000	// we are covered from the rain.
#define STATF_Criminal		0x02000000	// The guards will attack me. (someone has called guards)
#define STATF_Conjured		0x04000000	// This creature is conjured and will expire. (leave no corpse or loot)
#define STATF_Pet			0x08000000	// I am a pet/hirling. check for my owner memory.
#define STATF_Spawned		0x10000000	// I am spawned by a spawn item.
#define STATF_SaveParity	0x20000000	// Has this char been saved or not ?
#define STATF_Ridden		0x40000000	// This is the horse. (don't display me) I am being ridden
#define STATF_OnHorse		0x80000000	// Mounted on horseback.

	// Plus_Luck	// bless luck or curse luck ?

	DWORD m_StatFlag;		// Flags above

#define SKILL_VARIANCE 100		// Difficulty modifier for determining success. 10.0 %
	STAT_LEVEL m_Skill[SKILL_QTY];	// List of skills ( skill* 10 ) (might go temporariliy negative!)

	WORD m_StatRegen[STAT_REGEN_QTY];		// Tick time since last regen.
	CServTime m_timeLastStatRegen;	// When did i get my last regen tick ?

	// This is a character that can either be NPC or PC.
	// Player vs NPC Stuff
	CClientPtr m_pClient;	// is the char a logged in m_pPlayer ?

public:
	// 
	// ??TAG_TOWN_*
	// ??TAG_QUEST_X_*
	// TAG_TITLE = "the guard"
	// TAG_PROFILE = "Long, multiline, Profile text"
	// TAG_VENDORMARKUP = x = I am a vendor and this is my markup % for all.
	// TAG_PLOT1 and TAG_PLOT2 = Old Bit masks (not really supported anymore)
	// TAG_PARTY_CANLOOTME = Can I be looted by others in my party? or CCharPlayer?
	// TAG_JAIL_RELEASEPOINT=X,Y,Z = Where we return to after jailing.
	// TAG_SUMMON_RETURNPOINT=X,Y,Z = Where we return to after SummonTo.

	STAT_LEVEL m_Stat[STAT_QTY];		// karma is signed. (stats should be able to go temporariliy negative !)

#define m_StatStr		m_Stat[STAT_Str]
#define m_StatInt		m_Stat[STAT_Int]
#define m_StatDex		m_Stat[STAT_Dex]

#define m_StatMaxHealth	m_Stat[STAT_MaxHealth]
#define m_StatMaxMana	m_Stat[STAT_MaxMana]
#define m_StatMaxStam	m_Stat[STAT_MaxStam]

#define m_StatHealth	m_Stat[STAT_Health]
#define m_StatMana		m_Stat[STAT_Mana]
#define m_StatStam		m_Stat[STAT_Stam]

#define m_StatFood		m_Stat[STAT_Food]

	CNewPtr<CCharPlayer> m_pPlayer;	// May even be an off-line player !
	CNewPtr<CCharNPC> m_pNPC;		// we can be both a player and an NPC if "controlled" ?
	CRefPtr<CPartyDef> m_pParty;		// What party am i in ?
	CRegionComplexPtr m_pArea;		// What region are we in now. (for guarded message)

public:
	CSCRIPT_CLASS_DEF2(Char);
	enum P_TYPE_
	{
#define CCHARPROP(a,b,c) P_##a,
#include "ccharprops.tbl"
#undef CCHARPROP
		P_QTY,
	};
	enum M_TYPE_
	{
#define CCHARMETHOD(a,b,c,d) M_##a,
#include "ccharmethods.tbl"
#undef CCHARMETHOD
		M_QTY,
	};

	static const CScriptProp sm_Props[P_QTY+1];
	static const CScriptMethod sm_Methods[M_QTY+1];
	static const LAYER_TYPE sm_VendorLayers[4];

#ifdef USE_JSCRIPT
#define CCHARMETHOD(a,b,c,d) JSCRIPT_METHOD_DEF(a)
#include "ccharmethods.tbl"
#undef CCHARMETHOD
#endif

	// Combat stuff. cached data. (not saved)
	CSphereUID m_uidWeapon;		// current Wielded weapon.	(could just get rid of this ?)
	WORD m_ArmorDisplay;		// calculated armor worn + intrinsic armor

	// If we are climbing stairs we need this info for our next step.
	DIR_TYPE m_dirClimb;	// we are standing on a CAN_I_CLIMB or UFLAG2_CLIMBABLE, DIR_QTY = not on climbable
	PNT_Z_TYPE m_zClimbHeight;	// The height at the end of the climbable.

	// Saved stuff.
	HUE_TYPE m_SpeechHue;	// previous Client select speech hue. or npc selected.
	DIR_TYPE m_dirFace;		// facing this dir.
	CPointMap m_ptHome;		// What is our "home" region. (towns and bounding of NPC's)
	FONT_TYPE m_fonttype;	// Speech font to use // can client set this ?

	// In order to revert to original Hue and body.
	CREID_TYPE m_prev_id;		// Backup of body type for ghosts and poly
	HUE_TYPE m_prev_Hue;	// Backup of skin color. in case of polymorph etc.

	// Some character action in progress.
	CCharActState m_Act;

public:
	// Status and attributes ------------------------------------
	virtual int IsWeird() const;
	bool IsStatFlag( DWORD dwStatFlag ) const
	{
		return(( m_StatFlag& dwStatFlag) ? true : false );
	}
	void StatFlag_Set( DWORD dwStatFlag )
	{
		m_StatFlag |= dwStatFlag;
	}
	void StatFlag_Clear( DWORD dwStatFlag )
	{
		m_StatFlag &= ~dwStatFlag;
	}
	void StatFlag_Mod( DWORD dwStatFlag, bool fMod )
	{
		if ( fMod )
			m_StatFlag |= dwStatFlag;
		else
			m_StatFlag &= ~dwStatFlag;
	}

	CCharDefPtr Char_GetDef() const
	{
		return( REF_CAST(CCharDef,Base_GetDef()));
	}
	CRegionComplexPtr GetArea() const
	{
		return m_pArea; // What region are we in now. (for guarded message)
	}
	virtual CScriptObj* GetAttachedObj() 
	{
		return( this );
	}

	bool IsGM() const
	{
		return( IsPrivFlag(PRIV_GM));
	}
	bool IsResourceMatch( CSphereUID rid, DWORD dwArg );

	bool IsSpeakAsGhost() const
	{
		return( IsStatFlag(STATF_DEAD) &&
			! IsStatFlag( STATF_SpiritSpeak ) &&
			! IsGM());
	}
	bool CanUnderstandGhost() const
	{
		// Can i understand player ghost speak ?
		if ( m_pNPC &&
			m_pNPC->m_Brain == NPCBRAIN_HEALER )
			return( true );
		return( IsStatFlag( STATF_SpiritSpeak | STATF_DEAD ) || IsPrivFlag( PRIV_GM|PRIV_HEARALL ));
	}
	bool IsRespawned() const
	{
		// Can be resurrected later.
		return( m_pPlayer.IsValidNewObj() ||
			( IsStatFlag( STATF_RespawnNPC ) &&
			m_ptHome.IsValidPoint() &&
			! IsStatFlag( STATF_Spawned|STATF_Conjured )));
	}
	bool IsHuman() const
	{
		return( CCharDef::IsHumanID( GetDispID()));
	}
	int	 GetHealthPercent() const;
	LPCTSTR GetTradeTitle() const; // Paperdoll title for character p (2)

	// Information about us.
	CREID_TYPE GetID() const
	{
		CCharDefPtr pCharDef = Char_GetDef();
		ASSERT(pCharDef);
		return( pCharDef->GetID());
	}
	CREID_TYPE GetDispID() const
	{
		CCharDefPtr pCharDef = Char_GetDef();
		ASSERT(pCharDef);
		return( pCharDef->GetDispID());
	}
	CCharDefPtr SetID( CREID_TYPE id );

	virtual int GetPrivLevel() const
	{
		// The higher the better. // PLEVEL_Counsel
		if ( ! m_pPlayer && m_pNPC )
			return( PLEVEL_Player );
		return( CAccountConsole::GetPrivLevel());
	}
	virtual CGString GetName() const;
	virtual bool SetName( LPCTSTR pName );

	bool CanSeeMode( const CChar* pChar = NULL ) const
	{
		// Can i see normally invisible things ?
		if ( pChar == NULL || pChar == this )
			return( false );
		// if ( pChar->IsStatFlag( STATF_Sleeping )) return( false );
		return( pChar->GetPrivLevel() >= GetPrivLevel());
	}
	virtual int GetVisibleDistance() const;
	int GetVisualAbility() const;
	bool CanSeeInContainer( const CItemContainer* pContItem ) const;
	bool CanSee( const CObjBaseTemplate* pObj ) const;
	bool CanSeeLOS( const CPointMap& pd, CPointMap* pBlock = NULL, int iMaxDist = SPHEREMAP_VIEW_SIGHT ) const;
	bool CanSeeLOS( const CObjBaseTemplate* pObj ) const;
	bool CanHear( const CObjBaseTemplate* pSrc, TALKMODE_TYPE mode ) const;
	bool CanSeeItem( const CItem* pItem ) const
	{
		// Item on the ground.
		ASSERT(pItem);
		if ( ! IsGM())
		{
			if ( pItem->IsAttr( ATTR_INVIS ))
				return( false );
		}
		return( true );
	}
	bool CanTouch( const CPointMap& pt ) const;
	bool CanTouch( const CObjBase* pObj ) const;
	IT_TYPE CanTouchStatic( CPointMap& pt, ITEMID_TYPE id, CItem* pItem ) const;
	bool CanMove( CItem* pItem, bool fMsg = true );
	bool CanUse( CItem* pItem, bool fMoveOrConsume );

	BYTE GetLightLevel() const;
	NPCBRAIN_TYPE GetCreatureType() const; // return 1 for animal, 2 for monster, 3 for NPC humans and PCs

	int  Food_CanEat( CObjBase* pObj ) const;
	int  Food_GetLevelPercent() const;
	LPCTSTR Food_GetLevelMessage( bool fPet, bool fHappy ) const;

public:
	void Stat_Set( STAT_TYPE i, STAT_LEVEL iVal );
	STAT_LEVEL Stat_GetMax( STAT_TYPE i ) const;
	STAT_LEVEL Stat_Get( STAT_TYPE i ) const
	{
		ASSERT(((DWORD)i)<STAT_QTY );
		return m_Stat[i];
	}
	void Stat_Change( STAT_TYPE i, int iChange = 0, int iLimit = 0 );

	int ContentConsume( CSphereUID rid, int iQty = 1, bool fTest = false, DWORD dwArg = 0 );

	// Location and movement ------------------------------------
private:
	HRESULT TeleportToCli( int iType, int iArgs );
	HRESULT TeleportToObj( int iType, const CGVariant& vArgs );

	CRegionPtr CheckValidMove( CPointMapBase& pt, CMulMapBlockState& block ) const;
	bool MoveToRegion( CRegionComplex* pNewArea, bool fAllowReject );

public:
	CObjBasePtr GetTopLevelObj( void ) const
	{
		// Get the object that has a location in the world. (Ground level)
		return( const_cast <CChar*>(this) ); // cast away const
	}

	virtual void DeleteThis();

	bool IsSwimming() const;

	bool MoveToRegionReTest( DWORD dwType )
	{
		CRegionPtr pRegion = GetTopRegion( dwType );
		return( MoveToRegion( REF_CAST(CRegionComplex,pRegion), false ));
	}
	bool MoveTo( CPointMap pt );
	bool MoveToChar( CPointMap pt )
	{
		return MoveTo( pt );
	}
	bool MoveToValidSpot( DIR_TYPE dir, int iDist, int iDistStart = 1 );
	virtual bool MoveNearObj( const CObjBaseTemplate* pObj, int iSteps = 0, WORD wCan = CAN_C_WALK | CAN_C_CLIMB | CAN_C_INDOORS )
	{
		return CObjBase::MoveNearObj( pObj, iSteps, GetMoveCanFlags());
	}
	void MoveNear( CPointMap pt, int iSteps = 0, WORD wCan = CAN_C_WALK | CAN_C_CLIMB | CAN_C_INDOORS )
	{
		CObjBase::MoveNear( pt, iSteps, GetMoveCanFlags());
	}

	CRegionPtr CheckMoveWalkDir( CPointMapBase& ptDst, DIR_TYPE dir, bool fCheckChars );
	CRegionPtr CheckMoveWalkToward( const CPointMapBase& ptDst, bool fCheckChars )
	{
		CPointMap pt = GetTopPoint();
		return( CheckMoveWalkDir( pt, pt.GetDir(ptDst), fCheckChars ));
	}

	void CheckRevealOnMove();
	bool CheckLocation( bool fStanding = false );

public:
	// Client Player specific stuff. -------------------------
	void ClientAttach( CClient* pClient );
	void ClientDetach();
	bool IsClient() const { return( m_pClient != NULL ); }
	CClientPtr GetClient() const
	{
		return( m_pClient );
	}

	bool SetPrivLevel( CScriptConsole* pSrc, LPCTSTR pszFlags );
	bool Player_IsDND() const
	{
		return( GetPrivLevel() >= PLEVEL_Counsel && IsStatFlag( STATF_Insubstantial ));
	}
	bool CanDisturb( CChar* pChar ) const;
	void SetDisconnected();

	HRESULT Player_SetAccount( CAccount* pAccount );
	HRESULT Player_SetAccount( LPCTSTR pszAccount );
	void Player_Clear();

	HRESULT NPC_SetBrain( NPCBRAIN_TYPE NPCBrain );
	void NPC_Clear();

public:
	void ObjMessage( LPCTSTR pMsg, const CObjBase* pSrc ) const;
	virtual void SetMessageColorType( int iMsgColorType );
	virtual bool WriteString( LPCTSTR pMsg );	// Push a message back to the client if there is one.

	void UpdateStatsFlag() const;
	bool UpdateAnimate( ANIM_TYPE action, bool fTranslate = true, bool fBackward = false, BYTE iFrameDelay = 1 );

	void UpdateMode(  CClient* pExcludeClient = NULL, bool fFull= false );
	void UpdateMove( CPointMap pold, CClient* pClientExclude = NULL, bool fFull = false );
	void UpdateDir( DIR_TYPE dir );
	void UpdateDir( const CPointMap& pt );
	void UpdateDir( const CObjBaseTemplate* pObj );
	void UpdateDrag( CItem* pItem, CObjBase* pCont = NULL, CPointMap* pp = NULL );
	virtual void Update( const CClient* pClientExclude = NULL );

public:
	LPCTSTR GetPronoun() const;	// he
	LPCTSTR GetPossessPronoun() const;	// his
	BYTE GetModeFlag( bool fTrueSight = false ) const;
	BYTE GetDirFlag() const
	{
		BYTE dir = m_dirFace;
		ASSERT( dir<DIR_QTY );
		if ( IsStatFlag( STATF_Fly ))
			dir |= 0x80; // running/flying ?
		return( dir );
	}
	CAN_TYPE GetMoveCanFlags() const;
	int FixWeirdness();
	void CreateNewCharCheck();

private:
	// Contents/Carry stuff. ---------------------------------
	void ContentAdd( CItemPtr pItem )
	{
		LayerAdd( pItem, LAYER_QTY );
	}
protected:
	void UnEquipItem( CItem* pItem );
	virtual void OnRemoveOb( CGObListRec* pObRec );	// Override this = called when removed from list.
public:
	LAYER_TYPE CanEquipLayer( CItem* pItem, LAYER_TYPE layer, CChar* pCharMsg, bool fTest );
	CItemPtr LayerFind( LAYER_TYPE layer ) const;
	bool LayerAdd( CItem* pItem, LAYER_TYPE layer );

	virtual void OnWeightChange( int iChange );

	virtual long GetMakeValue();
	virtual int GetWeight() const
	{
		return( CContainer::GetTotalWeight());
	}
	int GetWeightLoadPercent( int iWeight ) const;
	bool CanCarry( const CItem* pItem ) const;

	CItemPtr GetSpellbook() const;
	CItemContainerPtr GetPack( void ) const
	{
		return( REF_CAST(CItemContainer,LayerFind( LAYER_PACK )));
	}
	CItemContainerPtr GetBank( LAYER_TYPE layer = LAYER_BANKBOX );
	CItemContainerPtr GetPackSafe( void )
	{
		return( GetBank(LAYER_PACK));
	}
	void AddGoldToPack( int iAmount, CItemContainer* pPack=NULL );

private:
	virtual TRIGRET_TYPE OnTrigger( LPCTSTR pTrigName, CScriptExecContext& exec );

public:
	// Load/Save----------------------------------

	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vValRet, CScriptConsole* pSrc );
	virtual bool s_LoadProps( CScript& s );  // Load a character from Script
	virtual void s_WriteProps( CScript& s );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script
	virtual void s_Serialize( CGFile& a );	// binary

	void s_WriteParity( CScript& s );

	TRIGRET_TYPE OnTrigger( CCharDef::T_TYPE_ trigger, CScriptExecContext& exec )
	{
		ASSERT( trigger < CCharDef::T_QTY );
		return( OnTrigger( MAKEINTRESOURCE(trigger), exec ));
	}

private:
	// Noto/Karma stuff. --------------------------------
	void Noto_Karma( int iKarma, int iBottom=-10000 );
	void Noto_Fame( int iFameChange );
	void Noto_ChangeNewMsg( int iPrv );
	void Noto_ChangeDeltaMsg( int iDelta, LPCTSTR pszType );

public:
	NOTO_TYPE Noto_GetFlag( const CChar* pChar, bool fIncog = false ) const;
	HUE_TYPE Noto_GetHue( const CChar* pChar, bool fIncog = false ) const;
	bool Noto_IsNeutral() const;
	bool Noto_IsMurderer() const;
	bool Noto_IsEvil() const;
	bool Noto_IsCriminal() const
	{
		// do the guards hate me ?
		if ( IsStatFlag( STATF_Criminal ))
			return( true );
		return Noto_IsEvil();
	}
	int Noto_GetLevel() const;
	LPCTSTR Noto_GetFameTitle() const;
	LPCTSTR Noto_GetTitle() const;

	void Noto_Kill( CChar* pKill, bool fPetKill = false );
	void Noto_Criminal();
	void Noto_Murder();
	void Noto_KarmaChangeMessage( int iKarmaChange, int iLimit );

	bool IsTakeCrime( const CItem* pItem, CCharPtr* ppCharMark = NULL ) const;

public:
	// skills and actions. -------------------------------------------
	static bool IsSkillBase( SKILL_TYPE skill );
	static bool IsSkillNPC( SKILL_TYPE skill );

	SKILL_TYPE Skill_GetBest( int iRank = 0 ) const; // Which skill is the highest for character p
	SKILL_TYPE Skill_GetActive() const
	{
		return( m_Act.m_SkillCurrent );
	}
	LPCTSTR Skill_GetName( bool fUse = false ) const;
	SKILL_LEVEL Skill_GetBase( SKILL_TYPE skill ) const
	{
		ASSERT( IsSkillBase(skill));
		return( m_Skill[skill] );
	}
	int Skill_GetMax( SKILL_TYPE skill ) const;
	SKILLLOCK_TYPE Skill_GetLock( SKILL_TYPE skill ) const
	{
		if ( ! m_pPlayer.IsValidNewObj() )
			return( SKILLLOCK_UP );
		return( m_pPlayer->Skill_GetLock(skill));
	}
	SKILL_LEVEL Skill_GetAdjusted( SKILL_TYPE skill ) const;
	void Skill_SetBase( SKILL_TYPE skill, SKILL_LEVEL wValue );
	bool Skill_UseQuick( SKILL_TYPE skill, int difficulty );
	bool Skill_CheckSuccess( SKILL_TYPE skill, int difficulty );
	bool Skill_Wait( SKILL_TYPE skilltry );
	bool Skill_Start( SKILL_TYPE skill, int iDifficulty = 0 ); // calc skill progress.
	void Skill_Fail( bool fCancel = false );
	int Skill_Stage( CSkillDef::T_TYPE_ stage );

	bool Skill_Mining_Smelt( CItem* pItemOre, CItem* pItemTarg );
	bool Skill_Tracking( CSphereUID uidTarg, DIR_TYPE& dirPrv, int iDistMax = SHRT_MAX );
	bool Skill_MakeItem( ITEMID_TYPE id, CSphereUID uidTarg, CSkillDef::T_TYPE_ stage );
	bool Skill_MakeItem_Success( int iAmount );
	bool Skill_Snoop_Check( const CItemContainer* pItem );

private:
	void Skill_Cleanup();	 // may have just cancelled targeting.
	int Skill_Done();	 // complete skill (async)
	bool Skill_Degrade( SKILL_TYPE skill );
	void Skill_Experience( SKILL_TYPE skill, int difficulty );

	int Skill_NaturalResource_Setup( CItem* pResBit );
	CItemPtr Skill_NaturalResource_Create( CItem* pResBit, SKILL_TYPE skill );
	void Skill_SetTimeout();

	int Skill_Inscription( CSkillDef::T_TYPE_ stage );
	int Skill_MakeItem( CSkillDef::T_TYPE_ stage );
	int Skill_Information( CSkillDef::T_TYPE_ stage );
	int Skill_Hiding( CSkillDef::T_TYPE_ stage );
	int Skill_Enticement( CSkillDef::T_TYPE_ stage );
	int Skill_Bowcraft( CSkillDef::T_TYPE_ stage );
	int Skill_Snooping( CSkillDef::T_TYPE_ stage );
	int Skill_Stealing( CSkillDef::T_TYPE_ stage );
	int Skill_Mining( CSkillDef::T_TYPE_ stage );
	int Skill_Lumberjack( CSkillDef::T_TYPE_ stage );
	int Skill_Taming( CSkillDef::T_TYPE_ stage );
	int Skill_Fishing( CSkillDef::T_TYPE_ stage );
	int Skill_Cartography(CSkillDef::T_TYPE_ stage);
	int Skill_DetectHidden(CSkillDef::T_TYPE_ stage);
	int Skill_Herding(CSkillDef::T_TYPE_ stage);
	int Skill_Blacksmith(CSkillDef::T_TYPE_ stage);
	int Skill_Lockpicking(CSkillDef::T_TYPE_ stage);
	int Skill_Peacemaking(CSkillDef::T_TYPE_ stage);
	int Skill_Alchemy(CSkillDef::T_TYPE_ stage);
	int Skill_Carpentry(CSkillDef::T_TYPE_ stage);
	int Skill_Provocation(CSkillDef::T_TYPE_ stage);
	int Skill_Poisoning(CSkillDef::T_TYPE_ stage);
	int Skill_Cooking(CSkillDef::T_TYPE_ stage);
	int Skill_Healing(CSkillDef::T_TYPE_ stage);
	int Skill_Meditation(CSkillDef::T_TYPE_ stage);
	int Skill_RemoveTrap(CSkillDef::T_TYPE_ stage);
	int Skill_Begging( CSkillDef::T_TYPE_ stage );
	int Skill_SpiritSpeak( CSkillDef::T_TYPE_ stage );
	int Skill_Magery( CSkillDef::T_TYPE_ stage );
	int Skill_Tracking( CSkillDef::T_TYPE_ stage );
	int Skill_Fighting( CSkillDef::T_TYPE_ stage );
	int Skill_Musicianship( CSkillDef::T_TYPE_ stage );
	int Skill_Tailoring( CSkillDef::T_TYPE_ stage );

	int Skill_Act_Napping(CSkillDef::T_TYPE_ stage);
	int Skill_Act_Throwing(CSkillDef::T_TYPE_ stage);
	int Skill_Act_Breath(CSkillDef::T_TYPE_ stage);
	int Skill_Act_Training( CSkillDef::T_TYPE_ stage );
	int Skill_Act_Looting( CSkillDef::T_TYPE_ stage );

	bool Spell_TargCheck();
	bool Spell_Unequip( LAYER_TYPE layer );

	int  Spell_CastStart();
	bool Spell_CastDone();
	void Spell_CastFail();

public:
	virtual bool OnSpellEffect( SPELL_TYPE spell, CChar* pCharSrc, int iSkillLevel, CItem* pSourceItem );

	bool Spell_Effect_Poison( int iLevel, int iTicks, CChar* pCharSrc, bool fMagic = false );
	bool Spell_Effect_Cure( int iLevel, bool fExtra );
	bool Spell_Effect_AnimateDead( CItemCorpse* pCorpse );
	bool Spell_Effect_BoneArmor( CItemCorpse* pCorpse );
	bool Spell_Effect_Resurrection( int iSkillLoss, CItemCorpse* pCorpse );
	bool Spell_Effect_Teleport( CPointMapBase pt, bool fTakePets = false, bool fCheckAntiMagic = true, ITEMID_TYPE iEffect = ITEMID_FX_TELE_VANISH, SOUND_TYPE iSound = 0x01fe );
	void Spell_Effect_Bolt( CObjBase* pObj, ITEMID_TYPE idBolt, int iSkill );
	void Spell_Effect_Field( CPointMap pt, ITEMID_TYPE idEW, ITEMID_TYPE idNS, int iSkill );
	void Spell_Effect_Area( CPointMap pt, int iDist, int iSkill );
	void Spell_Effect_Dispel( int iskilllevel );
	bool Spell_Effect_Recall( CItem* pRune, bool fGate, int iSkillLevel );
	CCharPtr Spell_Effect_Summon( CREID_TYPE id, CPointMap pt, bool fPet );

	void Spell_Equip_Remove( CItem* pSpell );
	void Spell_Equip_Add( CItem* pSpell );
	CItemPtr Spell_Equip_Create( SPELL_TYPE spell, LAYER_TYPE layer, int iSkillLevel, int iDuration, CObjBase* pSrc, bool fDispellable );
	bool Spell_Equip_OnTick( CItem* pItem );

	bool Spell_CanCast( SPELL_TYPE spell, bool fTest, CObjBase* pSrc, bool fFailMsg );

	// Memories about objects in the world. -------------------
private:
	bool Memory_OnTick( CItemMemory* pMemory );
	bool Memory_UpdateFlags( CItemMemory* pMemory );
	bool Memory_UpdateClearTypes( CItemMemory* pMemory, WORD MemTypes );
	void Memory_AddTypes( CItemMemory* pMemory, WORD MemTypes );
	CItemMemoryPtr Memory_CreateObj( CSphereUID uid, WORD MemTypes );
	CItemMemoryPtr Memory_CreateObj( const CObjBase* pObj, WORD MemTypes )
	{
		ASSERT(pObj);
		return Memory_CreateObj( pObj->GetUID(), MemTypes );
	}

public:
	void Memory_ClearTypes( WORD MemTypes );
	bool Memory_ClearTypes( CItemMemory* pMemory, WORD MemTypes );

	CItemMemoryPtr Memory_FindObj( CSphereUID uid ) const;
	CItemMemoryPtr Memory_FindObj( const CObjBase* pObj ) const
	{
		if ( pObj == NULL )
			return( NULL );
		return Memory_FindObj( pObj->GetUID());
	}
	CItemMemoryPtr Memory_AddObjTypes( CSphereUID uid, WORD wMemTypes );
	CItemMemoryPtr Memory_AddObjTypes( const CObjBase* pObj, WORD wMemTypes )
	{
		if (pObj==NULL)
			return NULL;
		return Memory_AddObjTypes( pObj->GetUID(), wMemTypes );
	}
	CItemMemoryPtr Memory_FindTypes( WORD MemTypes ) const;
	CItemMemoryPtr Memory_FindObjTypes( const CObjBase* pObj, WORD wMemTypes ) const
	{
		CItemMemoryPtr pMemory = Memory_FindObj(pObj);
		if ( pMemory == NULL )
			return( NULL );
		if ( ! pMemory->IsMemoryTypes( wMemTypes ))
			return( NULL );
		return( pMemory );
	}

public:
	SOUND_TYPE SoundChar( CRESND_TYPE type );
	void Action_StartSpecial( CREID_TYPE id );

private:
	void OnNoticeCrime( CChar* pCriminal, const CChar* pCharMark );
public:
	bool CheckCrimeSeen( SKILL_TYPE SkillToSee, CChar* pCharMark, const CObjBase* pItem, LPCTSTR pAction );

private:
	// Armor, weapons and combat ------------------------------------
	int CalcArmorDefense( void ) const;
	SKILL_TYPE Fight_GetWeaponSkill() const;
	void Fight_ResetWeaponSwingTimer();
	int Fight_GetWeaponSwingTimer();
	bool Fight_IsActive() const;
public:
	void Memory_Fight_Retreat( CChar* pTarg, CItemMemory* pFight );
	void Memory_Fight_Start( const CChar* pTarg );
	bool Memory_Fight_OnTick( CItemMemory* pMemory );

	bool Fight_Stunned( DIR_TYPE dir );
	bool Fight_Attack( const CChar* pCharTarg );
	bool Fight_Clear( const CChar* pCharTarg );
	void Fight_ClearAll();
	CCharPtr Fight_FindBestTarget();
	bool Fight_AttackNext();
	void Fight_HitTry( void );
	WAR_SWING_TYPE Fight_Hit( CChar* pCharTarg );
	int  Fight_CalcDamage( CItem* pWeapon, SKILL_TYPE skill ) const;

	HRESULT s_MethodPlayer( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script

	void InitPlayer( const CUOEvent* pEvent, CClient* pClient );
	bool ReadScriptTrig( CCharDef* pCharDef, CCharDef::T_TYPE_ trig );
	bool ReadScript( CResourceLock& s );
	void NPC_LoadScript( bool fRestock );

	// Mounting and figurines
	bool Horse_Mount( CCharPtr pHorse ); // Remove horse char and give player a horse item
	CCharPtr Horse_UnMount(); // Remove horse char and give player a horse item

private:
	CItemPtr Horse_GetMountItem() const;
	CCharPtr Horse_GetMountChar() const;
public:
	CCharPtr Use_Figurine( CItem* pItem, int iPaces = 0 );
	CItemPtr Make_Figurine( CSphereUID uidOwner, ITEMID_TYPE id = ITEMID_NOTHING );
	CItemPtr NPC_Shrink();

	int  ItemPickup( CItem* pItem, int amount );
	HRESULT ItemEquip( CItem* pItem, CChar* pCharMsg = NULL );
	HRESULT ItemEquipWeapon( bool fForce );
	HRESULT ItemEquipArmor( bool fForce );
	HRESULT ItemDrop( CItem* pItem, const CPointMap& pt );
	HRESULT ItemBounce( CItem* pItem );

	void Flip( LPCTSTR pCmd = NULL );

	bool CheckCorpseCrime( const CItemCorpse *pCorpse, bool fLooting, bool fTest );
	CItemCorpsePtr FindMyCorpse( int iRadius = 2 ) const;
	CItemCorpsePtr MakeCorpse( bool fFrontFall );
	bool RaiseCorpse( CItemCorpse* pCorpse );
	bool Death();
	bool Reveal( DWORD dwFlags = ( STATF_Invisible | STATF_Hidden | STATF_Sleeping ));
	void JailEffect();
	void Jail( CScriptConsole* pSrc, int iTimeMinutes );
	void EatAnim( LPCTSTR pszName, int iQty );
	void CallGuards( CChar* pCriminal );

	virtual void Speak( LPCTSTR pText, HUE_TYPE wHue = HUE_TEXT_DEF, TALKMODE_TYPE mode = TALKMODE_SAY, FONT_TYPE font = FONT_NORMAL );
	virtual void SpeakUTF8( LPCTSTR pText, HUE_TYPE wHue= HUE_TEXT_DEF, TALKMODE_TYPE mode= TALKMODE_SAY, FONT_TYPE font= FONT_NORMAL, CLanguageID lang = 0 );

	bool OnFreezeCheck( int iDmg );
	void DropAll( CItemCorpse* pCorpse = NULL, WORD wAttr = 0 );
	void UnEquipAllItems( CItemContainer* pCorpse = NULL );
	void CancelAllTrades();
	void SleepWake();
	CItemCorpsePtr SleepStart( bool fFrontFall );

	void Guild_Resign( MEMORY_TYPE memtype );
	CItemStonePtr Guild_Find( MEMORY_TYPE memtype ) const;
	LPCTSTR Guild_Abbrev( MEMORY_TYPE memtype ) const;
	LPCTSTR Guild_AbbrevAndTitle( MEMORY_TYPE memtype ) const;

	bool Use_SpinWheel( CItem* pWheel, CItem* pHair );
	bool Use_Loom( CItem* pLoom, CItem* pYarn );
	void Use_EatQty( CItem* pFood, int iQty = 1 );
	bool Use_Eat( CItem* pItem, int iQty = 1 );
	bool Use_MultiLockDown( CItem* pItemTarg );
	void Use_CarveCorpse( CItemCorpse* pCorpse );
	bool Use_Repair( CItem* pItem );
	int  Use_PlayMusic( CItem* pInstrument, int iDifficultyToPlay );
	bool Use_Drink( CItem* pItem );
	bool Use_Cannon_Feed( CItem* pCannon, CItem* pFeed );
	bool Use_Item_Web( CItem* pItem );
	void Use_AdvanceGate( CItem* pItem );
	void Use_MoonGate( CItem* pItem );
	bool Use_Kindling( CItem* pKindling );
	bool Use_BedRoll( CItem* pItem );
	bool Use_Seed( CItem* pItem, CPointMap* pPoint );
	bool Use_Key( CItem* pKey, CItem* pItemTarg );
	bool Use_KeyChange( CItem* pItemTarg );
	bool Use_Train_PickPocketDip( CItem* pItem, bool fSetup );
	bool Use_Train_Dummy( CItem* pItem, bool fSetup );
	bool Use_Train_ArcheryButte( CItem* pButte, bool fSetup );
	bool Use_Item( CItem* pItem, bool fLink = false );
	bool Use_Obj( CObjBase* pObj, bool fTestTouch );

	// NPC AI -----------------------------------------
private:
	static CREID_TYPE NPC_GetAllyGroupType(CREID_TYPE idTest);

	void NPC_Food_Search();
	bool NPC_Food_MeatCheck(int iAmount, int iBite, int iSearchDist);
	bool NPC_Food_FruitCheck(int iAmount, int iBite, int iSearchDist);
	bool NPC_Food_CropCheck(int iAmount, int iBite, int iSearchDist);
	bool NPC_Food_GrainCheck(int iAmount, int iBite, int iSearchDist);
	bool NPC_Food_FishCheck(int iAmount, int iBite, int iSearchDist);
	bool NPC_Food_GrassCheck(int iAmount, int iBite, int iSearchDist);
	bool NPC_Food_GarbageCheck(int iAmount, int iBite, int iSearchDist);
	bool NPC_Food_LeatherCheck(int iAmount, int iBite, int iSearchDist);
	bool NPC_Food_HayCheck(int iAmount, int iBite, int iSearchDist);
	bool NPC_Food_EdibleCheck(int iAmount, int iBite, int iSearchDist);
	bool NPC_Food_VendorCheck(int iAmount, int iBite);
	bool NPC_Food_VendorFind(int iSearchDist);
	CItemPtr NPC_Food_Trade( bool fTest );

	HRESULT NPC_StablePetRetrieve( CChar* pCharPlayer );
	HRESULT NPC_StablePetSelect( CChar* pCharPlayer );

	MOTIVE_LEVEL NPC_WantThisItem( CItem* pItem );
	MOTIVE_LEVEL NPC_WantToUseThisItem( const CItem* pItem ) const;
	int NPC_GetWeaponUseScore( CItem* pItem );

	MOTIVE_LEVEL NPC_GetHostilityLevelToward( const CChar* pCharTarg ) const;
	MOTIVE_LEVEL NPC_GetAttackContinueMotivation( CChar* pChar, MOTIVE_LEVEL iMotivation = 0 ) const;
	MOTIVE_LEVEL NPC_GetAttackMotivation( CChar* pChar, MOTIVE_LEVEL iMotivation = 0 ) const;
	int  NPC_GetTrainMax( const CChar* pStudent, SKILL_TYPE Skill ) const;

	HRESULT s_MethodNPC( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script

	bool NPC_MotivationCheck( MOTIVE_LEVEL iMotivation );
	int	 NPC_OnTrainCheck( CChar* pCharSrc, SKILL_TYPE Skill );
	bool NPC_OnTrainPay( CChar* pCharSrc, CItemMemory* pMemory, CItem* pGold );
	HRESULT NPC_OnTrainHear( CChar* pCharSrc, LPCTSTR pCmd );
	bool NPC_CheckWalkHere( const CPointMapBase& pt, const CRegionBasic* pArea, bool fIsCovered ) const;
	void NPC_OnNoticeSnoop( CChar* pCharThief, CChar* pCharMark );

	bool NPC_LootContainer( CItemContainer* pItem );
	void NPC_LootMemory( CItem* pItem );
	bool NPC_LookAtCharGuard( CChar* pChar );
	bool NPC_LookAtCharHealer( CChar* pChar );
	bool NPC_LookAtCharHuman( CChar* pChar );
	bool NPC_LookAtCharMonster( CChar* pChar );
	bool NPC_LookAtChar( CChar* pChar, int iDist );
	bool NPC_LookAtItem( CItem* pItem, int iDist );
	bool NPC_LookAround( bool fForceCheckItems = false );
	int  NPC_WalkToPoint( bool fRun = false );
	bool NPC_FightMagery( CChar* pChar );
	bool NPC_FightArchery( CChar* pChar );
	bool NPC_FightMayCast() const;

	bool NPC_Act_Follow( bool fFlee = false, int maxDistance = 1, bool forceDistance = false );
	void NPC_Act_GoHome();
	bool NPC_Act_Talk();
	void NPC_Act_Wander();
	bool NPC_Act_Begging( CChar* pChar );
	void NPC_Act_Fight();
	void NPC_Act_Idle();
	void NPC_Act_Looting();
	void NPC_Act_Eating();
	void NPC_Act_Flee();
	void NPC_Act_Goto();

	void NPC_ActStart_SpeakTo( CChar* pSrc );

	void NPC_OnTickAction();
	bool NPC_OnTickFood( int nFoodLevel );
	bool NPC_OnTickPetStatus( int nFoodLevel );

	void NPC_OnHirePayMore( CItem* pGold, bool fHire = false );
	bool NPC_OnHirePay( CChar* pCharSrc, CItemMemory* pMemory, CItem* pGold );
	HRESULT NPC_OnHireHear( CChar* pCharSrc );
public:
	void NPC_PetDesert();
	void NPC_PetClearOwners();
	bool NPC_PetSetOwner( const CChar* pChar );
	CCharPtr NPC_PetGetOwner() const;
	bool NPC_IsOwnedBy( const CChar* pChar, bool fAllowGM = true ) const;
	bool NPC_IsSpawnedBy( const CItem* pItem ) const;
	bool NPC_CanSpeak() const;

	static CItemVendablePtr NPC_FindVendableItem( CItemVendable* pVendItem, CItemContainer* pVend1, CItemContainer* pVend2 );

	bool NPC_IsVendor() const
	{
		if ( ! m_pNPC.IsValidNewObj() )
			return( false );
		return( m_pNPC->IsVendor());
	}
	HRESULT NPC_Vendor_Restock( int iTimeSec );
	bool NPC_Vendor_Dump( CItemContainer* pBank );
	MOTIVE_LEVEL NPC_GetVendorMarkup( const CChar* pCharCustomer ) const;

	bool NPC_PetDrop( CChar* pMaster );
	bool NPC_PetSpeakStatus( CChar* pMaster );
	bool NPC_PetVendorCash( CChar* pMaster );
	void NPC_PetResponse( bool fSuccess, const char* pszSpeak, CChar* pMaster );
	bool NPC_OnHearPetCmd( LPCTSTR pszCmd, CChar* pSrc, bool fAllpets );
	bool NPC_OnHearPetCmdTarg( int iCmd, CChar* pSrc, CObjBase* pObj, const CPointMap& pt, LPCTSTR pszArgs );
	int  NPC_OnHearName( LPCTSTR pszText ) const;
	bool NPC_OnHearFirst( CChar* pCharSrc, CItemMemory* pMemory );
	void NPC_OnHear( LPCTSTR pCmd, CChar* pSrc );
	bool NPC_OnItemGive( CChar* pCharSrc, CItem* pItem );
	bool NPC_SetVendorPrice( CItem* pItem, int iPrice );

	HRESULT ScriptBook_Command( LPCTSTR pCmd, bool fSystemCheck );
	void ScriptBook_OnTick( CItemMessage* pScriptItem, bool fForce = false );

	// Outside events that occur to us.
	virtual int  OnGetHit( int iDmg, CChar* pSrc, DAMAGE_TYPE uType = DAMAGE_HIT_BLUNT );
	virtual int  OnTakeDamage( int iDmg, CChar* pSrc, DAMAGE_TYPE uType = DAMAGE_HIT_BLUNT );
	int  OnTakeDamageHitPoint( int iDmg, CChar* pSrc, DAMAGE_TYPE uType );
	void OnHelpedBy( CChar* pCharSrc );
	void OnHarmedBy( CChar* pCharSrc, int iHarmQty );
	bool OnAttackedBy( CChar* pCharSrc, int iHarmQty, bool fPetsCommand = false );

	void OnHearEquip( CChar* pChar, TCHAR* szText );
	bool OnTickEquip( CItem* pItem );
	void OnTickFood();
	bool OnTick();

	static CCharPtr CreateBasic( CREID_TYPE baseID );
	static CCharPtr CreateNPC( CREID_TYPE id );

	CChar( CREID_TYPE id );
	virtual ~CChar(); // Delete character
};

inline bool CChar::IsSkillBase( SKILL_TYPE skill ) // static
{
	// Is this in the base set of skills.
	return( IS_SKILL_BASE(skill));
}
inline bool CChar::IsSkillNPC( SKILL_TYPE skill )  // static
{
	// Is this in the NPC set of skills.
	return( skill >= NPCACT_FOLLOW_TARG && skill < NPCACT_QTY );
}

#endif
