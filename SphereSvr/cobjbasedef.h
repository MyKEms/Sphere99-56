//
// CObjBaseDef.h
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
//

#ifndef _INC_CBASE_H
#define _INC_CBASE_H
#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

#include "cresource.h"

struct CObjBaseDef : public CResourceTriggered
{
	// Minimal amount of common info to define RES_ItemDef or RES_CharDef, (it might just be a DUPE)
	// The unique index id.	(WILL not be the same as artwork if outside artwork range)

public:
	CObjBaseDef( CSphereUID id );
	virtual ~CObjBaseDef();

	CSphereUID GetResourceID() const
	{
		return GetUIDIndex();
	}
	LPCTSTR GetTypeName() const
	{
		return( m_sName );
	}
	virtual CGString GetName() const
	{
		return( GetTypeName());
	}
	bool HasTypeName() const	// some CItem may not.
	{
		return( ! m_sName.IsEmpty());	// default type name.
	}
	virtual void SetTypeName( LPCTSTR pszName )
	{
		GETNONWHITESPACE( pszName );
		m_sName = pszName;
	}
	CAN_TYPE GetCanFlags() const
	{
		// CAN_C_GHOST
		return m_CanFlags;
	}
	bool Can( CAN_TYPE wCan ) const
	{
		return(( m_CanFlags & wCan ) ? true : false );
	}
	virtual void UnLink()
	{
		m_BaseResources.RemoveAll();
		CResourceTriggered::UnLink();
	}

	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );

	bool IsValid() const
	{
#ifdef _DEBUG
		ASSERT( IsValidHeap());
#endif
		return( m_sName.IsValid());
	}

	PNT_Z_TYPE GetHeight() const
	{
		return( m_Height );
	}
	void SetHeight( PNT_Z_TYPE Height )
	{
		m_Height = Height;
	}

	void CopyBasic( const CObjBaseDef* pSrc );
	void CopyTransfer( CObjBaseDef* pSrc );

	// Base type of both CItemDef and CCharDef
public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define COBJBASEDEFPROP(a,b,c) P_##a,
#include "cobjbasedefprops.tbl"
#undef COBJBASEDEFPROP
		P_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];

	CResourceQtyArray m_BaseResources;	// RESOURCES=10 MEAT (What is this made of)

protected:
	// ITEMID_TYPE or CREID_TYPE
	WORD	 m_wDispIndex;	// The base artwork id. (the same as GetResourceID() in base set but not in high set.)
	CAN_TYPE m_CanFlags;	// Base attribute flags. CAN_C_GHOST

	// NOTE: ??? change NAME= in the scripts such that it seperates the trade name from the person name.
	CGString m_sName;		// default type name. (i.e., "human" vs specific "Dennis")
private:
	PNT_Z_TYPE m_Height;		// height of object or creature.
};

