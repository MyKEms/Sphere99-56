#include <stddef.h>

#include "../spherelib/crefobj.h"

int main()
{
	CRefPtr<int> empty;
	if ( empty.IsValidNewObj())
		return 1;

	int value = 42;
	CRefPtr<int> present( &value );
	if ( !present.IsValidNewObj())
		return 2;

	present.DetachObj();
	if ( present.IsValidNewObj())
		return 3;

	return 0;
}
