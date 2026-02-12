#ifndef RECORDING_PLAYBACK_CONTROLLER_H
#define RECORDING_PLAYBACK_CONTROLLER_H

#include <string>
#include "lib/tosser/llist.h"
#include "recordings/RecordingWriter.h"

class Server;
class ServerToClientLetter;
struct DcgrHeader;

class RecordingController
{
public:
    RecordingController(Server *server);
    ~RecordingController();

    bool LoadRecording(const std::string &filename);
    void Reset();

    void SetPaused(bool paused);
    void SetSpeed (float speed);

    void EnableSeeking (int targetSeqId);
    void DisableSeeking();

    bool IsActive            () const;
    bool IsPaused            () const;
    bool IsRecordingFinished () const;
    bool IsInCPUTakeover     () const;
    bool IsInFastForward     () const;
    bool IsSeeking           () const;
    bool MarkFinished        ();                             // Call when history runs out
    bool EnableCPUTakeover   ();                             // Convert human teams to AI and enable CPU takeover

    int   GetCurrentSeqId() const;
    int   GetStartSeqId  () const;
    int   GetEndSeqId    () const;
    float GetSpeed       () const;
    int   GetHistorySize () const;

    float GetAdvanceSpeedMultiplier() const;                 // Get current speed multiplier for time advancement

    const std::string& GetFilename () const;
    const DcgrHeader&  GetHeader   () const;

    ServerToClientLetter* GetNextRecordedLetter();           // Returns next letter to inject, or NULL if not ready
    ServerToClientLetter* GetHistoryLetter     (int index) const;

private:
    Server *m_server;

    bool m_active;                                           // Is playback system active?
    std::string m_filename;                                  // Current recording filename
    DcgrHeader m_header;                                     // Recording header metadata
    LList<ServerToClientLetter *> m_history;                 // All recorded messages

    int m_currentSeqId;                                      // Current playback position
    int m_startSeqId;                                        // Sequence ID where game starts from DCGR header
    int m_endSeqId;                                          // Final sequence ID in recording

    bool m_paused;
    bool m_finished;                                         // True when playback reached end of history
    bool m_cpuTakeoverMode;                                 // True when CPU takeover is active (reading from live history)
    float m_speed;                                           // Current playback speed
    double m_lastAdvanceTime;                                // Last time we advanced for timing

    bool m_fastForwardMode;                                  // Fast forward mode for speed control

    bool m_seekingMode;                                      // Seeking mode
    int m_seekingTargetSeqId;                                // Target sequence ID for seeking

    void ClearHistory();
    void CheckDisableSeeking();                              // Check if seeking is complete
};

#endif
