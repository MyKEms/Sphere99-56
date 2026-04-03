#ifndef _INC_CARRAY_H
#define _INC_CARRAY_H

#include <cstring>

//*************************************************

class CGObList;		// forward declaration

class CMemDynamic
{
	// This item will always be dynamically allocated with new/delete!
	// Never stack or data seg based.

#if defined(_DEBUG) || defined(DEBUG)

#define DECLARE_MEM_DYNAMIC
#define COBJBASE_SIGNATURE  0xDEADBEEF      // used just to make sure this is valid.
private:
	DWORD m_dwSignature;

public:
	bool IsValidDynamic() const
	{
		if (m_dwSignature != COBJBASE_SIGNATURE)
		{
			return(false);
		}
#ifdef GRAY_SVR
		// return( DEBUG_ValidateAlloc( GetTopPtr()) ? true : false );
		return(true);
#else
		return(true);
#endif      // GRAY_SVR
	}
	CMemDynamic()
	{
		// NOTE: virtuals don't work in constructors or destructors !
		m_dwSignature = COBJBASE_SIGNATURE;
		// ASSERT( IsValidDynamic());
	}
	virtual ~CMemDynamic()
	{
		ASSERT(IsValidDynamic());
		m_dwSignature = 0;
	}
	bool IsValidHeap() const { return IsValidDynamic(); }

#else       // _DEBUG

#define DECLARE_MEM_DYNAMIC
public:
	bool IsValidDynamic() const
	{
		return(true);
	}
	virtual ~CMemDynamic()  // always virtual so we can always use dynamic_cast correctly.
	{
	}
	bool IsValidHeap() const { return true; }

#endif      // _DEBUG
};

//*************************************************
// CGObList

class CGObListRec : public CMemDynamic      // generic list record base class. 
{
	// This item belongs to JUST ONE LIST
	friend class CGObList;
private:
	CGObList* m_pParent;              // link me back to my parent object.
	CGObListRec* m_pNext;
	CGObListRec* m_pPrev;
public:
	CGObList* GetParent() const { return(m_pParent); }
	CGObListRec* GetNext() const { return(m_pNext); }
	CGObListRec* GetPrev() const { return(m_pPrev); }
public:
	CGObListRec()
	{
		m_pParent = NULL;       // not linked yet.
		m_pNext = NULL;
		m_pPrev = NULL;
	}
	void RemoveSelf();      // remove myself from my parent list.
	virtual ~CGObListRec()
	{
		RemoveSelf();
	}
};

