#ifndef RECORDING_SERVER_H
#define RECORDING_SERVER_H

#include <string>

class App;
class Team;

class RecordingServer
{
public:
    RecordingServer();
    ~RecordingServer();

    bool StartPlayback(const std::string& filename, bool startFromLobby);
    static bool CreateFakeDisconnect(Team *team);

private:

    bool CheckClientListenerReady();
};

#endif
