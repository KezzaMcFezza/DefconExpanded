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
    if (g_windowManager)
    {
        baseX = std::max(25.0f, static_cast<float>(g_windowManager->WindowW()) - 420.0f);
    }

    float y = 55.0f;
    const float line = 16.0f;
    const float sectionGap = 6.0f;
    char buffer[256];

    g_renderer->TextSimple(baseX, y, Colour(255, 220, 80, 255), 14.0f, "Sound Safety Overlay (F4)");
    y += line;

    Colour sectionColour(180, 220, 255, 255);
    Colour textColour(255, 255, 255, 255);
    Colour warnColour(255, 80, 80, 255);

    // Shared headroom
    g_renderer->TextSimple(baseX, y, sectionColour, 12.0f, "Headroom");
    y += line;
    float headroomDb = g_preferences ? g_preferences->GetFloat("SoundHeadroomDb", 12.0f) : 12.0f;
    snprintf(buffer, sizeof(buffer), "SoundHeadroomDb     : %.1f dB", headroomDb);
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
            g_renderer->TextSimple(baseX, y, sectionColour, 12.0f, "SDL Mix-Bus Limiter");
            y += line;

            float thr = sw->GetLimiterThreshold();
            float busGain = sw->GetLimiterBusGain();
            bool limiting = busGain < 0.999f;

            snprintf(buffer, sizeof(buffer), "Limiter threshold   : %.0f (PCM)", thr);
            g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer); y += line;

            float atk = g_preferences ? g_preferences->GetFloat("SoundLimiterAttack", 1.0f) : 1.0f;
            float rel = g_preferences ? g_preferences->GetFloat("SoundLimiterRelease", 0.02f) : 0.02f;
            snprintf(buffer, sizeof(buffer), "Limiter attack/rel  : %.2f / %.2f", atk, rel);
            g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer); y += line;

            snprintf(buffer, sizeof(buffer), "Limiter bus gain    : %.3f", busGain);
            g_renderer->TextSimple(baseX, y, limiting ? warnColour : textColour, 11.0f, buffer); y += line;
            if (limiting)
            {
                g_renderer->TextSimple(baseX, y, warnColour, 11.0f, "LIMITER ACTIVE"); y += line;
            }

            // SDL device snapshot
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

            float fixHd = ds->GetFixedHeadroomDb();
            snprintf(buffer, sizeof(buffer), "Fixed headroom      : %.1f dB", fixHd);
            g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer); y += line;

            float dynDb = ds->GetDynamicBusAttenDb();
            snprintf(buffer, sizeof(buffer), "Dynamic attenuation : %.1f dB", dynDb);
            g_renderer->TextSimple(baseX, y, dynDb > 0.05f ? warnColour : textColour, 11.0f, buffer); y += line;
            if (dynDb > 0.05f)
            {
                g_renderer->TextSimple(baseX, y, warnColour, 11.0f, "ATTENUATION ACTIVE"); y += line;
            }

            snprintf(buffer, sizeof(buffer), "Start@loud count    : %d  loud vol >= %.1f",
                     ds->GetDynStartCount(), ds->GetDynLoudVolThresh());
            g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer); y += line;

            snprintf(buffer, sizeof(buffer), "Per extra / max dB  : %.1f / %.1f",
                     ds->GetDynDbPerExtra(), ds->GetDynMaxDb());
            g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer); y += line;

            snprintf(buffer, sizeof(buffer), "Attack / release    : %.2f / %.2f",
                     ds->GetDynAttack(), ds->GetDynRelease());
            g_renderer->TextSimple(baseX, y, textColour, 11.0f, buffer); y += line;
        }
#endif
    }

    // Help text
    y += sectionGap;
    g_renderer->TextSimple(baseX, y, Colour(200,200,200,255), 11.0f,
                           "Headroom is the main safety knob; others fine-tune behavior.");
}
