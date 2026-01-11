#include "systemiv.h"

#include <stdlib.h>
#include <string.h>

#include <vorbis/vorbisfile.h>
#include <vorbis/codec.h>

#include "lib/filesys/binary_stream_readers.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/debug/debug_utils.h"

#include "soundsystem.h"
#include "sound_sample_decoder.h"
#include <climits>
#include "resampler_polyphase.h"



SoundSampleDecoder::SoundSampleDecoder(BinaryReader *_in)
:	m_in(_in),
	m_bits(0),
	m_numChannels(0),
	m_freq(0),
	m_numSamples(0),
	m_fileType(TypeUnknown),
    m_vorbisFile(NULL),
    m_sampleCache(NULL),
    m_sampleCacheFloat(NULL)
{
	const char *fileType = _in->GetFileType();
	if (stricmp(fileType, "wav") == 0)
	{
		m_fileType = TypeWav;
		ReadWavHeader();
	}
	else if (stricmp(fileType, "ogg") == 0)
	{
		m_fileType = TypeOgg;
		ReadOggHeader();
	}
	else
	{
		AppReleaseAssert(0, "Unknown sound file format %s", m_in->m_filename.c_str());
	}


    // We assume a single sample is ALWAYS mono
    // therefore a stereo file has twice as many samples inside as it says it does

    m_numSamples *= m_numChannels;
	m_amountCached = 0;
}


SoundSampleDecoder::~SoundSampleDecoder()
{
    if( m_in )
    {
	    delete m_in;
    }

    if( m_vorbisFile )
    {
        ov_clear( m_vorbisFile );
        delete m_vorbisFile;
    }

    if( m_sampleCache )
    {
        delete[] m_sampleCache;
    }
    if( m_sampleCacheFloat )
    {
        delete[] m_sampleCacheFloat;
    }
}


unsigned int SoundSampleDecoder::GetFrameCount() const
{
    if (m_numChannels == 0)
    {
        return 0;
    }

    return m_numSamples / m_numChannels;
}


// Defensive helper to validate a polyphase kernel before use
static inline bool s_IsValidKernel(const SoundResampler::Kernel *k)
{
    if (!k) return false;
    if (k->taps == 0 || k->phases == 0) return false;
    // Guard against obviously corrupt values that would overflow/abort
    const unsigned int kMaxTaps = 2048;      // well above our Sinc128 (128)
    const unsigned int kMaxPhases = 8192;    // well above our typical 512
    if (k->taps > kMaxTaps || k->phases > kMaxPhases) return false;

    const size_t needed = (size_t)k->taps * (size_t)k->phases;
    if (k->coeffs.size() < needed) return false;
    return true;
}