class CGObList      // generic list of objects based on CGObListRec.
{
	friend class CGObListRec;
private:
	CGObListRec* m_pHead;
	CGObListRec* m_pTail;  // Do we really care about tail ? (as it applies to lists anyhow)
	int m_iCount;
private:
	void RemoveAtSpecial(CGObListRec* pObRec)
	{
		// only called by pObRec->RemoveSelf()
		OnRemoveOb(pObRec);   // call any approriate virtuals.
	}
protected:
	// Override this to get called when an item is removed from this list.
	// Never called directly. call pObRec->RemoveSelf()
	virtual void OnRemoveOb(CGObListRec* pObRec)
	{
		// just remove from list. DON'T delete !
		if (pObRec == NULL) return;

		CGObListRec* pNext = pObRec->GetNext();
		CGObListRec* pPrev = pObRec->GetPrev();

		if (pNext != NULL)
			pNext->m_pPrev = pPrev;
		else
			m_pTail = pPrev;
		if (pPrev != NULL)
			pPrev->m_pNext = pNext;
		else
			m_pHead = pNext;

		pObRec->m_pNext = NULL;
		pObRec->m_pPrev = NULL;
		pObRec->m_pParent = NULL;
		m_iCount--;
	}
public:
	bool IsMyChild(const CGObListRec* pElement) const
	{
		if (pElement == NULL) return false;
		return pElement->GetParent() == this;
	}
	CGObListRec* GetAt(int index) const
	{
		CGObListRec* pRec = GetHead();
		while (index-- > 0 && pRec != NULL)
		{
			pRec = pRec->GetNext();
		}
		return pRec;
	}
	// pPrev = NULL = first
	virtual void InsertAfter(CGObListRec* pNewRec, CGObListRec* pPrev = NULL)
	{
		if (pNewRec == NULL) return;
		pNewRec->RemoveSelf();
		if (pPrev == pNewRec) return;

		pNewRec->m_pParent = this;

		CGObListRec* pNext;
		if (pPrev != NULL)
		{
			pNext = pPrev->GetNext();
			pPrev->m_pNext = pNewRec;
		}
		else
		{
			pNext = GetHead();
			m_pHead = pNewRec;
		}

		pNewRec->m_pPrev = pPrev;

		if (pNext != NULL)
		{
			pNext->m_pPrev = pNewRec;
		}
		else
		{
			m_pTail = pNewRec;
		}

		pNewRec->m_pNext = pNext;
		m_iCount++;
	}
	void InsertBefore(CGObListRec* pNewRec, CGObListRec* pNext)
	{
		// pPrev = NULL = last
		InsertAfter(pNewRec, (pNext) ? (pNext->GetPrev()) : GetTail());
	}
	void InsertHead(CGObListRec* pNewRec)
	{
		InsertAfter(pNewRec, NULL);
	}
	void InsertTail(CGObListRec* pNewRec)
	{
		InsertAfter(pNewRec, GetTail());
	}
	void DeleteAll()
	{
		for (;;)
		{
			CGObListRec* pRec = GetHead();
			if (pRec == NULL) break;
			delete pRec;
		}
		m_iCount = 0;
		m_pHead = NULL;
		m_pTail = NULL;
	}
	void Empty() { DeleteAll(); }
	CGObListRec* GetHead(void) const { return(m_pHead); }
	CGObListRec* GetTail(void) const { return(m_pTail); }
	int GetCount() const { return(m_iCount); }
	bool IsEmpty() const
	{
		return(!GetCount());
	}
	CGObList()
	{
		m_pHead = NULL;
		m_pTail = NULL;
		m_iCount = 0;
	}
	virtual ~CGObList()
	{
		DeleteAll();
	}
};

inline void CGObListRec::RemoveSelf()       // remove myself from my parent list.
{
	// Remove myself from my parent list (if i have one)
	if (GetParent() == NULL)
		return;
	m_pParent->RemoveAtSpecial(this);
	ASSERT(GetParent() == NULL);
}

template<class TYPE>
class CGObListType : public CGObList
{
public:
	CRefPtr<TYPE>& GetHead() const
	{
		// Return a reference to a static CRefPtr wrapping the head cast to TYPE*
		static CRefPtr<TYPE> s_ptr;
		s_ptr = static_cast<TYPE*>(CGObList::GetHead());
		return s_ptr;
	}
};

///////////////////////////////////////////////////////////
// CGTypedArray<class TYPE, class ARG_TYPE>

/**
* @brief Typed Array.
*
* NOTE: This will not call true constructors or destructors !
* TODO: Really needed two types in template?
*/
template<class TYPE, class ARG_TYPE>
class CGTypedArray
{
private:
	TYPE* m_pData;	///< Pointer to allocated mem.
	size_t m_nCount;	///< count of elements stored.
	size_t m_nRealCount;	///< Size of allocated mem.

public:
	static const char* m_sClassName;
	/**
	* @brief Initializes array.
	*
	* Sets m_pData to NULL and counters to zero.
	*/
	CGTypedArray()
	{
		m_pData = NULL;
		m_nCount = 0;
		m_nRealCount = 0;
	}
	virtual ~CGTypedArray()
	{
		SetCount(0);
	}
	const CGTypedArray<TYPE, ARG_TYPE>& operator=(const CGTypedArray<TYPE, ARG_TYPE>& array)
	{
		Copy(&array);
		return *this;
	}
private:
public:
	CGTypedArray<TYPE, ARG_TYPE>(const CGTypedArray<TYPE, ARG_TYPE>& copy)
		: m_pData(NULL), m_nCount(0), m_nRealCount(0)
	{
		Copy(&copy);
	}

