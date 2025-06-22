/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

// #include "config.h"

#ifdef _WIN32
    #define strcasecmp _stricmp
#endif


#include "Settings.h"

#include "Lib/Codes.h"

#include <fstream>
#include <sstream>
#include "stdlib.h"
#include "Main/Log.h"
#include "Recordings/TagWriter.h"
#include "Network/Exception.h"

#ifdef _WIN32
    #include <string.h>  // Use this for Windows
#else
    #include <strings.h>  // Use this for Unix/Linux
    #include <cstring>
#endif


static ListBase< SettingBase > & GetSettings()
{
    static ListBase< SettingBase > settings;
    return settings;
}

//! controls the admin level of other settings
class AdminLevelSetting: public ActionSetting
{
public:
    AdminLevelSetting():ActionSetting( Admin, "AdminLevel" )
    {
        // only the owner is allowed to use this by default
        SetAdminLevel(0);
    }

    //! reads the value of the setting from a stream.
    virtual void DoRead( std::istream & source )
    {
        std::string name;
        source >> name;

        int level;
        source >> level;

        if ( !source.fail() )
        {
            bool found = false;
            for ( ListBase<SettingBase>::iterator setting = GetSettings().begin(); setting; ++setting )
            {
                if ( setting->GetName() == name )
                {
                    found = true;

                    if ( level != setting->GetAdminLevel() )
                    {
                        setting->SetAdminLevel( level );

                        Log::Out() << "Admin level of " << setting->GetName() << " changed to " << level << ".\n";
                    }
                }
            }
            if (!found)
            {
                Log::Err() << "No setting " << name << " known.";
            }
        }
        else
        {
            Log::Err() << "Usage: AdminLevel <setting> <admin level>\n";
        }
    }
};

static AdminLevelSetting adminLevelSetting;

//! allows to raise the rights inside a script
class SudoSetting: public ActionSetting
{
public:
    SudoSetting():ActionSetting( Admin, "Sudo" )
    {
        // everyone is allowed to use this
        SetAdminLevel(1000);
    }

    //! reads the value of the setting from a stream.
    virtual void DoRead( std::istream & source )
    {
        SettingsReader * reader = SettingsReader::GetActive();
        if ( !reader )
        {
            Log::Err() << "No reading process found to raise the rights of.\n";
            return;
        }

        int minLevel = -100;
        int resultLevel = 1000;
        std::ws(source);
        source >> minLevel;
        std::ws(source);
        source >> resultLevel;

        if ( !source.fail() )
        {
            if ( reader->GetAdminLevel() <= minLevel )
            {
                // bump up admin rights
                reader->SetAdminLevel( resultLevel );
            }
            else
            {
                Log::Err() << "Admin level insufficient to raise rights, aborting the script.\n";
                reader->Abort();
            }
        }
        else
        {
            Log::Err() << "Usage: Sudo <minimal admin level> <effective admin level for rest of script>\n";
        }
    }
};

static SudoSetting sudoSetting;

// the current revision of network settings
int SettingBase::currentRevision = 1;

// the last revision that was sent to the clients
int SettingBase::lastSentRevision = 1;
// (as settings get shuffled around in game mode settings, clients keep the
// setting id -> revision mapping, so storing a per-setting-revision here
// would be counterproductive)

SettingBase::SettingBase()
{
    // every setting needs to be sent at least once
    needToSend = true;

    // default: admins can change this
    adminLevel = 1;

#ifdef COMPILE_DEDWINIA
    numericName = 1;
#endif
}

SettingBase::~SettingBase()
{
}

// flag set when any setting is changed
static bool anythingChanged = false;

static BoolSetting caseSensitive( Setting::Internal, "CaseSensitive", 1 );

//! exception to throw when a setting can't be processed now
struct TryAgainLater
{
    int time; //!< number of seconds to wait

    TryAgainLater( int t ): time( t ) {}
};

