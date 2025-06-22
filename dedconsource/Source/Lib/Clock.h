/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_Clock_INCLUDED
#define DEDCON_Clock_INCLUDED

#include "Misc.h"

// timeval functions

const int million = 1000000;

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

class Time
{
public:
    Time();

    // returns the application start time
    static Time const & GetStartTime();

    //! normalizes the time so that the microseconds part is always between 0 and 1000000
    void Normalize();

    // time difference
    Time operator - (Time const & b ) const;

    void AddSeconds( int seconds )
    {
        time.tv_sec += seconds;
    }

    void AddMilliseconds( int milliseconds )
    {
        int seconds = milliseconds/1000 - 1;
        time.tv_sec += seconds;
        milliseconds -= seconds * 1000;
        time.tv_usec += milliseconds * 1000;
        Normalize();
    }

    void AddMicroseconds( int microseconds )
    {
        time.tv_usec += microseconds;
        Normalize();
    }

    // transform to microseconds
    int Microseconds() const
    {
        return time.tv_sec * million + time.tv_usec;
    }

    // transform to milliseconds
    int Milliseconds() const
    {
        return time.tv_sec * 1000 + time.tv_usec/1000;
    }

    // transofrm to seconds
    int Seconds() const
    {
        return time.tv_sec + time.tv_usec/million;
    }

    //! fills time
    void GetTime();

    //! returns the current time as a string (wrapper for strftime)
    static std::string GetCurrentTimeString( char const * format );

    //! returns the start time as a string (wrapper for strftime)
    static std::string GetStartTimeString( char const * format );

    timeval time;
};

class Clock
{
public:
    //! returns the current SeqID
    int Current() { return seqID; }

    //! sets the current time
    void Set( int seqID_ )
    {
        seqID = seqID_;
    }

    //! returns the estimated time until the clock will advanced next
    void GetNextBump( Time & tv );

    //! try to hold bck ticks/2 ticks, for example to wait for further data to arrive
    void InhibitTick( int ticks );

    //! bumps the SeqID up if the time has come. If urgent, make that a bit earlier.
    void AutoBump( int externalSeqID );

    //! advances the SeqID one step.
    void Bump( bool internal = false );

    Clock();

    int ticksPerSecond;             //!< number of ticks per second
private:
    Time nextSync;                  //!< the rough time for the next sync, the actual time gets modified depending on seqID and inhibit

    int seqID;                      //!< the global SeqID, always increasing

    int inhibit;                    //!< slow down ticks for a while

    Time timeToNextBump;            //!< time to the next bump
};

extern Clock gameClock;

#endif // DEDCON_Clock_INCLUDED
