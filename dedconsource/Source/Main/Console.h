/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_Console_INCLUDED
#define DEDCON_Console_INCLUDED

//! console input
class Console
{
public:
    //! reads from stdin, interprets input as configuration commands
    static void Read();
};

#endif // DEDCON_Console_INCLUDED
