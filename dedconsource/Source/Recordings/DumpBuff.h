/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_DumpBuff_INCLUDED
#define DEDCON_DumpBuff_INCLUDED

class TagReader;

// various ways to dump network data to stdio
void dumpbuf( TagReader & reader );
void dumpbuf( unsigned char const * recbuf, int recbuflen );
void dumpbuf( char const * recbuf, int recbuflen );

#endif // DEDCON_DumpBuff_INCLUDED
