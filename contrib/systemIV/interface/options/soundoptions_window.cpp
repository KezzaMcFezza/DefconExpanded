#include "lib/universal_include.h"
#include "lib/sound/sound_library_3d.h"
#include "lib/sound/sound_sample_bank.h"
#include "lib/sound/soundsystem.h"
#include "lib/render2d/renderer.h"
#include "lib/render/styletable.h"
#include "lib/language_table.h"
#include "lib/preferences.h"
#include "lib/profiler.h"
#if defined(WINDOWS_SDL) || defined(TARGET_OS_MACOSX) || defined(TARGET_OS_LINUX)
#include "lib/sound/sound_library_2d_sdl.h"
#endif

#include "interface/components/drop_down_menu.h"
#include "interface/components/inputfield.h"

#include "soundoptions_window.h"



#if defined(WINDOWS_SDL) || defined(TARGET_OS_MACOSX) || defined(TARGET_OS_LINUX)
namespace
{
std::string TruncateToFitButtonWidth(const std::string &_text, float _maxWidth)
{
    if (_text.empty() || _maxWidth <= 0.0f)
    {
        return _text;
    }

    Style *buttonFont = g_styleTable->GetStyle(FONTSTYLE_BUTTON);
    AppAssert(buttonFont);

    const bool useDefaultFont = stricmp(buttonFont->m_fontName, "default") == 0;
    g_renderer->SetFont(useDefaultFont ? NULL : buttonFont->m_fontName,
                        false,
                        buttonFont->m_negative);

    const float fontSize = buttonFont->m_size;
    std::string result = _text;

    const float fullWidth = g_renderer->TextWidth(_text.c_str(), fontSize);
    if (fullWidth > _maxWidth)
    {
        const char *ellipsis = "...";
        const float ellipsisWidth = g_renderer->TextWidth(ellipsis, fontSize);
        const bool canAppendEllipsis = ellipsisWidth > 0.0f && ellipsisWidth < _maxWidth;
        const float maxTextWidth = canAppendEllipsis ? _maxWidth - ellipsisWidth : _maxWidth;

        std::string fitted;
        fitted.reserve(_text.size());

        for (size_t i = 0; i < _text.size(); ++i)
        {
            fitted.push_back(_text[i]);
            const float width = g_renderer->TextWidth(fitted.c_str(), fontSize);
            if (width > maxTextWidth)
            {
                fitted.pop_back();
                break;
            }
        }

        if (fitted.empty())
        {
            result = canAppendEllipsis ? ellipsis : std::string();
        }
        else if (canAppendEllipsis && fitted.size() < _text.size())
        {
            result = fitted + ellipsis;
        }
        else
        {
            result = fitted;
        }
    }

    g_renderer->SetFont();

    return result;
}
} // namespace
#endif


