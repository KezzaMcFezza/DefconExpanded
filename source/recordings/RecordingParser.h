#include <iostream>
#include <string>
#include <fstream>
#include <vector> 

#include "SecChecksum.h"
#include "RecordingWriter.h"

#define PREFS_RECORDING_ENABLE_CHECKSUM    "RecordingEnableChecksum"
#define PREFS_RECORDING_END_BEHAVIOUR      "RecordingEndBehaviour"

class Directory;
class Server;

class RecordingParser
{
public:
    RecordingParser           ( std::istream &in, const std::string &filename, Server *server );
    RecordingParser           ( const std::string &filename, Server *server );  // Direct file loading
    ~RecordingParser          ();

    bool ParseRecording       ( bool sendDirectly, bool debugPrint = false, bool trackSyncBytes = false );
    bool Parse                ( bool debugPrint );
    bool ReadHeaderPacket     ( Directory &matchHeader );   

    static bool ExtractHeader ( const std::string &filename, DcgrHeader &header, Server *server );

    const DcgrHeader &GetParsedHeader() const;

private:
    std::istream               *m_in;                                   // Changed to pointer for optional ownership
    DcgrHeader                 m_parsedHeader;                          // Filled when ParseRecording reads DCGR
    std::ifstream              m_fileStream;                            // Own file stream when loading directly
    std::string                m_filename;
    Server                     *m_server;
    bool                       m_ownStream;                             // Track if we own the stream

    bool ReadPacket            ( Directory &dir, bool &zeroMarker, SecChecksum *checksumToAdd = nullptr, std::vector<char> *rawBytesOut = nullptr );
    void AddToHistory          ( Directory *dir );
    void Send                  ( Directory *dir );
};
