/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_Settings_INCLUDED
#define DEDCON_Settings_INCLUDED

#include <string>
#include <iosfwd>
#include <deque>
#include "Lib/List.h"
#include "Lib/Clock.h"

class TagWriter;
class SettingsReader;

//! generic setting
class SettingBase
{
    friend class SettingsReader;
    friend class Settings;
public:
    //! the tpye of setting
    enum Type
    {
        Internal,
        GameOption,
        ExtendedGameOption,
        CPU,
        Admin,
        Player,
        Grief,
        Log,
        Network,
        Flow        
    };

    enum Class
    {
        Enum,    // enumeration types (game mode, AI level)
        String,  // string settings (map name)
        Generic  // type determined by data type written
    };

    SettingBase();
    virtual ~SettingBase();

    //! reverts to default values
    virtual void Revert() = 0;

    //! call on all changes
    void Change();

    //! start logging setting changes to the event log
    static void StartLogging();

    //! lists all settings to stdout
    static void ListAll( Type type, std::ostream & o, int charsPerLine = 80 );

    //! reverts all settings
    static void RevertAll();

    SettingBase & SetAdminLevel( int level ){ adminLevel = level; return *this; }
    int GetAdminLevel() const { return adminLevel; }

    //! returns the setting name
    std::string const & GetName() const { return name; }

    //! returns whether the setting was changed away from the default value
    virtual bool IsDefault() const = 0;

    //! returns whether setting changes should be logged
    virtual bool LogChanges() const;

    // returns the revision
    // int Revision() const { return revision; }

#ifdef COMPILE_DEDWINIA
    // returns the setting group
    int GetNumericName() const { return numericName; }

    void SetNumericName( int name ) { numericName = name; }
#endif

    // returns whether limits should be checked
    static bool CheckLimits();

    // returns whether any setting was changed since the last call of Reset..()
    static bool AnythingChanged();

    // resets AnythingChanged().
    static void ResetAnythingChanged();

    //! writes the value to a stream
    void Write( std::ostream & dest ) const { DoWrite( dest ); }
protected:
    //! log current value into event log
    void LogNow();

    //! read a setting from a line. The return value is 0 if the setting was
    //! applied and otherwise the number of seconds after which to try again.
    static int ReadLine( std::string const & lineName, std::string const & line, int adminLevel );

    //! read all settings from a stream
    // static void ReadAll( std::istream & source, char const * filename, int adminLevel );

    //! call on every change
    virtual void OnChange();

private:
    //! reads the value of the setting from a stream.
    virtual void DoRead( std::istream & source ) = 0;

    //! writes the value to a stream
    virtual void DoWrite( std::ostream & dest ) const = 0;

    //! writes the setting value to the tag
    virtual void DoWrite( TagWriter & writer ) const = 0;

    // returns the setting class
    virtual Class GetClass() const { return Generic; }

    bool needToSend;   //!< flag indicating whether the setting needs to be retransmitted over the network

    int adminLevel;    //!< the required admin level to change/call this setting
protected:
    std::string name;  //!< the setting name
    Type type;         //!< the setting type
#ifdef COMPILE_DEDWINIA
    int numericName;   //!< the numerical name of the setting
#endif
    // the current revision of network settings
    static int currentRevision;

    // the last revision that was sent to the clients
    static int lastSentRevision;
};

//! class to set the admin level of settings with
class SettingAdminLevel
{
public:
    SettingAdminLevel( SettingBase & setting, int adminLevel )
    {
        setting.SetAdminLevel( adminLevel );
    }
};

// a pending command; use them to execute
// anything that may not finish immediately
class PendingCommand
{
public:
    virtual ~PendingCommand(){}

    // executes the command
    virtual void Try() = 0;

protected:
    // call from within Try() to try the command again later
    static void TryAgainLaterBase( int seconds );
};

//! reads and stores settings from a file and allows to apply them later
class SettingsReader
{
public:
    struct Line
    {
        std::string name; //!< the name of the line
        std::string line; //!< the line contents

        Line( std::string const & name_, std::string const line_ ): name( name_ ), line( line_ ){}
    };

    typedef std::deque< Line > Lines;

    SettingsReader();
    virtual ~SettingsReader();

