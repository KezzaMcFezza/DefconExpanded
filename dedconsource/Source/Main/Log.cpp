/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "Log.h"
#include "GameSettings/Settings.h"

#include <fstream>
#include <memory>

#include <assert.h>
#include <sys/stat.h>
#include <errno.h>

#include <stdlib.h>
#include <string.h>

#ifdef WIN32
#include <shlobj.h>
#include <windows.h>
#include <direct.h>
#define mkdir(x, y)   _mkdir(x)
#endif

OStreamWrapper::OStreamWrapper()
  : stream( 0 ), secondaryStream( 0 ), RemoteStream( 0 ), timestamp( false ), timestampOutOverride( true )
{
}

OStreamWrapper::OStreamWrapper( std::ostream & stream )
: stream( &stream ), secondaryStream( 0 ), RemoteStream( 0 ), timestamp( false ), timestampOutOverride( true )
{
}


//! flushes all pending writes
void OStreamWrapper::Flush()
{
    if ( stream )
        stream->flush();
}

static StringSetting timestampFormat( Setting::Log, "TimestampFormat" );
static StringSetting rareTimestampFormat( Setting::Log, "RareTimestampFormat" );

// both formats are passed verbatim to some C function, you never know what that does
static SettingAdminLevel timestampFormatAdmin( timestampFormat, 0 );
static SettingAdminLevel rareTimestampFormatAdmin( rareTimestampFormat, 0 );

static IntegerSetting timestampOut( Setting::Log, "TimestampOut", 1, 1 );

//! prints a timestamp if one is scheduled
void OStreamWrapper::Timestamp()
{
    if ( timestamp && !timestampFormat.IsDefault() )
    {
        timestamp = false;
        std::string stamp = Time::GetCurrentTimeString( timestampFormat.Get().c_str());

        if ( stream )
        {
            if ( timestampOut && timestampOutOverride )
            {
                *stream << stamp;
            }

            *stream << timestampSuffix;
        }

        if ( secondaryStream )
        {
            *secondaryStream << stamp << timestampSuffix;
        }
        
        // new stream for the RCON server to prevent blocking the secondary stream from writing to the outfile
        if ( RemoteStream )
        {
            *RemoteStream << stamp << timestampSuffix;
        }
    }
}

OStreamSetter::OStreamSetter( OStreamWrapper & wrapper_ )
: wrapper( wrapper_ ), backup( wrapper_.GetStream() ), secondaryBackup( wrapper_.GetSecondaryStream() ), 
  RemoteBackup( wrapper_.GetRemoteStream() ), timestampBackup( wrapper_.timestampOutOverride ), 
  timestampSuffixBackup( wrapper_.timestampSuffix )
{
}

OStreamSetter::OStreamSetter( OStreamWrapper & wrapper_, std::ostream * stream )
: wrapper( wrapper_ ), backup( wrapper_.GetStream() ), secondaryBackup( wrapper_.GetSecondaryStream() ),
  RemoteBackup( wrapper_.GetRemoteStream() ), timestampBackup( wrapper_.timestampOutOverride )
{
    wrapper.SetStream( stream );
}

void OStreamSetter::SetTimestampOut( bool timestampOut )
{
    wrapper.timestampOutOverride = timestampOut;
}

// adds a temporary suffix to timestamps
void OStreamSetter::SetTimestampSuffix( std::string const & timestampSuffix )
{
    wrapper.timestampSuffix = timestampSuffix;
}

OStreamSetter::~OStreamSetter()
{
    wrapper.SetStream( backup );
    wrapper.SetSecondaryStream( secondaryBackup );
    wrapper.SetRemoteStream( RemoteBackup );
    wrapper.timestampOutOverride = timestampBackup;
    wrapper.timestampSuffix = timestampSuffixBackup;
}

void OStreamSetter::Set( std::ostream * stream )
{
    wrapper.SetStream( stream );
}

void OStreamSetter::SetSecondary( std::ostream * stream )
{
    wrapper.SetSecondaryStream( stream );
}

void OStreamSetter::SetRemote( std::ostream * stream )
{
    wrapper.SetRemoteStream( stream );
}

FStreamProxy::FStreamProxy( std::unique_ptr<std::ofstream> && stream )
: stream( std::move(stream) )
{
}

FStreamProxy::FStreamProxy( FStreamProxy && other )
: stream( std::move(other.stream) )
{
}

FStreamProxy::~FStreamProxy()
{
}

