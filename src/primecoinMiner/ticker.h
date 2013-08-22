#ifndef TICKER_H
#define TICKER_H

#if (defined(__MACH__) && defined(__APPLE__))
#include <mach/mach_time.h>
#elif defined(__linux__)
#include <time.h>
#else
#include <sys/time.h>
#endif

#ifdef _WIN32
typedef unsigned __int64 uint64_t
#else 
#include <stdint.h>
#include <errno.h>
#endif

uint64_t getTimeMilliseconds(void);

#endif // TICKER_H
