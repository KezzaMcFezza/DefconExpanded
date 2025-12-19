#include "systemiv.h"

#include "hi_res_time.h"

#ifdef WIN32
static double s_tickInterval = 1.0;

void InitialiseHighResTime()
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    s_tickInterval = 1.0 / (double)freq.QuadPart;
}


double GetHighResTime()
{
    LARGE_INTEGER count;
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart * s_tickInterval;
}
#endif

#if TARGET_OS_MACOSX
#include <CoreServices/CoreServices.h>
#include <mach/mach.h>
#include <mach/mach_time.h>

// Declare these as static outside of the functions
static uint64_t s_start = 0;
static mach_timebase_info_data_t s_timebase = {0, 0};

void InitialiseHighResTime()
{
    // Initialize the timebase info only if not already done
    if (s_timebase.denom == 0 && s_timebase.numer == 0) {
        mach_timebase_info(&s_timebase);
    }
    s_start = mach_absolute_time();
}

double GetHighResTime()
{
    // If timebase hasn't been initialized, initialize it
    if (s_timebase.denom == 0 && s_timebase.numer == 0) {
        mach_timebase_info(&s_timebase);
    }

    uint64_t elapsed = mach_absolute_time() - s_start;
    return static_cast<double>(elapsed) * (s_timebase.numer / static_cast<double>(s_timebase.denom)) / 1000000000.0;
}
#elif TARGET_OS_LINUX
// Be cool to use the High resolution Posix timers 
// or hrtimers if supported http://lwn.net/Articles/167897/

#include <sys/time.h>
#include <time.h>


static timeval s_start;

void InitialiseHighResTime()
{
	gettimeofday(&s_start, NULL);
}


double GetHighResTime()
{
    timeval now;
    gettimeofday(&now, NULL);

	return double(now.tv_sec - s_start.tv_sec) +
	       (double(now.tv_usec) - double(s_start.tv_usec)) / 1000000.0;
}

#endif
