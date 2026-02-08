#include <iostream>
#include <string>
#include <fstream> 

class Directory;
class Server;

class RecordingParser
{
public:
    RecordingParser           ( std::istream &in, const std::string &filename, Server *server );
    RecordingParser           ( const std::string &filename, Server *server );  // Direct file loading
    ~RecordingParser          ();

    bool ParseToHistory       ();                                       // Load into server history for playback
    bool ReadHeaderPacket     ( Directory &matchHeader );               // Made public for header parsing

private:
    std::istream               *m_in;                                   // Changed to pointer for optional ownership
    std::ifstream              m_fileStream;                            // Own file stream when loading directly
    std::string                m_filename;
    Server                     *m_server;
    bool                       m_ownStream;                             // Track if we own the stream

    bool ReadPacket            ( Directory &dir, bool &zeroMarker );
    void AddToHistory          ( Directory *dir );
    int  ExtractGameStartFromHeader  ();                                // Extract game start from DCGR header
};
