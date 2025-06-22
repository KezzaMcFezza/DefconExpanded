/*
 *  * DedCon: Dedicated Server for Defcon (and Multiwinia)
 *  *
 *  * Copyright (C) 2008 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_FTP_INCLUDED
#define DEDCON_FTP_INCLUDED

class Client;
class TagReader;
class FTPFile;

class FTPClient;

class FTP
{
public:
    //! sends pending FTP packets
    static void Send();

    //! processes an FTP packet (if broadcast is true, the file is
    //! mirrored to all connected clients)
    static void Process( Client & sender, 
                         TagReader & reader,
                         bool broadcast = false );

    //! loads a file and preparses it for sending. The FTP system
    //! owns the returned file.
    static FTPFile * LoadFile( char const * filename, char const * officialName, bool broadcast = false );

    //! sends an existing file to a client
    static void SendFile( FTPFile * file, Client & client );

    static void DeleteFTPClient( FTPClient * client );

    //! checks whether all FTP transfers (to the specified client or to all clients) are complete
    static bool Complete( Client * client = 0 );
};

#endif // DEDCON_FTP_INCLUDED
