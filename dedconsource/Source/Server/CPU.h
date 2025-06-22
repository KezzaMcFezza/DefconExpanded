/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_CPU_INCLUDED
#define DEDCON_CPU_INCLUDED

#include "Client.h"

//! manages CPU players
class CPU: public SuperPowerList
{
public:
    enum { MaxCPUs = MaxSuperPowers };

    CPU();

    //! adds a CPU player
    static CPU * Add( char const * name = NULL );
    
    //! removes a CPU player
    void Remove();
    
    //! logs existence (if you create it manually. Not required for CPU::Add().
    void Log();

    //! removes all CPU players
    static void RemoveAll();

    //! readies up all CPus
    static void ReadyAll();

    //! callback for start of countdown
    static void OnStartCountdown();

    //! callback for abort of countdown
    static void OnAbortCountdown();
};

#endif // DEDCON_CPU_INCLUDED
