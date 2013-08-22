#include "ticker.h"

uint64_t getTimeMilliseconds(void) {
  uint64_t milliseconds = 0;
#if (defined(__MACH__) && defined(__APPLE__))
  struct mach_timebase_info convfact;
  mach_timebase_info(&convfact); // get ticks->nanoseconds conversion factor
  // get time in nanoseconds since computer was booted
  // the measurement is different per core
  uint64_t tick = mach_absolute_time();
  milliseconds = (tick * convfact.numer) / (convfact.denom * 1000000);
#elif defined(_WIN32)
  milliseconds = GetTickCount64();
#elif defined(__unix__)
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  milliseconds = now.tv_sec*1000 + now.tv_nsec/1000000;
#endif
  return milliseconds;
}