enum IT_TYPE		// double click type action.
{
	// Define the action/functionality of an item. (not what it looks like)
	// NOTE: Never change this list as it will mess up the RES_ItemDef or RES_WorldItem already stored.
	// Just add new stuff to end.
	IT_NORMAL = 0,
	IT_CONTAINER = 1,		// any unlocked container or corpse. CContainer based
	IT_CONTAINER_LOCKED,	// 2 =
	IT_DOOR,				// 3 = door can be opened
	IT_DOOR_LOCKED,		// 4 = a locked door.
	IT_KEY,				// 5 =
	IT_LIGHT_LIT,			// 6 = Local Light giving object. (needs no fuel, never times out)
	IT_LIGHT_OUT,			// 7 = Can be lit.
	IT_FOOD,				// 8 = edible food. (poisoned food ?)
	IT_FOOD_RAW,			// 9 = Must cook raw meat unless your an animal.
	IT_ARMOR,				// 10 = some type of armor. (no real action)
	IT_WEAPON_MACE_SMITH,	// 11 = Can be used for smithing
	IT_WEAPON_MACE_SHARP,	// 12 = war axe can be used to cut/chop trees.
	IT_WEAPON_SWORD,		// 13 =
	IT_WEAPON_FENCE,		// 14 = can't be used to chop trees. (make kindling)
	IT_WEAPON_BOW,		// 15 = bow or xbow
	IT_WAND,			    // 16 = A magic storage item
	IT_TELEPAD,			// 17 = walk on teleport
	IT_SWITCH,			// 18 = this is a switch which effects some other object in the world.
	IT_BOOK,				// 19 = read this book. (static or dynamic text)
	IT_RUNE,				// 20 = can be marked and renamed as a recall rune.
	IT_BOOZE,				// 21 = booze	(drunk effect)
	IT_POTION,			// 22 = Some liquid in a container. Possibly Some magic effect.
	IT_FIRE,				// 23 = It will burn you.
	IT_CLOCK,				// 24 = or a wristwatch
	IT_TRAP,				// 25 = walk on trap.
	IT_TRAP_ACTIVE,		// 26 = some animation
	IT_MUSICAL,			// 27 = a musical instrument.
	IT_SPELL,				// 28 = magic spell effect.
	IT_GEM,				// 29 = no use yet
	IT_WATER,				// 30 = This is water (fishable) (Not a glass of water)
	IT_CLOTHING,			// 31 = All cloth based wearable stuff,
	IT_SCROLL,			// 32 = magic scroll
	IT_CARPENTRY,			// 33 = tool of some sort.
	IT_SPAWN_CHAR,		// 34 = spawn object. should be invis also.
	IT_GAME_PIECE,		// 35 = can't be removed from game.
	IT_PORTCULIS,			// 36 = Z delta moving gate. (open)
	IT_FIGURINE,			// 37 = magic figure that turns into a creature when activated.
	IT_SHRINE,			// 38 = can res you
	IT_MOONGATE,			// 39 = linked to other moon gates (hard coded locations)
	IT_CHAIR,				// 40 = Any sort of a chair item. we can sit on.
	IT_FORGE,				// 41 = used to smelt ore, blacksmithy etc.
	IT_ORE,				// 42 = smelt to ingots.
	IT_LOG,				// 43 = make into furniture etc. lumber,logs,
	IT_TREE,				// 44 = can be chopped.
	IT_ROCK,				// 45 = can be mined for ore.
	IT_CARPENTRY_CHOP,	// 46 = tool of some sort.
	IT_MULTI,				// 47 = multi part object like house or ship.
	IT_REAGENT,			// 48 = alchemy when clicked ?
	IT_SHIP,				// 49 = this is a SHIP MULTI
	IT_SHIP_PLANK,		// 50
	IT_SHIP_SIDE,			// 51 = Should extend to make a plank
	IT_SHIP_SIDE_LOCKED,	// 52
	IT_SHIP_TILLER,		// 53 = Tiller man on the ship.
	IT_EQ_TRADE_WINDOW,		// 54 = container for the trade window.
	IT_FISH,				// 55 = fish can be cut up.
	IT_SIGN_GUMP,			// 56 = Things like grave stones and sign plaques
	IT_STONE_GUILD,		// 57 = Guild stones
	IT_ANIM_ACTIVE,		// 58 = active anium that will recycle when done.
	IT_ADVANCE_GATE,		// 59 = advance gate. m_AdvanceGateType = CREID_TYPE
	IT_CLOTH,				// 60 = bolt or folded cloth
	IT_HAIR,				// 61
	IT_BEARD,				// 62 = just for grouping purposes.
	IT_INGOT,				// 63 = Ingot of some type made from IT_ORE.
	IT_COIN,				// 64 = coin of some sort. gold or otherwise.
	IT_CROPS,				// 65 = a plant that will regrow. picked type.
	IT_DRINK,				// 66 = some sort of drink (non booze, potion or water) (ex. cider)
	IT_ANVIL,				// 67 = for repair.
	IT_PORT_LOCKED,			// 68 = this portcullis must be triggered.
	IT_SPAWN_ITEM,		// 69 = spawn other items.
	IT_TELESCOPE,			// 70 = big telescope pic.
	IT_BED,					// 71 = bed. facing either way
	IT_GOLD,				// 72 = Gold Coin
	IT_MAP,				// 73 = Map object with pins.
	IT_EQ_MEMORY_OBJ,		// 74 = A Char has a memory link to some object. (I am fighting with someone. This records the fight.)
	IT_WEAPON_MACE_STAFF,	// 75 = staff type of mace. or just other type of mace.
	IT_EQ_HORSE,			// 76 = equipped horse object represents a riding horse to the client.
	IT_COMM_CRYSTAL,		// 77 = communication crystal.
	IT_GAME_BOARD,		// 78 = this is a container of pieces.
	IT_TRASH_CAN,			// 79 = delete any object dropped on it.
	IT_CANNON_MUZZLE,		// 80 = cannon muzzle. NOT the other cannon parts.
	IT_CANNON,			// 81 = the rest of the cannon.
	IT_CANNON_BALL,
	IT_ARMOR_LEATHER,		// 83 = Non metallic armor.
	IT_SEED,				// 84 = seed from fruit
	IT_JUNK,		// 85 = ring of reagents.
	IT_CRYSTAL_BALL,		// 86
	IT_CASHIERS_CHECK,	// 87 = represents large amounts of money.
	IT_MESSAGE,			// 88 = user written message item. (for bboard ussually)
	IT_REAGENT_RAW,		// 89 = Freshly grown reagents...not processed yet. NOT USED!
	IT_EQ_CLIENT_LINGER,	// 90 = Change player to NPC for a while.
	IT_DREAM_GATE,		// 91 = Push you to another server. (no items transfered, client insta logged out)
	IT_ITEM_STONE,		// 92 = Double click for items
	IT_METRONOME,			// 93 = ticks once every n secs.
	IT_EXPLOSION,			// 94 = async explosion.
	IT_EQ_SCRIPT_BOOK,		// 95 = Script npc actions in the form of a book.
	IT_WEB,				// 96 = walk on this and transform into some other object. (stick to it)
	IT_GRASS,				// 97 = can be eaten by grazing animals
	IT_AROCK,				// 98 = a rock or boulder. can be thrown by giants.
	IT_TRACKER,			// 99 = points to a linked object.
	IT_SOUND,				// 100 = this is a sound source.
	IT_STONE_TOWN,		// 101 = Town stones. everyone free to join.
	IT_WEAPON_MACE_CROOK,	// 102
	IT_WEAPON_MACE_PICK,	// 103
	IT_LEATHER,			// 104 = Leather or skins of some sort.(not wearable)
	IT_SHIP_OTHER,		// 105 = some other part of a ship.
	IT_BBOARD,			// 106 = a container and bboard object.
	IT_SPELLBOOK,			// 107 = spellbook (with spells)
	IT_CORPSE,			// 108 = special type of item.
	IT_TRACK_ITEM,		// 109 - track a id or type of item.
	IT_TRACK_CHAR,		// 110 = track a char or range of char id's
	IT_WEAPON_ARROW,		// 111
	IT_WEAPON_BOLT,		// 112
	IT_EQ_VENDOR_BOX,		// 113 = an equipped vendor .
	IT_EQ_BANK_BOX,		// 114 = an equipped bank box
	IT_DEED,			// 115
	IT_LOOM,			// 116
	IT_BEE_HIVE,		// 117
	IT_ARCHERY_BUTTE,	// 118
	IT_EQ_MURDER_COUNT,	// 119 = my murder count flag.
	IT_EQ_STUCK,		// 120
	IT_TRAP_INACTIVE,	// 121 = a safe trap.
	IT_STONE_ROOM,		// 122 = house type stone. (for mapped house regions)
	IT_BANDAGE,			// 123 = can be used for healing.
	IT_CAMPFIRE,		// 124 = this is a fire but a small one.
	IT_MAP_BLANK,		// 125 = blank map.
	IT_SPY_GLASS,		// 126
	IT_SEXTANT,			// 127
	IT_SCROLL_BLANK,	// 128
	IT_FRUIT,			// 129
	IT_WATER_WASH,		// 130 = water that will not contain fish. (for washing or drinking)
	IT_WEAPON_AXE,		// 131 = not the same as a sword. but uses swordsmanship skill
	IT_WEAPON_XBOW,		// 132
	IT_SPELLICON,		// 133
	IT_DOOR_OPEN,		// 134 = You can walk through doors that are open.
	IT_MEAT_RAW,		// 135 = raw (uncooked meat) or part of a corpse.
	IT_GARBAGE,			// 136 = this is junk.
	IT_KEYRING,			// 137
	IT_TABLE,			// 138 = a table top
	IT_FLOOR,			// 139
	IT_ROOF,			// 140
	IT_FEATHER,			// 141 = a birds feather
	IT_WOOL,			// 142 = Wool cut from a sheep.
	IT_FUR,				// 143
	IT_BLOOD,			// 144 = blood of some creature
	IT_FOLIAGE,			// 145 = does not go invis when reaped. but will if eaten
	IT_GRAIN,			// 146
	IT_SCISSORS,		// 147
	IT_THREAD,			// 148
	IT_YARN,			// 149
	IT_SPINWHEEL,		// 150
	IT_BANDAGE_BLOOD,	// 151 = must be washed in water to get bandage back.
	IT_FISH_POLE,		// 152
	IT_SHAFT,			// 153 = used to make arrows and xbolts
	IT_LOCKPICK,		// 154
	IT_KINDLING,		// 155 = lit to make campfire
	IT_TRAIN_DUMMY,		// 156
	IT_TRAIN_PICKPOCKET,// 157
	IT_BEDROLL,			// 158
	IT_BELLOWS,			// 159
	IT_HIDE,			// 160 = hides are cured to make leather.
	IT_CLOTH_BOLT,		// 161 = must be cut up to make cloth squares.
	IT_LUMBER,			// 162 = logs are plained into decent lumber
	IT_PITCHER,			// 163
	IT_PITCHER_EMPTY,	// 164
	IT_DYE_VAT,			// 165
	IT_DYE,				// 166
	IT_POTION_EMPTY,	// 167 = empty bottle.
	IT_MORTAR,			// 168 = for grinding potions.
	IT_HAIR_DYE,		// 169
	IT_SEWING_KIT,		// 170
	IT_TINKER_TOOLS,	// 171
	IT_WALL,			// 172 = wall of a structure.
	IT_WINDOW,			// 173 = window for a structure.
	IT_COTTON,			// 174 = Cotton from the plant
	IT_BONE,			// 175
	IT_EQ_SCRIPT,		// 176
	IT_SHIP_HOLD,		// 177 = ships hold.
	IT_SHIP_HOLD_LOCK,	// 178
	IT_LAVA,			// 179
	IT_SHIELD,			// 180 = equippable armor.
	IT_JEWELRY,			// 181 = equippable.
	IT_DIRT,			// 182 = a patch of dirt where i can plant something
	IT_SCRIPT,			// 183 = Scriptable item (but not equippable)
	IT_EQ_MESSAGE,		// 184 = Message gump attached to player.
	IT_SHOVEL,			// 185 = can't even equip this really !
	IT_LADDER,			// 186 = I climb this, These have direction.
	IT_STAIRS,			// 187 = I climb this, These have direction.
	IT_EQ_DIALOG,		// 188 = track an open gump dialog for the client.
	IT_QTY,

