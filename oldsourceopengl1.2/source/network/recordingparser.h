#include <iostream>
#include <string>

// added the preprocessor for visual studio, this means we can have the parsing files included in the configuration but will only get used if we define RECORDING_PARSING
#if RECORDING_PARSING

class Directory;
class Server;

class RecordingParser
{
public:
    RecordingParser           ( std::istream &in, const std::string &filename, Server *server );
    ~RecordingParser          ();

    bool Parse                ( bool debugPrint );

private:
    std::istream               &m_in;
    std::string                m_filename;
    Server                     *m_server;

    bool ReadPacket            ( Directory &dir, bool &zeroMarker );
    bool ReadHeaderPacket      ( Directory &matchHeader );
    void Send                  ( Directory *dir );
};

#endif
