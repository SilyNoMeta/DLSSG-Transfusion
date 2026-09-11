// Exercise the production decompressor and fatbin builder against a provider file,
// without loading the provider DLL or starting its initialization.
#include "../../source/native/midpoint_fix.cpp"
#include <fstream>
#include <iterator>
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 3) return 1;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) return 2;
    const std::vector<uint8_t> data{std::istreambuf_iterator<char>(input), {}};
    for (size_t i = 0; i + 16 <= data.size(); ++i)
    {
        const auto* fat = data.data() + i;
        if (midpoint_fix::ReadU32(fat) != 0xBA55ED50) continue;
        const auto payload = midpoint_fix::ReadU64(fat + 8);
        if (payload > data.size() - i - 16) continue;
        const size_t size = static_cast<size_t>(payload) + 16;
        if (midpoint_fix::GetFatbinKernelName(fat, size) != "Kernel_BlendCandidatesFused") continue;
        std::vector<uint8_t> out;
        std::string why;
        midpoint_fix::SetQualityFixEnabled(true);
        if (!midpoint_fix::BuildBlackwellTransfusionFatbin(fat, size,
            "Kernel_BlendCandidatesFused", out, why)) return 3;
        const std::string patched(out.begin(), out.end());
        if (patched.find("QUALITY_VALID_WARP_V4") == std::string::npos) return 4;
        if (midpoint_fix::ReadU64(out.data() + 8) != out.size() - 16) return 5;
        std::ofstream output(argv[2], std::ios::binary);
        output.write(reinterpret_cast<const char*>(out.data()), out.size());
        if (!output) return 6;
        midpoint_fix::SetQualityFixEnabled(false);
        if (!midpoint_fix::BuildBlackwellTransfusionFatbin(fat, size,
            "Kernel_BlendCandidatesFused", out, why)) return 7;
        const std::string disabled(out.begin(), out.end());
        if (disabled.find("QUALITY_VALID_WARP_V4") != std::string::npos) return 8;
        std::cout << "PROVIDER_QUALITY_FATBIN_ON_OFF_OK\n";
        return 0;
    }
    return 9;
}