	IT_TRIGGER		= 1000,	// Create custom new script trigger types
};

typedef STAT_LEVEL SKILL_LEVEL;	// fixed point decimal * 10.

class CItemTypeDef : public CResourceTriggered
{
	// RES_TypeDef
	// Handler for item type events.
	// NOTE: can be used as stub for RES_ItemDef
public:
	CItemTypeDef( UID_INDEX rid ) : CResourceTriggered( rid )
	{
	}
public:
	CSCRIPT_CLASS_DEF1();
};

class CItemDef : public CObjBaseDef
{
	// RES_ItemDef
	// Describe basic stuff about all items.
	// Partly based on CUOItemTypeRec from tiledata.mul
public:
	CItemDef( ITEMID_TYPE id );
	virtual ~CItemDef();

	static bool IsTypeArmor( IT_TYPE type );
	static bool IsTypeWeapon( IT_TYPE type );
	static bool IsTypeMulti( IT_TYPE type );

	static CItemDefPtr TranslateBase( CResourceDef* pBaseStub );
	static bool IsValidDispID( ITEMID_TYPE id );

	// NOTE: ??? All this stuff should be moved to scripts !
	// Classify IT_TYPE by ITEMID_TYPE
	static IT_TYPE GetTypeBase( ITEMID_TYPE id, const CUOItemTypeRec &tile );
	static bool IsID_Multi( ITEMID_TYPE id );

