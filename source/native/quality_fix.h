#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include "scatter_experiment.h"

#ifndef QUALITY_DIAGNOSTIC
#define QUALITY_DIAGNOSTIC 0
#endif
static_assert(QUALITY_DIAGNOSTIC >= 0 && QUALITY_DIAGNOSTIC <= 2);
namespace quality_fix
{
// Runs after candidate construction and before the provider's UI composition.
// Only RGB changes. Auxiliary channels, rejection masks and memory accesses stay intact.
inline constexpr std::string_view kRegisters = R"ptx(
.reg .pred %qv<16>;
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

setp.ge.f32 %qv5, %f148, 0f3E4CCCCD;
or.pred %qv5, %qv5, %qv9;
and.pred %qv0, %qv0, %qv5;

setp.ge.f32 %qv6, %f149, 0f3E4CCCCD;
or.pred %qv6, %qv6, %qv9;
and.pred %qv1, %qv1, %qv6;

// Measure candidate color differences:
sub.f32 %qf6, %f125, %f131;
sub.f32 %qf7, %f126, %f132;
sub.f32 %qf8, %f127, %f133;

// E_motion = |D_R| + |D_G| + |D_B|
abs.f32 %qf9, %qf6;
abs.f32 %qf10, %qf7;
abs.f32 %qf11, %qf8;
add.f32 %qf9, %qf9, %qf10;
add.f32 %qf9, %qf9, %qf11;

// E_chroma = |D_R - D_G| + |D_G - D_B| + |D_B - D_R|
// Absolute thresholds are scene-dependent heuristics, not shadow/light classifiers.
// Modes 8 and 9 add brightness-scale protection before candidate copying.
sub.f32 %qf12, %qf6, %qf7;
sub.f32 %qf13, %qf7, %qf8;
sub.f32 %qf14, %qf8, %qf6;
abs.f32 %qf12, %qf12;
abs.f32 %qf13, %qf13;
abs.f32 %qf14, %qf14;
add.f32 %qf12, %qf12, %qf13;
add.f32 %qf12, %qf12, %qf14;

// Candidate Conflict Firewall:
// Trigger when motion error > 0.35f (0f3EB33333) AND chromatic error > 0.25f (0f3E800000):
setp.gt.f32 %qv4, %qf9, 0f3EB33333;
and.pred %qv4, %qv4, %qv3;
setp.gt.f32 %qv7, %qf12, 0f3EB33333;
and.pred %qv4, %qv4, %qv7;
mov.pred %qv8, %qv4;

// QUALITY_SHADOW_SCALE_V1
// Experimental shadow veto. Independent of the existing signed chroma threshold.
// Normalize each RGB by its L1 magnitude before computing squared cosine residual:
// 1 - dot(A,B)^2 / (dot(A,A)*dot(B,B)). Scalar brightness changes yield zero.
// Require positive alignment and sufficient signal; very dark inputs retain the existing conflict rule.
abs.f32 %qf0, %f125;
abs.f32 %qf1, %f126;
abs.f32 %qf2, %f127;
add.f32 %qf0, %qf0, %qf1;
add.f32 %qf0, %qf0, %qf2;
abs.f32 %qf1, %f131;
abs.f32 %qf2, %f132;
abs.f32 %qf3, %f133;
add.f32 %qf1, %qf1, %qf2;
add.f32 %qf1, %qf1, %qf3;
setp.gt.f32 %qv10, %qf0, 0f3C23D70A;
setp.gt.f32 %qv11, %qf1, 0f3C23D70A;
and.pred %qv10, %qv10, %qv11;
max.f32 %qf0, %qf0, 0f3C23D70A;
max.f32 %qf1, %qf1, 0f3C23D70A;
div.rn.f32 %qf2, %f125, %qf0;
div.rn.f32 %qf3, %f126, %qf0;
div.rn.f32 %qf4, %f127, %qf0;
div.rn.f32 %qf5, %f131, %qf1;
div.rn.f32 %qf6, %f132, %qf1;
div.rn.f32 %qf7, %f133, %qf1;
mul.f32 %qf8, %qf2, %qf5;
fma.rn.f32 %qf8, %qf3, %qf6, %qf8;
fma.rn.f32 %qf8, %qf4, %qf7, %qf8;
setp.gt.f32 %qv11, %qf8, 0f00000000;
and.pred %qv10, %qv10, %qv11;
mul.f32 %qf9, %qf2, %qf2;
fma.rn.f32 %qf9, %qf3, %qf3, %qf9;
fma.rn.f32 %qf9, %qf4, %qf4, %qf9;
mul.f32 %qf10, %qf5, %qf5;
fma.rn.f32 %qf10, %qf6, %qf6, %qf10;
fma.rn.f32 %qf10, %qf7, %qf7, %qf10;
mul.f32 %qf9, %qf9, %qf10;
mul.f32 %qf8, %qf8, %qf8;
sub.f32 %qf8, %qf9, %qf8;
mul.f32 %qf9, %qf9, 0f3DF5C28F;
setp.le.f32 %qv11, %qf8, %qf9;
and.pred %qv10, %qv10, %qv11;
not.pred %qv10, %qv10;
and.pred %qv8, %qv8, %qv10;

// QUALITY_SIREN_CHROMA_025_V1
// Under chromatic conflict passing shadow veto (%qv8), do NOT force geometric warp.
// Revert to stock DLSS-G candidates (natural soft dissolve + temporal reprojection).
not.pred %qv15, %qv8;
and.pred %qv0, %qv0, %qv15;
and.pred %qv1, %qv1, %qv15;

max.f32 %qf0, %f148, 0f3F800000;
min.f32 %qf0, %qf0, 0f3F800000;
max.f32 %qf1, %f149, 0f3F800000;
min.f32 %qf1, %qf1, 0f3F800000;

sub.f32 %qf2, %f139, %f115;
sub.f32 %qf3, %f140, %f116;
sub.f32 %qf12, %f141, %f117;
@%qv0 fma.rn.f32 %f39, %qf0, %qf2, %f115;
@%qv0 fma.rn.f32 %f38, %qf0, %qf3, %f116;
@%qv0 fma.rn.f32 %f37, %qf0, %qf12, %f117;

sub.f32 %qf2, %f145, %f119;
sub.f32 %qf3, %f146, %f120;
sub.f32 %qf12, %f147, %f121;
@%qv1 fma.rn.f32 %f43, %qf1, %qf2, %f119;
@%qv1 fma.rn.f32 %f42, %qf1, %qf3, %f120;
@%qv1 fma.rn.f32 %f41, %qf1, %qf12, %f121;
)ptx";

