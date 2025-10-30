#include "lib/universal_include.h"

#include <math.h>
#include <string.h>
#include <algorithm>
#include <mutex>

#include "lib/debug_utils.h"
#include "lib/hi_res_time.h"
#include "lib/preferences.h"

#include "resampler_polyphase.h"

namespace
{
    using namespace SoundResampler;

    struct KernelBank
    {
        Kernel kernel;
        bool built;
        KernelBank()
        {
            built = false;
            kernel.taps = 0;
            kernel.phases = 0;
            kernel.cutoff = 0.0f;
        }
    };

    struct QualityConfig
    {
        Quality quality;
        unsigned int taps;
        unsigned int phases;
    };

    struct ManagerState
    {
        std::mutex buildMutex;
        std::vector<KernelBank> banksLinear;
        std::vector<KernelBank> banksSinc64;
        std::vector<KernelBank> banksSinc96;
        std::vector<KernelBank> banksSinc128;
        Quality sfxQuality;
        Quality musicQuality;
        bool preferencesInitialised;

        ManagerState()
        :   sfxQuality(Quality::Sinc64),
            musicQuality(Quality::Sinc128),
            preferencesInitialised(false)
        {
        }
    };

    ManagerState g_state;

    const float kCutoffBanks[] = { 0.9f, 0.7f, 0.5f };
    const unsigned int kNumBanks = sizeof(kCutoffBanks) / sizeof(kCutoffBanks[0]);

    QualityConfig GetQualityConfig(Quality quality)
    {
        switch (quality)
        {
            case Quality::Sinc64:  return { quality, 64u, 256u };
            case Quality::Sinc96:  return { quality, 96u, 256u };
            case Quality::Sinc128: return { quality, 128u, 512u };
            case Quality::Linear:
            default:
                return { Quality::Linear, 0u, 0u };
        }
    }

    float Sinc(float x)
    {
        if (fabsf(x) < 1e-6f)
        {
            return 1.0f;
        }
        return sinf(M_PI * x) / (M_PI * x);
    }

    float BlackmanHarris(unsigned int n, unsigned int taps)
    {
        if (taps <= 1)
        {
            return 1.0f;
        }

        const float a0 = 0.35875f;
        const float a1 = 0.48829f;
        const float a2 = 0.14128f;
        const float a3 = 0.01168f;
        float theta = (2.0f * M_PI * n) / (taps - 1);
        return a0
             - a1 * cosf(theta)
             + a2 * cosf(2.0f * theta)
             - a3 * cosf(3.0f * theta);
    }

    void BuildKernel(Kernel &kernel, unsigned int taps, unsigned int phases, float cutoff)
    {
        kernel.taps = taps;
        kernel.phases = phases;
        kernel.cutoff = cutoff;
        kernel.coeffs.assign(taps * phases, 0.0f);

        double centre = 0.5 * static_cast<double>(taps) - 1.0;

        for (unsigned int phase = 0; phase < phases; ++phase)
        {
            float frac = static_cast<float>(phase) / static_cast<float>(phases);

            double sum = 0.0;
            for (unsigned int tap = 0; tap < taps; ++tap)
            {
                unsigned int index = phase * taps + tap;
                double offset = static_cast<double>(tap) - centre - static_cast<double>(frac);
                double x = offset * static_cast<double>(cutoff);
                float sincVal = Sinc(static_cast<float>(x));
                float window = BlackmanHarris(tap, taps);
                float value = cutoff * sincVal * window;
                kernel.coeffs[index] = value;
                sum += value;
            }

            if (sum != 0.0)
            {
                double inv = 1.0 / sum;
                for (unsigned int tap = 0; tap < taps; ++tap)
                {
                    unsigned int index = phase * taps + tap;
                    kernel.coeffs[index] = static_cast<float>(kernel.coeffs[index] * inv);
                }
            }
        }
    }