//! returns a temporary stream to write to
FStreamProxy Log::GetStream()
{
    std::unique_ptr<std::ofstream> s{};
    if ( name.size() > 0 )
    {
        s.reset(new std::ofstream( name.c_str(), std::ios::app ));

        if ( !s->good() )
        {
            Log::Err() << "Could not open log file " << name << " for appending.\n";
            name = "";
            s.reset();
        }
        else if ( !rareTimestampFormat.IsDefault() )
        {
            *s << Time::GetCurrentTimeString( rareTimestampFormat.Get().c_str() );
        }
    }

    return FStreamProxy( std::move(s) );
}

//! get standard output stream
OStreamProxy Log::Out()
{
    static OStreamWrapper out( std::cout );

    out.AddTimestamp();

    return out;
}

//! get standard error stream
OStreamProxy Log::Err()
{
    static OStreamWrapper err( std::cerr );

    err.AddTimestamp();

    return err;
}

//! get standard error stream
OStreamProxy Log::Event()
{
    static OStreamWrapper event;

    event.AddTimestamp();

    return event;
}

//! setting that adds a prefix in front of every log file
class LogPrefixSetting: public StringActionSetting
{
public:
    LogPrefixSetting( char const * name )
    : StringActionSetting( Log, name )
    {
    }

    //! activates the action with the specified value
    virtual void Activate( std::string const & v )
    {
        std::string val = Time::GetStartTimeString( v.c_str() );

        if ( !Log::IsLegalFilename( val ) )
            return;

        if ( val != prefix && Log::GetLevel() >= 2 )
            Log::Out() << name << " = " << val << "\n";

        prefix = val;
    }

    std::string const & Get() const { return prefix; }
private:
    std::string prefix;
};
static LogPrefixSetting logPrefix("LogFilePrefix");

//! stream creating action setting
class StreamActionSetting: public StringActionSetting
{
public:
    StreamActionSetting( char const * name )
    : StringActionSetting(Log, name )
    {
    }

    virtual void Activate( std::ofstream * f ) = 0;

    //! activates the action with the specified value
    virtual void Activate( std::string const & v )
    {
        // open new stream
        std::string val1 = Time::GetStartTimeString( v.c_str() );
        std::string val = logPrefix.Get() + val1;

        if ( !Log::IsLegalFilename( val ) )
            return;

        std::unique_ptr<std::ofstream> s{ new std::ofstream( val.c_str(), std::ios::app ) };

        // and set it
        if ( s->good() )
        {
            stream = std::move( s );

            Activate( stream.get() );
            
            Log::Out() << name << " = " << val1 << "\n";
        }
        else
        {
            Log::Err() << "Could not open log file " << val << " for appending.\n";
        }
    }
private:
    //! the stream to write to
    std::unique_ptr< std::ofstream > stream;
};

//! bends the output streams
class ActionSettingOut: public StreamActionSetting
{
public:
    ActionSettingOut( )
    : StreamActionSetting( "OutFile" )
    {
    }

    virtual void Activate( std::ofstream * f )
    {
        Log::Out().GetWrapper().SetSecondaryStream( f );
        Log::Err().GetWrapper().SetSecondaryStream( f );
    }
};
static ActionSettingOut outStreamSetting;

//! bends the error stream
class ActionSettingErr: public StreamActionSetting
{
public:
    ActionSettingErr( )
    : StreamActionSetting( "ErrFile" )
    {
    }

    virtual void Activate( std::ofstream * f )
    {
        Log::Err().GetWrapper().SetSecondaryStream( f );
    }
};
static ActionSettingErr errStreamSetting;

//! bends the event stream
class ActionSettingEvent: public StreamActionSetting
{
public:
    ActionSettingEvent()
    : StreamActionSetting( "EventFile" )
    {
    }

    virtual void Activate( std::ofstream * f )
    {
        Log::Event().GetWrapper().SetStream( f );
    }
};
static ActionSettingEvent outStreamEvent;

#ifdef DEBUG

//! bends the error stream
class ActionSettingNet: public StreamActionSetting
{
public:
    ActionSettingNet()
    : StreamActionSetting( "NetFile" )
    {
    }

    virtual void Activate( std::ofstream * f )
    {
        Log::NetLog().GetWrapper().SetStream( f );
    }
};
static ActionSettingNet netStreamSetting;

//! bends the error stream
class ActionSettingOrder: public StreamActionSetting
{
public:
    ActionSettingOrder()
    : StreamActionSetting( "OrderFile" )
    {
    }

    virtual void Activate( std::ofstream * f )
    {
        Log::OrderLog().GetWrapper().SetStream( f );
    }
};
static ActionSettingOrder orderStreamSetting;


//! get network log stream
OStreamProxy Log::NetLog()
{
    static OStreamWrapper net;

    net.AddTimestamp();

    return net;
}

