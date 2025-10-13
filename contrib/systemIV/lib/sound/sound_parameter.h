#ifndef INCLUDED_SOUND_PARAMETER
#define INCLUDED_SOUND_PARAMETER


class TextReader;
class TextFileWriter;


//*****************************************************************************
// Class SoundParameter
//*****************************************************************************

class SoundParameter
{
public:
    enum
    {
        TypeFixedValue,
        TypeRangedRandom,
        TypeLinked,
        NumParameterTypes
    };

    enum
    {
        UpdateConstantly,
        UpdateOnceEveryRestart,
        NumUpdateTypes
    };

    int         m_type;
    int         m_link;                                 // Indexes into list from SoundSystemInterface::ListObjectProperties
    int         m_updateType;

    float       m_inputLower;
    float       m_outputLower;
    float       m_inputUpper;
    float       m_outputUpper;

    float       m_desiredOutput;

    float       m_smooth;

    float       m_input;
    float       m_output;

public:
    SoundParameter();
    SoundParameter( float _fixedValue );

    void        Copy( SoundParameter *_copyMe );

    void        Recalculate( float _input=0.0f );
    float       GetOutput();    

    void        Read    ( TextReader *_in );
    void        Write   ( TextFileWriter *_file, const char *_paramName, int _tabs );

	float		GetSmooth();	// Currently returns sqrtf(m_smooth)

    bool        IsFixedValue( float _value );

    
    static const char *GetParameterTypeName   ( int _type );
    static int   GetParameterType       ( const char *_name );
    static const char *GetLinkName            ( int _type );
    static int   GetLinkType            ( const char *_name );
    static const char *GetUpdateTypeName      ( int _type );
    static int   GetUpdateType          ( const char *_name );
};


#endif