	static bool IsVisibleLayer( LAYER_TYPE layer );

	static TCHAR* GetNamePluralize( LPCTSTR pszNameBase, bool fPluralize );
	static bool GetItemData( ITEMID_TYPE id, CUOItemTypeRec* ptile );
	static PNT_Z_TYPE GetItemHeight( ITEMID_TYPE id, CAN_TYPE& MoveFlags );

	IT_TYPE GetType() const
	{
		return( m_type );
	}
	bool IsType( IT_TYPE type ) const
	{
		return( type == m_type );
	}
	bool IsTypeEquippable() const;
	GUMP_TYPE IsTypeContainer() const;

	LAYER_TYPE GetEquipLayer() const
	{
		// Is this item really equippable ?
		return( (LAYER_TYPE) m_layer );
	}

	DWORD GetFlags() const
	{
		return m_dwFlags;	// UFLAG4_DOOR
	}

	void SetTypeName( LPCTSTR pszName );
	virtual CGString GetName() const;
	LPCTSTR GetArticleAndSpace() const;

	ITEMID_TYPE GetID() const
	{
		// Get type identifier id. (not always the same as artwork)
		return((ITEMID_TYPE) GetResourceID().GetResIndex());
	}
	ITEMID_TYPE GetDispID() const
	{
		// Get base artwork id.
		return((ITEMID_TYPE) m_wDispIndex );
	}
	bool IsSameDispID( ITEMID_TYPE id ) const;
	ITEMID_TYPE GetFlippedID( int i ) const
	{
		if ( i >= m_flip_id.GetSize())
			return( ITEMID_NOTHING );
		return( m_flip_id[i] );
	}
	ITEMID_TYPE GetNextFlipID( ITEMID_TYPE id ) const;

	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );

	bool IsMovableType() const
	{
		return( m_weight != USHRT_MAX );
	}
	bool IsStackableType() const
	{
		return( Can(CAN_I_PILE));
	}
	WORD GetWeight() const
	{
		// Get weight in tenths of a stone.
#define WEIGHT_UNITS 10
		if ( ! IsMovableType())
			return( WEIGHT_UNITS );	// If we can pick them up then we should be able to move them
		return( m_weight );
	}
	WORD GetVolume() const
	{
		return( m_weight / WEIGHT_UNITS );
	}
	long GetMakeValue( int iQualityPercent );
	int GetBaseValueMin() const
	{
		return( m_values.GetMin());
	}
	void Restock();

	virtual void UnLink()
	{
		m_flip_id.RemoveAll();
		m_SkillMake.RemoveAll();
		CObjBaseDef::UnLink();
	}

	void CopyBasic( const CItemDef* pBase );
	void CopyTransfer( CItemDef* pBase );

