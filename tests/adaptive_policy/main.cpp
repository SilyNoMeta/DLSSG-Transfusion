// Adaptive multiplier controller (source/native/adaptive_policy.h) against a
// simulated game whose source cadence drops as frame generation costs more.
#include "../../source/native/adaptive_policy.h"

#include <cstdio>
#include <random>
#include <vector>

static int gFailures = 0;
#define CHECK(condition)                                                     \
    do                                                                       \
    {                                                                        \
        if (!(condition))                                                    \
        {                                                                    \
            std::printf("FAILED line %d: %s\n", __LINE__, #condition);       \
            ++gFailures;                                                     \
        }                                                                    \
    } while (false)

// Source frames per second at a given factor: each generated frame costs a
// share of the render budget.
struct Game
{
    double baseFps;
    double costPerGenerated;
    double Source(unsigned factor) const { return baseFps / (1.0 + costPerGenerated * (factor - 1)); }
};

struct Run
{
    adaptive_policy::Controller controller;
    uint32_t frame = 1;
    double now = 0.0;
};

// Drives the controller as the engine does: sample, submit, accept.
static void Play(Run& run, const Game& game, unsigned target, double seconds, std::mt19937& random, double noise = 0.03)
{
    std::uniform_real_distribution<double> jitter(1.0 - noise, 1.0 + noise);
    const double end = run.now + seconds;
    while (run.now < end)
    {
        run.now += jitter(random) / game.Source(run.controller.factor);
        const unsigned next = run.controller.Sample(run.frame++, run.now, false, target);
        run.controller.Accept(next, run.now);
    }
}

// The smallest factor whose output reaches the target, as the thresholds define it.
static unsigned Expected(const Game& game, unsigned target, unsigned ceiling)
{
    for (unsigned f = adaptive_policy::kMinimum; f < ceiling; ++f)
        if (game.Source(f) * f >= target * 0.97) return f;
    return ceiling;
}

int main()
{
    std::mt19937 random(1234);
    const Game game{70.0, 0.04};

    // Without frame-generation cost the target selects one factor, reached
    // from the bottom and from the top.
    const Game free{70.0, 0.0};
    for (unsigned expected = 2; expected <= 6; ++expected)
    {
        const unsigned target = static_cast<unsigned>(free.Source(expected) * expected * 0.99);
        CHECK(Expected(free, target, 6) == expected);
        for (unsigned start : {2u, 6u})
        {
            Run run;
            run.controller.Reset(start, 6);
            Play(run, free, target, 40.0, random);
            if (run.controller.factor != expected)
                std::printf("target %u from X%u: X%u, expected X%u\n", target, start, run.controller.factor, expected);
            CHECK(run.controller.factor == expected);
        }
    }

    // With a cost, the controller settles where neither rule moves it: the
    // output reaches the target, and one factor less, at the cadence measured
    // now, would not. It may stay one factor above the cheapest sufficient
    // one, never below it.
    for (unsigned expected = 2; expected <= 6; ++expected)
    {
        const unsigned target = static_cast<unsigned>(game.Source(expected) * expected * 0.99);
        for (unsigned start : {2u, 6u})
        {
            Run run;
            run.controller.Reset(start, 6);
            Play(run, game, target, 40.0, random);
            const unsigned f = run.controller.factor;
            CHECK(f == expected || f == expected + 1);
            CHECK(game.Source(f) * f >= target * 0.97 * 0.97);
            CHECK(f == 2 || game.Source(f) * (f - 1) < target * 0.995 * 1.03);
        }
    }

    // The ceiling holds (4x unless 5x/6x are allowed).
    {
        Run run;
        run.controller.Reset(2, 4);
        Play(run, game, 1000, 30.0, random);
        CHECK(run.controller.factor == 4);
    }

    // A repeated frame token is not a new frame.
    {
        adaptive_policy::Controller c;
        c.Reset(3, 6);
        for (int i = 0; i < 100; ++i) c.Sample(7, 0.01 * i, false, 200);
        CHECK(c.samples == 0 && !c.ready);
    }

    // A reset, a skipped token or a stall clears the estimate, not the factor.
    {
        Run run;
        run.controller.Reset(4, 6);
        Play(run, game, 0, 2.0, random); // target 0: learn only
        CHECK(run.controller.ready && run.controller.factor == 4);
        run.controller.Sample(run.frame++, run.now + 0.016, true, 0);
        CHECK(!run.controller.ready && run.controller.samples == 0 && run.controller.factor == 4);
        Play(run, game, 0, 2.0, random);
        run.frame += 5;
        run.controller.Sample(run.frame++, run.now + 0.016, false, 0);
        CHECK(!run.controller.ready && run.controller.factor == 4);
        Play(run, game, 0, 2.0, random);
        run.now += 0.5;
        run.controller.Sample(run.frame++, run.now, false, 0);
        CHECK(!run.controller.ready && run.controller.factor == 4);
    }

    // A raise that brings nothing (the game is capped) is undone; the next
    // attempts wait 5, 10, 20 then 40 s. A new target allows raising at once.
    {
        adaptive_policy::Controller c;
        c.Reset(2, 6);
        uint32_t frame = 1;
        double now = 0.0;
        std::vector<double> raises, undos;
        unsigned previous = c.factor;
        unsigned target = 240;
        // Output is fixed at 120 FPS whatever the factor: source = 120 / factor.
        auto play = [&](double until) {
            while (now < until)
            {
                now += c.factor / 120.0;
                c.Accept(c.Sample(frame++, now, false, target), now);
                if (c.factor > previous) raises.push_back(now);
                if (c.factor < previous) undos.push_back(now);
                previous = c.factor;
            }
        };
        play(120.0);
        CHECK(c.factor <= 3); // never climbs
        CHECK(raises.size() >= 5 && undos.size() >= 4);
        for (size_t i = 0; i < undos.size() && i < 4; ++i)
            CHECK(undos[i] > raises[i] && undos[i] - raises[i] < 2.0);
        const double waits[] = {5.0, 10.0, 20.0, 40.0};
        for (size_t i = 0; i + 1 < raises.size() && i < 4; ++i)
            CHECK(raises[i + 1] - undos[i] >= waits[i] && raises[i + 1] - undos[i] < waits[i] + 1.0);
        // The controller is waiting 40 s; a new target lifts the wait.
        const size_t before = raises.size();
        while (c.factor != 2) play(now + 0.1);
        const double changed = now;
        target = 250;
        play(now + 3.0);
        CHECK(raises.size() > before && raises.back() - changed < 2.0);
    }

    // A refused factor stops the controller until it is reset.
    {
        adaptive_policy::Controller c;
        c.Reset(3, 6);
        c.rejected = true;
        for (uint32_t i = 1; i < 200; ++i) CHECK(c.Sample(i, i * 0.02, false, 1000) == 3);
    }

    std::printf(gFailures ? "adaptive_policy: %d failure(s)\n" : "adaptive_policy: all checks passed\n", gFailures);
    return gFailures ? 1 : 0;
}
