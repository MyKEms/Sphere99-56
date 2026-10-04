// Offline regression tests for resource value-range parsing and lookup tables.

#include "SphereSvr/stdafx.h"

#include <cstdio>
#include <cstring>

class CContainerLookupProbe : public CContainer
{
public:
	virtual void ContentAdd(CItemPtr pItem)
	{
		(void)pItem;
	}
};

class CSpawnedWeaponProbe : public CItem
{
public:
	CSpawnedWeaponProbe(ITEMID_TYPE id, CItemDef* pItemDef)
		: CItem(id, pItemDef)
	{
	}
};

static bool Expect(bool fCondition, const char* pszDescription)
{
	if ( fCondition )
		return true;
	std::fprintf(stderr, "FAIL: %s\n", pszDescription);
	return false;
}

static bool TestIntegerRanges()
{
	CValueRangeInt range;
	CGVariant serialized;
	range.v_Get(serialized);
	if ( !Expect(serialized.IsVoid(), "invalid integer range serializes as absent"))
		return false;

	CGVariant input;
	input.SetStr("7");
	range.v_Set(input);
	if ( !Expect(range.GetMin() == 7 && range.GetMax() == 7, "integer scalar"))
		return false;
	range.v_Get(serialized);
	if ( !Expect(serialized.GetInt() == 7, "integer scalar serialization"))
		return false;

	input.SetStr("-4,9");
	range.v_Set(input);
	if ( !Expect(range.GetMin() == -4 && range.GetMax() == 9, "integer comma range"))
		return false;

	input.SetStr("{ 3 8 }");
	range.v_Set(input);
	if ( !Expect(range.GetMin() == 3 && range.GetMax() == 8, "integer braced range"))
		return false;

	range.v_Get(serialized);
	if ( !Expect(std::strcmp(serialized.GetPSTR(), "3,8") == 0, "integer range serialization"))
		return false;

	CValueRangeInt roundTrip;
	roundTrip.v_Set(serialized);
	if ( !Expect(roundTrip.GetMin() == 3 && roundTrip.GetMax() == 8, "integer round trip"))
		return false;

	input.SetStr("not-a-range,");
	range.v_Set(input);
	return Expect(range.GetMin() == 3 && range.GetMax() == 8, "malformed integer input preserves previous value");
}

static bool TestByteRanges()
{
	CValueRangeByte range;
	CGVariant input;
	input.SetStr("5,12");
	range.v_Set(input);
	if ( !Expect(range.GetMin() == 5 && range.GetMax() == 12, "byte comma range"))
		return false;

	input.SetStr("{ 0x10 0x20 }");
	range.v_Set(input);
	if ( !Expect(range.GetMin() == 16 && range.GetMax() == 32, "byte braced hexadecimal range"))
		return false;

	CGVariant serialized;
	range.v_Get(serialized);
	if ( !Expect(std::strcmp(serialized.GetPSTR(), "16,32") == 0, "byte range serialization"))
		return false;

	CValueRangeByte roundTrip;
	roundTrip.v_Set(serialized);
	if ( !Expect(roundTrip.GetMin() == 16 && roundTrip.GetMax() == 32, "byte round trip"))
		return false;
	if ( !Expect(roundTrip.GetAvg() == 24, "byte midpoint includes the lower endpoint"))
		return false;

	input.SetStr("-1,300");
	range.v_Set(input);
	return Expect(range.GetMin() == 0 && range.GetMax() == 255, "byte endpoints clamp instead of wrapping");
}

static bool TestPropertyAndLookupDispatch()
{
	CCharDef charDef(static_cast<CREID_TYPE>(0x0190));
	CGVariant input;
	input.SetStr("4,9");
	if ( !Expect(charDef.s_PropSet("ARMOR", input) == NO_ERROR, "CHARDEF ARMOR set"))
		return false;
	CGVariant output;
	if ( !Expect(charDef.s_PropGet("ARMOR", output, NULL) == NO_ERROR &&
		std::strcmp(output.GetPSTR(), "4,9") == 0, "CHARDEF ARMOR get preserves both endpoints"))
		return false;

	CItemDef base(ITEMID_MULTI_MAX);
	CItemDefWeapon itemDef(&base);
	input.SetStr("6,11");
	if ( !Expect(itemDef.s_PropSet("DAM", input) == NO_ERROR, "ITEMDEF DAM set"))
		return false;
	output.SetVoid();
	if ( !Expect(itemDef.s_PropGet("DAM", output, NULL) == NO_ERROR &&
		std::strcmp(output.GetPSTR(), "6,11") == 0, "ITEMDEF DAM get preserves both endpoints"))
		return false;
	CGVariant weaponType;
	weaponType.SetInt(IT_WEAPON_SWORD);
	if ( !Expect(itemDef.s_PropSet("TYPE", weaponType) == NO_ERROR, "synthetic weapon type set"))
		return false;
	input.SetStr("200,202");
	if ( !Expect(itemDef.s_PropSet("DAM", input) == NO_ERROR, "synthetic weapon range set"))
		return false;
	{
		// Instantiate a real CItem against the synthetic definition. The
		// combat accessor must use the definition's range, not its default.
		CSpawnedWeaponProbe spawnedItem(ITEMID_MULTI_MAX, &itemDef);
		const int iAttack = spawnedItem.Weapon_GetAttack(true);
		spawnedItem.RemoveSelf();
		if ( !Expect(iAttack == 201, "spawned weapon uses ITEMDEF DAM range"))
			return false;
	}

	CContainerLookupProbe container;
	if ( !Expect(container.s_FindMyMethodKey("ResCount") >= 0 &&
		container.s_FindMyMethodKey("NoSuchMethod") == -1, "container method table lookup"))
		return false;
	return Expect(g_World.s_FindMyPropKey("Version") >= 0 &&
		g_World.s_FindMyPropKey("NoSuchProperty") == -1, "world property table lookup");
}