//! reads setting line
int SettingBase::ReadLine( std::string const & lineName, std::string const & line, int adminLevel )
{
    // ignore empty lines
    if( line.size() == 0 )
    {
        return 0;
    }

    // parse the line
    std::istringstream s( line );

    std::ws(s);
    std::string name;
    s >> name;

    // ignore lines beginning with "#"
    if( name.size() == 0 || name[0] == '#' )
    {
        return 0;
    }

    try
    {
        OStreamSetter setter( Log::Err().GetWrapper() );
        if ( lineName.size() )
        {
            setter.SetTimestampSuffix( lineName + " : " );
        }

        // look up the setting
        bool found = false;
        for ( ListBase<SettingBase>::iterator setting = GetSettings().begin(); setting && !found; ++setting )
        {
            if ( caseSensitive.Get() ? ( setting->name == name ) : ( strcasecmp( setting->name.c_str(), name.c_str() ) == 0 ) )
            {
                found = true;

                if ( setting->GetAdminLevel() >= adminLevel )
                {
                    setting->DoRead( s );

                    if ( s.fail() )
                    {
                        Log::Err() << "config option format error.\n";
                    }
                }
                else
                {
                    Log::Err() << setting->name << " requires higher admin privileges.\n";
                }
            }
        }

        if ( !found )
            Log::Err() << "config option " << name << " invalid.\n";

    }
    catch( TryAgainLater const & tryAgain )
    {
        return tryAgain.time > 1 ? tryAgain.time : 1;
    }

    return 0;
}

//! read all settings from a stream
/*
void SettingBase::ReadAll( std::istream & source, char const * filename, int adminLevel )
{
    int lineNo = 0;
    while ( source.good() && !source.eof() )
    {
        lineNo ++;

        // read line after line
        std::string line;
        getline(source, line );

        std::ostringstream lineName;
        lineName << filename << ':' << lineNo;

        ReadLine( lineName.str(), line, adminLevel );
    }

    CheckRestrictions();
    SendSettings();
}
*/

//! read all settings from a file
/*
void SettingBase::ReadAll( char const * filename )
{
    if ( !Log::IsLegalFilename( filename ) )
        return;

    std::ifstream f( filename );

    if ( !f.good() )
    {
        Log::Err() << "Configuration file " << filename << " not found.\n";
        throw QuitException( 0 );
    }

    ReadAll( f, filename, 0 );
}
*/

//! call on every change
void SettingBase::OnChange()
{
    needToSend = true;
    currentRevision = lastSentRevision + 1;
}

static bool logSettings = false;

void SettingBase::StartLogging()
{
    logSettings = true;

    // log all non-default settings and all settings sent over the network
    Setting * * important = SettingHooks::GetImportantSettings();

    for ( ListBase<SettingBase>::iterator setting = GetSettings().begin(); setting; ++setting )
    {
        // make an exception for the MaxTeams setting.
        bool log = !setting->IsDefault() || setting->GetName() == "MaxTeams";

        for( Setting * * run = important; *run && !log; ++run )
        {
            if ( &(*setting) == *run )
            {
                log = true;
            }
        }

        if ( log )
        {
            setting->LogNow();
        }
    }
}

std::ostream & operator << ( std::ostream & o, SettingBase const & setting )
{
    setting.Write( o );
    return o;
}

// log setting value
void SettingBase::LogNow()
{
    // std::ostream * event = Log::Event().GetWrapper().GetStream();
    if ( LogChanges() )
    {
        Log::Event() << "SETTING " << name << ' ' << *this << '\n';
        // DoWrite( *event );
    }
}

//! call on all changes
void SettingBase::Change()
{
    anythingChanged = true;
    OnChange();

    if ( logSettings )
    {
        LogNow();
    }
}

//! reverts all settings
void SettingBase::RevertAll()
{
    for ( ListBase<SettingBase>::iterator setting = GetSettings().begin(); setting; ++setting )
    {
        setting->Revert();
    }
}

//! lists all settings to stdout
void SettingBase::ListAll( Type type, std::ostream & o, int charsPerLine )
{
    bool comma = false;
    int len = 0;
    for ( ListBase<SettingBase>::iterator setting = GetSettings().begin(); setting; ++setting )
    {
        if ( setting->type == type )
        {
            if ( comma )
            {
                o << ", ";
                len += 2;
                if ( len + int( setting->name.size() ) > charsPerLine )
                {
                    o << "\n";
                    Log::Out();
                    len = 0;
                }
            }
            else
            {
                char const * start = "List of settings: ";
                o << start;
                len += strlen( start );
            }
            comma = true;
            len += setting->name.size();

            o << setting->name;
        }
    }

    o << ".\n";
}