void SoundSampleDecoder::GetFrame(double _frame, bool _stereo, SoundResampler::Quality _quality, double _ratio, float &_outLeft, float &_outRight)
{
    _outLeft = 0.0f;
    _outRight = 0.0f;

    unsigned int totalFrames = GetFrameCount();
    if (totalFrames == 0)
    {
        return;
    }

    if (_frame < 0.0)
    {
        _frame = 0.0;
    }

    if (_frame > (double)(totalFrames - 1))
    {
        _frame = (double)(totalFrames - 1);
    }

    unsigned int baseFrame = (unsigned int)_frame;
    unsigned int samplesPerFrame = m_numChannels ? m_numChannels : 1;
    double fraction = _frame - (double)baseFrame;
    double fractionForLinear = fraction;

    auto linearSample = [&]()
    {
        unsigned int nextFrame = baseFrame + 1;
        double fractionLinear = fractionForLinear;
        if (nextFrame >= totalFrames)
        {
            nextFrame = baseFrame;
            fractionLinear = 0.0;
        }

        unsigned int sampleIndexBase = baseFrame * samplesPerFrame;
        unsigned int sampleIndexNext = nextFrame * samplesPerFrame;

        unsigned int ensureEndLinear = sampleIndexNext + samplesPerFrame;
        if (ensureEndLinear > m_numSamples)
        {
            ensureEndLinear = m_numSamples;
        }
        EnsureCached(ensureEndLinear);

        if (m_amountCached == 0 || (!m_sampleCache && !m_sampleCacheFloat))
        {
            _outLeft = 0.0f;
            _outRight = 0.0f;
            return;
        }

        unsigned int baseIndex = sampleIndexBase;
        if (baseIndex >= m_amountCached)
        {
            baseIndex = m_amountCached > 0 ? m_amountCached - 1 : 0;
        }

        unsigned int nextIndex = sampleIndexNext;
        if (nextIndex >= m_amountCached)
        {
            nextIndex = m_amountCached > 0 ? m_amountCached - 1 : 0;
        }

        float sampleAtBase = m_sampleCacheFloat ? m_sampleCacheFloat[baseIndex] : (m_sampleCache ? (float)m_sampleCache[baseIndex] : 0.0f);
        float sampleAtNext = m_sampleCacheFloat ? m_sampleCacheFloat[nextIndex] : (m_sampleCache ? (float)m_sampleCache[nextIndex] : 0.0f);

        if (m_numChannels == 1)
        {
            float interpolated = sampleAtBase + (sampleAtNext - sampleAtBase) * (float)fractionLinear;

            _outLeft = interpolated;
            _outRight = interpolated;
        }
        else
        {
            unsigned int baseRightIndex = sampleIndexBase + 1;
            if (baseRightIndex >= m_amountCached)
            {
                baseRightIndex = m_amountCached > 0 ? m_amountCached - 1 : 0;
            }

            unsigned int nextRightIndex = sampleIndexNext + 1;
            if (nextRightIndex >= m_amountCached)
            {
                nextRightIndex = m_amountCached > 0 ? m_amountCached - 1 : 0;
            }

            float baseLeft = sampleAtBase;
            float baseRight = m_sampleCacheFloat ? m_sampleCacheFloat[baseRightIndex] : (m_sampleCache ? (float)m_sampleCache[baseRightIndex] : 0.0f);
            float nextLeft = sampleAtNext;
            float nextRight = m_sampleCacheFloat ? m_sampleCacheFloat[nextRightIndex] : (m_sampleCache ? (float)m_sampleCache[nextRightIndex] : 0.0f);

            float left = baseLeft + (nextLeft - baseLeft) * (float)fractionLinear;
            float right = baseRight + (nextRight - baseRight) * (float)fractionLinear;

            if (_stereo)
            {
                _outLeft = left;
                _outRight = right;
            }
            else
            {
                float mono = (left + right) * 0.5f;
                _outLeft = mono;
                _outRight = mono;
            }
        }
    };

    if (_quality == SoundResampler::Quality::Linear || _ratio <= 0.0)
    {
        linearSample();
        return;
    }

    SoundResampler::Selection selection = SoundResampler::SelectKernel(_quality, _ratio);
    const SoundResampler::Kernel *kernel = selection.kernel;

    // Validate kernel thoroughly; fall back to linear if anything looks wrong
    if (!s_IsValidKernel(kernel))
    {
        linearSample();
        return;
    }

    unsigned int phaseIndex = (unsigned int)(fraction * kernel->phases + 0.5);
    if (phaseIndex >= kernel->phases)
    {
        phaseIndex = kernel->phases - 1;
    }

    // Re-check that the coefficient offset is in-range for safety
    const size_t coeffOffset = (size_t)phaseIndex * (size_t)kernel->taps;
    const size_t coeffEnd = coeffOffset + (size_t)kernel->taps;
    if (coeffEnd > kernel->coeffs.size())
    {
        linearSample();
        return;
    }

    const float *coeffs = &kernel->coeffs[coeffOffset];
    int centre = (int)(0.5 * (double)kernel->taps - 1.0);
    int firstFrame = (int)baseFrame - centre;
    // Use 64-bit for intermediate to avoid overflow on corrupted inputs
    long long lastFrame64 = (long long)firstFrame + (long long)kernel->taps - 1;
    int lastFrame = (lastFrame64 > INT_MAX) ? INT_MAX : (lastFrame64 < INT_MIN ? INT_MIN : (int)lastFrame64);

    int cacheStartFrame = firstFrame < 0 ? 0 : firstFrame;
    int cacheEndFrame = lastFrame + 1;
    if (cacheEndFrame < 0) cacheEndFrame = 0;
    if (cacheEndFrame > (int)totalFrames) cacheEndFrame = (int)totalFrames;

    if (cacheEndFrame > cacheStartFrame)
    {
        // Compute ensureEnd using 64-bit, clamp to m_numSamples
        unsigned long long ensureEnd64 = (unsigned long long)(unsigned int)cacheEndFrame * (unsigned long long)samplesPerFrame;
        if (ensureEnd64 > (unsigned long long)m_numSamples) ensureEnd64 = (unsigned long long)m_numSamples;
        unsigned int ensureEnd = (unsigned int)ensureEnd64;
        if (ensureEnd > m_numSamples) ensureEnd = m_numSamples;
        EnsureCached(ensureEnd);
    }

    auto fetchSample = [&](int frameIndex, unsigned int channel) -> float
    {
        if (frameIndex < 0 || frameIndex >= (int)totalFrames)
        {
            return 0.0f;
        }

        unsigned int sampleIndex = (unsigned int)frameIndex * samplesPerFrame + channel;
        if (sampleIndex >= m_amountCached)
        {
            return 0.0f;
        }
        if (m_sampleCacheFloat)
        {
            return m_sampleCacheFloat[sampleIndex];
        }
        else if (m_sampleCache)
        {
            return (float)m_sampleCache[sampleIndex];
        }
        return 0.0f;
    };

    auto filterChannel = [&](unsigned int channel) -> float
    {
        double sum = 0.0;
        for (unsigned int tap = 0; tap < kernel->taps; ++tap)
        {
            int frameIndex = firstFrame + (int)tap;
            float sample = fetchSample(frameIndex, channel < m_numChannels ? channel : 0);
            sum += (double)coeffs[tap] * (double)sample;
        }
        return (float)sum;
    };

    if (m_numChannels == 1)
    {
        float mono = filterChannel(0);
        _outLeft = mono;
        _outRight = mono;
    }
    else
    {
        float left = filterChannel(0);
        float right = filterChannel(1);

        if (_stereo)
        {
            _outLeft = left;
            _outRight = right;
        }
        else
        {
            float mono = (left + right) * 0.5f;
            _outLeft = mono;
            _outRight = mono;
        }
    }
}


