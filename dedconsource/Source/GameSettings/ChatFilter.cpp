/*
 *  DedCon: Dedicated Server for Defcon (and Multiwinia)
 *
 *  Copyright (C) 2008 Manuel Moos
 *
 */

#include "Main/config.h"
#include "ChatFilter.h"
#include "Main/Log.h"
#include "Server/Host.h"
#include "Network/Exception.h"

#include <ios>
#include <iomanip>
#include <regex>
#include <string>

#ifdef _WIN32
    #define strcasecmp _stricmp
#endif

#ifdef HAVE_STRCASESTR
char const * strcasestr( char const * haystack, char const * needle )
{
    // Not very efficient, but works
    while( *haystack )
    {
        if ( strcasecmp( haystack, needle ) == 0 )
        {
            return haystack;
        }
        ++haystack;
    }
    return 0;
}
#endif

extern FStreamProxy GetGriefLog( Client const & griefer, char const * grief, bool eventLog = true );

// Exception to throw when a chat line should be censored.
struct ChatFilterBlock{};

// Exception to throw when processing should be aborted.
struct ChatFilterAbort{};

// Action on chat filtering
class ChatFilterAction
{
public:
    virtual void OnTrigger( ChatFilter const & filter, Client & foulmouth, std::string & chat ) = 0;
    virtual ~ChatFilterAction(){}
};

// An actual filter
class ChatFilter
{
    friend class ChatFilterSystem;

public:
    typedef std::vector< ChatFilterAction * > Actions;

    // Initializes the filter
    ChatFilter( std::string const & pattern_, bool wordsOnly_ )
    : wordsOnly( wordsOnly_ )
    {
        next = NULL;
        pattern = pattern_;

        try {
            patternRegex = std::regex(pattern, std::regex::icase);
        } catch (const std::regex_error &e) {
            Log::Err() << "Error compiling regular expression \"" << pattern << "\" : " << e.what() << "\n";
        }
    }

    // Checks a chat line
    void Check( Client & chatter, std::string & chat ) const
    {
        if ( Match( chat.c_str() ) )
        {
            ChatFilter const * run = this;
            while ( run )
            {
                for( Actions::const_iterator i = run->actions.begin(); i != run->actions.end(); ++i )
                {
                    (*i)->OnTrigger( *this, chatter, chat );
                }
                run = run->next;
            }
        }
    }

    ~ChatFilter()
    {
        for( Actions::iterator i = actions.begin(); i != actions.end(); ++i )
        {
            delete *i;
            *i = 0;
        }
    }

    void Replace( std::string & chat, std::string const & replacement ) const
    {
        int lastoffset = 0;
        while( true )
        {
            int offset = 0, size = 0;
            if ( !Match( chat.c_str() + lastoffset, offset, size ) )
            {
                break;
            }
            
            offset = offset + lastoffset;

            std::string thisReplacement = replacement;
            // Check whether original is all upper or lowercase
            bool allUpper = true;
            bool allLower = true;
            for( int i = size-1; i >= 0; --i )
            {
                allUpper &= !islower( chat[offset+i] );
                allLower &= !isupper( chat[offset+i] );
            }
            
            // Change the replacement to match the original case
            if ( allUpper )
            {
                for( int i = thisReplacement.size()-1; i >= 0; --i )
                {
                    thisReplacement[i] = toupper( thisReplacement[i] );
                }
            }
            if ( allLower )
            {
                for( int i = thisReplacement.size()-1; i >= 0; --i )
                {
                    thisReplacement[i] = tolower( thisReplacement[i] );
                }
            }

            // Replace the match
            chat = std::string( chat, 0, offset ) + thisReplacement + std::string( chat, offset + size );
            lastoffset = offset + replacement.size();
        }
    }
    
    void AddAction( ChatFilterAction * action )
    {
        actions.push_back( action );
    }

protected:
    bool Match( char const * chat ) const
    {
        int offset = 0, size = 0;
        return Match( chat, offset, size );
    }

    bool Match( char const * chat, int & offset, int & size ) const
    {
        std::cmatch matchResult;
        if ( std::regex_search(chat, matchResult, patternRegex) )
        {
            offset = matchResult.position();
            size = matchResult.length();
            return true;
        }
        return false;
    }

private:
    std::regex patternRegex;  // Using C++ regex
    std::string pattern;
    Actions actions;
    ChatFilter * next;
    bool wordsOnly;
};