static bool TestBoundaryArithmetic()
{
	CRectMap rect;
	rect.m_left = 0;
	rect.m_top = 0;
	rect.m_right = 1;
	rect.m_bottom = 1;
	rect.m_map = 255;
	rect.NormalizeRect();
	if ( !Expect(rect.m_map == 0, "map rectangle rejects the first out-of-range map index"))
		return false;

	CItemDef itemDef(ITEMID_MULTI_MAX);
	CGVariant weight;
	// USHRT_MAX is the non-movable sentinel, so use the largest movable
	// definition weight and exercise the overflowing stack product.
	weight.SetStr("65534.0");
	if ( !Expect(itemDef.s_PropSet("WEIGHT", weight) == NO_ERROR,
		"synthetic maximum item weight is accepted"))
		return false;
	CSpawnedWeaponProbe item(ITEMID_MULTI_MAX, &itemDef);
	item.SetAmount(USHRT_MAX);
	const bool fClamped = item.GetWeight() == INT_MAX;
	item.RemoveSelf();
	return Expect(fClamped, "large stack weight saturates instead of overflowing");
}

static bool TestPointValueParsing()
{
	// Script DEFNAMES commonly store destinations as a quoted,
	// space-separated triplet (for example: "5678 3871 -80").  Keep the
	// comma form used by saved HOME values covered as well.
	CPointMap point( 1, 2, 3, 4 );
	CGVariant input;
	input.SetStr( "\"5678 3871 -80\"" );
	point.v_Set( input );
	if ( !Expect(point.m_x == 5678 && point.m_y == 3871 && point.m_z == -80 &&
		point.m_mapplane == 4, "quoted space-separated point assignment"))
		return false;

	input.SetStr( "2344,2542,0,2" );
	point.v_Set( input );
	return Expect(point.m_x == 2344 && point.m_y == 2542 && point.m_z == 0 &&
		point.m_mapplane == 2, "comma-separated point assignment");
}

// The indexed table must answer every lookup exactly as the plain table
// does, and keep its elements in the same order.
static bool SameTables(const CVarDefArray& plain, const CVarDefIndexedArray& indexed,
	const char* const* ppszKeys, int iKeys, const char* pszStep)
{
	if ( !Expect(plain.GetSize() == indexed.GetSize(), pszStep))
		return false;
	for ( int i = 0; i < (int) plain.GetSize(); i++ )
	{
		if ( !Expect(!strcmp(plain.GetAt(i)->GetKey(), indexed.GetAt(i)->GetKey()) &&
			!strcmp(plain.GetAt(i)->GetValStr(), indexed.GetAt(i)->GetValStr()), pszStep))
			return false;
	}
	for ( int i = 0; i < iKeys; i++ )
	{
		CVarDef* pPlain = plain.FindKeyPtr(ppszKeys[i]);
		CVarDef* pIndexed = indexed.FindKeyPtr(ppszKeys[i]);
		if ( !Expect((pPlain == NULL) == (pIndexed == NULL), pszStep))
			return false;
		if ( pPlain && !Expect(!strcmp(pPlain->GetKey(), pIndexed->GetKey()) &&
			!strcmp(pPlain->GetValStr(), pIndexed->GetValStr()), pszStep))
			return false;
	}
	return true;
}

