#include "systemiv.h"
#include "lib/filesys/binary_stream_readers.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/filesys/file_system.h"
#include "lib/debug_utils.h"

#include "soundsystem.h"
#include "sound_sample_bank.h"
#include "sound_sample_decoder.h"


SoundSampleBank *g_soundSampleBank;



//*****************************************************************************
// Class SoundSampleHandle
//*****************************************************************************

SoundSampleHandle::SoundSampleHandle(SoundSampleDecoder *_sample)
:	m_nextSampleIndex(0),
	m_soundSample(_sample)
{
}


SoundSampleHandle::~SoundSampleHandle()
{
	m_soundSample = NULL;
	m_nextSampleIndex = 0xffffffff;
}


unsigned int SoundSampleHandle::Read(signed short *_data, unsigned int _numSamples, bool _stereo, float relFreq)
{
	unsigned int samplesRemaining = m_soundSample->m_numSamples - m_nextSampleIndex;
	if (_numSamples > samplesRemaining)
	{
		_numSamples = samplesRemaining;
	}
    
	m_soundSample->Read(_data, m_nextSampleIndex, _numSamples, _stereo, relFreq);

    if( _stereo )
    {
        int actualNumSamples = _numSamples * relFreq;
        if( actualNumSamples % 2 == 1 ) actualNumSamples--;
        m_nextSampleIndex += actualNumSamples;
    }
    else
    {
        m_nextSampleIndex += _numSamples;
    }

	return _numSamples;
}

unsigned int SoundSampleHandle::GetFrameCount() const
{
    if (!m_soundSample || m_soundSample->m_numChannels == 0)
    {
        return 0;
    }

    return m_soundSample->m_numSamples / m_soundSample->m_numChannels;
}


void SoundSampleHandle::GetFrame(double _frame, bool _stereo, SoundResampler::Quality _quality, double _ratio, float &_outLeft, float &_outRight)
{
    if (!m_soundSample)
    {
        _outLeft = 0.0f;
        _outRight = 0.0f;
        return;
    }

    m_soundSample->GetFrame(_frame, _stereo, _quality, _ratio, _outLeft, _outRight);
}


void SoundSampleHandle::Restart()
{
	m_nextSampleIndex = 0;
}


int SoundSampleHandle::NumSamplesRemaining()
{
    if( !m_soundSample )
    {
        return -1;
    }

	int samplesRemaining = m_soundSample->m_numSamples - m_nextSampleIndex;
    return samplesRemaining;
}


//*****************************************************************************
// Class SoundSampleBank
//*****************************************************************************

SoundSampleBank::SoundSampleBank()
{
}


SoundSampleBank::~SoundSampleBank()
{
	for (int i = 0; i < m_cache.Size(); ++i)
	{
		SoundSampleDecoder *temp = m_cache.GetData(i);
		m_cache.RemoveData(i);
		delete temp;
	}
	m_cache.EmptyAndDelete();
}


SoundSampleHandle *SoundSampleBank::GetSample(char *_sampleName)
{    
	SoundSampleDecoder *cachedSample = m_cache.GetData(_sampleName);

	if (!cachedSample)
	{
#ifdef TARGET_DEBUG
        for( int i = 0; i < m_cache.Size(); ++i )
        {
            const char *key = m_cache.GetName(i);
            if( key && stricmp(key, _sampleName) == 0 )
            {
                AppDebugOut( "Error with sample cache:\n" );
                m_cache.DumpKeys();
                AppAbort("Error with Sample Cache.");
            }           
        }
#endif
        
        char fullFilename[512];
        sprintf( fullFilename, "data/sounds/%s.ogg", _sampleName );
        BinaryReader *dataReader = g_fileSystem->GetBinaryReader(fullFilename);
        AppReleaseAssert( dataReader && dataReader->IsOpen(), "Failed to open sound stream decoder : %s", _sampleName );

		cachedSample = new SoundSampleDecoder(dataReader);
		m_cache.PutData(_sampleName, cachedSample);
    }

	SoundSampleHandle *rv = new SoundSampleHandle(cachedSample);
	return rv;
}



void SoundSampleBank::EmptyCache()
{
    m_cache.EmptyAndDelete();
}


int SoundSampleBank::GetMemoryUsage()
{
    int memoryUsage = 0;

    for( int i = 0; i < m_cache.Size(); ++i )
    {
        if( m_cache.ValidIndex(i) )
        {
            SoundSampleDecoder *sample = m_cache.GetData(i);
            int sampleSizeShort = sizeof(signed short) * sample->m_numChannels * sample->m_numSamples;
            memoryUsage += sampleSizeShort;
            if (sample->m_sampleCacheFloat)
            {
                int sampleSizeFloat = sizeof(float) * sample->m_numChannels * sample->m_numSamples;
                memoryUsage += sampleSizeFloat;
            }
        }
    }

    return memoryUsage;
}


void SoundSampleBank::DiscardSample(char *_sampleName)
{
	SoundSampleDecoder *cachedSample = m_cache.GetData(_sampleName);
    if( cachedSample )
    {
        m_cache.RemoveData(_sampleName);
        delete cachedSample;
    }
}