private:
	static CItemDefPtr MakeDupeReplacement( CItemDef* pBase, CGVariant& vValMaster );
	static PNT_Z_TYPE GetItemHeightFlags( const CUOItemTypeRec& tile, CAN_TYPE& wBlockThis );
	bool Load();
	int CalculateMakeValue( int iQualityPercent ) const;
	HRESULT SetBaseID( const char* pszArg, CItemDefPtr* ppItemDef );
	HRESULT SetBaseType( IT_TYPE type, CItemDefPtr* ppItemDef );

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CITEMDEFPROP(a,b,c) P_##a,
#include "citemdefprops.tbl"
#undef CITEMDEFPROP
		P_QTY,
	};
	enum T_TYPE_
	{
		// XTRIG_UNKNOWN = some named trigger not on this list.
#define CITEMEVENT(a,b,c) T_##a,
		CITEMEVENT(AAAUNUSED,CSCRIPTPROP_UNUSED,NULL)
#include "citemevents.tbl"
#undef CITEMEVENT
		T_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];
	static const CScriptProp sm_Triggers[T_QTY+1];

	// Not applicable to all. only items that can be made.
	CResourceQtyArray m_SkillMake;	// what skills to create this ? (and non-consumed items)

	union
	{
		// IT_NORMAL
		struct	// used only to script ref all this junk.
		{
			DWORD m_tData1;	// TDATA1=
			DWORD m_tData2;	// TDATA2=
			DWORD m_tData3;	// TDATA3=
			DWORD m_tData4;	// TDATA4=
		} m_ttNormal;

		// IT_WAND
		// IT_WEAPON_*
		// IT_ARMOR
		// IT_ARMOR_LEATHER
		// IT_SHIELD
		// IT_CLOTHING
		// IT_LIGHT_OUT
		// IT_LIGHT_LIT
		// IT_SPELLBOOK
		// IT_JEWELRY
		// IT_EQ_SCRIPT
		// Container pack is the only exception here. IT_CONTAINER
		struct	// ALL equippable items ex. Weapons and armor
		{
			int		m_junk1;
			int		m_StrReq;	// REQSTR= Strength required to weild weapons/armor.
			CSphereUIDBase m_Light_ID;		// TDATA3=Change light state to on/off
			CSphereUIDBase m_Light_Burnout;	// TDATA4 = what happens when burns out ? 1=delete ie.torch, 0=nothing ie. lantern
		} m_ttEquippable;

		// IT_WEAPON_BOW
		// IT_WEAPON_XBOW
		struct	// equippable items of Weapons and armor
		{
			int		m_junk1;	// TDATA1= Sound it makes ?
			int		m_StrReq;	// REQSTR= Strength required to weild weapons/armor.
			CSphereUIDBase m_idAmmo;	// TDATA3= required source ammo.
			CSphereUIDBase m_idAmmoX;	// TDATA4= fired ammo fx.
		} m_ttWeaponBow;

		// IT_CONTAINER
		// IT_SIGN_GUMP
		// IT_SHIP_HOLD
		// IT_BBOARD
		// IT_CORPSE
		// IT_TRASH_CAN
		// IT_GAME_BOARD
		// IT_EQ_BANK_BOX
		// IT_EQ_VENDOR_BOX
		// IT_KEYRING
		struct
		{
			int	m_junk1;
			GUMP_TYPE m_gumpid;	// TDATA2= the gump that comes up when this container is opened.
			DWORD m_dwMinXY;	// TDATA3= Gump size used.
			DWORD m_dwMaxXY;	// TDATA4=
		} m_ttContainer;

		// IT_FIGURINE
		// IT_EQ_HORSE
		struct
		{
			int	m_junk1;
			int		   m_StrReq;	// REQSTR= Strength required to mount
			CSphereUIDBase m_charid;	// TDATA3= (CREID_TYPE) (duplicates m_itFigurine.m_ID)
		} m_ttFigurine;

		// IT_FOOD_RAW
		// IT_MEAT_RAW
		struct
		{
			CSphereUIDBase m_cook_id;		// TDATA1=Cooks into this. (ITEMID_TYPE)
			CSphereUIDBase m_MeatType;	// TDATA2=Type of meat this is from ? (CREID_TYPE)
			int		m_CookSkill;			// TDATA3 = how hard to cook this ? 0-100
		} m_ttFoodRaw;

		// IT_MUSICAL
		struct
		{
			int m_iSoundGood;	// TDATA1= SOUND_TYPE if played well.
			int m_iSoundBad;	// TDATA2=sound if played poorly.
		} m_ttMusical;

		// IT_ORE
		struct
		{
			ITEMID_TYPE m_IngotID;	// tdata1= what ingot is this to be made into.
		} m_ttOre;

		// IT_INGOT
		struct
		{
			int m_iSkillMin;	// tdata1= what is the lowest skill
			int m_iSkillMax;	// tdata1= what is the highest skill for max yield
		} m_ttIngot;

		// IT_DOOR
		// IT_DOOR_OPEN
		struct
		{
			int m_iSoundOpen;	// tdata1= sound to open. SOUND_TYPE
			int m_iSoundClose;	// tdata2= Sound on Close. SOUND_TYPE
		} m_ttDoor;

		// IT_GAME_PIECE
		struct
		{
			int m_iStartPosX;	// tdata1=
			int m_iStartPosY;	// tdata2=
		} m_ttGamePiece;

		// IT_BED
		struct
		{
			int m_iDir;		// tdata1= direction of the bed. DIR_TYPE
		} m_ttBed;

		// IT_FOLIAGE - is not consumed on reap (unless eaten then will regrow invis)
		// IT_CROPS	- is consumed and will regrow invis.
		struct
		{
			ITEMID_TYPE m_idReset;	// tdata1= what will it be reset to regrow from ? 0=nothing
			ITEMID_TYPE m_idGrow;	// tdata2= what will it grow further into ? 0=fully mature.
			ITEMID_TYPE m_idFruit;	// tdata3= what can it be reaped for ? 0=immature can't be reaped
		} m_ttCrops;

		// IT_SEED
		// IT_FRUIT
		struct
		{
			ITEMID_TYPE m_idReset;	// tdata1= what will it be reset to regrow from ? 0=nothing
			ITEMID_TYPE m_idSeed;	// tdata2= what does the seed look like ? Copper coin = default.
		} m_ttFruit;

		// IT_DRINK
		// IT_BOOZE
		// IT_POTION
		// IT_PITCHER
		// IT_WATER_WASH
		struct
		{
			ITEMID_TYPE m_idEmpty;	// tdata1= the empty container. IT_POTION_EMPTY IT_PITCHER_EMPTY
		} m_ttDrink;

		// IT_SHIP_PLANK
		// IT_SHIP_SIDE
		// IT_SHIP_SIDE_LOCKED
		struct
		{
			ITEMID_TYPE m_idState;	// tdata1= next state open/close for the Ship Side
		} m_ttShipPlank;
	};