//! returns whether setting changes should be logged
bool SettingBase::LogChanges() const
{
    return true;
}

SettingsReader * SettingsReader::activeReader = 0;

// all active setting readers
static ListBase< SettingsReader > activeReaders;

SettingsReader::SettingsReader()
{
    tryAgain.GetTime();
    tryAgain.AddSeconds( -1 );
    checkNeeded = true;
    pending = 0;
    adminLevel = 0;
}

SettingsReader::~SettingsReader()
{
    // assert( lines.size() == 0 );

    delete pending;
    pending = 0;
}

//! adds a single line
void SettingsReader::AddLine( std::string const & name, std::string const & line )
{
    lines.push_back( Line( name, line ) );

    tryAgain.GetTime();
    tryAgain.AddSeconds( -1 );
}

//! read settings from a stream
void SettingsReader::Read( std::istream & source, char const * filename )
{
    int lineNo = 0;
    while ( source.good() && !source.eof() )
    {
        lineNo ++;

        // read line after line
        std::string line;
        getline(source, line );

        // remove trailing \r people get when they copy a config file from Windows to Unix
        if ( line.size() >= 1 && line[line.size()-1] == '\r' )
        {
            line = std::string( line, 0, line.size()-1 );
        }

        std::ostringstream lineName;
        lineName << filename << ':' << lineNo;

        AddLine( lineName.str(), line );
    }
}

//! throws an exception causing the execution of the nesting
//! reader to suspend for a bit
void PendingCommand::TryAgainLaterBase( int seconds )
{
    throw ::TryAgainLater( seconds );
}

class SettingsReaderActiveSetter
{
public:
    SettingsReaderActiveSetter( SettingsReader * & target, SettingsReader * value )
    : target( target )
    {
        back = target;
        target = value;
    }

    ~SettingsReaderActiveSetter()
    {
        target = back;
    }

    SettingsReader * NestedIn()
    {
        return back;
    }
private:
    SettingsReader * & target;
    SettingsReader * back;

};

//! read settings from a file
void SettingsReader::Read( char const * filename )
{
    if ( Log::GetLevel() >= 2 )
    {
        Log::Err() << "Loading config file " << filename << "\n";
    }

    SettingsReaderActiveSetter setter( activeReader, this );

    if ( !Log::IsLegalFilename( filename ) )
        return;

    std::ifstream f( filename );

    if ( !f.good() )
    {
        std::ostringstream s;
        s << filename << ".txt";
        std::ifstream f( s.str().c_str() );

        if ( !f.good() )
        {
            Log::Err() << "Configuration file " << filename << "(.txt) not found.\n";
        }
        else
        {
            Read( f, filename );
        }
    }
    else
    {
        Read( f, filename );
    }
}

void SettingsReader::Apply()
{
    // see if it is time to try again
    Time current;
    current.GetTime();
    if ( ( current - tryAgain ).Seconds() < 0 )
    {
        return;
    }

    SettingsReaderActiveSetter setter( activeReader, this );

    ApplyCore( current );

    // check limits and send changes to clients
    if ( checkNeeded )
    {
        if ( setter.NestedIn() )
        {
            setter.NestedIn()->checkNeeded = true;
        }
        else
        {
            SettingHooks::RunChecks();
        }
        checkNeeded = false;
    }

    // don't try again too soon if the queue is empty
    if ( lines.size() == 0 )
    {
        tryAgain.AddSeconds( 10 );
    }

    // handle children 
    for ( ListBase< SettingsReader >::iterator c = children.begin(); c; ++c )
    {
        c->Apply();
        // purge children that are done
        if ( c->LinesLeft() == 0 )
        {
            c->TransferChildren( *this );
            delete &(*c);
        }
    }

    for ( ListBase< SettingsReader >::iterator c = children.begin(); c; ++c )
    {
        // make sure the quickest child determines when this function is called again
        if ( ( c->tryAgain - tryAgain ).Seconds() < 0 )
        {
            tryAgain = c->tryAgain;
        }
    }
}

//! returns the contents of the currently active line
SettingsReader::Line const & SettingsReader::GetLine()
{
    assert( lines.size() > 0 );
    return lines.front();
}

//! advance to the next line, abandoning the current line
void SettingsReader::Advance()
{
    assert( lines.size() > 0 );
    lines.pop_front();
}