class ChatFilterSetting: public ActionSetting
{
public:
    ChatFilterSetting( char const * name, bool wordsOnly_ )
    : ActionSetting( Setting::Grief, name ), wordsOnly( wordsOnly_ )
    {
    }
private:
    virtual void DoRead( std::istream & s )
    {
        std::ws( s );
        std::string pattern;
        std::getline( s, pattern );
        
        ChatFilterSystem::Get().AddFilter( new ChatFilter( pattern, wordsOnly ) );
    }

    // deletes all filters
    virtual void Revert()
    {
        ChatFilterSystem::Get().Clear();
    }

    bool wordsOnly;
};

ChatFilterSetting chatFilter( "ChatFilter", false );
ChatFilterSetting chatFilterWord( "ChatFilterWord", true );

ChatFilterSystem::ChatFilterSystem(){}

void ChatFilterSystem::Clear()
{
    lastFilter = 0;
    
    for( Filters::iterator i = filters.begin(); i != filters.end(); ++i )
    {
        delete *i;
        *i = 0;
    }

    filters.clear();
}

ChatFilterSystem::~ChatFilterSystem()
{
    Clear();
}

// filters chat. Returns true if it is to be suppressed.
bool ChatFilterSystem::Filter( Client & chatter, std::string & chat ) const
{
    try
    {
        for( Filters::const_iterator i = filters.begin(); i != filters.end(); ++i )
        {
            (*i)->Check( chatter, chat );
        }
        
        return false;
    }
    catch( ChatFilterAbort const & )
    {
        return false;
    }
    catch( ChatFilterBlock const & )
    {
        return true;
    }
}

void ChatFilterSystem::AddFilter( ChatFilter * filter )
{
    // chain unused filters
    if ( lastFilter && lastFilter->actions.size() == 0 )
        lastFilter->next = filter;

    filters.push_back( lastFilter = filter );
}

static ChatFilterSystem chatSystem;

ChatFilterSystem & ChatFilterSystem::Get()
{
    return chatSystem;
}

static IntegerSetting chatFilterSilence( Setting::Grief, "ChatFilterSilence", 0 );
static IntegerSetting chatFilterKick( Setting::Grief, "ChatFilterKick", 0 );
static IntegerSetting chatFilterGrief( Setting::Grief, "ChatFilterGrief", 0 );

// warns the player
class ChatFilterActionWarn: public ChatFilterAction
{
public:
    bool Threshold( int current, int thresh )
    {
        return ( current < thresh && current + warnLevel >= thresh );
    }

    virtual void OnTrigger( ChatFilter const & filter, Client & foulmouth, std::string & chat )
    {
        // warn
        if ( warning.size() > 0 && ( !once || !warned ) )
        {
            if ( priv )
            {
                Host::GetHost().Say( warning, foulmouth );
            }
            else
            {
                Host::GetHost().Say( warning );
            }
            
            warned = true;
        }

        // check punishment thresholds
        if ( Threshold( foulmouth.chatFilterWarnings, chatFilterSilence ) )
        {
            foulmouth.Silence();
        }

        if ( Threshold( foulmouth.chatFilterWarnings, chatFilterKick ) )
        {
            foulmouth.Quit( SuperPower::Kick );
            throw ReadException();
        }

        if ( Threshold( foulmouth.chatFilterWarnings, chatFilterGrief ) )
        {
            GetGriefLog( foulmouth, "chatFilter" );
        }

        foulmouth.chatFilterWarnings += warnLevel;
        if( foulmouth.chatFilterWarnings < 0 )
        {
            foulmouth.chatFilterWarnings = 0;
        }

        if ( block )
        {
            throw ChatFilterBlock();
        }
    }
        
    ChatFilterActionWarn( int warnLevel_, std::string const & warning_, bool priv_, bool block_, bool once_ )
    : warnLevel( warnLevel_ ), warning( warning_ ), priv( priv_ ), block( block_ ), once( once_ ), warned( false )
    {
    }
private:
    int warnLevel;        // the warn level to add
    std::string warning;  // the warning message
    bool priv;            // whether the warning is private or public
    bool block;           // whether the chat should be blocked
    bool once;            // whether the warning should only be printed once