    //! adds a single line
    void AddLine( std::string const & name, std::string const & line );

    //! read settings from a stream
    void Read( std::istream & source, char const * filename );

    //! read settings from a file
    void Read( char const * filename );

    //! apply the settings.
    void Apply();

    //! returns the contents of the currently active line
    Line const & GetLine();

    //! advance to the next line, abandoning the current line
    void Advance();

    //! abort the execution of this script (children that were already spawned continue to run)
    void Abort();

    //! abort the execution of this script and all child scripts
    void AbortRecursive();

    //! returns the number of settings left
    int LinesLeft(){ return lines.size(); }

    //! access custom data that config items that want to make a pause can store stuff in.
    static void AddPending( PendingCommand * pending );

    //! returns the currently active settings reader
    static SettingsReader * GetActive(){ return activeReader; }

    //! returns a singleton reader (does not need to be the only one)
    static SettingsReader & GetSettingsReader();

    //! spawn a child
    SettingsReader * Spawn();

    //! return try again time
    Time const & TryAgain(){ return tryAgain; }

    void SetAdminLevel( int level ){ adminLevel = level; }
    int GetAdminLevel() const { return adminLevel; }

    //! transfers the child readers to another reader
    void TransferChildren( SettingsReader & other );
private:
    //! apply the settings.
    void ApplyCore( Time const & current );

    Lines lines;          //!< the lines containing the config items
    Time  tryAgain;       //!< when to try again to read the current line
    bool  checkNeeded;    //!< flag indicating whether restriction tests are needed

    int adminLevel;       //!< the required admin level to change/call this setting

    ListBase< SettingsReader > children; //!< forked readers

    PendingCommand * pending;       //!< the currently pending command
    static SettingsReader * activeReader; //!< the currently active settings reader
};

// a pending command executing further commands with a reader
class NestedPending: public PendingCommand
{
public:
    NestedPending();

    void Try();

    SettingsReader reader;
};

class Setting: public List< SettingBase >
{
public:
    Setting( Type type, char const * name );
};

//! string valued setting
class StringSetting: public Setting
{
public:
    StringSetting( Type type, char const * name, char const * def = "" );

    //! transform to the value
    operator std::string() const;

    //! set the value
    virtual void Set( std::string const & val );

    //! get the value
    std::string const & Get() const { return value; }

    //! returns whether the setting was changed away from the default value
    virtual bool IsDefault() const;

    //! reverts to default values
    virtual void Revert();
private:
    //! reads the value of the setting from a stream.
    virtual void DoRead( std::istream & source );

    //! writes the value to a stream
    virtual void DoWrite( std::ostream & dest ) const;

    //! writes the setting value to the tag
    virtual void DoWrite( TagWriter & writer ) const;

#ifdef COMPILE_DEDWINIA
    // returns the setting class
    virtual Class GetClass() const { return String; }
#endif
protected:
    std::string value;   //!< the current value
    std::string def;     //!< the default value
};

//! string valued setting that obfuscates log output
class SecretStringSetting: public StringSetting
{
public:
    SecretStringSetting( Type type, char const * name, char const * def = "" );

    //! set the value
    virtual void Set( std::string const & val );

    //! writes the value to a stream
    virtual void DoWrite( std::ostream & dest ) const;

    //! returns whether setting changes should be logged
    virtual bool LogChanges() const;

    //! returns true if between the last call of ResetSecret and now, a secret setting was changed.
    static bool SettingWasSecret();

    //! initializes secrecy test time
    static void ResetSecret();
};

//! setting that has some immediate action
class ActionSetting: public Setting
{
public:
    ActionSetting( Type type, char const * name );

    //! returns whether the setting was changed away from the default value
    virtual bool IsDefault() const;

    //! reverts to default values
    virtual void Revert();
private:
    //! writes the value to a stream
    virtual void DoWrite( std::ostream & dest ) const;

    //! writes the setting value to the tag
    virtual void DoWrite( TagWriter & writer ) const;
};

//! setting that has some immediate action on a string value
class StringActionSetting: public ActionSetting
{
public:
    StringActionSetting( Type type, char const * name );

