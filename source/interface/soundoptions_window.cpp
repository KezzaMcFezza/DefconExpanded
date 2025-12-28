#include "systemiv.h"
#include "lib/sound/sound_library_3d.h"
#include "lib/sound/sound_sample_bank.h"
#include "lib/sound/soundsystem.h"
#include "lib/sound/resampler_polyphase.h"
#include "lib/render/renderer.h"
#include "lib/render2d/renderer_2d.h"
#include "lib/language_table.h"
#include "lib/preferences.h"
#include "lib/profiler.h"

#include "lib/eclipse/components/drop_down_menu.h"
#include "lib/eclipse/components/inputfield.h"

#include "soundoptions_window.h"

namespace
{
    using SoundResampler::Quality;

    const char *SelectionToString(int selection)
    {
        switch (selection)
        {
            case 1: return "sinc64";
            case 2: return "sinc128";
            default: return "linear";
        }
    }

    Quality SelectionToQuality(int selection)
    {
        switch (selection)
        {
            case 1: return Quality::Sinc64;
            case 2: return Quality::Sinc128;
            default: return Quality::Linear;
        }
    }

    int QualityToSelection(Quality quality)
    {
        switch (quality)
        {
            case Quality::Sinc64:  return 1;
            case Quality::Sinc128: return 2;
            case Quality::Sinc96:  return 1;
            case Quality::Linear:
            default:
                return 0;
        }
    }
}

class RestartSoundButton : public InterfaceButton
{
public:
    void MouseUp()
    { 
        SoundOptionsWindow *parent = (SoundOptionsWindow *) m_parent;

        int oldMemoryUsage = g_preferences->GetInt( PREFS_SOUND_MEMORY );

        g_preferences->SetInt( PREFS_SOUND_MIXFREQ, parent->m_mixFreq );
        g_preferences->SetInt( PREFS_SOUND_SWAPSTEREO, parent->m_swapStereo );
        g_preferences->SetInt( PREFS_SOUND_DSPEFFECTS, parent->m_dspEffects );
        g_preferences->SetInt( PREFS_SOUND_MEMORY, parent->m_memoryUsage );
        g_preferences->SetInt( PREFS_SOUND_MASTERVOLUME, parent->m_masterVolume );
        g_preferences->SetInt( PREFS_SOUND_CHANNELS, parent->m_channelCount );
        int targetMusicChannels = (parent->m_channelCount >= 128) ? 12 : 8;
        g_preferences->SetInt( "SoundMusicChannels", targetMusicChannels );
#ifdef TARGET_MSVC
        g_preferences->SetInt( PREFS_SOUND_AUDIODRIVER, parent->m_audioDriver );
#endif
        
        //
        // Apply sound quality setting

        g_preferences->SetString(PREFS_SOUND_RESAMPLER_SFX, SelectionToString(parent->m_sfxQuality));
        g_preferences->SetString(PREFS_SOUND_RESAMPLER_MUSIC, SelectionToString(parent->m_musicQuality));
        SoundResampler::SetSfxQuality(SelectionToQuality(parent->m_sfxQuality));
        SoundResampler::SetMusicQuality(SelectionToQuality(parent->m_musicQuality));
        
        g_soundSystem->RestartSoundLibrary();

        if( parent->m_memoryUsage != oldMemoryUsage )
        {
            //
            // Clear out the sample cache

            g_soundSampleBank->EmptyCache();

            for( int i = 0; i < g_soundSystem->m_sounds.Size(); ++i )    
            {
                if( g_soundSystem->m_sounds.ValidIndex(i) )
                {
                    SoundInstance *instance = g_soundSystem->m_sounds[i];
                    if( instance->m_soundSampleHandle )
                    {
                        delete instance->m_soundSampleHandle;
                        instance->m_soundSampleHandle = NULL;
                        instance->OpenStream(false);
                    }
                }
            }
        }


        if( parent->m_dspEffects )
        {
            g_soundSystem->PropagateBlueprints();
            // Causes all sounds to reload their DSP effects from the blueprints
        }
        else
        {
            g_soundSystem->StopAllDSPEffects();
        }

		g_preferences->Save();

    }
};