    std::vector<KernelBank> &GetBanks(Quality quality)
    {
        switch (quality)
        {
            case Quality::Sinc64:  return g_state.banksSinc64;
            case Quality::Sinc96:  return g_state.banksSinc96;
            case Quality::Sinc128: return g_state.banksSinc128;
            case Quality::Linear:
            default:
                return g_state.banksLinear;
        }
    }

    void EnsureBanksPrepared(Quality quality)
    {
        std::vector<KernelBank> &banks = GetBanks(quality);
        if (!banks.empty())
        {
            return;
        }

        banks.resize(kNumBanks);
        QualityConfig cfg = GetQualityConfig(quality);
        if (cfg.taps == 0)
        {
            return;
        }

        for (unsigned int i = 0; i < kNumBanks; ++i)
        {
            banks[i].kernel.taps = cfg.taps;
            banks[i].kernel.phases = cfg.phases;
            banks[i].kernel.cutoff = kCutoffBanks[i];
            banks[i].kernel.coeffs.clear();
            banks[i].built = false;
        }
    }

    void EnsureKernelBuilt(Quality quality, unsigned int bankIndex)
    {
        std::vector<KernelBank> &banks = GetBanks(quality);
        if (quality == Quality::Linear || bankIndex >= banks.size())
        {
            return;
        }

        KernelBank &bank = banks[bankIndex];
        if (bank.built)
        {
            return;
        }

        std::lock_guard<std::mutex> guard(g_state.buildMutex);
        if (bank.built)
        {
            return;
        }

        QualityConfig cfg = GetQualityConfig(quality);
        AppDebugAssert(cfg.taps > 0);
        BuildKernel(bank.kernel, cfg.taps, cfg.phases, kCutoffBanks[bankIndex]);
        bank.built = true;
    }

    unsigned int FindBestBank(double ratio)
    {
        float targetCutoff = 0.9f;
        if (ratio > 1.0)
        {
            float inv = 1.0f / static_cast<float>(ratio);
            if (inv < 1.0f)
            {
                targetCutoff = 0.9f * inv;
            }
        }

        unsigned int best = 0;
        float bestDiff = fabsf(targetCutoff - kCutoffBanks[0]);

        for (unsigned int i = 1; i < kNumBanks; ++i)
        {
            float diff = fabsf(targetCutoff - kCutoffBanks[i]);
            if (diff < bestDiff)
            {
                bestDiff = diff;
                best = i;
            }
        }

        return best;
    }
}

namespace SoundResampler
{
    static void UpdateQualityFromPrefs(const Preferences *prefs)
    {
        if (!prefs)
        {
            return;
        }

        if (!g_state.preferencesInitialised)
        {
            g_state.preferencesInitialised = true;
        }

        const char *sfxPref = prefs->GetString(PREFS_SOUND_RESAMPLER_SFX, "sinc64");
        const char *musicPref = prefs->GetString(PREFS_SOUND_RESAMPLER_MUSIC, "sinc128");

        g_state.sfxQuality = QualityFromString(sfxPref, Quality::Sinc64);
        g_state.musicQuality = QualityFromString(musicPref, Quality::Sinc128);
    }

    void Initialise(const Preferences *prefs)
    {
        UpdateQualityFromPrefs(prefs);

        // Pre-size kernel banks so the audio thread only needs to trigger builds.
        EnsureBanksPrepared(Quality::Sinc64);
        EnsureBanksPrepared(Quality::Sinc96);
        EnsureBanksPrepared(Quality::Sinc128);
        EnsureBanksPrepared(Quality::Linear);

        Quality sfxQuality = g_state.sfxQuality;
        Quality musicQuality = g_state.musicQuality;
        for (unsigned int i = 0; i < kNumBanks; ++i)
        {
            EnsureKernelBuilt(sfxQuality, i);
            EnsureKernelBuilt(musicQuality, i);
        }
    }

    void ReloadPreferences(const Preferences *prefs)
    {
        UpdateQualityFromPrefs(prefs);
    }

