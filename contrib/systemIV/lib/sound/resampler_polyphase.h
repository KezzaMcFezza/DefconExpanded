#ifndef INCLUDED_RESAMPLER_POLYPHASE_H
#define INCLUDED_RESAMPLER_POLYPHASE_H

#include <vector>
#include <string>

class Preferences;

namespace SoundResampler
{
    enum class Quality
    {
        Linear = 0,
        Sinc64,
        Sinc96,
        Sinc128
    };

    struct Kernel
    {
        unsigned int taps;
        unsigned int phases;
        float cutoff;
        std::vector<float> coeffs;
    };

    struct KernelSet
    {
        Quality quality;
        std::vector<Kernel> banks;
    };

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

    Selection SelectKernel(Quality quality, double ratio);

    const KernelSet &GetKernelSet(Quality quality);
    const Kernel &GetKernel(Quality quality, unsigned int bankIndex);
    unsigned int GetKernelBankCount();
    const char *QualityToString(Quality quality);
    Quality QualityFromString(const char *name, Quality fallback);
}

#endif
