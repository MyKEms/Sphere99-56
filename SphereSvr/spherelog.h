// spherelog.h — Configurable debug trace logging for Sphere99
//
// Usage:
//   SPHERE_LOG(level, fmt, ...)  — log at given level
//   SPHERE_LOG_NET(fmt, ...)     — network/login events
//   SPHERE_LOG_LOAD(fmt, ...)    — world loading events
//   SPHERE_LOG_CLIENT(fmt, ...)  — client packet processing
//   SPHERE_LOG_SCRIPT(fmt, ...)  — script execution events
//   SPHERE_LOG_ERR(fmt, ...)     — always logged (errors)
//
// Levels (set via DEBUGLEVEL= in sphere.ini or -dlevel command line):
//   0 = errors only (production)
//   1 = warnings + key events (login, logout, save)
//   2 = verbose network trace (packets, relay, encryption)
//   3 = full trace (every function entry, script execution)

#ifndef SPHERE_LOG_H
#define SPHERE_LOG_H

// Global debug verbosity level
extern int g_iDebugLevel;

// Core logging macro — writes to stderr (captured in sphere99svr.log)
#define SPHERE_LOG(level, fmt, ...) \
	do { if (g_iDebugLevel >= (level)) { \
		fprintf(stderr, "[%s] " fmt "\n", \
			(level) == 0 ? "ERR" : (level) == 1 ? "INF" : (level) == 2 ? "NET" : "TRC", \
			##__VA_ARGS__); \
		fflush(stderr); \
	} } while(0)

// Convenience macros for each subsystem
#define SPHERE_LOG_ERR(fmt, ...)    SPHERE_LOG(0, fmt, ##__VA_ARGS__)
#define SPHERE_LOG_NET(fmt, ...)    SPHERE_LOG(2, fmt, ##__VA_ARGS__)
#define SPHERE_LOG_LOAD(fmt, ...)   SPHERE_LOG(1, fmt, ##__VA_ARGS__)
#define SPHERE_LOG_CLIENT(fmt, ...) SPHERE_LOG(2, fmt, ##__VA_ARGS__)
#define SPHERE_LOG_SCRIPT(fmt, ...) SPHERE_LOG(3, fmt, ##__VA_ARGS__)

#endif // SPHERE_LOG_H