void SoundSampleDecoder::ReadWavHeader()
{
	char buffer[25];
	int chunkLength;
	
	// Check RIFF header 
	m_in->ReadBytes(12, (unsigned char *)buffer);
	if (memcmp(buffer, "RIFF", 4) || memcmp(buffer + 8, "WAVE", 4))
	{
		return;
	}
	
	while (!m_in->m_eof) 
	{
		if (m_in->ReadBytes(4, (unsigned char*)buffer) != 4)
		{
			break;
		}
		
		chunkLength = m_in->ReadS32();   // read chunk length 
		
		if (memcmp(buffer, "fmt ", 4) == 0)
		{
			int i = m_in->ReadS16();     // should be 1 for PCM data 
			chunkLength -= 2;
			if (i != 1) 
			{
				return;
			}
			
			m_numChannels = m_in->ReadS16();// mono or stereo data 
			chunkLength -= 2;
			if ((m_numChannels != 1) && (m_numChannels != 2))
			{
				return;
			}
			
			m_freq = m_in->ReadS32();    // sample frequency 
			chunkLength -= 4;
			
			m_in->ReadS32();             // skip six bytes 
			m_in->ReadS16();
			chunkLength -= 6;
			
			m_bits = (unsigned char) m_in->ReadS16();    // 8 or 16 bit data? 
			chunkLength -= 2;
			if ((m_bits != 8) && (m_bits != 16))
			{
				return;
			}
		}
		else if (memcmp(buffer, "data", 4) == 0)
		{
			int bytesPerSample = m_numChannels * m_bits / 8;
			m_numSamples = chunkLength / bytesPerSample;

			m_samplesRemaining = m_numSamples;

			return;
		}
		
		// Skip the remainder of the chunk 
		while (chunkLength > 0) 
		{             
			if (m_in->ReadU8() == EOF)
			{
				break;
			}
			
			chunkLength--;
		}
	}
}

