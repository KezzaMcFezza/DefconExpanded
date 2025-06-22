/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_Log_INCLUDED
#define DEDCON_Log_INCLUDED

#include "Lib/Misc.h"

#include <iostream>
#include <fstream>
#include <memory>

//! wraps streams with extra flexibility, so you can switch them around on the fly
class OStreamWrapper
{
    friend class OStreamSetter;
public:
    //! output operator (kezza changed this so the outfile is always refreshed in realtime for the discord bot to read the /gamealert command)
    template <class T>
    OStreamWrapper & operator<<( T const & a )
    {
        Timestamp();

        if ( stream )
        {
            *stream << a;
            stream->flush();
        }

        if ( secondaryStream )
        {
            *secondaryStream << a;
            secondaryStream->flush(); // set to flush and not buffer
        }
        
        // new stream for the RCON server to prevent blocking the secondary stream from writing to the outfile
        // after dedconmain initilised rcon. which looked like this...
        //  Metaserver IP: 34.197.189.80
        //  Server encryption initialized
        //  RCON encryption enabled
        //  RCON server listening on port 8801 (timeouts disabled)
        // logfile would end after the port was set.
        
        if ( RemoteStream )
        {
            *RemoteStream << a;
            RemoteStream->flush();
        }

        return *this;
    }

    OStreamWrapper();
    OStreamWrapper( std::ostream & stream );

    //! sets a new stream
    void SetStream( std::ostream * stream )
    {
        this->stream = stream;
    }
    
    //! sets the secondary stream
    void SetSecondaryStream( std::ostream * stream )
    {
        this->secondaryStream = stream;
    }
    
    //! sets the remote stream for RCON
    void SetRemoteStream( std::ostream * stream )
    {
        this->RemoteStream = stream;
    }
    
    //! returns the current primary stream
    std::ostream * GetStream() const
    {
        return stream;
    }

    //! returns the current primary stream
    std::ostream * GetSecondaryStream() const
    {
        return secondaryStream;
    }
    
    //! returns the current rcon stream
    std::ostream * GetRemoteStream() const
    {
        return RemoteStream;
    }

    //! returns whether we have a stream set currently
    bool IsSet() const
    {
        return stream || secondaryStream || RemoteStream;
    }

    //! flushes all pending writes
    void Flush();

    //! prints a timestamp if one is scheduled
    void Timestamp();

    //! schedules a timestamp
    void AddTimestamp()
    {
        timestamp = true;
    }
protected:
    std::ostream * stream;           //!< primary stream
    std::ostream * secondaryStream;  //!< secondary stream
    std::ostream * RemoteStream;     //!< Remote stream for RCON
    bool timestamp;                  //!< whether a timestamp should be printed next
    bool timestampOutOverride;       //!< set to false while no timestamp is wanted
    std::string timestampSuffix;     //!< additional timestamp suffix
};

//! class that temproarily sets a different stream into a stream wrapper
class OStreamSetter
{
public:
    OStreamSetter( OStreamWrapper & wrapper );
    OStreamSetter( OStreamWrapper & wrapper, std::ostream * stream );
    ~OStreamSetter();

    void Set( std::ostream * stream );
    void SetSecondary( std::ostream * stream );
    void SetRemote( std::ostream * stream );

    // determines whether standard output should be timestamped
    void SetTimestampOut( bool timestampOut );

    // adds a temporary suffix to timestamps
    void SetTimestampSuffix( std::string const & timestampSuffix );
private:
    OStreamWrapper & wrapper; //!< the wrapper
    std::ostream * backup;    //!< the previous stream
    std::ostream * secondaryBackup;    //!< the previous secondary stream
    std::ostream * RemoteBackup;    //!< the previous Remote stream
    int timestampBackup; //!< backup of the setting responsible for timestamping standard output
    std::string timestampSuffixBackup; //!< backup for the timestamp suffix
};

//! proxy wrapper for ostreamwrappers
class OStreamProxy
{
public:
    OStreamProxy( OStreamWrapper & stream_ ):stream( stream_ ){}
    OStreamProxy( OStreamProxy const & other ):stream( other.stream ){}
    ~OStreamProxy(){ stream.Flush(); }

    //! output operator
    template < class T >
    OStreamWrapper & operator << ( T const & a )
    {
        stream << a;

        return stream;
    }
    OStreamWrapper & GetWrapper() { return stream; }
private:
    OStreamWrapper & stream;
};

//! proxy wrapper for fstreams for log files (stream gets automatically closed when it is no longer needed)
class FStreamProxy
{
public:
    FStreamProxy( std::unique_ptr<std::ofstream> && stream );
    FStreamProxy( FStreamProxy && other );
    ~FStreamProxy();

    //! output operator
    template < class T >
    FStreamProxy & operator << ( T const & a ) &
    {
        return write(a);
    }

    //! output operator
    template < class T >
    FStreamProxy && operator << ( T const & a ) &&
    {
        return std::move(write(a));
    }
private:
    std::unique_ptr< std::ofstream > stream;

    //! output
    template < class T >
    FStreamProxy & write ( T const & a )
    {
        std::ostream * s = stream.get();
        if ( s )
            (*s) << a;

        return *this;
    }
};

//! special wrappers for logging
class Log
{
public:
    //! returns a temporary stream to write to
    FStreamProxy GetStream();

    //! checks whether a file name is legal
    static bool IsLegalFilename( std::string const & filename );

    //! sets the filename to write to
    void SetFilename( std::string const & filename );

    //! gets the log level
    static int GetLevel();

    //! sets the log level
    static void SetLevel( int level );
    
    //! get standard output stream
    static OStreamProxy Out();

    //! get standard error stream
    static OStreamProxy Err();

    //! get event stream
    static OStreamProxy Event();

#ifdef DEBUG
    //! get network log stream
    static OStreamProxy NetLog();

    //! get network log stream specialized for client input
    static OStreamProxy OrderLog();
#endif

    //! get client log stream
    static Log & PlayerLog();

    //! get score log stream
    static Log & ScoreLog();

    //! get griefer log stream
    static Log & GriefLog();

    //! the log prefix
    static std::string const & Prefix();

    //! gets the filename to write to
    std::string const & GetFilename() const
    {
        return name;
    }
private:
    //! the name of the file to log to
    std::string name;
};

#endif // DEDCON_Log_INCLUDED
