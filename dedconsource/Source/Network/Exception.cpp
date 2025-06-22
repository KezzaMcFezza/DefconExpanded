/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "Exception.h"

#include "Lib/Misc.h"
#include "GameSettings/GameSettings.h"
#include "Main/Log.h"

#include <sstream>

#include <stdlib.h>
#include <string.h>

void Breakpoint()
{
    int x;
    x = 0;
}

Exception::Exception()
{
}

Exception::Exception( char const * type_, char const * reason_ )
{
    if ( strlen( reason_ ) > 0 )
    {
        std::ostringstream s;
        s << type_ << " : " << reason_;
        reason = s.str();
    }

    Breakpoint();
}

Exception::~Exception()
{
    if ( Log::GetLevel() >= 2 && reason.size() > 0 )
    {
        Log::Err() << reason << '\n';
    }
}

void Exception::Silence()
{
    reason = "";
}

ReadException::ReadException( char const * reason ): Exception( "Network read exception", reason ){}

WriteException::WriteException( char const * reason ): Exception( "Network write exception", reason ){}
