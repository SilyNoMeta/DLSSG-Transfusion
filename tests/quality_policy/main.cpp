// Offline check of the blend quality policies: applies each policy to NVIDIA's
// sm_120 Kernel_BlendCandidatesFused PTX and writes it retargeted to sm_86,
// sm_89 and (lowered) sm_75, for ptxas to validate.
// Usage: QualityPolicy <blend_sm120.ptx> <output-directory>
#include "../../source/native/ptx_lowering.h"
#include "../../source/native/quality_fix.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

static void Retarget(std::string& ptx, const char* sm)
{
    const size_t directive = ptx.find(".target sm_");
    size_t end = directive + 8;
    while (end < ptx.size() && ptx[end] != '\r' && ptx[end] != '\n' && ptx[end] != ' ' && ptx[end] != ',') ++end;
    ptx.replace(directive, end - directive, std::string(".target ") + sm);
}

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::fprintf(stderr, "usage: %s <blend_sm120.ptx> <output-directory>\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1], std::ios::binary);
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::filesystem::path output = argv[2];
    std::filesystem::create_directories(output);
    int failures = 0;
    for (const auto& [name, policy] : {std::pair{"transfusion", quality_fix::Policy::Transfusion},
                                       std::pair{"explained-warp", quality_fix::Policy::ExplainedWarp}})
    {
        std::string ptx = buffer.str();
        std::string why;
        if (!quality_fix::Patch(ptx, why, policy))
        {
            std::fprintf(stderr, "%s: %s\n", name, why.c_str());
            ++failures;
            continue;
        }
        for (const char* sm : {"sm_86", "sm_89", "sm_75"})
        {
            std::string target = ptx;
            Retarget(target, sm);
            ptx_lowering::Stats stats;
            if (std::string_view(sm) == "sm_75" && !ptx_lowering::LowerToSm75(target, stats)) ++failures;
            std::ofstream(output / (std::string(name) + "_" + sm + ".ptx"), std::ios::binary) << target;
        }
        std::printf("%s: %s\n", name, why.c_str());
    }
    return failures ? 1 : 0;
}
