#ifndef RECORDING_WRITER_H
#define RECORDING_WRITER_H

#include <string>

class Server;
class ClientToServer;

//
// DCGR header:
// first packet in a .dcrec file. same format as Dedcon.

struct DcgrHeader
{
    int         nextClientId;   // cid  – next client id to assign
    int         gameStartSeqId; // ssid – sequence id when game started
    int         endSeqId;       // esid – last sequence id in the recording
    std::string serverVersion;  // sv   – server version string
    bool        pack;           // pack – compressed flag, false for now as dedcon does have LZMA compression
    DcgrHeader();
};

template<typename SourceType>
struct RecordingSourceTraits;

//
// Writes Defcon server/client history to .dcrec files in the same format
// that RecordingParser and Dedcon produce and expect.

class RecordingWriter
{
public:
    template<typename SourceType>
    bool WriteToFile( SourceType *source, const std::string &filename );
    
    template<typename SourceType>
    bool SaveToRecordingsFolder( SourceType *source );
};

#endif
