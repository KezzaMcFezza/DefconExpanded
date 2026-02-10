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

    void EnableFastForward(int targetSeqId, float speedMultiplier = 500.0f);

    bool IsActive       () const;
    bool IsPaused       () const;
    bool IsInFastForward() const;

    int   GetCurrentSeqId() const;
    int   GetStartSeqId  () const;
    int   GetEndSeqId    () const;
    float GetSpeed       () const;
    int   GetHistorySize () const;

    const std::string& GetFilename() const;
    const DcgrHeader&  GetHeader  () const;

    void  CheckDisableFastForward  ();                           // Check if we've reached fast-forward target
    float GetAdvanceSpeedMultiplier() const;                     // Get current speed multiplier for time advancement

    ServerToClientLetter* GetNextRecordedLetter();               // Returns next letter to inject, or NULL if not ready
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
    float m_speed;                                           // Current playback speed
    double m_lastAdvanceTime;                                // Last time we advanced for timing

    bool m_fastForwardMode;
    int m_fastForwardTargetSeqId;
    float m_fastForwardSpeed;

    void ClearHistory();
};

#endif
