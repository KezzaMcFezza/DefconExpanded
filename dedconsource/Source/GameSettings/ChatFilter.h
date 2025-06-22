/*
 *  * DedCon: Dedicated Server for Defcon (and Multiwinia)
 *  *
 *  * Copyright (C) 2008 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_ChatFilter_INCLUDED
#define DEDCON_ChatFilter_INCLUDED

#include <vector>
#include <string>

class Client;
class ChatFilter;

class ChatFilterSystem
{
    typedef std::vector< ChatFilter * > Filters;

public:
    ChatFilterSystem();

    ~ChatFilterSystem();

    // retunrs the last created filter
    ChatFilter * GetLast()
    {
        return lastFilter;
    }

    // returns the singleton chat filter setting
    static ChatFilterSystem & Get();

    // filters chat. Returns true if it is to be suppressed.
    bool Filter( Client & chatter, std::string & chat ) const;

    // adds a chat filter
    void AddFilter( ChatFilter * filter );

    // clears all filters
    void Clear();
private:

    ChatFilter * lastFilter;
    Filters filters;
};

#endif // DEDCON_ChatFilter_INCLUDED
