//
// SpherePublic.h
// Copyright 1996 - 2001 Menace Software (www.menasoft.com)
// Public interface into the SphereSvr code.

#ifndef _INC_SPHEREPUBLIC_H
#define _INC_SPHEREPUBLIC_H

typedef DWORD SPHERE_UID;

typedef int SPHERE_CLASS_TYPE;

class CSphereObject
{
	// Any object in the SphereWorld. Resource, Item or Char.

private:
	SPHERE_UID m_dwUID;	// The resource UID in Sphere.

public:
	// Every object can be identified by it's UID
	SPHERE_UID GetUID() const;

	// Attach to another UID object.
	void SetUID( SPHERE_UID uid );

	// Get the objects class type.
	SPHERE_CLASS_TYPE GetClassType() const;

	// Every object has a name (visual) and a resource type name
	CString GetResourceName() const;
	CGString GetName() const;

	// Set some attribute.
	virtual HRESULT s_PropSet( LPCTSTR pszKey, CGVariant& vVal );
	// Query some attribute
	virtual HRESULT s_PropGet( LPCTSTR pszKey, CGVariant& vValRet );
	// Engage some sort of method on the object.
	virtual HRESULT s_Method( LPCTSTR pszKey, CGVariant& vArgs, CGVariant& vValRet ); // Execute command from script

	CSphereObject( SPHERE_UID uid = 0 );
	~CSphereObject();
};

class CSphereEvents : public CSphereObject
{
	// NOTE: This event handler IS an object in the Sphere database.

	// Attach this handler to an object.
	// UID will be assigned by the Spheredatabase
	bool Attach( CSphereObject object );

	// Register this handler object with the Sphere database.
	bool Register( const TCHAR* pszResourceName );

	// Detach this handler from all objects and unregister from Sphere database.
	bool Unregister();

	// Getting events on an object ?!
	virtual bool OnTrigger( CSphereObject obj, const TCHAR* pszTrigger, CSphereObject eventcontext );
};

// Map searching mechanisms. (around a point)
class CSphereSearch
{

};

// Iterators and data searching.
class CSphereWorld
{

};

#endif // _INC_SPHEREPUBLIC_H