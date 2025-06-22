/*
 *  * DedCon: Dedicated Server for Defcon (and Multiwinia)
 *  *
 *  * Copyright (C) 2008 Manuel Moos
 *  *
 *  */

#include "Rotator.h"
#include "GameSettings.h"
#include "Main/Log.h"

#include <map>
#include <vector>
#include <deque>
#include <string>
#include <fstream>
#include <sstream>

#include <stdlib.h>
#include <math.h>

class RandomItem;

class RandomPending: public NestedPending
{
public:
    RandomPending(){};
};

// one element that is to be randomly selected
class RandomItem
{
public:
    std::string name; //! the name of the random item
    float malus; //!< the bad mojo this item has accumulated over the past runs

    RandomItem()
    : malus(0)
    {
    }

    virtual ~RandomItem(){}

    //! called when the random item is actually activated
    //! returns true if execution is finished
    static void Activate( RandomItem * item )
    {
        // create pending command
        RandomPending * pending = new RandomPending();

        // fill it
        item->DoActivate( pending->reader );

        // and activate it
        SettingsReader::AddPending( pending );
    }
private:
    //! called when the random item is actually activated
    virtual void DoActivate( SettingsReader & reader ) = 0;
};

// array of memories
class Memory;
static std::vector< Memory * > memories;

// remembering stuff from previous runs
class Memory
{
public:
    Memory()
    {
        memories.push_back( this );
    }
    virtual ~Memory(){}
    virtual void Save(){}
protected:
    std::string filename;
};

static FloatSetting weightFluctuation( Setting::Flow, "WeightFluctuation", 900 );
static FloatSetting weightExponent( Setting::Flow, "WeightExponent", 1.5 );

// memory for map rotation
class WeightMemory: public Memory
{
public:
    ~WeightMemory()
    {
        Clear();
    }

    WeightMemory( std::string const & filename, std::deque< RandomItem * > & takeOver )
    {
        std::swap( takeOver, stack );

        this->filename = filename;
        selected = 0;
        start.GetTime();
    }

    void Activate()
    {
        start.GetTime();

        std::ifstream s( filename.c_str() );
        while( !s.eof() && s.good() )
        {
            // read malus and name of setting
            float malus;
            std::string name;

            s >> malus;
            std::ws( s );
            std::getline( s, name );
            
            if ( !s.eof() )
            {
                // find item
                for( std::deque< RandomItem * >::const_iterator i = stack.begin(); i != stack.end(); ++i )
                {
                    if ( (*i)->name == name && (*i)->malus <= 0 )
                    {
                        (*i)->malus = malus;
                    }
                }
            }
        }

        // randomize
        float leastMalus = 0;
        float originalLeastMalus = 0;
        for( std::deque< RandomItem * >::const_iterator i = stack.begin(); i != stack.end(); ++i )
        {
            float fluctuation = pow( float(rand())/RAND_MAX * weightFluctuation.Get(), weightExponent.Get() );
            float malus = (*i)->malus + fluctuation;
            if ( malus < leastMalus || !selected )
            {
                selected = (*i);
                leastMalus = malus;
                originalLeastMalus = (*i)->malus;
            }
        }

        // normalize malus
        for( std::deque< RandomItem * >::const_iterator i = stack.begin(); i != stack.end(); ++i )
        {
            (*i)->malus -= originalLeastMalus - 1.0;
        }

        // activate item
        assert( selected );
        RandomItem::Activate( selected );
    }

    virtual void Save()
    {
        if ( !selected )
        {
            return;
        }

        Time end;
        end.GetTime();
        int waited = ( end - start ).Seconds();
        
        selected->malus += pow( waited, weightExponent.Get() );
        
        std::ofstream s( filename.c_str() );
        if( !s.good() )
        {
            Log::Err() << "Could not open " << filename << " for writing.\n";
            return;
        }

        for( std::deque< RandomItem * >::const_iterator i = stack.begin(); i != stack.end(); ++i )
        {
            s << (*i)->malus << '\t' << (*i)->name << '\n';
        }
        
    }

    void Clear()
    {
        // clear stack
        while( stack.size() > 0 )
        {
            delete stack.front();
            stack.pop_front();
        }
    }

private:
    Time start; // time the random item was selected
    std::deque< RandomItem * > stack; // the random selections
    RandomItem * selected;            // which item wass selected
};

// actually randomizes settings
class Randomizer
{
public:
    // adds a new random item, taking control of it
    void Add( RandomItem * item )
    {
        stack.push_back( item );
    }