class RestartSoundButton : public InterfaceButton
{
public:
    void MouseUp()
    { 
        SoundOptionsWindow *parent = (SoundOptionsWindow *) m_parent;

        int oldMemoryUsage = g_preferences->GetInt( PREFS_SOUND_MEMORY );

        g_preferences->SetInt( PREFS_SOUND_MIXFREQ, parent->m_mixFreq );
        g_preferences->SetInt( PREFS_SOUND_HW3D, parent->m_useHardware3D );
        g_preferences->SetInt( PREFS_SOUND_SWAPSTEREO, parent->m_swapStereo );
        g_preferences->SetInt( PREFS_SOUND_DSPEFFECTS, parent->m_dspEffects );
        g_preferences->SetInt( PREFS_SOUND_MEMORY, parent->m_memoryUsage );
        g_preferences->SetInt( PREFS_SOUND_MASTERVOLUME, parent->m_masterVolume );
#if defined(WINDOWS_SDL) || defined(TARGET_OS_MACOSX) || defined(TARGET_OS_LINUX)
        const char *selectedDevice = parent->GetSelectedOutputDeviceName();
        if (selectedDevice && selectedDevice[0])
        {
            g_preferences->SetString( PREFS_SOUND_OUTPUT_DEVICE, selectedDevice );
        }
        else
        {
            g_preferences->SetString( PREFS_SOUND_OUTPUT_DEVICE, "" );
        }
        parent->m_preferredOutputDevice = selectedDevice ? selectedDevice : "";
        g_preferences->SetString( PREFS_SOUND_LIBRARY, "software" );
#else
        if( parent->m_soundLib == 0 ) g_preferences->SetString( PREFS_SOUND_LIBRARY, "software" );
        else                          g_preferences->SetString( PREFS_SOUND_LIBRARY, "dsound" );
#endif
        
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



class HW3DDropDownMenu : public DropDownMenu
{
public:
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        bool available = g_soundLibrary3d->Hardware3DSupport();
        if( available )
        {
            DropDownMenu::Render( realX, realY, highlighted, clicked );
        }
        else
        {
            g_renderer->TextSimple( realX+10, realY+9, White, 13, LANGUAGEPHRASE("dialog_unavailable") );
        }
    }

    void MouseUp()
    {
        bool available = g_soundLibrary3d->Hardware3DSupport();
        if( available )
        {
            DropDownMenu::MouseUp();
        }
    }
};


SoundOptionsWindow::SoundOptionsWindow()
:   InterfaceWindow( "Sound", "dialog_soundoptions", true )
{
#if !defined(TARGET_MSVC) || defined(WINDOWS_SDL)
    SetSize( 390, 350 );
#else
    SetSize( 390, 320 );
#endif

    m_mixFreq       = 44100;
    g_preferences->SetInt( PREFS_SOUND_MIXFREQ, m_mixFreq );
    m_useHardware3D = g_preferences->GetInt( PREFS_SOUND_HW3D, 0 );
    m_swapStereo    = g_preferences->GetInt( PREFS_SOUND_SWAPSTEREO, 0 );
    m_dspEffects    = g_preferences->GetInt( PREFS_SOUND_DSPEFFECTS, 1 );
    m_memoryUsage   = g_preferences->GetInt( PREFS_SOUND_MEMORY, 1 );
    m_masterVolume  = g_preferences->GetInt( PREFS_SOUND_MASTERVOLUME, 255 );

    const char *soundLib  = g_preferences->GetString( PREFS_SOUND_LIBRARY );
    
    if( stricmp( soundLib, "dsound" ) == 0 ) m_soundLib = 1;
    else                                     m_soundLib = 0;
#if defined(WINDOWS_SDL) || defined(TARGET_OS_MACOSX) || defined(TARGET_OS_LINUX)
    const char *preferredDevice = g_preferences->GetString( PREFS_SOUND_OUTPUT_DEVICE, "" );
    m_preferredOutputDevice = preferredDevice ? preferredDevice : "";
    m_selectedDeviceIndex = 0;
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

    
#if defined(WINDOWS_SDL) || defined(TARGET_OS_MACOSX) || defined(TARGET_OS_LINUX)
    PopulateOutputDevices();
    DropDownMenu *outputDevice = new DropDownMenu();
    outputDevice->SetProperties( "Sound Output Device", x, y+=h, w, 20, "dialog_soundoutputdevice", " ", true, false );
    outputDevice->AddOption( "dialog_sounddevice_default", 0, true );
    const float maxDeviceLabelWidth = (float)w - 20.0f;
    for( size_t i = 1; i < m_outputDevices.size(); ++i )
    {
        const std::string displayName = TruncateToFitButtonWidth( m_outputDevices[i], maxDeviceLabelWidth );
        outputDevice->AddOption( displayName.c_str(), (int)i, false );
    }
    if( m_selectedDeviceIndex < 0 || m_selectedDeviceIndex >= (int)m_outputDevices.size() )
    {
        m_selectedDeviceIndex = 0;
    }
    outputDevice->RegisterInt( &m_selectedDeviceIndex );
    RegisterButton( outputDevice );
#else
    DropDownMenu *soundLib = new DropDownMenu();
    soundLib->SetProperties( "Sound Library", x, y+=h, w, 20, "dialog_soundlibrary", " ", true, false );
#ifdef HAVE_DSOUND
    soundLib->AddOption( "dialog_directsound", 1, true );
#else
    soundLib->AddOption( "dialog_softwaresound", 0, true );
#endif
    soundLib->RegisterInt( &m_soundLib );
    RegisterButton( soundLib );
#endif


    //DropDownMenu *memoryUsage = new DropDownMenu();
    //memoryUsage->SetProperties( "Memory Usage", x, y+=h, w, 20, "dialog_memoryusage", " ", true, false );
    //memoryUsage->AddOption( "dialog_high", 1, true );
    //memoryUsage->AddOption( "dialog_medium", 2, true );
    //memoryUsage->AddOption( "dialog_low", 3, true );
    //memoryUsage->RegisterInt( &m_memoryUsage );
    //RegisterButton( memoryUsage );

    DropDownMenu *swapStereo = new DropDownMenu();
    swapStereo->SetProperties( "Swap Stereo", x, y+=h, w, 20, "dialog_swapstereo", " ", true, false );
    swapStereo->AddOption( "dialog_enabled", 1, true );
    swapStereo->AddOption( "dialog_disabled", 0, true );
    swapStereo->RegisterInt( &m_swapStereo );
    RegisterButton( swapStereo );

#ifdef SOUNDOPTIONSWINDOW_USEHARDWARE3D
    HW3DDropDownMenu *hw3d = new HW3DDropDownMenu();
    hw3d->SetProperties( "Hardware3d Sound", x, y+=h, w, 20, "dialog_hw3dsound", " ", true, false );
    hw3d->AddOption( "dialog_enabled", 1, true );
    hw3d->AddOption( "dialog_disabled", 0, true );
    hw3d->RegisterInt( &m_useHardware3D );
    RegisterButton( hw3d );
#endif

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

#if defined(WINDOWS_SDL) || defined(TARGET_OS_MACOSX) || defined(TARGET_OS_LINUX)
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_soundoutputdevice") );
#else
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_soundlibrary") );
#endif
    //g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_memoryusage") );
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_swapstereo") );

#ifdef SOUNDOPTIONSWINDOW_USEHARDWARE3D
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_hw3dsound") );
#endif

#ifdef SOUNDOPTIONSWINDOW_USEDSPEFFECTS
    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_realtimeeffects") );
#endif

    g_renderer->TextSimple( x, y+=h, White, size, LANGUAGEPHRASE("dialog_mastervolume") );
    

    //
    // Sound system CPU usage

    //ProfiledElement *element = g_profiler->m_rootElement->m_children.GetData( "SoundSystem" );
    //if( element && element->m_lastNumCalls > 0 )
    //{
    //    float occup = element->m_lastTotalTime * 100;
    //    
    //    Colour colour = White;
    //    if( occup > 15 ) colour.Set( 255, 50, 50 );
    //    g_renderer->TextCentre( m_x + m_w/2, m_y + m_h - 50, colour, 17, "%s %d%%", LANGUAGEPHRASE("dialog_cpuusage"), int(occup) );
    //}
    //else
    //{
    //    g_renderer->TextCentreSimple( m_x + m_w/2, m_y + m_h - 50, Colour(255,50,50), size, LANGUAGEPHRASE("dialog_cpuusageunknown") );
    //}


    //
    // Memory usage
    
    // Removed glColor4f - color is handled by the modern renderer system
    float memoryUsage = g_soundSampleBank->GetMemoryUsage();
    memoryUsage /= 1024.0f;
    memoryUsage /= 1024.0f;
    g_renderer->TextCentre( m_x + m_w/2, m_y + m_h - 70, White, size, "%s %2.1f Mb", LANGUAGEPHRASE("dialog_memoryusage"), memoryUsage );
}

#if defined(WINDOWS_SDL) || defined(TARGET_OS_MACOSX) || defined(TARGET_OS_LINUX)
const char *SoundOptionsWindow::GetSelectedOutputDeviceName() const
{
    if( m_selectedDeviceIndex <= 0 || m_selectedDeviceIndex >= (int)m_outputDevices.size() )
    {
        return "";
    }
    return m_outputDevices[m_selectedDeviceIndex].c_str();
}


void SoundOptionsWindow::PopulateOutputDevices()
{
    m_outputDevices.clear();
    m_outputDevices.push_back("");

    std::vector<std::string> detectedDevices;
    SoundLibrary2dSDL::EnumerateOutputDevices( detectedDevices );

    for( size_t i = 0; i < detectedDevices.size(); ++i )
    {
        m_outputDevices.push_back( detectedDevices[i] );
    }

    m_selectedDeviceIndex = 0;
    if( !m_preferredOutputDevice.empty() )
    {
        for( size_t i = 1; i < m_outputDevices.size(); ++i )
        {
            if( stricmp( m_outputDevices[i].c_str(), m_preferredOutputDevice.c_str() ) == 0 )
            {
                m_selectedDeviceIndex = (int) i;
                break;
            }
        }
    }
}
#endif



