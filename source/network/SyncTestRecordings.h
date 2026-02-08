#include <memory>
#include <queue>
#include <string>

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
    bool                               m_writeResults;
    std::queue<std::string>            m_paths;
    std::string                        m_currentRecordingFilename;
    double                             m_currentRecordingStartTime;

public:

    SyncTestRecordings();
    void Initialise();
    void Update();
    bool IsEnabled();

};
