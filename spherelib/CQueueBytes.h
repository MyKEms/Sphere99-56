#ifndef _INC_CQUEUEBYTES_H
#define _INC_CQUEUEBYTES_H

class CGQueueBytes
{
	// Create an arbitrary queue of data.
	// NOTE: I know this is not a real queue yet, but i'm working on it.
private:
	CMemLenBlock m_Mem;  ///< Data buffer.
	size_t m_iDataQty;  ///< Item count of the data queue.

public:
	CGQueueBytes() : m_iDataQty(0) {}

	// Peak into/read from the Queue's data.
	int GetDataQty() const
	{
		// How much data is avail?
		return m_iDataQty;
	}
	void AddNewData(const BYTE* pBuf, int iLen)
	{
		// Append data to the end of the queue.
		if ( iLen <= 0 || pBuf == NULL ) return;
		BYTE* pDest = AddNewDataLock( iLen );
		memcpy( pDest, pBuf, iLen );
		m_iDataQty += iLen;
	}
	BYTE* AddNewDataLock(int iLen)
	{
		// Get space to write new data after the queued data.
		// RemoveDataAmount() never shrinks the buffer, so it is usually
		// big enough already; reallocate only when it is too small.
		size_t iNeed = m_iDataQty + ( iLen > 0 ? iLen : 0 );
		if ( m_Mem.GetData() == NULL || iNeed > m_Mem.GetDataLength())
		{
			if ( m_iDataQty > 0 && m_Mem.GetData() )
				m_Mem.Resize( iNeed );	// keeps the queued data
			else
				m_Mem.Alloc( iNeed ? iNeed : 1 );
		}
		return m_Mem.GetData() + m_iDataQty;
	}
	void AddNewDataFinish(int iLen)
	{
		m_iDataQty += iLen;
	}
	const BYTE* RemoveDataLock() const
	{
		// Get pointer to the start of the data for reading.
		if ( m_iDataQty <= 0 ) return NULL;
		return m_Mem.GetData();
	}
	void RemoveDataAmount(int iSize)
	{
		// Remove data from the front of the queue.
		if ( iSize <= 0 ) return;
		if ( iSize >= (int)m_iDataQty )
		{
			m_iDataQty = 0;
			return;
		}
		m_iDataQty -= iSize;
		memmove( m_Mem.GetData(), m_Mem.GetData() + iSize, m_iDataQty );
	}
	void Empty()
	{
		m_iDataQty = 0;
	}
};

#endif // _INC_CQUEUEBYTES_H