//! abort the execution
void SettingsReader::Abort()
{
    lines.clear();

    delete pending;
    pending = 0;
}

//! abort the execution
void SettingsReader::AbortRecursive()
{
    children.Clear();

    Abort();
}

void SettingsReader::ApplyCore( Time const & current )
{
    while ( lines.size() || pending )
    {
        int time = 0;
        bool wasPending = false;
        if ( pending )
        {
            try
            {
                wasPending = true;
                pending->Try();
                delete pending;
                pending = 0;
            }
            catch( TryAgainLater const & tryAgain )
            {
                time = tryAgain.time > 1 ? tryAgain.time : 1;
            }
        }
        else
        {
            // process one line
            Line const & line = GetLine();
            time = SettingBase::ReadLine( line.name, line.line, adminLevel );
            if ( time == 0 )
            {
                Advance();
                checkNeeded = true;
            }
        }

        // was it applied?
        if ( time > 0 )
        {
            // set the time to try again later
            tryAgain = current;
            tryAgain.AddSeconds( time );

            return;
        }
    }
}

SettingsReader * SettingsReader::Spawn()
{
    List< SettingsReader > * spawn = new List< SettingsReader >();
    spawn->Insert(children);
    spawn->SetAdminLevel( GetAdminLevel() );

    tryAgain.GetTime();
    tryAgain.AddSeconds( -1 );

    return spawn;
}

//! transfers the child readers to another reader
void SettingsReader::TransferChildren( SettingsReader & other )
{
    while ( children )
    {
        children.begin()->InsertSafely( other.children );
    }
}

//! access custom data that config items that want to make a pause can store stuff in.
void SettingsReader::AddPending( PendingCommand * pending )
{
    // get any reader, preferably the current one
    SettingsReader * reader = GetActive();
    if ( !reader )
    {
        Log::Err() << "It is not possible to use this command from the command line.\n";
        return;
    }

    // try to get rid of pending commands
    if ( reader->pending )
    {
        try
        {
            reader->pending->Try();
            delete reader->pending;
            reader->pending = 0;
        }
        catch( TryAgainLater const & tryAgain )
        {
            Log::Err() << "Internal error: can't have two pending commands in the same reader.\n";
            return;
        }
    }

    // and add the pending item as such
    reader->pending = pending;
}

SettingsReader & SettingsReader::GetSettingsReader()
{
    static SettingsReader reader;
    return reader;
}


Setting::Setting( Type type_, char const * name_ )
: List< SettingBase >( GetSettings() )
{
    name = name_;
    type = type_;
}


StringSetting::StringSetting( Type type, char const * name, char const * def_ )
: Setting( type, name )
{
    def = value = def_;
}

//! transform to the value
StringSetting::operator std::string() const
{
    return value;
}

//! set the value
void StringSetting::Set( std::string const & val )
{
    std::string oldval = value;
    if ( value != val )
    {
        value = val;
        if ( oldval != value  && Log::GetLevel() >= 2 )
            Log::Out() << name << " = " << value << '\n';
        Change();
    }
}

//! returns whether the setting was changed away from the default value
bool StringSetting::IsDefault() const
{
    return value == def;
}

//! reverts to default values
void StringSetting::Revert()
{
    if ( value != def && Log::GetLevel() >= 2 )
        Log::Err() << "Reverting: " << name << " = " << def << '\n';

    value = def;

    Change();
}

//! reads the value of the setting from a stream.
void StringSetting::DoRead( std::istream & source )
{
    std::string v = "";
    if ( !source.eof() )
    {
        std::ws(source);
        if ( !source.eof() )
            getline(source, v );

    }
    Set( v );
}

//! writes the value to a stream
void StringSetting::DoWrite( std::ostream & dest ) const
{
    dest << value;
}

//! writes the setting value to the tag
void StringSetting::DoWrite( TagWriter & writer ) const
{
    writer.WriteString( C_CONFIG_VALUE, value );
}

static bool settingWasSecret = false;

//! returns whether setting changes should be logged
bool SecretStringSetting::LogChanges() const
{
    return false;
}

SecretStringSetting::SecretStringSetting( Type type, char const * name, char const * def )
: StringSetting( type, name, def ){}

