// ctime.h
// Copyright Menace Software (www.menasoft.com).
//

#ifndef _INC_CTIME_H
#define _INC_CTIME_H

#include <time.h>

#define CTIME_FORMAT_DEFAULT "%Y/%m/%d %H:%M:%S"

#define CServTimeBase CServTime
#define CServTimeMaster CServTime
class CServTime
{
#undef GetCurrentTime
#define TICK_PER_SEC 10
    // A time stamp in the server/game world.
public:
    long m_lPrivateTime;
    DWORD m_dwTickCount;
public:
    long GetTimeRaw() const
    {
        return m_lPrivateTime;
        }
    int GetTimeDiff(const CServTime& time = GetCurrentTime()) const
    {
        return(m_lPrivateTime - time.m_lPrivateTime);
    }
    int GetCacheAge() const
    {
        // How IsOld IsOld IsOld IsOld IsOld IsOld IsOld IsOld Is this IsOld
        return(GetCurrentTime().GetTimeRaw() - m_lPrivateTime);
    }
    void Init()
    {
        m_lPrivateTime = 0;
    }
    void InitTime()
    {
        // Set to the current server time.
        m_lPrivateTime = GetCurrentTime().GetTimeRaw();
        m_dwTickCount = 0;
    }
    void InitTime(long lTimeBase)
    {
        m_lPrivateTime = lTimeBase;
    }
    void InitTimeCurrent()
    {
        // Set to the current server time.
        m_lPrivateTime = GetCurrentTime().GetTimeRaw();
    }
    void InitTimeCurrent(long lTimeBase)
    {
        // Set to the current time plus a time offset.
        m_lPrivateTime = GetCurrentTime().GetTimeRaw() + lTimeBase;
    }
    
    bool IsTimeValid() const
    {
        return(m_lPrivateTime ? true : false);
    }
    CServTime operator+(int iTimeDiff) const
    {
        CServTime time;
        time.m_lPrivateTime = m_lPrivateTime + iTimeDiff;
        return(time);
    }
    CServTime operator-(int iTimeDiff) const
    {
        CServTime time;
        time.m_lPrivateTime = m_lPrivateTime - iTimeDiff;
        return(time);
    }
    int operator-(CServTime time) const
    {
        return(m_lPrivateTime - time.m_lPrivateTime);
    }
    bool operator==(CServTime time) const
    {
        return(m_lPrivateTime == time.m_lPrivateTime);
    }
    bool operator!=(CServTime time) const
    {
        return(m_lPrivateTime != time.m_lPrivateTime);
    }
    bool operator<(CServTime time) const
    {
        return(m_lPrivateTime < time.m_lPrivateTime);
    }
    bool operator>(CServTime time) const
    {
        return(m_lPrivateTime > time.m_lPrivateTime);
    }
    bool operator<=(CServTime time) const
    {
        return(m_lPrivateTime <= time.m_lPrivateTime);
    }
    bool operator>=(CServTime time) const
    {
        return(m_lPrivateTime >= time.m_lPrivateTime);
    }
    bool AdvanceTime();

    static CServTime GetCurrentTime();
};

#ifdef _AFXDLL

struct CGTime : public CTime		// why dupe this ?
{
public:
    bool IsTimeValid() const
    {
        return((GetTime() && GetTime() != -1) ? true : false);
    }
};

#else

class CGTime	// similar to the MFC CTime and CTimeSpan or COleDateTime
{
    // Get time stamp in the real world. based on struct tm
#undef GetCurrentTime
private:
    time_t m_time;
public:

    // Constructors
    static CGTime GetCurrentTime();

    CGTime()
    {
        m_time = 0;
    }
    CGTime(time_t time)
    {
        m_time = time;
    }
    CGTime(const CGTime& timeSrc)
    {
        m_time = timeSrc.m_time;
    }

    CGTime(struct tm time);
    CGTime(int nYear, int nMonth, int nDay, int nHour, int nMin, int nSec,
        int nDST = -1);

    const CGTime& operator=(const CGTime& timeSrc)
    {
        m_time = timeSrc.m_time; return *this;
    }
    const CGTime& operator=(time_t t)
    {
        m_time = t; return *this;
    }

    bool operator<=(time_t t) const
    {
        return(m_time <= t);
    }
    bool operator==(time_t t) const
    {
        return(m_time == t);
    }
    bool operator!=(time_t t) const
    {
        return(m_time != t);
    }

    time_t GetTime() const
    {
        return m_time;
    }

    // Attributes
    struct tm* GetGmtTm(struct tm* ptm = NULL) const;
    struct tm* GetLocalTm(struct tm* ptm = NULL) const;

    int GetYear() const
    {
        return (GetLocalTm(NULL)->tm_year) + 1900;
    }
    int GetMonth() const       // month of year (1 = Jan)
    {
        return GetLocalTm(NULL)->tm_mon + 1;
    }
    int GetDay() const         // day of month
    {
        return GetLocalTm(NULL)->tm_mday;
    }
    int GetTotalDays() const
    {
        // Get total days since epoch. Needs to be more consistent than accurate.
        return GetDaysTotal();
    }
    int GetHour() const
    {
        return GetLocalTm(NULL)->tm_hour;
    }
    int GetMinute() const
    {
        return GetLocalTm(NULL)->tm_min;
    }
    int GetSecond() const
    {
        return GetLocalTm(NULL)->tm_sec;
    }
    int GetDayOfWeek() const   // 1=Sun, 2=Mon, ..., 7=Sat
    {
        return GetLocalTm(NULL)->tm_wday + 1;
    }

    // Operations
        // formatting using "C" strftime
    LPCTSTR Format(LPCTSTR pszFormat) const;
    LPCTSTR FormatGmt(LPCTSTR pszFormat) const;

    // non CTime operations.
    bool Read(TCHAR* pVal);
    void Init()
    {
        m_time = -1;
    }
    void InitTimeCurrent()
    {
        m_time = time(NULL);
    }
    bool IsTimeValid() const
    {
        return((m_time && m_time != -1) ? true : false);
    }
    int GetDaysTotal() const
    {
        // Needs to be more consistant than accurate. just for compares.
        return((GetYear() * 366) + (GetMonth() * 31) + GetDay());
    }

    static int GetTimeZoneOffset()
    {
        // Get the local timezone offset in seconds from UTC.
        time_t now = time(NULL);
        struct tm tmLocal;
        struct tm tmGmt;
        localtime_r(&now, &tmLocal);
        gmtime_r(&now, &tmGmt);
        // Approximate offset in seconds.
        int iOffset = (tmLocal.tm_hour - tmGmt.tm_hour) * 3600
                    + (tmLocal.tm_min - tmGmt.tm_min) * 60;
        // Handle day boundary.
        int iDayDiff = tmLocal.tm_yday - tmGmt.tm_yday;
        if ( iDayDiff > 1 ) iDayDiff = -1;
        else if ( iDayDiff < -1 ) iDayDiff = 1;
        iOffset += iDayDiff * 24 * 3600;
        return iOffset;
    }
};

#endif // _AFXDLL
#endif // _INC_CTIME_H