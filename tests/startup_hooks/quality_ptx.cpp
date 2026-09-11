#include "../../source/native/quality_fix.h"
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc, char** argv)
{
    if (argc != 3) return 1;
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) return 2;
    const std::string original{std::istreambuf_iterator<char>(input), {}};
    std::string patched = original, why;
    if (!quality_fix::Patch(patched, why)) { std::cerr << why; return 3; }
    std::string different = original;
    const auto op = different.find("sub.ftz.f32 %f166");
    if (op == std::string::npos) return 4;
    different[op] = 'a';
    const auto before = different;
    if (quality_fix::Patch(different, why) || different != before) return 5;
    std::string repeated = patched;
    if (quality_fix::Patch(repeated, why) || repeated != patched) return 6;
    // Equivalent line endings must identify the same provider program.
    std::string crlf;
    for (char c : original) { if (c == '\n') crlf += '\r'; crlf += c; }
    if (!quality_fix::Patch(crlf, why) || crlf != patched) return 7;
    const auto target = patched.find(".target sm_120");
    patched.replace(target, std::string(".target sm_120").size(), ".target sm_89");
    std::ofstream output(argv[2], std::ios::binary);
    output << patched;
    if (!output) return 8;
    std::cout << "QUALITY_PROFILE_GATING_OK\n";
}