    bool Empty()
    {
        // select item randomly
        if ( stack.size() == 0 )
        {
            Log::Err() << "No random rotation items defined, nothing to do.\n";
            return true;
        }

        return false;
    }

    // activates all settings to test them
    void ActivateAll()
    {
        for( std::deque< RandomItem * >::iterator i = stack.begin(); i != stack.end(); ++i )
        {
            RandomItem::Activate( *i );
        }
    }

    // select one of the added elements randomly
    void Randomize()
    {
        if ( Empty() ) return;

        if ( settings.dryRun )
        {
            ActivateAll();
            return;
        }
        
        // crude, really just randomize
        int r = int( (float(rand())*stack.size())/RAND_MAX );
        r = r % stack.size();
        
        // select random item
        RandomItem * selected = stack[r];

        // and activate it
        RandomItem::Activate( selected );
        
        // clean up
        Clear();
    }

    // rotate through settings
    void Rotate( std::string const & filename )
    {
        if ( Empty() ) return;

        if ( settings.dryRun )
        {
            ActivateAll();
            return;
        }
        
        // fetch last setting from file
        int last = -1;
        {
            std::ifstream read( filename.c_str() );
            read >> last;
        }

        // rotate
        last = (last+1) % stack.size();

        // save
        {
            std::ofstream write( filename.c_str() );
            write << last << "\n";
        }

        // pick item
        RandomItem * selected = stack[last];

        // and activate it
        RandomItem::Activate( selected );
        
        // clean up
        Clear();
    }

    // rotate through settings
    void Weight( std::string const & filename )
    {
        if ( Empty() ) return;

        if ( settings.dryRun )
        {
            ActivateAll();
            return;
        }
        
        (new WeightMemory( filename, stack ))->Activate();
    }

    Randomizer()
    {
    }

    ~Randomizer()
    {
        Clear();
    }

    void Clear()
    {
        // clear stack
        while( stack.size() > 0 )
        {
            delete stack.front();
            stack.pop_front();
        }
    }
private:
    // stack of random items currently filled
    std::deque< RandomItem * > stack;
};

static Randomizer randomizer;
 
//! randomizes the previous selection
class RandomizeSetting: public ActionSetting
{
public:
    RandomizeSetting():ActionSetting( Flow, "Randomise" ) {}

    //! reads the file to include
    virtual void DoRead( std::istream & source )
    {
        randomizer.Randomize();
    }
};
static RandomizeSetting randomizeSetting;

//! rotates through the previous selection
class RotateSetting: public StringActionSetting
{
public:
    RotateSetting():StringActionSetting( Flow, "Rotate" ) {}

    //! reads the file to include
    virtual void Activate( std::string const & filename )
    {
        randomizer.Rotate( Time::GetCurrentTimeString( filename.c_str() ) );
    }
};
static RotateSetting rotateSetting;

//! randomizes the previous selection, prefering favorite settings
class WeightSetting: public StringActionSetting
{
public:
    WeightSetting():StringActionSetting( Flow, "Weight" ) {}

    //! reads the file to include
    virtual void Activate( std::string const & filename )
    {
        randomizer.Weight( Time::GetCurrentTimeString( filename.c_str() ) );
    }
};
static WeightSetting weightSetting;

// random item including a file
class RandomItemInclude: public RandomItem
{
public:
    RandomItemInclude( std::string const & file_ )
    : file( file_ )
    {
        name = "RINCLUDE " + file_;
    }

    //! called when the random item is actually activated
    virtual void DoActivate(  SettingsReader & reader )
    {
        // read the file to include
        reader.Read( file.c_str() );
    }

private:
    std::string file;
};

//! loads a configuration file and forks it
class RandomIncludeSetting: public ActionSetting
{
public:
    RandomIncludeSetting():ActionSetting( Flow, "RInclude" )
    {
    }

    //! reads the file to include
    virtual void DoRead( std::istream & source )
    {
        std::string filename;
        std::ws(source);
        getline( source, filename );

        randomizer.Add( new RandomItemInclude( filename ) );
    }
};

static RandomIncludeSetting randomIncludeSetting;

// random item including a file
class RandomItemFork: public RandomItem
{
public:
    RandomItemFork( std::string const & file_ )
    : file( file_ )
    {
        name = "RFORK " + file_;
    }

    //! called when the random item is actually activated
    virtual void DoActivate(  SettingsReader & reader )
    {
        // spawn a child reader and make it read the file
        SettingsReader * child = reader.Spawn();
        child->Read( file.c_str() );
    }

private:
    std::string file;
};

