#ifndef INCLUDED_RESAMPLER_POLYPHASE_H
#define INCLUDED_RESAMPLER_POLYPHASE_H


/*
 *  Polyphase resampler
 *
 *  Uses windowed sinc interpolation with multiple quality levels
 *  and automatically selects appropriate anti-aliasing filters
 *
 */

#include <vector>
#include <string>

class Preferences;

//*****************************************************************************
// Namespace SoundResampler
//*****************************************************************************

namespace SoundResampler
{
    //
    // Quality levels for resampling
    // Linear = simple interpolation
    // Sinc64/96/128 = windowed sinc with 64/96/128 tap filters
    
    enum class Quality
    {
        Linear = 0,
        Sinc64,
        Sinc96,
        Sinc128
    };

    //
    // Kernel contains the filter coefficients for one cutoff frequency
    
    struct Kernel
    {
        unsigned int taps;
        unsigned int phases;
        float cutoff;
        std::vector<float> coeffs;
    };

    //
    // KernelSet holds all filter banks for a given quality level
    
    struct KernelSet
    {
        Quality quality;
        std::vector<Kernel> banks;
    };

    //
    // Selection returned by SelectKernel
    
    struct Selection
    {
        const Kernel *kernel;
        unsigned int bankIndex;
    };

    void Initialise(const Preferences *prefs);
    void ReloadPreferences(const Preferences *prefs);

    Quality GetSfxQuality();
    Quality GetMusicQuality();
    void SetSfxQuality(Quality quality);
    void SetMusicQuality(Quality quality);

    Selection SelectKernel(Quality quality, double ratio);               // Selects best kernel for the given resampling ratio

    const KernelSet &GetKernelSet(Quality quality);
    const Kernel &GetKernel(Quality quality, unsigned int bankIndex);
    unsigned int GetKernelBankCount();
    const char *QualityToString(Quality quality);
    Quality QualityFromString(const char *name, Quality fallback);
}

#endif
