#ifndef RECORDING_SERVER_H
#define RECORDING_SERVER_H

#include <string>

class App;

class RecordingServer
{
public:
    RecordingServer();
    ~RecordingServer();

    bool StartPlayback(const std::string& filename, bool startFromLobby);

private:

    bool CheckClientListenerReady();
};

#endif
