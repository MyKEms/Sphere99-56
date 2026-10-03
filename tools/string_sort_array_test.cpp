#include "SphereSvr/stdafx.h"

#include <cstdio>

static bool Expect(bool condition, const char* description)
{
	if (condition)
		return true;
	std::fprintf(stderr, "FAIL: %s\n", description);
	return false;
}

static bool TestEmptyLookup()
{
	CStringSortArray values;
	return Expect(values.GetCount() == 0, "new string sort array is empty") &&
		Expect(values.FindKey(nullptr) == -1, "empty array rejects a null key") &&
		Expect(values.FindKey("missing") == -1, "empty array rejects a missing key");
}

static bool TestSortedCaseInsensitiveLookup()
{
	CStringSortArray values;
	values.AddSortString("delta");
	values.AddSortString("ALPHA");
	values.AddSortString("beta");
	values.AddSortString("BETA");
	values.AddSortString("alpha");
	values.AddSortString(nullptr);

	if (!Expect(values.GetCount() == 5, "null entries are ignored"))
		return false;
	for (size_t index = 1; index < values.GetCount(); ++index)
	{
		if (!Expect(values.GetAt(index - 1).CompareNoCase(values.GetAt(index)) <= 0,
			"sort insertion keeps case-insensitive order"))
			return false;
	}

	const int firstAlpha = values.FindKey("alpha");
	if (!Expect(firstAlpha >= 0 && values.GetAt(firstAlpha).CompareNoCase("ALPHA") == 0,
		"lookup finds a case-insensitive exact match"))
		return false;
	if (!Expect(values.FindKey("BETA") >= 0 && values.FindKey("beta") == values.FindKey("BETA"),
		"duplicate keys resolve to the first matching index"))
		return false;
	if (!Expect(values.FindKey("bet") == -1 && values.FindKey("alphabet") == -1,
		"lookup does not accept a prefix match"))
		return false;
	return Expect(values.FindKey("unknown") == -1, "lookup rejects an unknown key");
}

int main()
{
	if (!TestEmptyLookup() || !TestSortedCaseInsensitiveLookup())
		return 1;
	std::puts("string sort array: sorted exact case-insensitive lookup passed");
	return 0;
}
