#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include "scatter_experiment.h"

namespace quality_fix
{
// Runs after candidate construction and before the provider's UI composition.
// Only RGB changes. Auxiliary channels, rejection masks and memory accesses stay intact.
inline constexpr std::string_view kRegisters = R"ptx(
.reg .pred %qv<10>;
.reg .f32 %qf<16>;
.reg .u16 %qrs<2>;
)ptx";
inline constexpr std::string_view kPolicy = R"ptx(
// QUALITY_VALID_WARP_V4
cvt.rn.f32.u32 %qf0, %r10;
cvt.rn.f32.u32 %qf1, %r11;
div.approx.ftz.f32 %qf0, 0f3F000000, %qf0;
div.approx.ftz.f32 %qf1, 0f3F000000, %qf1;
sub.ftz.f32 %qf2, 0f3F800000, %qf0;
sub.ftz.f32 %qf3, 0f3F800000, %qf1;
setp.ge.f32 %qv0, %f123, %qf0;
setp.le.f32 %qv2, %f123, %qf2;
and.pred %qv0, %qv0, %qv2;
setp.ge.f32 %qv2, %f124, %qf1;
and.pred %qv0, %qv0, %qv2;
setp.le.f32 %qv2, %f124, %qf3;
and.pred %qv0, %qv0, %qv2;
not.pred %qv2, %p17;
and.pred %qv0, %qv0, %qv2;
setp.ge.f32 %qv1, %f129, %qf0;
setp.le.f32 %qv2, %f129, %qf2;
and.pred %qv1, %qv1, %qv2;
setp.ge.f32 %qv2, %f130, %qf1;
and.pred %qv1, %qv1, %qv2;
setp.le.f32 %qv2, %f130, %qf3;
and.pred %qv1, %qv1, %qv2;
not.pred %qv2, %p16;
and.pred %qv1, %qv1, %qv2;
abs.f32 %qf4, %f125;
abs.f32 %qf5, %f126;
abs.f32 %qf6, %f127;
add.f32 %qf4, %qf4, %qf5;
add.f32 %qf4, %qf4, %qf6;
setp.lt.f32 %qv2, %qf4, 0f7F800000;
and.pred %qv0, %qv0, %qv2;
abs.f32 %qf5, %f131;
abs.f32 %qf6, %f132;
abs.f32 %qf7, %f133;
add.f32 %qf5, %qf5, %qf6;
add.f32 %qf5, %qf5, %qf7;
setp.lt.f32 %qv2, %qf5, 0f7F800000;
and.pred %qv1, %qv1, %qv2;
// Do not turn low confidence into a trusted vector, or discard a valid
// disagreeing candidate just because its confidence is lower.
and.pred %qv3, %qv0, %qv1;
sub.f32 %qf6, %f125, %f131;
sub.f32 %qf7, %f126, %f132;
sub.f32 %qf8, %f127, %f133;
abs.f32 %qf6, %qf6;
abs.f32 %qf7, %qf7;
abs.f32 %qf8, %qf8;
add.f32 %qf6, %qf6, %qf7;
add.f32 %qf6, %qf6, %qf8;
max.f32 %qf4, %qf4, %qf5;
max.f32 %qf4, %qf4, 0f3F800000;
mul.f32 %qf4, %qf4, 0f3DCCCCCD;
setp.le.f32 %qv2, %qf6, %qf4;
not.pred %qv4, %qv3;
or.pred %qv2, %qv2, %qv4;
and.pred %qv0, %qv0, %qv2;
and.pred %qv1, %qv1, %qv2;
ld.param.u8 %qrs0, [%rd6+220];
setp.ne.s16 %qv9, %qrs0, 0;
setp.ge.f32 %qv5, %f148, 0f3E4CCCCD;
setp.le.f32 %qv2, %f148, 0f3F800000;
and.pred %qv5, %qv5, %qv2;
or.pred %qv5, %qv5, %qv9;
setp.ge.f32 %qv6, %f149, 0f3E4CCCCD;
setp.le.f32 %qv2, %f149, 0f3F800000;
and.pred %qv6, %qv6, %qv2;
or.pred %qv6, %qv6, %qv9;
// Single-source copying requires the other source to be geometrically invalid,
// not merely below the confidence threshold.
not.pred %qv7, %qv1;
and.pred %qv7, %qv7, %qv0;
not.pred %qv8, %qv0;
and.pred %qv8, %qv8, %qv1;
and.pred %qv0, %qv0, %qv5;
and.pred %qv1, %qv1, %qv6;
and.pred %qv7, %qv7, %qv0;
and.pred %qv8, %qv8, %qv1;
max.f32 %qf0, %f148, 0f3F59999A;
max.f32 %qf1, %f149, 0f3F59999A;
sub.f32 %qf2, %f125, %f115;
sub.f32 %qf3, %f126, %f116;
sub.f32 %qf4, %f127, %f117;
@%qv0 fma.rn.f32 %f39, %qf0, %qf2, %f115;
@%qv0 fma.rn.f32 %f38, %qf0, %qf3, %f116;
@%qv0 fma.rn.f32 %f37, %qf0, %qf4, %f117;
sub.f32 %qf2, %f131, %f119;
sub.f32 %qf3, %f132, %f120;
sub.f32 %qf4, %f133, %f121;
@%qv1 fma.rn.f32 %f43, %qf1, %qf2, %f119;
@%qv1 fma.rn.f32 %f42, %qf1, %qf3, %f120;
@%qv1 fma.rn.f32 %f41, %qf1, %qf4, %f121;
@%qv7 mov.f32 %f43, %f39;
@%qv7 mov.f32 %f42, %f38;
@%qv7 mov.f32 %f41, %f37;
@%qv8 mov.f32 %f39, %f43;
@%qv8 mov.f32 %f38, %f42;
@%qv8 mov.f32 %f37, %f41;
)ptx";