static size_t ReadFunc(void *ptr, size_t size, size_t nmemb, void *datasource)
{
	BinaryReader *in = (BinaryReader *)datasource;
	return in->ReadBytes(size * nmemb, (unsigned char*)ptr);
}


static int SeekFunc(void *datasource, ogg_int64_t _offset, int _origin)
{
	BinaryReader *in = (BinaryReader *)datasource;
	int rv = in->Seek((int)_offset, _origin);
	return rv; // -1 indicates seek not supported
}


static int CloseFunc(void *datasource)
{
	return 0;
}


static long TellFunc(void *datasource)
{
	BinaryReader *in = (BinaryReader *)datasource;
	int rv = in->Tell();
	return rv;
}


void SoundSampleDecoder::ReadOggHeader()
{
	ov_callbacks callbacks;
	callbacks.read_func = ReadFunc;
	callbacks.seek_func = SeekFunc;
	callbacks.close_func = CloseFunc;
	callbacks.tell_func = TellFunc;

	m_vorbisFile = new OggVorbis_File();
	int result = ov_open_callbacks(m_in, m_vorbisFile, NULL, 0, callbacks);
	AppReleaseAssert(result == 0, "Ogg file corrupt %s (result = %d)", m_in->m_filename.c_str(), result);

	vorbis_info *vi = ov_info(m_vorbisFile, -1);

	m_numSamples = (unsigned int) ov_pcm_total(m_vorbisFile, -1);
	AppReleaseAssert(m_numSamples > 0, "Ogg file contains no data %s", m_in->m_filename.c_str());
	m_freq = vi->rate;
	m_numChannels = vi->channels;
}


unsigned int SoundSampleDecoder::ReadWavData(signed short *_data, unsigned int _numSamples)
{
    if( m_amountCached + _numSamples > m_numSamples )
    {
        _numSamples = m_numSamples - m_amountCached;
    }

	if (m_bits == 8)
	{
		for (unsigned int i = 0; i < _numSamples; ++i)
		{
			signed short c = m_in->ReadU8() - 128;
			c <<= 8;
			_data[i] = c;

			if (m_in->m_eof)
			{
				_numSamples = i;
				break;
			}
		}
	}
	else 
	{
		int bytesPerSample = 2 * m_numChannels;
		_numSamples = m_in->ReadBytes(_numSamples * bytesPerSample, (unsigned char*)_data);
        _numSamples /= 2;
	}

	return _numSamples;
}

#ifdef _BIG_ENDIAN
#define IS_BIG_ENDIAN 1
#else
#define IS_BIG_ENDIAN 0
#endif

