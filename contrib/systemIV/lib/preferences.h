#ifndef INCLUDED_PREFERENCES_H
#define INCLUDED_PREFERENCES_H

#include <stdio.h>

#include "lib/tosser/hash_table.h"
#include "lib/tosser/fast_darray.h"


#define PREFS_SCREEN_WIDTH  		"ScreenWidth"
#define PREFS_SCREEN_HEIGHT		    "ScreenHeight"
#define PREFS_SCREEN_REFRESH		"ScreenRefresh"
#define PREFS_SCREEN_COLOUR_DEPTH	"ScreenColourDepth"
#define PREFS_SCREEN_Z_DEPTH		"ScreenZDepth"
#define PREFS_SCREEN_WINDOWED		"ScreenWindowed"
#define PREFS_SCREEN_BORDERLESS		"ScreenBorderless"
#define PREFS_SCREEN_MAXIMIZED      "ScreenMaximized"
#define PREFS_SCREEN_ANTIALIAS		"ScreenAntiAliasing"
#define PREFS_SCREEN_UI_SCALE		"ScreenUIScale"
#define PREFS_SCREEN_FPS_LIMIT		"ScreenFpsLimit"

#define PREFS_SOUND_LIBRARY         "SoundLibrary"
#define PREFS_SOUND_MIXFREQ         "SoundMixFreq"
#define PREFS_SOUND_CHANNELS        "SoundChannels"
#define PREFS_SOUND_HW3D            "SoundHW3D"
#define PREFS_SOUND_SWAPSTEREO      "SoundSwapStereo"
#define PREFS_SOUND_DSPEFFECTS      "SoundDSP"
#define PREFS_SOUND_MEMORY          "SoundMemoryUsage"
#define PREFS_SOUND_MASTERVOLUME    "SoundMasterVolume"
#define PREFS_SOUND_UPDATEPERIOD    "SoundUpdatePeriod"
#define PREFS_SOUND_QUALITY         "SoundQuality"
#if !defined(TARGET_MSVC) || defined(WINDOWS_SDL)
#define PREFS_SOUND_AUDIODRIVER     "SoundAudioDriver"
#define PREFS_SOUND_BUFFERSIZE      "SoundBufferSize"
#endif
#define PREFS_RECORDING_LAST_FOLDER "RecordingLastFolder"
#define PREFS_SOUND_RESAMPLER_SFX   "SoundResamplerSfx"
#define PREFS_SOUND_RESAMPLER_MUSIC "SoundResamplerMusic"
#define PREFS_CRASHPAD_URL          "CrashpadURL"


class PreferencesItem;


// ******************
// Class PrefsManager
// ******************

class Preferences
{
protected:
    char                           *m_filename;
	HashTable<PreferencesItem *>    m_items;
    FastDArray<char *>              m_fileText;

protected:
	bool    IsLineEmpty     (const char *_line);
	void    SaveItem        (FILE *out, PreferencesItem *_item);

public:
	Preferences             ();
	~Preferences            ();

	void    Load            (const char *_filename=NULL);        // If filename is NULL, then m_filename is used
	void    Save            ();

	const char    *GetString      (const char *_key, const char *_default=NULL);
	float   GetFloat        (const char *_key, float _default=-1.0f);
	int	    GetInt          (const char *_key, int _default=-1);
	
	void    SetString       (const char *_key, const char *_string);
	void    SetFloat	    (const char *_key, float _val);
	void    SetInt		    (const char *_key, int _val);
    
    void    AddLine         (const char *_line);
    
    bool    DoesKeyExist    (const char *_key);
};




// ***************
// Class PrefsItem
// ***************

class PreferencesItem
{
public:	
	enum
	{
		TypeString,
		TypeFloat,
		TypeInt
	};
	
	char        *m_key;
	int			m_type;
	char		*m_str;
    int		    m_int;
	float	    m_float;
	
	bool		m_hasBeenWritten;

    PreferencesItem   ();
	PreferencesItem   (char *_line);
	PreferencesItem   (const char *_key, const char *_str);
	PreferencesItem   (const char *_key, float _float);
	PreferencesItem   (const char *_key, int _int);
	~PreferencesItem  ();
};



extern Preferences *g_preferences;


#endif