//! set the value
void SecretStringSetting::Set( std::string const & val )
{
    settingWasSecret = true;

    std::string oldval = value;
    if ( value != val )
    {
        value = val;
        Change();
        if ( oldval != value  && Log::GetLevel() >= 2 )
        {
            Log::Out() << name << " = <secret>\n";
            // Log::Err() << name << " = " << value << "\n";
        }
    }
}

//! writes the value to a stream
void SecretStringSetting::DoWrite( std::ostream & dest ) const
{
    dest << "<secret>";
}

//! returns true if between the last call of ResetSecret and now, a secret setting was changed.
bool SecretStringSetting::SettingWasSecret()
{
    return settingWasSecret;
}

//! initializes secrecy test time
void SecretStringSetting::ResetSecret()
{
    settingWasSecret = false;
}

ActionSetting::ActionSetting( Type type, char const * name )
: Setting( type, name )
{
}

//! returns whether the setting was changed away from the default value
bool ActionSetting::IsDefault() const
{
    return true;
}

//! reverts to default values
void ActionSetting::Revert()
{
}

//! writes the value to a stream
void ActionSetting::DoWrite( std::ostream & dest ) const
{
}

//! writes the setting value to the tag
void ActionSetting::DoWrite( TagWriter & writer ) const
{
}





StringActionSetting::StringActionSetting( Type type, char const * name )
: ActionSetting( type, name )
{
}

//! activate
void StringActionSetting::Activate( std::string const & val )
{
}

//! reads the value of the setting from a stream.
void StringActionSetting::DoRead( std::istream & source )
{
    std::string v = "";
    if ( !source.eof() )
    {
        std::ws(source);
        if ( !source.eof() )
            getline(source, v );

    }
    Activate( v );
}


//! reads the value of the setting from a stream.
void EnumSetting::DoRead( std::istream & source )
{
    std::string v = "";
    if ( !source.eof() )
    {
        std::ws(source);
        if ( !source.eof() )
            getline(source, v );

    }
    Set( v );
}

//! set the value
void EnumSetting::Set( int val )
{
    IntegerSetting::Set( val );
}

// set value by name
void EnumSetting::Set( std::string const & val )
{
    int value = Find( values, val.c_str() );
    if ( value >= 0 )
    {
        Set( value + GetMin() );
        return;
    }
    
    if ( alternativeValues )
    {
        value = Find( alternativeValues, val.c_str() );
        if ( value >= 0 )
        {
            Set( value + GetMin() );
            return;
        }
    }

    Set( atoi( val.c_str() ) );
}

unsigned int EnumSetting::Count( char const * * values )
{
    if ( !values )
    {
        return 0;
    }

    unsigned int ret = 0;
    while ( *values )
    {
        ++values;
        ++ret;
    }

    return ret;
}

int EnumSetting::Find( char const * * values, char const * value )
{
    if ( !values )
    {
        return -1;
    }

    int ret = 0;
    while ( *values )
    {
        if ( strcasecmp( *values, value ) == 0 )
        {
            return ret;
        }

        ++values;
        ++ret;
    }

    return -1;
}

void EnumSetting::SetValues( char const * * v )
{
    SetMax( GetMin() + Count( v ) - 1 );
    alternativeValues = 0;
    values = v;
}

//! defines default, minimal and maximal values
char const * boolValues[] = { "off", "on", 0 };
char const * boolValues2[] = { "false", "true", 0 };
BoolSetting::BoolSetting( Type type, char const * name, int def )
: EnumSetting( type, name, boolValues, 0, def )
{
    SetAlternativeValues( boolValues2 );
}

//! defines default, minimal and maximal values
IntegerSetting::IntegerSetting( Type type, char const * name, int def_, int max_, int min_ )
:Setting( type, name )
{
    def = value = def_;
    min = min_;
    max = max_;
}

//! transform to the value
IntegerSetting::operator int() const
{
    return value;
}

//! set the value
void IntegerSetting::Set( int val )
{
    int oldval = value;
    if ( value != val )
    {
        value = val;
        Change();
        if ( oldval != value && Log::GetLevel() >= 2 )
            Log::Out() << name << " = " << value << '\n';
    }

}

// should setting limits be checked?
class CheckLimitsSetting: public IntegerSetting
{
public:
    CheckLimitsSetting(): IntegerSetting( Setting::Internal, "CheckLimits", 1, 1 )
    {
        SetAdminLevel( 0 );
    }
};