unsigned int SoundSampleDecoder::ReadOggData(signed short *_data, unsigned int _numSamples)
{
    // Ensure float cache exists for OGG so we can retain higher fidelity internally
    if (!m_sampleCacheFloat)
    {
        m_sampleCacheFloat = new float[m_numSamples];
        // Initialise to 0 for safety
        memset(m_sampleCacheFloat, 0, sizeof(float) * m_numSamples);
    }

    unsigned int totalSamplesWritten = 0;
    const float scale = 32767.0f; // store PCM units matching int16 range

    while (totalSamplesWritten < _numSamples)
    {
        int framesRequested = (int)((_numSamples - totalSamplesWritten) / (m_numChannels ? m_numChannels : 1));
        if (framesRequested <= 0)
        {
            break;
        }

        float **pcm = NULL;
        int currentSection = 0;
        long framesRead = ov_read_float(m_vorbisFile, &pcm, framesRequested, &currentSection);

        AppReleaseAssert(framesRead != OV_HOLE && framesRead != OV_EBADLINK,
                         "Ogg file corrupt %s", m_in->m_filename.c_str());

        if (framesRead <= 0)
        {
            // 0 indicates EOF, negative values are errors already handled above
            break;
        }

        unsigned int samplesFromFrames = (unsigned int)framesRead * (m_numChannels ? m_numChannels : 1);

        // Interleave and scale into both caches
        for (long i = 0; i < framesRead; ++i)
        {
            for (unsigned int c = 0; c < m_numChannels; ++c)
            {
                unsigned int dstIndex = m_amountCached + totalSamplesWritten + (unsigned int)i * m_numChannels + c;
                float v = pcm && pcm[c] ? (pcm[c][i] * scale) : 0.0f;
                // Clamp to int16 range in PCM units
                if (v > 32767.0f) v = 32767.0f;
                if (v < -32768.0f) v = -32768.0f;
                m_sampleCacheFloat[dstIndex] = v;
                if (_data)
                {
                    _data[totalSamplesWritten + (unsigned int)i * m_numChannels + c] = (signed short)v;
                }
            }
        }

        totalSamplesWritten += samplesFromFrames;
    }

    return totalSamplesWritten;
}


unsigned int SoundSampleDecoder::ReadToCache(unsigned int _numSamples)
{
    if( !m_sampleCache ) m_sampleCache = new signed short[m_numSamples];

    int samplesRead = 0;

    switch( m_fileType )
    {
	    case TypeUnknown:   AppReleaseAssert(0, "Unknown format of sound file %s", m_in->m_filename.c_str());
	    case TypeWav:       samplesRead = ReadWavData(&m_sampleCache[m_amountCached], _numSamples);         break;
	    case TypeOgg:       samplesRead = ReadOggData(&m_sampleCache[m_amountCached], _numSamples);         break;
    }

    m_amountCached += samplesRead;	
	return samplesRead;
}


void SoundSampleDecoder::EnsureCached(unsigned int _endSample)
{
    if (_endSample >= m_amountCached)
	{
		int amountToRead = _endSample - m_amountCached;
		unsigned int amountRead = ReadToCache(amountToRead);                    

		AppDebugAssert(m_amountCached <= m_numSamples);

		if (m_amountCached == m_numSamples)
		{
	        delete m_in;
            m_in = NULL;

            if( m_vorbisFile )
            {
                ov_clear( m_vorbisFile );
                delete m_vorbisFile;
                m_vorbisFile = NULL;
            }		
		}
	}
}


