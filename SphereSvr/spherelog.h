// spherelog.h — Configurable debug trace logging for Sphere99
//
// Usage:
//   SPHERE_LOG(level, fmt, ...)  — log at given level
//   SPHERE_LOG_NET(fmt, ...)     — network/login events
//   SPHERE_LOG_LOAD(fmt, ...)    — world loading events
//   SPHERE_LOG_CLIENT(fmt, ...)  — client packet processing
//   SPHERE_LOG_SCRIPT(fmt, ...)  — script execution events
//   SPHERE_LOG_ERR(fmt, ...)     — always logged (errors)
//   SPHERE_LOG_TIMER_PROVENANCE(fmt, ...) — opt-in timer-removal provenance
//
// Levels (set via DEBUGLEVEL= in sphere.ini or -dlevel command line):
//   0 = errors only (production)
//   1 = warnings + key events (login, logout, save)
//   2 = verbose network trace (packets, relay, encryption)
//   3 = full trace (every function entry, script execution)
// Timer-removal markers are separately enabled with TIMERREMOVALPROVENANCE=1.

#ifndef SPHERE_LOG_H
#define SPHERE_LOG_H

// Global debug verbosity level
extern int g_iDebugLevel;

// Timer-removal provenance is independent of DEBUGLEVEL and the grouped log
// mask, so enabling it does not turn on unrelated verbose logging.
extern bool g_fTimerRemovalProvenance;

// Keep the historical stderr stream and feed the same formatted line to the
// daily CLog sink. Formatting once avoids evaluating a logging argument twice
// when it has side effects (for example a socket or object lookup).
inline void SphereLogMessage(int iDebugLevel, LPCTSTR pszLabel, LOGL_TYPE level, LPCTSTR pszFormat, ...)
{
	if ( g_iDebugLevel < iDebugLevel )
		return;

	TCHAR szMessage[1024];
	va_list vargs;
	va_start(vargs, pszFormat);
	vsnprintf(szMessage, sizeof(szMessage), pszFormat, vargs);
	va_end(vargs);

	fprintf(stderr, "[%s] %s\n", pszLabel, szMessage);
	fflush(stderr);
	TCHAR szDailyMessage[sizeof(szMessage) + 1];
	snprintf(szDailyMessage, sizeof(szDailyMessage), "%s" LOG_CR, szMessage);
	g_Log.EventStrDaily(0, level, szDailyMessage);
}

#define SPHERE_LOG(level, fmt, ...) \
	SphereLogMessage((level), \
		(level) == 0 ? "ERR" : (level) == 1 ? "INF" : (level) == 2 ? "NET" : "TRC", \
		(level) == 0 ? LOGL_ERROR : LOGL_EVENT, fmt, ##__VA_ARGS__)

// Convenience macros for each subsystem
#define SPHERE_LOG_ERR(fmt, ...)    SPHERE_LOG(0, fmt, ##__VA_ARGS__)
#define SPHERE_LOG_NET(fmt, ...)    SPHERE_LOG(2, fmt, ##__VA_ARGS__)
#define SPHERE_LOG_LOAD(fmt, ...)   SPHERE_LOG(1, fmt, ##__VA_ARGS__)
#define SPHERE_LOG_CLIENT(fmt, ...) SPHERE_LOG(2, fmt, ##__VA_ARGS__)
#define SPHERE_LOG_SCRIPT(fmt, ...) SPHERE_LOG(3, fmt, ##__VA_ARGS__)
#define SPHERE_LOG_TIMER_PROVENANCE(fmt, ...) \
	do { if (g_fTimerRemovalProvenance) \
		SphereLogMessage(0, "INF", LOGL_EVENT, fmt, ##__VA_ARGS__); } while(0)

#endif // SPHERE_LOG_H
