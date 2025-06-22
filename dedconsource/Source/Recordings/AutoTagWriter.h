/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_AutoTagWriter_INCLUDED
#define DEDCON_AutoTagWriter_INCLUDED

#include "Lib/Misc.h"
#include "TagWriter.h"
#include "Lib/FakeEncryption.h"

class AutoTagWriterBase
{
public:
    unsigned char * GetBuffer()
    {
        return buffer;
    }
protected:
    ~AutoTagWriterBase();
    explicit AutoTagWriterBase( size_t size );

    void InitBuffer( size_t size );                            //!< initializes a buffer

    unsigned char * buffer;                                    //!< the write target
    size_t size;                                               //!< total size
};

//! class that writes a tag into a self-contained buffer.
class AutoTagWriter: public AutoTagWriterBase, public TagWriter
{
public:
    AutoTagWriter( char const * type = NULL );
    AutoTagWriter( char const * type, char const * clas, char const * typetag = NULL );

    int Size() const;                                          //!< returns the number of bytes that have been written.
    void SendTo( Socket & socket, sockaddr_in const & target, ENCRYPTION_VERSION encryption ) const;  //!< send to target over network socket
    void Compress( CompressedTag & compressed ) const;         //!< stores the buffer for later use

    void Save( std::ostream & s ) const;                       //!< writes contents to file

    void Dump() const;                                         //!< dump to stdio

    void GrowBuffer();                                         //!< resizes the buffer

    // called when the buffer would overrun and needs expansion
    virtual void OnBufferOverrun();
};

#endif // DEDCON_AutoTagWriter_INCLUDED