    Quality GetSfxQuality()
    {
        return g_state.sfxQuality;
    }

    Quality GetMusicQuality()
    {
        return g_state.musicQuality;
    }

    void SetSfxQuality(Quality quality)
    {
        g_state.sfxQuality = quality;
    }

    void SetMusicQuality(Quality quality)
    {
        g_state.musicQuality = quality;
    }

    Selection SelectKernel(Quality quality, double ratio)
    {
        Selection result = { NULL, 0u };
        if (quality == Quality::Linear)
        {
            return result;
        }

        EnsureBanksPrepared(quality);
        unsigned int bankIndex = FindBestBank(ratio);
        EnsureKernelBuilt(quality, bankIndex);

        std::vector<KernelBank> &banks = GetBanks(quality);
        if (bankIndex < banks.size())
        {
            result.kernel = &banks[bankIndex].kernel;
            result.bankIndex = bankIndex;
        }

        return result;
    }

    const KernelSet &GetKernelSet(Quality quality)
    {
        EnsureBanksPrepared(quality);

        switch (quality)
        {
            case Quality::Sinc64:
            {
                static KernelSet set = { Quality::Sinc64 };
                set.banks.clear();
                std::vector<KernelBank> &banks = GetBanks(quality);
                set.banks.reserve(banks.size());
                for (size_t i = 0; i < banks.size(); ++i)
                {
                    EnsureKernelBuilt(quality, static_cast<unsigned int>(i));
                    set.banks.push_back(banks[i].kernel);
                }
                return set;
            }

            case Quality::Sinc96:
            {
                static KernelSet set = { Quality::Sinc96 };
                set.banks.clear();
                std::vector<KernelBank> &banks = GetBanks(quality);
                set.banks.reserve(banks.size());
                for (size_t i = 0; i < banks.size(); ++i)
                {
                    EnsureKernelBuilt(quality, static_cast<unsigned int>(i));
                    set.banks.push_back(banks[i].kernel);
                }
                return set;
            }

            case Quality::Sinc128:
            {
                static KernelSet set = { Quality::Sinc128 };
                set.banks.clear();
                std::vector<KernelBank> &banks = GetBanks(quality);
                set.banks.reserve(banks.size());
                for (size_t i = 0; i < banks.size(); ++i)
                {
                    EnsureKernelBuilt(quality, static_cast<unsigned int>(i));
                    set.banks.push_back(banks[i].kernel);
                }
                return set;
            }

            case Quality::Linear:
            default:
            {
                static KernelSet set = { Quality::Linear };
                set.banks.clear();
                return set;
            }
        }
    }

    const Kernel &GetKernel(Quality quality, unsigned int bankIndex)
    {
        static Kernel dummy;
        EnsureBanksPrepared(quality);
        EnsureKernelBuilt(quality, bankIndex);
        std::vector<KernelBank> &banks = GetBanks(quality);
        if (bankIndex >= banks.size())
        {
            return dummy;
        }
        return banks[bankIndex].kernel;
    }

    unsigned int GetKernelBankCount()
    {
        return kNumBanks;
    }

    const char *QualityToString(Quality quality)
    {
        switch (quality)
        {
            case Quality::Linear:  return "linear";
            case Quality::Sinc64:  return "sinc64";
            case Quality::Sinc96:  return "sinc96";
            case Quality::Sinc128: return "sinc128";
        }
        return "linear";
    }

    Quality QualityFromString(const char *name, Quality fallback)
    {
        if (!name)
        {
            return fallback;
        }

        if (stricmp(name, "linear") == 0)  return Quality::Linear;
        if (stricmp(name, "sinc64") == 0)  return Quality::Sinc64;
        if (stricmp(name, "sinc96") == 0)  return Quality::Sinc96;
        if (stricmp(name, "sinc128") == 0) return Quality::Sinc128;

        return fallback;
    }
}
