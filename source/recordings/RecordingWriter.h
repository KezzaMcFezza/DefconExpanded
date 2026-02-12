#ifndef RECORDING_WRITER_H
#define RECORDING_WRITER_H

#include <string>

class Server;
class ClientToServer;

#define PREFS_RECORDING_SAVE_LOCATION       "RecordingSaveLocation"
#define PREFS_RECORDING_ENABLE_COMPRESSION  "RecordingEnableCompression"
#define PREFS_RECORDING_NAMING_FORMAT       "RecordingNamingFormat"
#define PREFS_RECORDING_AUTO_SAVE_BEHAVIOUR "RecordingAutoSaveBehaviour"

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

enum SaveRecordingResult
{
    SaveRecordingResult_Leave,         // Leave game (no save, or already auto saved)
    SaveRecordingResult_ShowSaveDialog // Show the save recording dialog
};

class RecordingWriter
{
public:
    static SaveRecordingResult SaveRecording();

    template<typename SourceType>
    bool WriteToFile( SourceType *source, const std::string &filename );
    
    template<typename SourceType>
    bool SaveToRecordingsFolder( SourceType *source );
};

#endif