	void CopyArray(const CGTypedArray& arr) { Copy(&arr); }

	TYPE* GetBasePtr() const { return m_pData; }

	size_t GetCount() const { return m_nCount; }

	int GetSize() const { return m_nCount; }

	size_t GetRealCount() const { return m_nRealCount; }

	bool IsValidIndex(size_t i) const { return (i < m_nCount); }

	void SetCount(size_t nNewCount)
	{
		if (nNewCount == 0)
		{
			if (m_nCount > 0)
			{
				DestructElements(m_pData, m_nCount);
				delete[] reinterpret_cast<BYTE*>(m_pData);
				m_nCount = m_nRealCount = 0;
				m_pData = NULL;
			}
			return;
		}
		if (nNewCount > m_nCount)
		{
			TYPE* pNewData = reinterpret_cast<TYPE*>(new BYTE[nNewCount * sizeof(TYPE)]);
			if (m_nCount)
			{
				memcpy(static_cast<void*>(pNewData), m_pData, sizeof(TYPE) * m_nCount);
				delete[] reinterpret_cast<BYTE*>(m_pData);
			}
			ConstructElements(pNewData + m_nCount, nNewCount - m_nCount);
			m_pData = pNewData;
			m_nRealCount = nNewCount;
		}
		m_nCount = nNewCount;
	}

	void RemoveAll() { SetCount(0); }

	void Empty() { RemoveAll(); }

	void SetAt(size_t nIndex, ARG_TYPE newElement)
	{
		if (!IsValidIndex(nIndex)) return;
		DestructElements(&m_pData[nIndex], 1);
		m_pData[nIndex] = newElement;
	}

	void SetAtGrow(size_t nIndex, ARG_TYPE newElement)
	{
		if (nIndex >= m_nCount)
			SetCount(nIndex + 1);
		SetAt(nIndex, newElement);
	}

	void InsertAt(size_t nIndex, ARG_TYPE newElement)
	{
		SetCount((nIndex >= m_nCount) ? (nIndex + 1) : (m_nCount + 1));
		memmove(static_cast<void*>(&m_pData[nIndex + 1]), &m_pData[nIndex], sizeof(TYPE) * (m_nCount - nIndex - 1));
		m_pData[nIndex] = newElement;
	}

	size_t Add(ARG_TYPE newElement)
	{
		SetAtGrow(GetCount(), newElement);
		return (m_nCount - 1);
	}

	void RemoveAt(size_t nIndex)
	{
		if (!IsValidIndex(nIndex)) return;
		DestructElements(&m_pData[nIndex], 1);
		memmove(static_cast<void*>(&m_pData[nIndex]), &m_pData[nIndex + 1], sizeof(TYPE) * (m_nCount - nIndex - 1));
		SetCount(m_nCount - 1);
	}

	TYPE GetAt(size_t nIndex) const
	{
		if (!IsValidIndex(nIndex))
			return *reinterpret_cast<TYPE*>(BadIndex());
		return m_pData[nIndex];
	}

	TYPE operator[](size_t nIndex) const { return GetAt(nIndex); }

	TYPE& ElementAt(size_t nIndex)
	{
		if (!IsValidIndex(nIndex))
			return *reinterpret_cast<TYPE*>(BadIndex());
		return m_pData[nIndex];
	}

	TYPE& ConstElementAt(size_t nIndex) const
	{
		return const_cast<CGTypedArray*>(this)->ElementAt(nIndex);
	}

	TYPE& operator[](size_t nIndex) { return ElementAt(nIndex); }

	const TYPE& ElementAt(size_t nIndex) const
	{
		if (!IsValidIndex(nIndex))
			return *reinterpret_cast<const TYPE*>(BadIndex());
		return m_pData[nIndex];
	}

	virtual void ConstructElements(TYPE* pElements, size_t nCount)
	{
		memset(static_cast<void*>(pElements), 0, nCount * sizeof(TYPE));
	}

	virtual void DestructElements(TYPE* pElements, size_t nCount)
	{
		(void)pElements;
		(void)nCount;
	}