//! get network log stream
OStreamProxy Log::OrderLog()
{
    static OStreamWrapper order;

    order.AddTimestamp();

    return order;
}
#endif

typedef Log & LogGetter();

//! stream creating action setting
class LogActionSetting: public StringActionSetting
{
public:
    LogActionSetting( char const * name, LogGetter * log )
    : StringActionSetting( Log, name )
      , log( log )
    {
        
    }

    //! activates the action with the specified value
    virtual void Activate( std::string const & val )
    {
        log().SetFilename( val );
        Log::Out() << name << " = " << val << "\n";
    }
private:
    //! the log
    LogGetter * log;
};

// get grief log stream
static Log & RealGriefLog2()
{
    static Log grief;

    return grief;
}


static LogActionSetting playerLog( "PlayerLog", Log::PlayerLog );
static LogActionSetting scoreLog( "ScoreLog", Log::ScoreLog );
static LogActionSetting griefLog( "GriefLog", RealGriefLog2 );

static StringSetting griefLogHeader( Setting::Log, "GriefLogHeader" );
static SettingAdminLevel griefLogHeaderAdmin( griefLogHeader, 0 );

//! get client log stream
Log & Log::PlayerLog()
{
    static Log player;

    return player;
}

//! get score log stream
Log & Log::ScoreLog()
{
    static Log score;

    return score;
}

// get grief log stream
static Log & RealGriefLog()
{
    static Log & grief = RealGriefLog2();

    // separate logs for different games with header
    if ( !griefLogHeader.IsDefault() )
    {
        grief.GetStream() << Time::GetCurrentTimeString( griefLogHeader.Get().c_str() ) << "\n";
    }

    return grief;
}

//! get grief log stream
Log & Log::GriefLog()
{
    static Log & grief = ::RealGriefLog();

    return grief;
}

//! the log prefix
std::string const & Log::Prefix()
{
    return logPrefix.Get();
}

//! checks whether a file name is legal
bool Log::IsLegalFilename( std::string const & filename )
{
    // the owner can use any path she likes
    if ( SettingsReader::GetActive() && SettingsReader::GetActive()->GetAdminLevel() <= 0 )
        return true;

    // check for relative paths leaving the current directory
    char const * s = filename.c_str();
    if ( strstr( s, ".." ) )
    {
        Err() << "Illegal relative path.\n";
        return false;
    }

    // check for absolute unix paths
    if ( s[0] == '/' )
    {
        Err() << "Illegal absolute path.\n";
        return false;
    }

    // check for absolute Windows paths
    if ( s[0] == '\\' )
    {
        Err() << "Illegal absolute path.\n";
        return false;
    }
    if ( filename.size() > 2 && s[1] == ':' )
    {
        Err() << "Illegal absolute path.\n";
        return false;
    }

    return true;
}

//! sets the filename to write to
void Log::SetFilename( std::string const & filename )
{
    std::string realName = logPrefix.Get() + Time::GetStartTimeString( filename.c_str() );

    if( IsLegalFilename( realName ) )
        name = realName;
    else
        name = "";
}

static IntegerSetting logLevel( Setting::Log, "LogLevel", 2, 4 );      //!< log level. The higher, the more output.

//! gets the log level
int Log::GetLevel()
{
    return logLevel.Get();
}

//! sets the log level
void Log::SetLevel( int level )
{
    logLevel.Set( level );
}

//! setting that adds a custom entry to the event log
class LogEventSetting: public StringActionSetting
{
public:
    LogEventSetting( char const * name )
    : StringActionSetting( Log, name )
    {
    }

    //! activates the action with the specified value
    virtual void Activate( std::string const & v )
    {
        Log::Event() << v << "\n";
    }
};
static LogEventSetting logEvent("LogEvent");

//! makes directories
class LogMakeDirSetting: public StringActionSetting
{
public:
    LogMakeDirSetting( char const * name )
    : StringActionSetting( Log, name )
    {
    }

    //! activates the action with the specified value
    virtual void Activate( std::string const & v )
    {
        std::string val = Time::GetStartTimeString( v.c_str() );

        if ( !Log::IsLegalFilename( val ) )
            return;

        if( mkdir( val.c_str(), 0755 ) == 0 )
        {
            if ( Log::GetLevel() >= 2 )
                Log::Out() << "Directory " << val << " created.\n";
        }
        else if ( errno != EEXIST )
        {
            if ( Log::GetLevel() >= 2 )
                Log::Err() << "Cannot create directory " << val << ".\n";
        }
    }
private:
};
static LogMakeDirSetting logMakeDir("MakeDir");