SoundOptionsWindow::SoundOptionsWindow()
:   InterfaceWindow( "Sound", "dialog_soundoptions", true )
{
#ifdef TARGET_MSVC
    SetSize( 390, 330 );
#else
    SetSize( 390, 300 );
#endif

    m_mixFreq       = 44100;
    g_preferences->SetInt( PREFS_SOUND_MIXFREQ, m_mixFreq );
    m_swapStereo    = g_preferences->GetInt( PREFS_SOUND_SWAPSTEREO, 0 );
    m_dspEffects    = g_preferences->GetInt( PREFS_SOUND_DSPEFFECTS, 1 );
    m_memoryUsage   = g_preferences->GetInt( PREFS_SOUND_MEMORY, 1 );
    m_masterVolume  = g_preferences->GetInt( PREFS_SOUND_MASTERVOLUME, 255 );
    int prefChannels = g_preferences->GetInt( PREFS_SOUND_CHANNELS, 128 );
    m_channelCount = (prefChannels <= 64) ? 64 : 128;

    SoundResampler::Quality sfxQuality = SoundResampler::Quality::Linear;
    SoundResampler::Quality musicQuality = SoundResampler::Quality::Linear;

    if (g_preferences->DoesKeyExist(PREFS_SOUND_RESAMPLER_SFX))
    {
        const char *pref = g_preferences->GetString(PREFS_SOUND_RESAMPLER_SFX, "linear");
        sfxQuality = SoundResampler::QualityFromString(pref, SoundResampler::Quality::Linear);
    }
    else if (g_preferences->DoesKeyExist(PREFS_SOUND_QUALITY))
    {
        int legacy = g_preferences->GetInt(PREFS_SOUND_QUALITY, 0);
        sfxQuality = SelectionToQuality(legacy);
    }

    if (g_preferences->DoesKeyExist(PREFS_SOUND_RESAMPLER_MUSIC))
    {
        const char *pref = g_preferences->GetString(PREFS_SOUND_RESAMPLER_MUSIC, "linear");
        musicQuality = SoundResampler::QualityFromString(pref, SoundResampler::Quality::Linear);
    }
    else if (g_preferences->DoesKeyExist(PREFS_SOUND_QUALITY))
    {
        int legacy = g_preferences->GetInt(PREFS_SOUND_QUALITY, 0);
        musicQuality = SelectionToQuality(legacy);
    }

    m_sfxQuality = QualityToSelection(sfxQuality);
    m_musicQuality = QualityToSelection(musicQuality);
#ifdef TARGET_MSVC
    m_audioDriver   = g_preferences->GetInt( PREFS_SOUND_AUDIODRIVER, 0 );
#endif
}