protected:
	DECLARE_MEM_DYNAMIC;
private:
	WORD	m_weight;	// weight in WEIGHT_UNITS (USHRT_MAX=not movable) defaults from the .MUL file.
	IT_TYPE	m_type;		// default double click action type. (if any)
	DWORD   m_dwFlags;	// UFLAG4_DOOR from CUOItemTypeRec
	BYTE    m_layer;	// Is this item equippable on paperdoll? LAYER_TYPE defaults from the .MUL file.

	// What might the item be worth? given a m_wQuality in CItemVendable
	CValueRangeInt m_values;	// range of 2 values given a quality skill from 0 to 100

	// Direction change, part of multi pic, depicted stack sizes.
	// NOTE: Items that are directionally flippable, should be clockwise sorted DIR_N,DIR_NE,...

	CGTypedArray<ITEMID_TYPE,ITEMID_TYPE> m_flip_id;	// can be flipped to make these display ids. CItemDefDupe
};

typedef CRefPtr<CItemDef> CItemDefPtr;

class CItemDefDupe : public CResourceDef
{
	// This item number is just a dupe or a flipped representation of another item.
	// RES_ItemDef
protected:
	DECLARE_MEM_DYNAMIC;
private:
	CItemDefPtr m_MasterItem;	// What is the "master" item ?
public:
	CItemDefPtr GetItemDef() const
	{
		ASSERT(m_MasterItem.GetRefObj());
		return( m_MasterItem );
	}
	virtual void UnLink()
	{
		m_MasterItem.ReleaseRefObj();
		CResourceDef::UnLink();
	}

	CItemDefDupe( ITEMID_TYPE id, CItemDef* pMasterItem ) :
		CResourceDef( CSphereUID( RES_ItemDef, id )),
		m_MasterItem( pMasterItem )
	{
		ASSERT(pMasterItem);
		ASSERT( pMasterItem->GetResourceID().GetResIndex() != id );
	}
	virtual	~CItemDefDupe()
	{
	}
};

class CItemDefWeapon : public CItemDef
{
	// Equippable armor, clothing or weapon, etc
	// Type IsTypeArmor() or IsTypeWeapon()

public:
	CItemDefWeapon( CItemDef* pBase );
	virtual	~CItemDefWeapon()
	{
	}

	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );

	BYTE GetSpeedAttack() const
	{
		return m_speedAttack;
	}

public:
	// This would normally be based on weight of the item.
	// NOTE: This means nothing for armor of course.
	BYTE m_speedAttack;	// Speed of the attack. Odd number (80=fast,20=slow)

	// Ability to dish out (or take) damage. 
	CValueRangeByte m_damage;

