    #ifndef _INC_CREFOBJ_H
#define _INC_CREFOBJ_H

#define PTR_CAST(a,b) dynamic_cast<a*>(b)
#define STATIC_CAST(a,b) static_cast<a*>(b)

#define CNewPtr CRefPtr
template <class T>
class CRefPtr
{
    template <class U> friend class CRefPtr;
private:
    T* m_pointer;
public:
    CRefPtr() : m_pointer(0) {}
    CRefPtr(T* p) : m_pointer(p) {}
    template <class U>
    CRefPtr(const CRefPtr<U>& rhs) : m_pointer(rhs.m_pointer) {}
    ~CRefPtr()
    {
    }

    void SetRefObj(T* pObj) { m_pointer = pObj; }
    T* GetRefObj() const { return m_pointer; }
    void ReleaseRefObj() {} // STUB
    virtual void UnLink() {} // STUB
    bool IsValidNewObj() const { return true; } // STUB
    bool IsValidRefObj() const { return m_pointer != NULL; }
    void Free() { m_pointer = NULL; }

    T* DetachObj() { T* p = m_pointer; m_pointer = NULL; return p; }

    CRefPtr& operator=(const CRefPtr& rhs)
    {                             // Assignment operator (same type).
        m_pointer = rhs.m_pointer;
        return *this;
    }

    template <class U>
    CRefPtr& operator=(const CRefPtr<U>& rhs)
    {                             // Assignment operator (cross type).
        m_pointer = rhs.m_pointer;
        return *this;
    }

    template <class U>
    bool operator==(const CRefPtr<U>& rhs) const
    {
        return (void*)m_pointer == (void*)rhs.m_pointer;
    }

    template <class U>
    bool operator!=(const CRefPtr<U>& rhs) const
    {
        return (void*)m_pointer != (void*)rhs.m_pointer;
    }

    template <class U>
    CRefPtr& operator=(U* rhs) { m_pointer = static_cast<T*>(rhs); return *this; }

    T& operator*() { return *m_pointer; }
    T* operator->() const { return m_pointer; }
    operator T* () const { return m_pointer; }
    operator bool() const { return m_pointer != 0; }
};

// REF_CAST: cast CRefPtr or raw pointer to target type
template<typename TTo, typename TFrom>
inline TTo* _ref_cast_helper(TFrom* p) { return dynamic_cast<TTo*>(p); }
template<typename TTo, typename TFrom>
inline TTo* _ref_cast_helper(CRefPtr<TFrom> p) { return dynamic_cast<TTo*>(p.GetRefObj()); }
#define REF_CAST(a,b) _ref_cast_helper<a>(b)

#endif // _INC_CREFOBJ_H
