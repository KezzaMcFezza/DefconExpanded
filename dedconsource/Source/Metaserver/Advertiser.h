/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_Advertiser_INCLUDED
#define DEDCON_Advertiser_INCLUDED

#include "Lib/Misc.h"
#include "MetaServer.h"

class IntegerSetting;
extern IntegerSetting portForwarding;

//! advertises server
class Advertiser
{
public:
    Advertiser();

    bool TimeToSend( int delay );                      //!< determines whether it is time to send the advertising packet again
    static void WritePacket( AutoTagWriter & writer, bool publishKey ); //!< writes an advertisement packet
    static void WriteRandom( TagWriter & writer ); //!< writes the random seeds (for metaserver and hello packet)
    static void Force(); //!< forces advertising once
private:
    Time next; //!< time of next advertisement
    static int bumpID; //!< artificial bumps that force sends; global variable that gets increased
    int bumpIDThis; //!< artificial bumps that force sends; follows global variable
};

//! advertises server on LAN
class LanAdvertiser: public Advertiser
{
public:
    void Advertise();       //!< does the advertising
};

// advertises server on the internet. Also holds contact to the metaserver.
class InternetAdvertiser: public Advertiser, public MetaServer
{
public:
    ~InternetAdvertiser();

    void Advertise(); //!< does the advertising
    void Stop();      //!< stops advertising

    static void WriteRecording( TagWriter & writer ); //! writes playback chunk
    static bool ParsePlayback( std::string const & key, TagReader & reader ); //! analyses playback chunk
};

#endif // DEDCON_Advertiser_INCLUDED