void SoundSampleDecoder::InterpolateSamples( signed short *_data, unsigned int _startSample, unsigned int _numSamples, bool _stereo, float _relFreq)
{
    /*
     *  We are trying to copy the sample data into the buffer, but at a certain Relative Frequency
     *  We are NOT using Direct Sound's channel frequency code, which means we have to do the frequency work ourselves.
     * 
     *  Doing a simple "nearest sample" calculation results in serious artifacts in the audio - like a Pixel Resize.
     *  Here we attempt to do a filtered approach - by taking the nearest sample, the previous sample, and the next sample
     *  and merging them together in the correct ratio.
     *
     *  It still doesn't sound as good as Direct Sound.
     *
     */

    if( _stereo )
    {
        for( int i = 0; i < _numSamples; ++i )
        {
            float samplePoint = (float)i * _relFreq;
            int sampleIndex = int(samplePoint);
            if( i % 2 == 0 && sampleIndex % 2 == 1 ) 
            {
                sampleIndex--;
                samplePoint -= 1.0f;
            }

            if( i % 2 == 1 && sampleIndex % 2 == 0 ) 
            {
                sampleIndex++;
                samplePoint += 1.0f;
            }

            float fraction = samplePoint - sampleIndex;
            float sample1, sample2, sample3;

            if( fraction > 0.5f )
            {
                sample1 = m_sampleCache[_startSample + sampleIndex - 2] * fraction +
                          m_sampleCache[_startSample + sampleIndex] * (1.0f-fraction);

                sample2 = m_sampleCache[_startSample + sampleIndex + 2] * (1.0f - fraction) +
                          m_sampleCache[_startSample + sampleIndex] * fraction;

                sample3 = m_sampleCache[_startSample + sampleIndex + 4] * (1.0f - fraction) +
                          m_sampleCache[_startSample + sampleIndex + 2] * fraction;
            }
            else
            {
                sample1 = m_sampleCache[_startSample + sampleIndex - 4] * fraction +
                          m_sampleCache[_startSample + sampleIndex - 2] * (1.0f-fraction);

                sample2 = m_sampleCache[_startSample + sampleIndex] * (1.0f - fraction) +
                          m_sampleCache[_startSample + sampleIndex - 2] * fraction;

                sample3 = m_sampleCache[_startSample + sampleIndex + 2] * (1.0f - fraction) +
                          m_sampleCache[_startSample + sampleIndex] * fraction;
            }

            float combinedSample = sample1 * 0.2f + sample2 * 0.6f + sample3 * 0.2f;

            _data[i] = (signed short)combinedSample;
        }
    }
    else
    {        
        memcpy( _data, &m_sampleCache[_startSample], sizeof(signed short) * _numSamples);                
    }
}

void SoundSampleDecoder::Read(signed short *_data, unsigned int _startSample, unsigned int _numSamples, bool _stereo, float _relFreq)
{
    /*
     *	STEREO SAMPLE SUPPORT
     *  
     *  Its all a bit confusing really
     *  but basically, if you request 10 samples of a mono file you'll get the next 10 samples
     *  if you request 10 samples of a stereo sample you'll still get 10 samples - but they will
     *  be left-right-left-right etc, ie only 5 stereo samples worth.
     *
     *  Direct Sound expects the data in this format, so its cool.
     *
     *  If you try to play a mono sample onto a stereo channel the code below will copy the
     *  mono sample into the left-right-left-right stream (ie double up the data)
     *
     */

    unsigned int highestSampleRequested = _startSample + _numSamples;
    EnsureCached( highestSampleRequested );

    bool stereoMatch = ( m_numChannels == 1 && !_stereo ) ||
                       ( m_numChannels == 2 && _stereo );

    if( stereoMatch )
    {
        InterpolateSamples( _data, _startSample, _numSamples, _stereo, _relFreq );
    }
    else if( m_numChannels == 1 && _stereo )
    {
        // Mix our mono sample data into a stereo buffer
        for( int i = 0; i < _numSamples; ++i )
        {
            _data[i] = m_sampleCache[int((_startSample+i)/2)];     
        }
    }
    else if( m_numChannels == 2 && !_stereo )
    {
        // Mix our stereo sample data into a mono buffer

        highestSampleRequested = _startSample*2 + _numSamples*2 - 1;
        EnsureCached( highestSampleRequested );

        for( int i = 0; i < _numSamples; i++ )
        {
            signed short sample1 = m_sampleCache[_startSample*2+i*2];
            signed short sample2 = m_sampleCache[_startSample*2+i*2+1];
            signed short sampleCombined = (signed short)( ( (int)sample1 + (int)sample2 ) / 2.0f );            

            _data[i] = sampleCombined;            
        }        
    }
}