//! loads a configuration file and forks it
class RandomForkSetting: public ActionSetting
{
public:
    RandomForkSetting():ActionSetting( Flow, "RFork" )
    {
    }

    //! reads the file to include
    virtual void DoRead( std::istream & source )
    {
        std::string filename;
        std::ws(source);
        getline( source, filename );

        randomizer.Add( new RandomItemFork( filename ) );
    }
};

static RandomForkSetting randomForkSetting;

#ifdef COMPILE_DEDWINIA
// random item including a file
class RandomItemMap: public RandomItem
{
public:
    RandomItemMap( std::string const & map_ )
    : map( map_ )
    {
        name = "RMAP " + map;
    }

    //! called when the random item is actually activated
    virtual void DoActivate(  SettingsReader & reader )
    {
        // load the map
        settings.mapFile.Set( map );
    }

private:
    std::string map;
};

//! sets a map
class RandomMapSetting: public ActionSetting
{
public:
    RandomMapSetting():ActionSetting( Flow, "RMapFile" )
    {
    }

    //! reads the file to include
    virtual void DoRead( std::istream & source )
    {
        std::string map;
        std::ws(source);
        getline( source, map );

        randomizer.Add( new RandomItemMap( map ) );
    }
};

static RandomMapSetting randomMapSetting;
#endif

// random item including a file
class RandomItemSet: public RandomItem
{
public:
    RandomItemSet( std::string const & command_ )
    : command( command_ )
    {
        name = "RSET " + command;
    }

    //! called when the random item is actually activated
    virtual void DoActivate(  SettingsReader & reader )
    {
        // read the file to include
        reader.AddLine( name, command );
    }

private:
    std::string command;
};

//! loads a configuration file and forks it
class RandomSetSetting: public ActionSetting
{
public:
    RandomSetSetting():ActionSetting( Flow, "RSet" )
    {
    }

    //! reads the file to include
    virtual void DoRead( std::istream & source )
    {
        std::string command;
        std::ws(source);
        getline( source, command );

        randomizer.Add( new RandomItemSet( command ) );
    }
};

static RandomSetSetting randomSetSetting;

// random item including a file
class RandomItemMultiSet: public RandomItem
{
    friend class RandomBeginSetting;
public:
    RandomItemMultiSet( std::string const & name )
    {
        this->name = "RMULTISET " + name;
    }

    //! called when the random item is actually activated
    virtual void DoActivate(  SettingsReader & reader )
    {
        // execute the settings
        while( commands.size() > 0 )
        {
            SettingsReader::Line const & line = commands.front();
            reader.AddLine( line.name, line.line );
            commands.pop_front();
        }
    }

private:
    std::deque< SettingsReader::Line > commands;
};

//! loads a configuration file and forks it
class RandomBeginSetting: public ActionSetting
{
public:
    RandomBeginSetting():ActionSetting( Flow, "RBegin" )
    {
    }

    //! reads the file to include
    virtual void DoRead( std::istream & source )
    {
        std::string name;
        std::ws(source);
        if ( !source.eof() )
        {
            getline( source, name );
        }

        RandomItemMultiSet * multiSet = new RandomItemMultiSet( name );

        // get the current settings reader and extract the random setting block
        SettingsReader * reader = SettingsReader::GetActive();
        if ( !reader )
        {
            Log::Err() << "Random rotation only works from configuration files.\n";
            return;
        }

        std::string lineName = reader->GetLine().name;

        int nesting = 1;
        while( nesting > 0 && reader->LinesLeft() > 0 )
        {
            // advance to next line
            reader->Advance();

            SettingsReader::Line const & line = reader->GetLine();
            std::istringstream s( line.line );
            std::string command;
            std::ws(s);
            s >> command;
            if ( command == "RBegin" )
            {
                nesting++;
            }

            if ( command == "REnd" )
            {
                nesting--;
            }

            if ( nesting > 0 )
            {
                multiSet->commands.push_back( line );
            }
        }

        if ( nesting > 0 )
        {
            Log::Err() << lineName << " : End of file while looking for matching REnd.\n";
            delete multiSet;
            return;
        }

        randomizer.Add( multiSet );
    }
};

static RandomBeginSetting randomBeginSetting;

void Rotator::Save()
{
    for( std::vector< Memory * >::iterator i = memories.begin(); i != memories.end(); ++i )
    {
        (*i)->Save();
        delete *i;
    }

    memories.clear();
}