static bool TestIndexedVariableTable()
{
	static const char* const sm_Keys[] =
	{
		"alpha", "ALPHA", "Beta", "beta", "gamma", "delta", "missing", "", "Alpha_2", "alpha_2",
	};
	const int iKeys = (int) (sizeof(sm_Keys) / sizeof(sm_Keys[0]));

	CVarDefArray plain;
	CVarDefIndexedArray indexed;
	bool fOk = true;
#define BOTH(call) do { plain.call; indexed.call; } while (0)
	BOTH(SetKeyInt("Alpha", 1));
	BOTH(SetKeyStr("beta", "two"));
	BOTH(SetKeyInt("GAMMA", 3));
	BOTH(SetKeyStr("alpha_2", "four"));
	fOk = fOk && SameTables(plain, indexed, sm_Keys, iKeys, "indexed table after inserts");

	// Keys compare without case; a write through another spelling updates
	// the existing entry instead of adding one.
	BOTH(SetKeyInt("ALPHA", 10));
	BOTH(SetKeyStr("BETA", "second"));
	fOk = fOk && Expect(indexed.GetSize() == 4, "case-insensitive write keeps one entry");
	fOk = fOk && SameTables(plain, indexed, sm_Keys, iKeys, "indexed table after updates");

	// A value that changes its type is removed and added again at the end.
	BOTH(SetKeyStr("alpha", "text"));
	BOTH(SetKeyInt("beta", 22));
	fOk = fOk && SameTables(plain, indexed, sm_Keys, iKeys, "indexed table after type changes");

	BOTH(RemoveKey("gamma"));
	BOTH(RemoveKey("missing"));
	fOk = fOk && Expect(indexed.FindKeyPtr("GAMMA") == NULL, "removed key is not found");
	fOk = fOk && SameTables(plain, indexed, sm_Keys, iKeys, "indexed table after key removal");

	// Removal by position, as the definition table is edited.
	for ( int i = 0; i < (int) plain.GetSize(); i++ )
	{
		if ( !_stricmp(plain.GetAt(i)->GetKey(), "alpha_2"))
		{
			CVarDef* pPlain = plain.GetAt(i);
			CVarDef* pIndexed = indexed.GetAt(i);
			plain.RemoveAt(i);
			indexed.RemoveAt(i);
			delete pPlain;
			delete pIndexed;
			break;
		}
	}
	fOk = fOk && Expect(indexed.FindKeyPtr("Alpha_2") == NULL, "entry removed by position is not found");
	fOk = fOk && SameTables(plain, indexed, sm_Keys, iKeys, "indexed table after positional removal");

	BOTH(SetKeyInt("gamma", 33));
	BOTH(SetKeyInt("delta", 4));
	fOk = fOk && SameTables(plain, indexed, sm_Keys, iKeys, "indexed table after re-adding a key");

	// Copying replaces the content and the index together.
	CVarDefIndexedArray copy;
	copy.SetKeyInt("stale", 1);
	CVarDef* pStale = copy.FindKeyPtr("stale");
	copy = plain;
	delete pStale;
	fOk = fOk && Expect(copy.FindKeyPtr("stale") == NULL, "copy drops the previous keys");
	fOk = fOk && SameTables(plain, copy, sm_Keys, iKeys, "indexed copy");

	// Many keys: every one is found, in insertion order, and a miss stays a miss.
	for ( int i = 0; i < 5000; i++ )
	{
		TCHAR szKey[32];
		snprintf(szKey, sizeof(szKey), "Bulk_%d", i);
		BOTH(SetKeyInt(szKey, (DWORD) i));
	}
	for ( int i = 0; i < 5000 && fOk; i += 97 )
	{
		TCHAR szKey[32];
		snprintf(szKey, sizeof(szKey), "bulk_%d", i);
		fOk = Expect((int) indexed.FindKeyInt(szKey) == i && plain.FindKeyPtr(szKey) != NULL,
			"bulk key is found without case");
	}
	fOk = fOk && Expect(indexed.FindKeyPtr("bulk_5000") == NULL, "bulk miss");
	fOk = fOk && SameTables(plain, indexed, sm_Keys, iKeys, "indexed table after bulk inserts");
#undef BOTH

	for ( CVarDefArray* pTable : { &plain, static_cast<CVarDefArray*>(&indexed), static_cast<CVarDefArray*>(&copy) } )
	{
		for ( int i = 0; i < (int) pTable->GetSize(); i++ )
			delete pTable->GetAt(i);
	}
	indexed.RemoveAll();
	return fOk && Expect(indexed.FindKeyPtr("alpha") == NULL && indexed.GetSize() == 0,
		"cleared indexed table is empty");
}

int main()
{
	if ( !TestIntegerRanges() || !TestByteRanges() ||
		!TestPropertyAndLookupDispatch() || !TestBoundaryArithmetic() ||
		!TestPointValueParsing() ||
		!TestIndexedVariableTable() )
		return 1;
	std::printf("value ranges, property lookups, and boundary arithmetic: all checks passed\n");
	return 0;
}