    //! activates the action with the specified value
    virtual void Activate( std::string const & val ) = 0;
private:
    //! reads the value of the setting from a stream.
    virtual void DoRead( std::istream & source );
};

//! integer valued setting
class IntegerSetting: public Setting
{
public:
    //! defines default, minimal and maximal values
    IntegerSetting( Type type, char const * name, int def = 0, int max = -1, int min = 0 );

    //! transform to the value
    operator int() const;

    //! set the value
    void Set( int val );

    //! get the value
    int Get() const { return value; }

    int GetMin() const { return min; }
    int GetMax() const { return max; }

    void SetMax( int max_ );
    void SetMin( int min_ );

    void SetDefault( int def );

    void Lock()
    {
        SetMax( Get() );
        SetMin( Get() );
    }

    //! returns whether the setting was changed away from the default value
    virtual bool IsDefault() const;

    //! reverts to default values
    virtual void Revert();
protected:
    //! call on every change
    void OnChange();
private:
    //! reads the value of the setting from a stream.
    virtual void DoRead( std::istream & source );

    //! writes the value to a stream
    virtual void DoWrite( std::ostream & dest ) const;

    //! writes the setting value to the tag
    virtual void DoWrite( TagWriter & writer ) const;

    int value;   //!< the current value
    int def;     //!< the default value
    int min;     //!< the minimal value
    int max;     //!< the maximal value
};

//! float valued setting
class FloatSetting: public Setting
{
public:
    //! defines default, minimal and maximal values
    FloatSetting( Type type, char const * name, float def );

    //! transform to the value
    operator float() const;

    //! set the value
    void Set( float val );

    //! get the value
    float Get() const { return value; }

    //! returns whether the setting was changed away from the default value
    virtual bool IsDefault() const;

    //! reverts to default values
    virtual void Revert();
protected:
    //! call on every change
    void OnChange();
private:
    //! reads the value of the setting from a stream.
    virtual void DoRead( std::istream & source );

    //! writes the value to a stream
    virtual void DoWrite( std::ostream & dest ) const;

    //! writes the setting value to the tag
    virtual void DoWrite( TagWriter & writer ) const;

    float value;   //!< the current value
    float def;     //!< the default value
};

//! enum valued setting
class EnumSetting: public IntegerSetting
{
public:
    //! defines value names, start value and default value
EnumSetting( Type type, char const * name, char const * * values_, int min = 0, int def = 0 )
: IntegerSetting( type, name, def, min + Count( values_ ) - 1, min ), values( values_ ), alternativeValues( 0 ){}

    //! reads the value of the setting from a stream.
    virtual void DoRead( std::istream & source );

    //! set the value
    void Set( int val );

    // set value by name
    void Set( std::string const & val );

    // sets value names
    void SetValues( char const * * v );

    // sets alternative value names
    void SetAlternativeValues( char const * * v )
    {
        assert( Count( values ) == Count( v ) );
        alternativeValues = v;
    }
private:
#ifdef COMPILE_DEDWINIA
    // returns the setting class
    virtual Class GetClass() const { return Enum; }
#endif

    static unsigned int Count( char const * * values );
    static int Find( char const * * values, char const * value );

    char const * * values;
    char const * * alternativeValues;
};

//! enum valued setting
class BoolSetting: public EnumSetting
{
public:
    //! defines default, minimal and maximal values
    BoolSetting( Type type, char const * name, int def );
};

//! setting that has some immediate action on an integer value
class IntegerActionSetting: public ActionSetting
{
public:
    IntegerActionSetting( Type type, char const * name );

    //! activates the action with the specified value
    virtual void Activate( int val ) = 0;
private:
    //! reads the value of the setting from a stream.
    virtual void DoRead( std::istream & source );
};

// create an object of this class to install hooks
class SettingHooks
{
public:
    SettingHooks();
    virtual ~SettingHooks();

    //! returns a list of important settings
    static Setting * * GetImportantSettings();

    //! runs checks on setting changes
    static void RunChecks();
private:
    //! returns a list of important settings
    virtual Setting * * DoGetImportantSettings() = 0;

    //! runs checks on setting changes
    virtual void DoRunChecks() = 0;
};

#endif // DEDCON_Settings_INCLUDED
