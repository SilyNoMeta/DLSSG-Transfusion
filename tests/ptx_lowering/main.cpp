// Offline check of the sm_75 lowering: retargets each input PTX to sm_75,
// lowers it and writes the result, for ptxas -arch=sm_75 to validate.
// Usage: PtxLowering <output-directory> <input.ptx>...
#include "../../source/native/ptx_lowering.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::fprintf(stderr, "usage: %s <output-directory> <input.ptx>...\n", argv[0]);
        return 2;
    }
    const std::filesystem::path output = argv[1];
    std::filesystem::create_directories(output);
    ptx_lowering::Stats total;
    int failures = 0;
    for (int i = 2; i < argc; ++i)
    {
        std::ifstream in(argv[i], std::ios::binary);
        std::stringstream buffer;
        buffer << in.rdbuf();
        std::string ptx = buffer.str();
        const size_t directive = ptx.find(".target sm_");
        if (directive != std::string::npos)
        {
            size_t end = directive + 8;
            while (end < ptx.size() && ptx[end] != '\r' && ptx[end] != '\n' && ptx[end] != ' ' && ptx[end] != ',') ++end;
            ptx.replace(directive, end - directive, ".target sm_75");
        }
        ptx_lowering::Stats stats;
        if (ptx_lowering::NeedsSm75Lowering(ptx) && !ptx_lowering::LowerToSm75(ptx, stats))
        {
            std::fprintf(stderr, "FAILED %s\n", argv[i]);
            ++failures;
            continue;
        }
        total.mma += stats.mma;
        total.cvt += stats.cvt;
        total.minmax += stats.minmax;
        std::ofstream(output / std::filesystem::path(argv[i]).filename(), std::ios::binary) << ptx;
    }
    std::printf("lowered mma=%zu cvt=%zu minmax=%zu failures=%d\n", total.mma, total.cvt, total.minmax, failures);
    return failures ? 1 : 0;
}