CheckLimitsSetting checkLimits;

bool SettingBase::CheckLimits()
{
    return checkLimits;
}

bool SettingBase::AnythingChanged()
{
    return anythingChanged;
}

void SettingBase::ResetAnythingChanged()
{
    anythingChanged = false;
}

//! set the value
void IntegerSetting::OnChange()
{
    Setting::OnChange();
    if ( max >= min && Setting::CheckLimits() )
    {
        if ( value > max )
            value = max;
        if ( value < min )
            value = min;
    }
}

//! returns whether the setting was changed away from the default value
bool IntegerSetting::IsDefault() const
{
    return value == def;
}

void IntegerSetting::SetMax( int max_ )
{
    max = max_; 
    if ( value > max && Setting::CheckLimits() )
        Set( max );
}

void IntegerSetting::SetMin( int min_ )
{
    min = min_;
    if ( value < min && checkLimits )
        Set( min );
}

void IntegerSetting::SetDefault( int def_ )
{
    // set value to new default
    if ( IsDefault() )
    {
        Set( def_ );
    }

    // and change default
    def = def_;
}

//! reverts to default values
void IntegerSetting::Revert()
{
    if ( value != def && Log::GetLevel() >= 2 )
        Log::Out() << "Reverting: " << name << " = " << def << '\n';

    value = def;

    Change();
}

//! reads the value of the setting from a stream.
void IntegerSetting::DoRead( std::istream & source )
{
    int v = value;
    source >> v;
    Set(v);
}

//! writes the value to a stream
void IntegerSetting::DoWrite( std::ostream & dest ) const
{
    dest << value;
}

//! writes the setting value to the tag
void IntegerSetting::DoWrite( TagWriter & writer ) const
{
    writer.WriteInt( C_CONFIG_VALUE, value );
}
 
//! defines default, minimal and maximal values
FloatSetting::FloatSetting( Type type, char const * name, float def_)
:Setting( type, name )
{
    def = value = def_;
}

//! transform to the value
FloatSetting::operator float() const
{
    return value;
}

//! set the value
void FloatSetting::Set( float val )
{
    float oldval = value;
    if ( value != val )
    {
        value = val;
        Change();
        if ( oldval != value && Log::GetLevel() >= 2 )
            Log::Out() << name << " = " << value << '\n';
    }

}

//! set the value
void FloatSetting::OnChange()
{
    Setting::OnChange();
}

//! returns whether the setting was changed away from the default value
bool FloatSetting::IsDefault() const
{
    return value == def;
}

//! reverts to default values
void FloatSetting::Revert()
{
    if ( value != def && Log::GetLevel() >= 2 )
        Log::Out() << "Reverting: " << name << " = " << def << '\n';

    value = def;

    Change();
}

//! reads the value of the setting from a stream.
void FloatSetting::DoRead( std::istream & source )
{
    float v = value;
    source >> v;
    Set(v);
}

//! writes the value to a stream
void FloatSetting::DoWrite( std::ostream & dest ) const
{
    dest << value;
}

//! writes the setting value to the tag
void FloatSetting::DoWrite( TagWriter & writer ) const
{
    writer.WriteFloat( C_CONFIG_VALUE, value );
}


IntegerActionSetting::IntegerActionSetting( Type type, char const * name )
: ActionSetting( type, name )
{
}

//! activate
void IntegerActionSetting::Activate( int val )
{
}

//! reads the value of the setting from a stream.
void IntegerActionSetting::DoRead( std::istream & source )
{
    int v;
    source >> v;
    Activate( v );
}

static SettingHooks * hooks = NULL;

// create an object of this class to install hooks
SettingHooks::SettingHooks()
{
    hooks = this;
}

SettingHooks::~SettingHooks()
{
    if ( hooks == this )
    {
        hooks = NULL;
    }
}

//! returns a list of important settings
Setting * * SettingHooks::GetImportantSettings()
{
    if ( hooks )
    {
        return hooks->DoGetImportantSettings();
    }
    else
    {
        static Setting * n[] = { NULL };
        return n;
    }
}

//! runs checks on setting changes
void SettingHooks::RunChecks()
{
    if ( hooks )
    {
        hooks->DoRunChecks();
    }
}