	void Copy(const CGTypedArray<TYPE, ARG_TYPE>* pArray)
	{
		if (!pArray || pArray == this) return;
		Empty();
		SetCount(pArray->GetCount());
		memcpy(static_cast<void*>(GetBasePtr()), pArray->GetBasePtr(), GetCount() * sizeof(TYPE));
	}
public:
	inline int BadIndex() const { return 0; }
};

/**
* @brief An Array of pointers.
*/
template<class TYPE>
class CGRefArray : public CGTypedArray<TYPE*, TYPE*>
{
protected:
	virtual void DestructElements(TYPE** pElements, size_t nCount)
	{
		memset(static_cast<void*>(pElements), 0, nCount * sizeof(*pElements));
	}
public:
	static const char* m_sClassName;

	size_t FindPtr(TYPE* pData) const
	{
		if (!pData) return this->BadIndex();
		for (size_t nIndex = 0; nIndex < this->GetCount(); nIndex++)
		{
			if (this->GetAt(nIndex) == pData)
				return nIndex;
		}
		return this->BadIndex();
	}

	bool ContainsPtr(TYPE* pData) const
	{
		size_t nIndex = FindPtr(pData);
		return nIndex != (size_t)this->BadIndex();
	}

	bool RemovePtr(TYPE* pData)
	{
		size_t nIndex = FindPtr(pData);
		if (nIndex == (size_t)this->BadIndex())
			return false;
		this->RemoveAt(nIndex);
		return true;
	}

	bool IsValidIndex(size_t i) const
	{
		if (i >= this->GetCount())
			return false;
		return (this->GetAt(i) != NULL);
	}

	TYPE* operator[](size_t nIndex) const
	{
		if (nIndex >= this->GetCount())
			return NULL;
		return this->GetBasePtr()[nIndex];
	}
	TYPE*& ConstElementAt(size_t nIndex) const
	{
		return const_cast<CGRefArray*>(this)->CGTypedArray<TYPE*, TYPE*>::ElementAt(nIndex);
	}
public:
	CGRefArray() { };
	virtual ~CGRefArray() { };
private:
	CGRefArray<TYPE>(const CGRefArray<TYPE>& copy);
	CGRefArray<TYPE>& operator=(const CGRefArray<TYPE>& other);
};

//*************************************************
// CGObArray

template<class TYPE>
class CGObArray : public CGRefArray<TYPE>
{
	// The point of this type is that the array now OWNS the element.
	// It will get deleted when the array is deleted.
protected:
	virtual void DestructElements(TYPE** pElements, size_t nCount)
	{
		// delete the objects that we own.
		for (size_t i = 0; i < nCount; i++)
		{
			TYPE* pDestruct = pElements[i];
			if (pDestruct == NULL)
				continue;
			pElements[i] = NULL;
			delete pDestruct;
		}
		CGRefArray<TYPE>::DestructElements(pElements, nCount);
	}
public:
	bool DeleteOb(TYPE* pData)
	{
		return(this->RemovePtr(pData));
	}
	void DeleteAt(int nIndex)
	{
		this->RemoveAt(nIndex);
	}
	TYPE* UnLinkIndex(int index)
	{
		TYPE* data = this->GetAt(index);
		this->ElementAt(index) = NULL;
		this->RemoveAt(index);
		return(data);
	}
	~CGObArray()
	{
		this->SetCount(0);
	}
};


//*************************************************
// CGSortedArray = A sorted array of objects.

