/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "Clock.h"
// #include "Log.h"
// #include "Globals.h"
#include <ctime>

Clock gameClock;

// returns the current time as a string
char const * GetTimeString( time_t & time, char const * format )
{
    static char temp[128];
    tm *time_tm;
    time_tm = localtime(&time);
    strftime(temp,sizeof(temp),format,time_tm);
    return temp;
}

// returns the current time as a string
std::string Time::GetCurrentTimeString( char const * format )
{
    time_t     now;
    now = ::time(NULL);
    return GetTimeString( now, format );
}

// returns the current time as a string
std::string Time::GetStartTimeString( char const * format )
{
    static time_t start = ::time(NULL);
    return GetTimeString( start, format );
}

Time::Time()
{
    time.tv_usec = 0;
    time.tv_sec = 0;
}

Time const & GetStartTime_()
{
    static Time time;
    time.GetTime();
    return time;
}

Time const & Time::GetStartTime()
{
    static Time const & time = GetStartTime_();
    return time;
}

void Time::Normalize()
{
    int usecSecs = time.tv_usec/million;
    time.tv_usec -= usecSecs * million;
    time.tv_sec  += usecSecs;

    // I hate off-by-one errors and this code is not performance critical,
    // so let me add these slow safety normalizers.
    while ( time.tv_usec >= million )
    {
        time.tv_usec -= million;
        time.tv_sec++;
    }
    while ( time.tv_usec < 0 )
    {
        time.tv_usec += million;
        time.tv_sec--;
    }
}

Time Time::operator - (Time const & b ) const
{
    Time result;
    result.time.tv_usec = million + time.tv_usec - b.time.tv_usec;
    result.time.tv_sec  = time.tv_sec - b.time.tv_sec - 1;

    result.Normalize();
    return result;
}

// fills time
void Time::GetTime()
{
#ifdef WIN32
    time.tv_sec = 0;
    time.tv_usec = 0;

    int tick = GetTickCount();
    static int firstTick = tick;

    AddMilliseconds( tick - firstTick );
#else
    // struct timezone tzp;

    gettimeofday(&time, NULL);
    // Log::Err() << time.tv_usec << "\n";
#endif
}

void Clock::GetNextBump( Time & tv )
{
    tv = timeToNextBump;
    tv.AddMicroseconds( 10 );
}

void Clock::InhibitTick( int ticks )
{
    if ( ticks * 2> inhibit )
        inhibit = ( inhibit + ticks ) >> 1;
}

// bumps the SeqID up if the time has come. If urgent, make that
// a bit earlier.
void Clock::AutoBump( int externalSeqID )
{
    // initialize to some sane value, 10 ticks per second
    timeToNextBump.time.tv_sec = 0;
    timeToNextBump.time.tv_usec = 100000;

    // what to do if the clock won't advance (fast enough) on its own
    if ( ticksPerSecond == 0 || ( ticksPerSecond <= 2 && externalSeqID > seqID ) )
    {
        // refresh the start time
        nextSync.GetTime();

        // is the bump urgent?
        if ( externalSeqID > seqID )
        {
            // Then do it.
            Bump();
            return;
        }
        else
        {
            // otherwise, do nothing.
            return;
        }
    }

    Time current;
    current.GetTime();

    // calculate time before next bump needs to happen
    timeToNextBump = nextSync - current;

    // modify it so time runs slower of faster depending on the needs
    int usecsPerTick = million/ticksPerSecond;

    int thresh = 0;
    if ( externalSeqID > seqID )
        thresh -= usecsPerTick >> 1;
    thresh += inhibit * usecsPerTick >> 1;

    timeToNextBump.AddMicroseconds( thresh );

    if ( timeToNextBump.time.tv_sec < 0 )
    {
        bool speedUp = ( externalSeqID > seqID );

        if ( inhibit > 0 || speedUp )
            --inhibit;
        if ( inhibit < 0 && !speedUp )
            ++inhibit;

        // bump
        Bump( true );

        nextSync.Normalize();

        // slow down clock if there is CPU lag, don't go faster than twice the requested speed
        if ( (nextSync - current).Microseconds() < ( usecsPerTick >> 1 )  )
        {
            nextSync = current;
            nextSync.AddMicroseconds( usecsPerTick >> 1 );
        }
    }

    // even if the clock is slow, make the main game loop run 10 times per second
    if ( timeToNextBump.time.tv_sec > 0 || timeToNextBump.time.tv_usec > 100000 )
    {
        timeToNextBump.time.tv_sec = 0;
        timeToNextBump.time.tv_usec = 100000;
    }
}

// really advances
void Clock::Bump( bool internal )
{
    ++seqID;

    // and set new next sync
    if ( ticksPerSecond > 2 || internal )
    {
        int usecsPerTick = million/ticksPerSecond;
        nextSync.AddMicroseconds( usecsPerTick );
    }
}

Clock::Clock()
        : ticksPerSecond( 0 ), seqID( 0 ), inhibit( 0 )
{
    nextSync.GetTime();

    timeToNextBump.time.tv_sec = 1;
    timeToNextBump.time.tv_usec = 0;
}