inline constexpr std::string_view kPolicyE2 = R"ptx(
// QUALITY_VALID_WARP_V4_E2
cvt.rn.f32.u32 %qf0, %r10;
cvt.rn.f32.u32 %qf1, %r11;
div.approx.ftz.f32 %qf0, 0f3F000000, %qf0;
div.approx.ftz.f32 %qf1, 0f3F000000, %qf1;
sub.ftz.f32 %qf2, 0f3F800000, %qf0;
sub.ftz.f32 %qf3, 0f3F800000, %qf1;
setp.ge.f32 %qv0, %f123, %qf0;
setp.le.f32 %qv2, %f123, %qf2;
and.pred %qv0, %qv0, %qv2;
setp.ge.f32 %qv2, %f124, %qf1;
and.pred %qv0, %qv0, %qv2;
setp.le.f32 %qv2, %f124, %qf3;
and.pred %qv0, %qv0, %qv2;
not.pred %qv2, %p17;
and.pred %qv0, %qv0, %qv2;
setp.ge.f32 %qv1, %f129, %qf0;
setp.le.f32 %qv2, %f129, %qf2;
and.pred %qv1, %qv1, %qv2;
setp.ge.f32 %qv2, %f130, %qf1;
and.pred %qv1, %qv1, %qv2;
setp.le.f32 %qv2, %f130, %qf3;
and.pred %qv1, %qv1, %qv2;
not.pred %qv2, %p16;
and.pred %qv1, %qv1, %qv2;
abs.f32 %qf4, %f125;
abs.f32 %qf5, %f126;
abs.f32 %qf6, %f127;
add.f32 %qf4, %qf4, %qf5;
add.f32 %qf4, %qf4, %qf6;
setp.lt.f32 %qv2, %qf4, 0f7F800000;
and.pred %qv0, %qv0, %qv2;
abs.f32 %qf5, %f131;
abs.f32 %qf6, %f132;
abs.f32 %qf7, %f133;
add.f32 %qf5, %qf5, %qf6;
add.f32 %qf5, %qf5, %qf7;
setp.lt.f32 %qv2, %qf5, 0f7F800000;
and.pred %qv1, %qv1, %qv2;
and.pred %qv3, %qv0, %qv1;
ld.param.u8 %qrs0, [%rd6+220];
setp.ne.s16 %qv9, %qrs0, 0;

// Base acceptance: 100% warp for valid moving geometry across the scene (fence, wire, background)
setp.ge.f32 %qv5, %f148, 0f3E4CCCCD;
or.pred %qv5, %qv5, %qv9;
and.pred %qv0, %qv0, %qv5;

setp.ge.f32 %qv6, %f149, 0f3E4CCCCD;
or.pred %qv6, %qv6, %qv9;
and.pred %qv1, %qv1, %qv6;

max.f32 %qf0, %f148, 0f3F59999A;
min.f32 %qf0, %qf0, 0f3F800000;
max.f32 %qf1, %f149, 0f3F59999A;
min.f32 %qf1, %qf1, 0f3F800000;
sub.f32 %qf2, %f125, %f115;
sub.f32 %qf3, %f126, %f116;
sub.f32 %qf4, %f127, %f117;
@%qv0 fma.rn.f32 %f39, %qf0, %qf2, %f115;
@%qv0 fma.rn.f32 %f38, %qf0, %qf3, %f116;
@%qv0 fma.rn.f32 %f37, %qf0, %qf4, %f117;
sub.f32 %qf2, %f131, %f119;
sub.f32 %qf3, %f132, %f120;
sub.f32 %qf4, %f133, %f121;
@%qv1 fma.rn.f32 %f43, %qf1, %qf2, %f119;
@%qv1 fma.rn.f32 %f42, %qf1, %qf3, %f120;
@%qv1 fma.rn.f32 %f41, %qf1, %qf4, %f121;