void SoundOptionsWindow::Create()
{
    InterfaceWindow::Create();

    int x = 200;
    int w = m_w - x - 20;
    int y = 30;
    int h = 30;
    
    InvertedBox *box = new InvertedBox();
  	box->SetProperties( "invert", 10, 50, m_w - 20, m_h-140, " ", " ", false, false );        
    RegisterButton( box );

    //DropDownMenu *memoryUsage = new DropDownMenu();
    //memoryUsage->SetProperties( "Memory Usage", x, y+=h, w, 20, "dialog_memoryusage", " ", true, false );
    //memoryUsage->AddOption( "dialog_high", 1, true );
    //memoryUsage->AddOption( "dialog_medium", 2, true );
    //memoryUsage->AddOption( "dialog_low", 3, true );
    //memoryUsage->RegisterInt( &m_memoryUsage );
    //RegisterButton( memoryUsage );

#ifdef TARGET_MSVC
    DropDownMenu *audioDriver = new DropDownMenu();
    audioDriver->SetProperties( "Audio Driver", x, y+=h, w, 20, "dialog_audiodriver", " ", true, false );
    audioDriver->AddOption( "dialog_audiodriver_wasapi", 0, true );
    audioDriver->AddOption( "dialog_audiodriver_directsound", 1, true );
    audioDriver->RegisterInt( &m_audioDriver );
    RegisterButton( audioDriver );
#endif

    DropDownMenu *swapStereo = new DropDownMenu();
    swapStereo->SetProperties( "Swap Stereo", x, y+=h, w, 20, "dialog_swapstereo", " ", true, false );
    swapStereo->AddOption( "dialog_enabled", 1, true );
    swapStereo->AddOption( "dialog_disabled", 0, true );
    swapStereo->RegisterInt( &m_swapStereo );
    RegisterButton( swapStereo );

    DropDownMenu *channelCount = new DropDownMenu();
    channelCount->SetProperties( "Sound Channels", x, y+=h, w, 20, "dialog_numchannels", " ", true, false );
    channelCount->AddOption( "64 Channels", 64, false );
    channelCount->AddOption( "128 Channels", 128, false );
    channelCount->RegisterInt( &m_channelCount );
    RegisterButton( channelCount );

    DropDownMenu *sfxQuality = new DropDownMenu();
    sfxQuality->SetProperties( "Sound FX Quality", x, y+=h, w, 20, "dialog_soundquality_sfx", " ", true, false );
    sfxQuality->AddOption( "dialog_soundquality_normal", 0, true );
    sfxQuality->AddOption( "dialog_soundquality_high", 1, true );
    sfxQuality->AddOption( "dialog_soundquality_veryhigh", 2, true );
    sfxQuality->RegisterInt( &m_sfxQuality );
    RegisterButton( sfxQuality );

    DropDownMenu *musicQuality = new DropDownMenu();
    musicQuality->SetProperties( "Music Quality", x, y+=h, w, 20, "dialog_soundquality_music", " ", true, false );
    musicQuality->AddOption( "dialog_soundquality_normal", 0, true );
    musicQuality->AddOption( "dialog_soundquality_high", 1, true );
    musicQuality->AddOption( "dialog_soundquality_veryhigh", 2, true );
    musicQuality->RegisterInt( &m_musicQuality );
    RegisterButton( musicQuality );

#ifdef SOUNDOPTIONSWINDOW_USEDSPEFFECTS
    DropDownMenu *dspEffects = new DropDownMenu();
    dspEffects->SetProperties( "Realtime Effects", x, y+=h, w, 20, "dialog_realtimeeffects", " ", true, false );
    dspEffects->AddOption( "dialog_enabled", 1, true );
    dspEffects->AddOption( "dialog_disabled", 0, true );
    dspEffects->RegisterInt( &m_dspEffects );
    RegisterButton( dspEffects );
#endif
    
    CreateValueControl( "Master Volume", x, y+=h, w, 20, InputField::TypeInt, &m_masterVolume, 10, 0, 255, NULL, " ", false );

    CloseButton *cancel = new CloseButton();
    cancel->SetProperties( "Close", 10, m_h - 30, m_w / 2 - 15, 20, "dialog_close", " ", true, false );
    RegisterButton( cancel );

    RestartSoundButton *apply = new RestartSoundButton();
    apply->SetProperties( "Apply", cancel->m_x+cancel->m_w+10, m_h - 30, m_w / 2 - 15, 20, "dialog_apply", " ", true, false );
    RegisterButton( apply );    
}


void SoundOptionsWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );
        
    int x = m_x + 20;
    int y = m_y + 30;
    int h = 30;
    int size = 13;

    //g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_memoryusage") );
#ifdef TARGET_MSVC
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_audiodriver") );
#endif
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_swapstereo") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_numchannels") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_soundquality_sfx") );
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_soundquality_music") );

#ifdef SOUNDOPTIONSWINDOW_USEDSPEFFECTS
    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_realtimeeffects") );
#endif

    g_renderer2d->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_mastervolume") );
    

    //
    // Sound system CPU usage

    //ProfiledElement *element = g_profiler->m_rootElement->m_children.GetData( "SoundSystem" );
    //if( element && element->m_lastNumCalls > 0 )
    //{
    //    float occup = element->m_lastTotalTime * 100;
    //    
    //    Colour colour = White;
    //    if( occup > 15 ) colour.Set( 255, 50, 50 );
    //    g_renderer2d->TextCentre( m_x + m_w/2, m_y + m_h - 50, colour, 17, "%s %d%%", LANGUAGEPHRASE("dialog_cpuusage"), int(occup) );
    //}
    //else
    //{
    //    g_renderer2d->TextCentreSimple( m_x + m_w/2, m_y + m_h - 50, Colour(255,50,50), size, LANGUAGEPHRASE("dialog_cpuusageunknown") );
    //}


    //
    // Memory usage
    
    // Removed glColor4f - color is handled by the modern renderer system
    float memoryUsage = g_soundSampleBank->GetMemoryUsage();
    memoryUsage /= 1024.0f;
    memoryUsage /= 1024.0f;
    g_renderer2d->TextCentre( m_x + m_w/2, m_y + m_h - 70, White, size, "%s %2.1f Mb", LANGUAGEPHRASE("dialog_memoryusage"), memoryUsage );
}