protected:
	void CopyBasic( const CItemDefWeapon* pWeapon )
	{
		m_speedAttack = pWeapon->m_speedAttack;
		m_damage = pWeapon->m_damage;
		CItemDef::CopyBasic( pWeapon );
	}

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CITEMDEFWEAPONPROP(a,b,c)	P_##a,
#include "CItemDefWeaponProps.tbl"
#undef CITEMDEFWEAPONPROP
		P_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];

	// This could modify stats/skills/spells by being equipped.
	// CResourceQtyArray m_StatMods;
};

typedef CRefPtr<CItemDefWeapon> CItemDefWeaponPtr;

class CItemDefMulti : public CItemDef
{
	// This item is really a multi with other items associated.
	// define the list of objects it is made of.
	// NOTE: It does not have to be a true multi item ITEMID_MULTI
	// Type IsTypeMulti()

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CITEMDEFMULTIPROP(a,b,c) P_##a,
#include "citemdefmultiprops.tbl"
#undef CITEMDEFMULTIPROP
		P_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];
public:
	// Extra components that are not part of the standard MUL.
	CGTypedArray<CMulMultiItemRec,CMulMultiItemRec&> m_Components;
	CGRect m_rect;			// my region.
	DWORD m_dwRegionFlags;	// Base region flags (REGION_FLAG_GUARDED etc)
	CResourceRefArray m_Speech;	// Speech fragment list (other stuff we know)

public:
	int GetMaxDist() const;

	HRESULT AddComponent( ITEMID_TYPE id, PNT_X_TYPE dx, PNT_Y_TYPE dy, PNT_Z_TYPE dz );
	HRESULT AddComponent( CGVariant& vVal );
	HRESULT SetMultiRegion( CGVariant& vVal );

	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );

	CItemDefMulti( CItemDef* pBase );
	virtual ~CItemDefMulti()
	{
	}
};

typedef CRefPtr<CItemDefMulti> CItemDefMultiPtr;

inline bool CItemDef::IsVisibleLayer( LAYER_TYPE layer ) // static
{
	return( LAYER_IS_VISIBLE( layer ));
}

inline bool CItemDef::IsValidDispID( ITEMID_TYPE id ) // static
{
	// Is this id in the base artwork set ? tile or multi.
	return( id > ITEMID_NOTHING && id < ITEMID_MULTI_MAX );
}

typedef DWORD RACECLASS_TYPE;

struct CRaceBodyPartType	// for BODYPART_TYPE
{
	// for a particular body part
	WORD m_wCoveragePercent;		// Percentage of body area
	const LAYER_TYPE* m_pLayers;	// Corresponding armor layers (if any)
};

class CRaceClassDef : public CResourceDef
{
	// RES_RaceClass
	// Define a general race type.
	// This can be used to limit stat and skill advancement as well.
	// distribution of body parts ? CRaceArmorLayerType
public:
	CRaceClassDef( CSphereUID id );
	virtual ~CRaceClassDef()
	{
	}

	DWORD GetBodyParts() const
	{
		// Mask of body parts.
		return m_dwBodyPartFlags;	// What body parts do we have ? BODYPART_TYPE
	}
	void SetBodyParts( DWORD dwBodyParts )
	{
		m_dwBodyPartFlags = dwBodyParts;
	}

	const CRaceBodyPartType* GetBodyPart( BODYPART_TYPE part ) const;
	void SetBodyPart( BODYPART_TYPE part, WORD wPercentArea, bool fCanHaveArmor );

	int GetRegenRate( STAT_TYPE iType ) const	// STAT_TYPE
	{
		ASSERT( (UINT) iType < COUNTOF(m_iRegenRate));
		return m_iRegenRate[iType];
	}
	bool SetRegenRate( STAT_TYPE iType, int iVal )
	{
		if ( (UINT) iType >= COUNTOF(m_iRegenRate))
			return( false );
		m_iRegenRate[iType] = iVal;
		return( true );
	}

	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet, CScriptConsole* pSrc ); // Execute command from script

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CRACECLASSPROP(a,b,c) P_##a,
#include "craceclassprops.tbl"
#undef CRACECLASSPROP
		P_QTY,
	};
	enum M_TYPE_
	{
#define CRACECLASSMETHOD(a,b,c) M_##a,
#include "CRaceClassMethods.tbl"
#undef CRACECLASSMETHOD
		M_QTY,
	};
	static const CScriptProp sm_Props[P_QTY+1];
	static const CScriptMethod sm_Methods[M_QTY+1];
#ifdef USE_JSCRIPT
#define CRACECLASSMETHOD(a,b,c) JSCRIPT_METHOD_DEF(a)
#include "CRaceClassMethods.tbl"
#undef CRACECLASSMETHOD
#endif

private:
	DWORD m_dwBodyPartFlags;	// What body parts do we have ? BODYPART_TYPE
	int   m_iRegenRate[STAT_QTY];	// rates of regen for stats TICKS_PER_SEC
};

class CCharEvents : public CResourceTriggered
{
	// RES_Events
	// Handler for char type events.
	// NOTE: May be used as stub for RES_CharDef
public:
	CCharEvents( UID_INDEX rid ) : CResourceTriggered( rid )
	{
	}
public:
	CSCRIPT_CLASS_DEF1();
};

