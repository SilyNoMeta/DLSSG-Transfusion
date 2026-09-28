#include "smooth_motion_fatbin.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    std::ifstream file(argv[1], std::ios::binary);
    if (!file) return 2;
    std::vector<uint8_t> dll(std::istreambuf_iterator<char>{file}, {});
    unsigned fp16 = 0, fp8 = 0, malformed = 0;
    std::vector<uint8_t> sample;
    for (size_t i = 0; i + 16 <= dll.size(); ++i)
    {
        const auto* p = dll.data() + i;
        if (smooth_motion_fatbin::U32(p) != 0xba55ed50u) continue;
        const size_t header = smooth_motion_fatbin::U16(p + 6);
        const uint64_t payload = smooth_motion_fatbin::U64(p + 8);
        if (header < 16 || header > 256 || header > dll.size() - i
            || payload > dll.size() - i - header) continue;
        const size_t size = header + static_cast<size_t>(payload);
        std::vector<uint8_t> output;
        const auto result = smooth_motion_fatbin::Rewrite(p, size, output);
        if (result == smooth_motion_fatbin::Result::rewritten)
        {
            ++fp16;
            if (sample.empty()) sample.assign(p, p + size);
            size_t changed = 0;
            for (size_t j = 0; j < size; ++j) changed += p[j] != output[j];
            if (changed != 2) return 3; // 89->86 in entry and 0x59->0x56 in ELF flags
        }
        else if (result == smooth_motion_fatbin::Result::rejected) ++fp8;
        else ++malformed;
        i += size - 1;
    }
    std::cout << "FP16 retargeted=" << fp16 << " FP8 refused=" << fp8
              << " other=" << malformed << '\n';
    if (fp16 != 20 || fp8 != 17 || malformed != 0) return 4;
    std::vector<uint8_t> output;
    const uint8_t broken[] = {0x50, 0xed, 0x55, 0xba, 1, 0, 16, 0, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0};
    if (smooth_motion_fatbin::Rewrite(broken, sizeof(broken), output)
        != smooth_motion_fatbin::Result::rejected) return 5;
    if (smooth_motion_fatbin::Rewrite(sample.data(), sample.size() - 1, output)
        != smooth_motion_fatbin::Result::rejected) return 6;
    smooth_motion_fatbin::W32(sample.data() + 16 + 8, UINT32_MAX);
    return smooth_motion_fatbin::Rewrite(sample.data(), sample.size(), output)
        == smooth_motion_fatbin::Result::rejected ? 0 : 7;
}
