/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_CompressedTag_INCLUDED
#define DEDCON_CompressedTag_INCLUDED

#include <vector>

#ifdef HAVE_ENDIAN_H
#include <endian.h>
#endif

#include <sys/types.h>
#ifdef _WIN32
    #include <winsock2.h>  // Use winsock2 for networking and byte order on Windows
#else
    #include <sys/param.h>
#endif
#include <iosfwd>

// compressed tag
class CompressedTag
{
    friend class TagWriter;
    friend class AutoTagWriter;

public:
    CompressedTag() {}

    CompressedTag( std::istream & s, unsigned int maxSize = 0 )
    {
        Load( s, maxSize );
    }

    bool Empty() const
    {
        return content.size() == 0;
    }

    unsigned char const * Buffer() const
    {
        return &content[0];
    }

    int Size() const
    {
        return content.size();
    }

    // saves a data block
    static void Save( std::ostream & s, unsigned char const * buffer, size_t size );
    
    // reads a data block in two steps
    static size_t ReadSize( std::istream & s );
    static void Load( std::istream & s, unsigned char * buffer, size_t size );

    //! saves tag to stream
    void Save( std::ostream & s ) const;

    //! loads tag from stream
    void Load( std::istream & s, unsigned int maxSize = 0 );

    std::vector< int > clientWhitelist;  //!< if non-empty, this tag only is to be sent to clients with IDs on this list
    std::vector< int > clientBlacklist;  //!< will never be sent to clients on this list
private:
    std::vector< unsigned char > content; //!< the binary content
};

// structures for reading/writing doubles
#ifdef _WIN32
    // Assume little-endian for Windows
    struct LongLong { unsigned int lower, higher; };
#else
    // For non-Windows systems, use the original BYTE_ORDER checks
    #ifndef BYTE_ORDER
    #error BYTE_ORDER not defined.
    #endif
    #ifndef BIG_ENDIAN
    #error BIG_ENDIAN not defined.
    #endif
    #ifndef LITTLE_ENDIAN
    #error LITTLE_ENDIAN not defined.
    #endif

    #if BYTE_ORDER == BIG_ENDIAN
    struct LongLong { unsigned int higher, lower; };
    #else
    #if BYTE_ORDER == LITTLE_ENDIAN
    struct LongLong { unsigned int lower, higher; };
    #endif
    #endif
#endif

union DoubleUnion { LongLong l; double d; };

#endif // DEDCON_CompressedTag_INCLUDED