class CCharDef : public CObjBaseDef // define basic info about each "TYPE" of monster/creature.
{
	// RES_CharDef
protected:
	DECLARE_MEM_DYNAMIC;
private:
	CRaceClassPtr m_pRaceGroup;		// My general racial group.
	CRaceClassPtr m_pRaceClass;		// My specific race.
public:
	int	m_iResourceLevel;		// Required resource level for client to see this. m_iClientResourceLevel
	CREID_TYPE  m_ResDispId;	// Alternate artwork for old clients.

	ITEMID_TYPE m_trackID;	// ITEMID_TYPE what look like on tracking.
	SOUND_TYPE m_soundbase;	// sounds ( typically 5 sounds per creature, humans and birds have more.)

	CResourceQtyArray m_FoodType; // FOODTYPE=MEAT 15 (3) What would I eat ?

	// ??? 3 possible attack types ??? bite, claw, butt, breath, etc...

	DWORD m_Anims;	// Bitmask of animations available for monsters. ANIM_TYPE from MUL file.
	HUE_TYPE m_wBloodHue;	// when damaged , what color is the blood (-1) = no blood
	ITEMID_TYPE m_MountID;	// Can creature be mounted?

	// Basic capabilities of the creature. (without additional armor, magic or weapons)
	CValueRangeByte m_armor;	// base defense. (basic to body type) can be modified by armor.
	CValueRangeByte m_attack;	// intrinsic attack. (claws, bite, etc) other attacks ?
	STAT_LEVEL m_MaxFood;	// Derived from foodtype...this is the max amount of food we can eat. (based on str ?)
	STAT_LEVEL m_Str;	// Base Str for type. (in case of polymorph)
	STAT_LEVEL m_Dex;

	// TAG.HIREDAYWAGE=x
	CVarDefArray m_TagDefs;	// attach extra tags here. (not applicable to all ???)

public:
	// NPC stuff.
	// SHELTER=FORESTS (P), MOUNTAINS (P)
	// AVERSIONS=TRAPS, CIVILIZATION
	CResourceQtyArray m_Desires;	// DESIRES= that are typical for the char class. see also m_sNeed

	// If this is an NPC.
	// We respond to what we here with this.
	CResourceRefArray m_Speech;	// Speech fragment list (other stuff we know)

	// When scripted events happen to the char. check here for reaction.
	CResourceRefArray m_Events;	// Action or motivation type indexes. (NPC only)

public:
	CSCRIPT_CLASS_DEF1();
	enum P_TYPE_
	{
#define CCHARDEFPROP(a,b,c) P_##a,
#include "cchardefprops.tbl"
#undef CCHARDEFPROP
		P_QTY,
	};
	enum T_TYPE_
	{
#define CCHAREVENT(a,b,c) T_##a,
#define CITEMEVENT(a,b,c) T_item##a,
#define CSKILLDEFEVENT(a,b,c) T_Skill##a,
		CCHAREVENT(AAAUNUSED,CSCRIPTPROP_UNUSED,NULL)
#include "ccharevents.tbl"
#undef CCHAREVENT	
#undef CITEMEVENT
#undef CSKILLDEFEVENT
		T_QTY,				// 
	};
	static const CScriptProp sm_Props[P_QTY+1];
	static const CScriptProp sm_Triggers[T_QTY+1];

private:
	void SetFoodType( LPCTSTR pszFood );
	void CopyBasic( const CCharDef* pCharDef );

public:
	virtual void UnLink();

	CREID_TYPE GetID() const
	{
		return((CREID_TYPE) GetResourceID().GetResIndex());
	}
	CREID_TYPE GetDispID() const
	{
		return((CREID_TYPE) m_wDispIndex );
	}
	bool SetDispID( CREID_TYPE id );

	CRaceClassPtr GetRace() const
	{
		return m_pRaceClass;
	}

	static CCharDefPtr TranslateBase( CResourceDef* pBaseStub );
	static bool IsValidDispID( CREID_TYPE id );
	static bool IsHumanID( CREID_TYPE id );

	bool IsFemale() const
	{
		return( Can(CAN_C_FEMALE));
	}

	LPCTSTR GetTradeName() const;

	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vVal, CScriptConsole* pSrc );
	virtual bool s_LoadProps( CScript& s );

	CCharDef( CREID_TYPE id );
	~CCharDef() {}
};

inline bool CCharDef::IsValidDispID( CREID_TYPE id ) //  static
{
	return( id > 0 && id < CREID_QTY );
}

inline bool CCharDef::IsHumanID( CREID_TYPE id ) // static
{
	return( id == CREID_MAN || id == CREID_WOMAN || id == CREID_EQUIP_GM_ROBE || id == CREID_SAVAGE_MALE || id == CREID_SAVAGE_FEMALE );
}

#endif
