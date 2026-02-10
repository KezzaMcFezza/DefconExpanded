#include "lib/universal_include.h"

#include "RecordingController.h"
#include "RecordingWriter.h"
#include "RecordingParser.h"

#include "network/Server.h"
#include "app/app.h"
#include "app/globals.h"
#include "interface/connecting_window.h"
#include "lib/hi_res_time.h"

#include <fstream>

RecordingController::RecordingController(Server *server)
:   m_server(server),
    m_active(false),
    m_currentSeqId(0),
    m_startSeqId(0),
    m_endSeqId(0),
    m_paused(false),
    m_speed(1.0f),
    m_lastAdvanceTime(0.0),
    m_fastForwardMode(false),
    m_fastForwardTargetSeqId(0),
    m_fastForwardSpeed(500.0f)
{
}

RecordingController::~RecordingController()
{
    ClearHistory();
}

void RecordingController::ClearHistory()
{
    for (int i = 0; i < m_history.Size(); ++i)
    {
        if (m_history.ValidIndex(i))
        {
            ServerToClientLetter *letter = m_history[i];
            delete letter;
        }
    }

    m_history.Empty();
}

bool RecordingController::LoadRecording(const std::string &filename)
{
    Reset();

    std::ifstream in(filename.c_str(), std::ios::binary | std::ios::in);
    if (!in)
    {
#ifdef _DEBUG
        AppDebugOut("RecordingController: Failed to open file '%s'\n", filename.c_str());
#endif
        return false;
    }

    //
    // Parse recording into history

    RecordingParser parser(in, filename, m_server);
    if (!parser.ParseRecording( false, false, false ))
    {
#ifdef _DEBUG
        AppDebugOut("RecordingController: Failed to parse recording\n");
#endif
        return false;
    }

    //
    // Copy history from servers temporary recording history into our own

    for (int i = 0; i < m_server->m_recordingParseBuffer.Size(); ++i)
    {
        if (m_server->m_recordingParseBuffer.ValidIndex(i))
        {
            m_history.PutDataAtEnd(m_server->m_recordingParseBuffer[i]);
        }
    }

    m_server->m_recordingParseBuffer.Empty();

    //
    // Extract metadata from header

    if (!RecordingParser::ExtractHeader(filename, m_header, m_server))
    {
#ifdef _DEBUG
        AppDebugOut("RecordingController: Failed to extract header metadata\n");
#endif
        return false;
    }

    m_filename = filename;
    m_startSeqId = m_header.gameStartSeqId;
    m_endSeqId = m_server->m_lastRecordedSeqId;

    //
    // Initialize playback state

    m_active = true;
    m_currentSeqId = 0;
    m_lastAdvanceTime = GetHighResTime();
    m_paused = false;
    m_speed = 1.0f;
    m_fastForwardMode = false;

#ifdef _DEBUG
    AppDebugOut("RecordingController: Loaded '%s' with %d messages (seqId %d-%d)\n",
                filename.c_str(), m_history.Size(), m_startSeqId, m_endSeqId);
#endif

    return true;
}

void RecordingController::Reset()
{
    ClearHistory();
    
    m_active = false;
    m_filename.clear();
    m_currentSeqId = 0;
    m_startSeqId = 0;
    m_endSeqId = 0;
    m_paused = false;
    m_speed = 1.0f;
    m_lastAdvanceTime = 0.0;
    m_fastForwardMode = false;
    m_fastForwardTargetSeqId = 0;
    m_fastForwardSpeed = 500.0f;
}

void RecordingController::SetPaused(bool paused)
{
    if (!m_active)
    {
        return;
    }

    m_paused = paused;
}

void RecordingController::SetSpeed(float speed)
{
    if (!m_active)
    {
        return;
    }

    m_speed = fmax(0.0f, speed);

    if (speed > 0.0f)
    {
        m_paused = false;
    }

    if (speed > 1.0f)
    {
        m_fastForwardMode = true;
        m_fastForwardSpeed = speed;
        m_fastForwardTargetSeqId = m_endSeqId;  // Target end of recording
    }
    else if (speed == 1.0f)
    {
        m_fastForwardMode = false;
        m_fastForwardSpeed = 1.0f;
    }
}

void RecordingController::EnableFastForward(int targetSeqId, float speedMultiplier)
{
    if (!m_active || targetSeqId <= m_currentSeqId)
    {
        return;
    }

    m_fastForwardMode = true;
    m_fastForwardTargetSeqId = targetSeqId;
    m_fastForwardSpeed = speedMultiplier;
}

void RecordingController::CheckDisableFastForward()
{
    if (!m_fastForwardMode)
    {
        return;
    }

    if (m_currentSeqId >= m_fastForwardTargetSeqId)
    {
        m_fastForwardMode = false;

        //
        // Notify connecting window that fast forward is complete

        ConnectingWindow *connectingWindow = (ConnectingWindow*)EclGetWindow("Connection Status");
        if (connectingWindow)
        {
            connectingWindow->SetFastForwardMode(false, 0);
        }
    }
}

float RecordingController::GetAdvanceSpeedMultiplier() const
{
    if (!m_active)
    {
        return 1.0f;
    }

    //
    // Pause is handled at main loop level, so always return actual speed

    if (m_fastForwardMode)
    {
        return m_fastForwardSpeed;
    }

    return m_speed;
}

ServerToClientLetter* RecordingController::GetNextRecordedLetter()
{
    if (!m_active || m_currentSeqId >= m_history.Size())
    {
        return NULL;
    }

    CheckDisableFastForward();

    if (m_fastForwardMode)
    {
        ConnectingWindow *connectingWindow = (ConnectingWindow*)EclGetWindow("Connection Status");
        if (connectingWindow)
        {
            connectingWindow->UpdateFastForwardProgress(m_currentSeqId);
        }
    }

    //
    // Get the next recorded message and advance position

    ServerToClientLetter *recordedLetter = m_history[m_currentSeqId];
    m_currentSeqId++;

    return recordedLetter;
}

int RecordingController::GetHistorySize() const
{
    return m_history.Size();
}

ServerToClientLetter* RecordingController::GetHistoryLetter(int index) const
{
    if (!m_history.ValidIndex(index))
    {
        return NULL;
    }

    return m_history.GetData(index);
}

bool RecordingController::IsActive() const
{
    return m_active;
}

bool RecordingController::IsPaused() const
{
    return m_paused;
}

bool RecordingController::IsInFastForward() const
{
    return m_fastForwardMode;
}

int RecordingController::GetCurrentSeqId() const
{
    return m_currentSeqId;
}

int RecordingController::GetStartSeqId() const
{
    return m_startSeqId;
}

int RecordingController::GetEndSeqId() const
{
    return m_endSeqId;
}

float RecordingController::GetSpeed() const
{
    return m_speed;
}

const std::string& RecordingController::GetFilename() const
{
    return m_filename;
}

const DcgrHeader& RecordingController::GetHeader() const
{
    return m_header;
}

