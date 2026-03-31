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
	// Peak into/read from the Queue's data.
	int GetDataQty() const
	{
		// How much data is avail?
		return m_iDataQty;
	}
	void AddNewData(const BYTE* pBuf, int iLen) { throw "not implemented"; }
	BYTE* AddNewDataLock(int iLen) { throw "not implemented"; }
	void AddNewDataFinish(int iLen) { throw "not implemented"; }
	const BYTE* RemoveDataLock() const { throw "not implemented"; }
	void RemoveDataAmount(int iSize) { throw "not implemented"; }
	void Empty() { throw "not implemented"; }
};

#endif // _INC_CQUEUEBYTES_H