    bool warned;          // whether the message has been printed already
};

static char const * noChatFilter = "No chat filter defined yet. You need a ChatFilter/ChatFilterWord setting before this one.\n";

class ChatWarnSetting: public ActionSetting
{
public:
    ChatWarnSetting( char const * name, bool priv_, bool block_, bool readWarnLevel_, bool once_ )
    : ActionSetting( Setting::Grief, name )
    , priv( priv_ ), block( block_ ), readWarnLevel( readWarnLevel_ ), once( once_ )
    {
    }

    void Create( std::string const & warning, int warnLevel )
    {
        ChatFilter * filter = ChatFilterSystem::Get().GetLast();
        if ( !filter )
        {
            Log::Err() << noChatFilter;
            return;
        }

        filter->AddAction( new ChatFilterActionWarn( warnLevel, warning, priv, block, once ) );
    }

    void DoRead( std::istream & s )
    {
        int warnLevel = 0;
        if ( readWarnLevel )
        {
            s >> warnLevel;

            if ( s.eof() || !s.good() )
            {
                Log::Err() << GetName() << " requires a warning level as first argument.\n";
            }
        }
        std::ws( s );

        std::string warning;
        std::getline( s, warning );
        
        Create( warning, warnLevel );
    }
private:
    bool priv, block; // passed on to ChatFilterActionWarn
    bool readWarnLevel; // should a warn level be read from the command line?
    bool once; // should the message only be printed once?
};

static ChatWarnSetting chatWarn0( "ChatFilterWarn", false, false, true, false );
static ChatWarnSetting chatWarn1( "ChatFilterWarnPrivate", true, false, true, false );
static ChatWarnSetting chatWarn2( "ChatFilterRespond", false, false, false, false );
static ChatWarnSetting chatWarn3( "ChatFilterRespondPrivate", true, false, false, false );
static ChatWarnSetting chatWarn5( "ChatFilterBlockPrivate", true, true, true, false );

static ChatWarnSetting chatWarn6( "ChatFilterWarnOnce", false, false, true, true );
static ChatWarnSetting chatWarn7( "ChatFilterRespondOnce", false, false, false, true );

// warns the player
class ChatFilterActionReplace: public ChatFilterAction
{
public:
    virtual void OnTrigger( ChatFilter const & filter, Client & foulmouth, std::string & chat )
    {
        filter.Replace( chat, replace );
    }
        
    ChatFilterActionReplace( std::string const & replace_ )
    : replace( replace_ )
    {
    }
private:
    std::string replace;  // what to replace the find with
};

class ChatReplaceSetting: public ActionSetting
{
public:
    ChatReplaceSetting()
    : ActionSetting( Setting::Grief, "ChatFilterReplace" )
    {
    }

    void Create( std::string const & replace )
    {
        ChatFilter * filter = ChatFilterSystem::Get().GetLast();
        if ( !filter )
        {
            Log::Err() << noChatFilter;
            return;
        }

        filter->AddAction( new ChatFilterActionReplace( replace ) );
    }

    void DoRead( std::istream & s )
    {
        std::ws( s );

        std::string replace;
        std::getline( s, replace );
        
        Create( replace );
    }
};
static ChatReplaceSetting chatReplace;

// warns the player
class ChatFilterActionAbort: public ChatFilterAction
{
public:
    virtual void OnTrigger( ChatFilter const & filter, Client & foulmouth, std::string & chat )
    {
        throw ChatFilterAbort();
    }
        
    ChatFilterActionAbort()
    {
    }
};

class ChatAbortSetting: public ActionSetting
{
public:
    ChatAbortSetting()
    : ActionSetting( Setting::Grief, "ChatFilterAbort" )
    {
    }

    void Create()
    {
        ChatFilter * filter = ChatFilterSystem::Get().GetLast();
        if ( !filter )
        {
            Log::Err() << noChatFilter;
            return;
        }

        filter->AddAction( new ChatFilterActionAbort() );
    }

    void DoRead( std::istream & s )
    {
        Create();
    }
};
static ChatAbortSetting chatAbort;