// E_motion = |Warped0 - Warped1|
sub.f32 %qf9, %f125, %f131;
sub.f32 %qf10, %f126, %f132;
sub.f32 %qf11, %f127, %f133;
abs.f32 %qf9, %qf9;
abs.f32 %qf10, %qf10;
abs.f32 %qf11, %qf11;
add.f32 %qf9, %qf9, %qf10;
add.f32 %qf9, %qf9, %qf11;

// Candidate Reconciliation & Anti-Ghosting Firewall:
// When candidates conflict (E_motion > 0.35f = 0f3EB33333):
// Propagate the winning candidate to eliminate duplicate ghost outlines.
setp.gt.f32 %qv4, %qf9, 0f3EB33333;
and.pred %qv4, %qv4, %qv3;

// Candidate 0 wins if %f148 > %f149 and Candidate 0 is valid:
setp.gt.f32 %qv7, %f148, %f149;
and.pred %qv7, %qv7, %qv4;
and.pred %qv7, %qv7, %qv0;

// Candidate 1 wins if %f149 > %f148 and Candidate 1 is valid:
setp.gt.f32 %qv8, %f149, %f148;
and.pred %qv8, %qv8, %qv4;
and.pred %qv8, %qv8, %qv1;

@%qv7 mov.f32 %f43, %f39;
@%qv7 mov.f32 %f42, %f38;
@%qv7 mov.f32 %f41, %f37;
@%qv7 mov.f32 %f40, %f36;

@%qv8 mov.f32 %f39, %f43;
@%qv8 mov.f32 %f38, %f42;
@%qv8 mov.f32 %f37, %f41;
@%qv8 mov.f32 %f36, %f40;
)ptx";

inline bool Patch(std::string& ptx, std::string& why)
{
    std::string normalized = ptx;
    normalized.erase(std::remove(normalized.begin(), normalized.end(), '\r'), normalized.end());
    while (!normalized.empty() && normalized.back() == '\0') normalized.pop_back();
    uint64_t hash = 14695981039346656037ull;
    for (unsigned char c : normalized) hash = (hash ^ c) * 1099511628211ull;
    // Exact known program, not a regex that might match unrelated instructions.
    if (normalized.size() != 39638 || hash != 0x7a6f5f41105c6d85ull)
    {
        why = "quality patch skipped: unrecognized BlendCandidatesFused PTX";
        return false;
    }
    const std::string anchor = "ld.param.u8 %rs8, [%rd6+220];";
    const auto site = normalized.find(anchor);
    const auto registers = normalized.find(".reg .pred %p<260>;");
    if (site == std::string::npos || registers == std::string::npos)
    {
        why = "quality patch skipped: expected insertion sites missing";
        return false;
    }
    std::string policy;
    if constexpr (scatter_experiment::kMode == 0 || scatter_experiment::kMode == 6 || scatter_experiment::kMode == 7)
    {
        policy = std::string(kPolicyE2);
        if constexpr (scatter_experiment::kMode == 0 || scatter_experiment::kMode == 7)
        {
            const std::string from = "0f3F59999A;";
            const std::string to   = "0f3F800000;";
            for (size_t pos = 0; (pos = policy.find(from, pos)) != std::string::npos; pos += to.size())
            {
                policy.replace(pos, from.size(), to);
            }
        }
    }
    else
    {
        policy = std::string(kPolicy);
        if constexpr (scatter_experiment::kMode == 5)
        {
            const std::string from = "mul.f32 %qf4, %qf4, 0f3DCCCCCD;";
            const std::string to   = "mul.f32 %qf4, %qf4, 0f3E800000;";
            const auto pos = policy.find(from);
            if (pos != std::string::npos)
                policy.replace(pos, from.size(), to);
        }
    }
    normalized.insert(site, policy);
    normalized.insert(registers, kRegisters);
    ptx = std::move(normalized);
    why = (scatter_experiment::kMode == 6)
        ? "VALID WARP QUALITY V4-E2 (geometric warp) applied"
        : (scatter_experiment::kMode == 5)
            ? "VALID WARP QUALITY V4-E1 (25% agreement) applied"
            : "VALID WARP QUALITY (pure 100% warp) applied";
    return true;
}
}
