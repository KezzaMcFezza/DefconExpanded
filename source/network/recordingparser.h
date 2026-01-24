#include <iostream>
#include <string>
#include <fstream> 

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
