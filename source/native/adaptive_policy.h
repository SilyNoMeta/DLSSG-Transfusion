#pragma once
// Adaptive multiplier controller for Vulkan, where Streamline offers no native
// Dynamic MFG: it picks a fixed multiplier (X2..X6) from the source-frame
// cadence and the target frame rate, and the engine submits it as a normal
// fixed DLSS-G request. Ported from dlssg_for_sm86 0.3.5
// (src/companion/adaptive_policy.hpp), validated there in No Man's Sky.
//
// The cadence is that of the game's unique source frames (one sample per
// slSetConstants frame token), which includes the cost of frame generation.
// Output is estimated as cadence x multiplier: it is not a presentation count,
// a GPU time or a frame limiter. No Windows dependency: tests/adaptive_policy.
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace adaptive_policy
{
inline constexpr unsigned kMinimum = 2;
inline constexpr unsigned kMaximum = 6;

struct Controller
{
    unsigned factor = kMinimum;
    unsigned ceiling = kMaximum;
    unsigned samples = 0;
    unsigned transitions = 0;
    unsigned failedRaises = 0;  // consecutive raises undone for lack of gain
    unsigned lastTarget = 0;
    uint32_t lastFrame = 0;
    double lastTime = 0.0;
    double elapsed = 0.0;
    double mean = 0.0;          // filtered source-frame interval, seconds
    double changedAt = 0.0;
    double blockRaiseUntil = 0.0;
    double beforeRaiseOutput = 0.0;
    bool haveFrame = false;
    bool ready = false;
    bool rejected = false;      // Streamline refused a factor: stop until reset
    bool probingRaise = false;

    void Reset(unsigned initial, unsigned cap)
    {
        *this = {};
        ceiling = std::clamp(cap, kMinimum, kMaximum);
        factor = std::clamp(initial, kMinimum, ceiling);
    }

    void ClearCadence()
    {
        samples = 0;
        elapsed = mean = 0.0;
        ready = false;
        probingRaise = false;
    }

    double SourceFps() const { return mean > 0.0 ? 1.0 / mean : 0.0; }

    // One call per frame token; a repeated token is ignored. Returns the
    // factor to use, which the caller submits and then confirms with Accept.
    unsigned Sample(uint32_t frame, double now, bool reset, unsigned target)
    {
        if (!std::isfinite(now) || now < 0.0 || rejected) return factor;
        if (haveFrame && frame == lastFrame) return factor;
        const uint32_t delta = frame - lastFrame;
        const double dt = now - lastTime;
        const bool first = !haveFrame;
        haveFrame = true;
        lastFrame = frame;
        lastTime = now;
        // A skipped token, a game reset or an interval outside 1-200 ms
        // (loading, pause, alt-tab) restarts the estimate, not the factor.
        if (first || reset || delta != 1 || dt < 0.001 || dt > 0.200)
        {
            ClearCadence();
            return factor;
        }
        mean = mean > 0.0 ? mean + (dt / (0.6 + dt)) * (dt - mean) : dt; // ~0.6 s time constant
        ++samples;
        elapsed += dt;
        ready = samples >= 20 && elapsed >= 0.75;
        if (target != lastTarget)
        {
            // A new target is a new question: raising is allowed again.
            lastTarget = target;
            failedRaises = 0;
            blockRaiseUntil = 0.0;
        }
        if (!ready || target < 1 || target > 1000 || now - changedAt < 1.5) return factor;
        const double source = SourceFps();
        // A raise that did not improve the estimated output by 1 % is undone.
        // Raising then waits 5 s, doubled after each such raise up to 40 s
        // (a capped game would otherwise pay a raise every few seconds).
        if (probingRaise)
        {
            probingRaise = false;
            if (source * factor < beforeRaiseOutput * 1.01)
            {
                blockRaiseUntil = now + 5.0 * (1u << std::min(failedRaises, 3u));
                ++failedRaises;
                return factor > kMinimum ? factor - 1 : factor;
            }
            failedRaises = 0;
        }
        if (factor > kMinimum && source * (factor - 1) >= target * 0.995) return factor - 1;
        if (factor < ceiling && now >= blockRaiseUntil && source * factor < target * 0.97) return factor + 1;
        return factor;
    }

    // The factor Streamline accepted. The cadence measured at the previous
    // factor no longer applies: the estimate restarts, and the next decision
    // uses at least 1.5 s of cadence at the new factor. (dlssg_for_sm86 kept
    // the estimate; 1.5 s after a change it still carries ~8 % of the old
    // cadence, enough for a useless raise in a capped game to look like a
    // gain, and to climb to X6.)
    void Accept(unsigned next, double now)
    {
        next = std::clamp(next, kMinimum, ceiling);
        if (next == factor) return;
        beforeRaiseOutput = SourceFps() * factor;
        ClearCadence();
        probingRaise = next > factor;
        factor = next;
        changedAt = now;
        ++transitions;
    }
};
}
