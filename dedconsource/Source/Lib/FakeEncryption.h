/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_FakeEncryption_INCLUDED
#define DEDCON_FakeEncryption_INCLUDED

// all that is supported in the community source version: all the same, no encryption
enum ENCRYPTION_VERSION{
    NO_ENCRYPTION = 0,
    METASERVER_ENCRYPTION = NO_ENCRYPTION,
    PLEBIAN_ENCRYPTION = NO_ENCRYPTION,
};

#endif // DEDCON_FakeEncryption_INCLUDED