inline constexpr std::string_view kShadowVeto = "";

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
    if constexpr (scatter_experiment::kMode == 0 || scatter_experiment::kMode == 6 || scatter_experiment::kMode == 7 || scatter_experiment::kMode == 8 || scatter_experiment::kMode == 9)
    {
        policy = std::string(kPolicyE2);
        if constexpr (scatter_experiment::kMode == 6)
        {
            const std::string from0 = "max.f32 %qf0, %f148, 0f3F800000;";
            const std::string to0   = "max.f32 %qf0, %f148, 0f3F59999A;";
            const std::string from1 = "max.f32 %qf1, %f149, 0f3F800000;";
            const std::string to1   = "max.f32 %qf1, %f149, 0f3F59999A;";
            auto p0 = policy.find(from0);
            if (p0 != std::string::npos) policy.replace(p0, from0.size(), to0);
            auto p1 = policy.find(from1);
            if (p1 != std::string::npos) policy.replace(p1, from1.size(), to1);
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
    if constexpr (QUALITY_DIAGNOSTIC != 0)
    {
        static_assert(QUALITY_DIAGNOSTIC == 0 || scatter_experiment::kMode == 9, "Diagnostics require mode 9");
        policy += "\nand.pred %qv12, %qv0, %qv1;\n";
        if constexpr (QUALITY_DIAGNOSTIC == 1)
            policy += R"ptx(
// QUALITY_DIAGNOSTIC_SOURCE0
@%qv12 mov.f32 %f39, %f125;
@%qv12 mov.f32 %f38, %f126;
@%qv12 mov.f32 %f37, %f127;
@%qv12 mov.f32 %f43, %f125;
@%qv12 mov.f32 %f42, %f126;
@%qv12 mov.f32 %f41, %f127;
)ptx";
        else
            policy += R"ptx(
// QUALITY_DIAGNOSTIC_SOURCE1
@%qv12 mov.f32 %f39, %f131;
@%qv12 mov.f32 %f38, %f132;
@%qv12 mov.f32 %f37, %f133;
@%qv12 mov.f32 %f43, %f131;
@%qv12 mov.f32 %f42, %f132;
@%qv12 mov.f32 %f41, %f133;
)ptx";
    }
    normalized.insert(site, policy);
    normalized.insert(registers, kRegisters);
    ptx = std::move(normalized);
    why = (scatter_experiment::kMode == 9)
        ? "SIREN CHROMA 0.25 with SHADOW SCALE V1 veto applied"
        : (scatter_experiment::kMode == 8)
        ? "SHADOW SCALE V1 veto applied"
        : (scatter_experiment::kMode == 6)
        ? "VALID WARP QUALITY V4-E2 (geometric warp) applied"
        : (scatter_experiment::kMode == 5)
        ? "VALID WARP QUALITY V4-E1 (25% agreement) applied"
        : "VALID WARP QUALITY (pure 100% warp with shadow veto) applied";
    if constexpr (QUALITY_DIAGNOSTIC == 1) why = "DIAGNOSTIC SOURCE 0 RGB isolation applied (not a quality fix)";
    return true;
}
}


