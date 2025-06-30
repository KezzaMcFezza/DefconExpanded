#include <memory>
#include <queue>
#include <string>

// added the preprocessor for visual studio, this means we can have the parsing files included in the configuration but will only get used if we define RECORDING_PARSING
#if RECORDING_PARSING

/*
 * Class that will manage testing the compatibility of a set of DEDCON recordings against
 * the Defcon executable.
 */

class SyncTestRecordings
{
private:
    bool                               m_enabled;
    bool                               m_printRecording;
    int                                m_dumpSyncAt;
    std::queue<std::string>            m_paths;
    std::string                        m_currentRecordingFilename;
    double                             m_currentRecordingStartTime;

public:

    SyncTestRecordings();
    void Initialise();
    void Update();

};

#endif