template<class TYPE, class ARG_TYPE, class KEY_TYPE>
struct CGSortedArray : public CGTypedArray<TYPE, ARG_TYPE>
{
	int FindKeyNear(KEY_TYPE key, int& iCompareRes) const
	{
		int iHigh = this->GetCount() - 1;
		if (iHigh < 0)
		{
			iCompareRes = -1;
			return(0);
		}

		int iLow = 0;
		int i = 0;
		while (iLow <= iHigh)
		{
			i = (iHigh + iLow) / 2;
			iCompareRes = CompareKey(key, this->GetAt(i));
			if (iCompareRes == 0)
				break;
			if (iCompareRes > 0)
			{
				iLow = i + 1;
			}
			else
			{
				iHigh = i - 1;
			}
		}
		return(i);
	}
	int FindKey(KEY_TYPE key) const
	{
		int iCompareRes;
		int index = FindKeyNear(key, iCompareRes);
		if (iCompareRes)
			return(-1);
		return(index);
	}
	int AddPresorted(int index, int iCompareRes, TYPE pNew)
	{
		if (iCompareRes > 0)
		{
			index++;
		}
		this->InsertAt(index, pNew);
		return(index);
	}
	int AddSortKey(TYPE pNew, KEY_TYPE key)
	{
		int iCompareRes;
		int index = FindKeyNear(key, iCompareRes);
		if (!iCompareRes)
		{
			this->SetAt(index, pNew);
			return(-1);
		}
		return AddPresorted(index, iCompareRes, pNew);
	}

	void DeleteKey(KEY_TYPE key)
	{
		this->RemoveAt(FindKey(key));
	}
#ifdef _DEBUG
	bool TestSort() const;
#endif
protected:
	virtual int CompareKey(KEY_TYPE, ARG_TYPE) const = 0;
};

template<class TYPE>
struct CHashArray : public CGSortedArray< TYPE*, TYPE*, HASH_INDEX>
{
	virtual int CompareKey(HASH_INDEX index, TYPE* obj) const
	{
		HASH_INDEX h = obj ? (HASH_INDEX)obj->GetHashCode() : (HASH_INDEX)0;
		if (index == h) return 0;
		return (index > h) ? 1 : -1;
	}
	void UnLinkArg(const TYPE* pObj)
	{
		for (int i = 0; i < (int)this->GetCount(); i++)
		{
			if (this->CGTypedArray<TYPE*,TYPE*>::GetAt(i) == pObj)
			{
				this->RemoveAt(i);
				return;
			}
		}
	}
	TYPE* GetAt(int index) const
	{
		return this->CGTypedArray<TYPE*,TYPE*>::GetAt(index);
	}
	TYPE* GetAt(HASH_INDEX key, int index) const
	{
		return this->CGTypedArray<TYPE*,TYPE*>::GetAt(index);
	}
	TYPE* GetAtArray(int i, int j) const
	{
		return this->CGTypedArray<TYPE*,TYPE*>::GetAt(j);
	}
	TYPE*& ElementAt(int index)
	{
		return this->CGTypedArray<TYPE*,TYPE*>::ElementAt(index);
	}
	HASH_INDEX FindKeyFree(HASH_INDEX startKey) const
	{
		return startKey; // STUB
	}
};

struct CGStringArray : public CGTypedArray<CGString, const CGString&>
{
public:
	void AddFormat(LPCTSTR lpszFormat, ...)
	{
		TCHAR szBuf[1024];
		va_list vargs;
		va_start(vargs, lpszFormat);
		vsprintf(szBuf, lpszFormat, vargs);
		va_end(vargs);
		Add(szBuf);
	}
};

struct CStringSortArray : public CGStringArray
{
public:
	void AddSortString(LPCTSTR pszStr);
	int FindKey(LPCTSTR pszKey) const { return -1; } // STUB
};

template<class TYPE, class KEY_TYPE>
struct CGRefSortArray : public CGSortedArray<TYPE*, TYPE*, KEY_TYPE>
{
public:
	void QSort() { /* STUB */ }
	bool RemovePtr(TYPE* pData)
	{
		for (size_t i = 0; i < this->GetCount(); i++)
		{
			if (this->GetAt(i) == pData)
			{
				this->RemoveAt(i);
				return true;
			}
		}
		return false;
	}
};

template<class TYPE, class ARG_TYPE>
struct CGPtrSortArray : public CGTypedArray<TYPE, ARG_TYPE>
{
public:
	virtual int CompareData( TYPE pLeft, TYPE pRight ) const { return 0; }
	void QSort() { /* STUB */ }
};

#endif // _INC_CARRAY_H
