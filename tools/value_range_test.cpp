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

int main()
{
	if ( !TestIntegerRanges() || !TestByteRanges() ||
		!TestPropertyAndLookupDispatch() || !TestBoundaryArithmetic() )
		return 1;
	std::printf("value ranges, property lookups, and boundary arithmetic: all checks passed\n");
	return 0;
}
