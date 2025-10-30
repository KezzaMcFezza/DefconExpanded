#include "lib/universal_include.h"

#include <stdio.h>
#include <algorithm>

#include "lib/sound/sound_protection_overlay.h"
#include "lib/sound/sound_library_3d.h"
#include "lib/sound/sound_library_2d_sdl.h"
#include "lib/sound/sound_library_3d_software.h"
#include "lib/preferences.h"
#include "lib/render2d/renderer.h"
#include "lib/gucci/window_manager.h"
#include "lib/hi_res_time.h"

#if defined(TARGET_MSVC) && !defined(WINDOWS_SDL)
#include "lib/sound/sound_library_3d_dsound.h"
#endif

SoundProtectionOverlay::SoundProtectionOverlay()
{
}

void SoundProtectionOverlay::Update(double /*frameTime*/)
{
    // Nothing to precompute for now
}

void SoundProtectionOverlay::Render()
{
    if (!g_renderer) return;

    float baseX = 25.0f;
    float y = 55.0f;
    const float line = 16.0f;
    const float sectionGap = 6.0f;
    char buffer[256];

    g_renderer->TextSimple(baseX, y, Colour(255, 220, 80, 255), 14.0f, "Sound Safety Overlay (F4)");
    y += line;

    Colour sectionColour(180, 220, 255, 255);
    Colour textColour(255, 255, 255, 255);

    // Shared headroom (pref value)
    g_renderer->TextSimple(baseX, y, sectionColour, 12.0f, "Headroom");
    y += line;
    float prefHeadroomDb = g_preferences ? g_preferences->GetFloat("SoundHeadroomDb", 12.0f) : 12.0f;
    bool prefHeadroomEnabled = g_preferences ? (g_preferences->GetInt("SoundHeadroomEnabled", 0) != 0) : false;
    bool headroomEnabled = prefHeadroomEnabled;
    if (g_soundLibrary3d)
    {
        if (SoundLibrary3dSoftware *sw = dynamic_cast<SoundLibrary3dSoftware *>(g_soundLibrary3d))
        {
            headroomEnabled = sw->GetHeadroomEnabled();
        }
#if defined(TARGET_MSVC) && !defined(WINDOWS_SDL)
        else if (SoundLibrary3dDirectSound *ds = dynamic_cast<SoundLibrary3dDirectSound *>(g_soundLibrary3d))
        {
            headroomEnabled = ds->GetHeadroomEnabled();
        }
#endif
    }
    snprintf(buffer, sizeof(buffer), "SoundHeadroomDb     : %.1f dB%s",
             prefHeadroomDb, headroomEnabled ? "" : " (disabled)");
    g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer);
    y += line;

    y += sectionGap;

    // Backend-specific blocks
    if (g_soundLibrary3d)
    {
        // SDL software path
        if (dynamic_cast<SoundLibrary3dSoftware *>(g_soundLibrary3d))
        {
            SoundLibrary3dSoftware *sw = (SoundLibrary3dSoftware *)g_soundLibrary3d;
            g_renderer->TextSimple(baseX, y, sectionColour, 12.0f, "SDL Mixer");
            y += line;

            bool swHeadroomEnabled = sw->GetHeadroomEnabled();
            float swHeadroomDb = swHeadroomEnabled ? sw->GetHeadroomDb() : 0.0f;
            snprintf(buffer, sizeof(buffer), "Effective headroom  : %.1f dB%s",
                     swHeadroomDb, swHeadroomEnabled ? "" : " (disabled)");
            g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer); y += line;

            SoundLibrary2dSDL *sdl2d = g_soundLibrary2d ? dynamic_cast<SoundLibrary2dSDL *>(g_soundLibrary2d) : NULL;
            if (sdl2d)
            {
                unsigned fmt = sdl2d->GetActualFormat();
                unsigned ch = sdl2d->GetActualChannels();
                unsigned freq = sdl2d->GetActualFreq();
                snprintf(buffer, sizeof(buffer), "SDL device fmt/ch/f : 0x%x / %u / %u Hz", fmt, ch, freq);
                g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer); y += line;
            }
        }

#if defined(TARGET_MSVC) && !defined(WINDOWS_SDL)
        // DirectSound path
        else if (dynamic_cast<SoundLibrary3dDirectSound *>(g_soundLibrary3d))
        {
            SoundLibrary3dDirectSound *ds = (SoundLibrary3dDirectSound *)g_soundLibrary3d;
            g_renderer->TextSimple(baseX, y, sectionColour, 12.0f, "DirectSound Anti-Clip");
            y += line;

            bool dsHeadroomEnabled = ds->GetHeadroomEnabled();
            float fixHd = ds->GetFixedHeadroomDb();
            snprintf(buffer, sizeof(buffer), "Fixed headroom      : %.1f dB%s", fixHd, dsHeadroomEnabled ? "" : " (disabled)");
            g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer); y += line;

        }
#endif
    }
}
