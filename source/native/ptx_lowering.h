#pragma once
// Lowers the sm_80 instructions used by the DLSS-G provider PTX to sm_75
// (Turing) equivalents. The provider's PTX otherwise needs no change below Ada;
// only these four forms stop ptxas at sm_75:
//
//   mma.sync.aligned.m16n8k16.row.col.<d>.f16.f16.<c>
//       -> two m16n8k8 (A = {a0,a1} then {a2,a3}, B = b0 then b1). The A/B
//          fragments of m16n8k16 are exactly the two K halves of m16n8k8.
//   cvt.rn.f16x2.f32 d, a, b
//       -> cvt.rn.f16.f32 per half; a goes to the upper half, b to the lower.
//   max|min.f16 d, a, b / max|min.f16x2 d, a, b
//       -> through f32, which is exact for f16 inputs and outputs.
//
// Every replacement is emitted in its own { } scope with scoped temporaries,
// and repeats the instruction's predicate guard, if any.
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace ptx_lowering
{
struct Stats
{
    size_t mma = 0;
    size_t cvt = 0;
    size_t minmax = 0;
    size_t Total() const { return mma + cvt + minmax; }
};

namespace detail
{
inline std::string_view Trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
    return s;
}

inline bool StartsWith(std::string_view s, std::string_view prefix)
{
    return s.substr(0, prefix.size()) == prefix;
}

// Splits "a, b, c" at top-level commas (outside braces).
inline std::vector<std::string> SplitOperands(std::string_view s)
{
    std::vector<std::string> out;
    int depth = 0;
    size_t start = 0;
    for (size_t i = 0; i <= s.size(); ++i)
    {
        if (i < s.size() && s[i] == '{') ++depth;
        if (i < s.size() && s[i] == '}') --depth;
        if (i == s.size() || (s[i] == ',' && depth == 0))
        {
            out.emplace_back(Trim(s.substr(start, i - start)));
            start = i + 1;
        }
    }
    return out;
}

// "{%r1, %r2}" -> {"%r1", "%r2"}; empty when the operand is not a vector.
inline std::vector<std::string> VectorItems(std::string_view operand)
{
    operand = Trim(operand);
    if (operand.size() < 2 || operand.front() != '{' || operand.back() != '}') return {};
    return SplitOperands(operand.substr(1, operand.size() - 2));
}

inline std::string Join(const std::vector<std::string>& items, size_t first, size_t count)
{
    std::string s = "{";
    for (size_t i = 0; i < count; ++i)
    {
        if (i) s += ", ";
        s += items[first + i];
    }
    return s + "}";
}

// Parses "[@guard ]opcode operands;" into its parts.
struct Instruction
{
    std::string indent, guard, opcode;
    std::vector<std::string> operands;
};

inline bool Parse(std::string_view line, Instruction& ins)
{
    size_t pos = 0;
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) ++pos;
    ins.indent = std::string(line.substr(0, pos));
    std::string_view rest = Trim(line.substr(pos));
    if (rest.empty() || rest.back() != ';') return false;
    rest.remove_suffix(1);
    if (!rest.empty() && rest.front() == '@')
    {
        const size_t space = rest.find_first_of(" \t");
        if (space == std::string_view::npos) return false;
        ins.guard = std::string(rest.substr(0, space)) + " ";
        rest = Trim(rest.substr(space));
    }
    const size_t space = rest.find_first_of(" \t");
    if (space == std::string_view::npos) return false;
    ins.opcode = std::string(rest.substr(0, space));
    ins.operands = SplitOperands(rest.substr(space + 1));
    return true;
}

inline bool LowerMma(const Instruction& ins, std::string& out)
{
    constexpr std::string_view kPrefix = "mma.sync.aligned.m16n8k16.row.col.";
    if (!StartsWith(ins.opcode, kPrefix) || ins.operands.size() != 4) return false;
    const std::string types = ins.opcode.substr(kPrefix.size());
    // f16 multiplicands only (the m16n8k8 fragment split below assumes .f16).
    if (types.find(".f16.f16.") == std::string::npos) return false;
    const auto d = VectorItems(ins.operands[0]);
    const auto a = VectorItems(ins.operands[1]);
    const auto b = VectorItems(ins.operands[2]);
    const auto c = VectorItems(ins.operands[3]);
    if (a.size() != 4 || b.size() != 2 || d.empty() || d.size() != c.size()) return false;
    const bool f32Accumulator = StartsWith(types, "f32");
    std::vector<std::string> t;
    for (size_t i = 0; i < d.size(); ++i) t.push_back("%tfm" + std::to_string(i));
    const std::string op = ins.indent + ins.guard + "mma.sync.aligned.m16n8k8.row.col." + types + " ";
    out += ins.indent + "{\n";
    out += ins.indent + ".reg " + (f32Accumulator ? ".f32" : ".b32") + " %tfm<" + std::to_string(d.size()) + ">;\n";
    out += op + Join(t, 0, t.size()) + ", " + Join(a, 0, 2) + ", {" + b[0] + "}, " + Join(c, 0, c.size()) + ";\n";
    out += op + Join(d, 0, d.size()) + ", " + Join(a, 2, 2) + ", {" + b[1] + "}, " + Join(t, 0, t.size()) + ";\n";
    out += ins.indent + "}\n";
    return true;
}

inline bool LowerCvt(const Instruction& ins, std::string& out)
{
    if (ins.opcode != "cvt.rn.f16x2.f32" || ins.operands.size() != 3) return false;
    const std::string g = ins.indent + ins.guard;
    out += ins.indent + "{\n";
    out += ins.indent + ".reg .b16 %tfh<2>;\n";
    out += g + "cvt.rn.f16.f32 %tfh0, " + ins.operands[2] + ";\n"; // b -> lower half
    out += g + "cvt.rn.f16.f32 %tfh1, " + ins.operands[1] + ";\n"; // a -> upper half
    out += g + "mov.b32 " + ins.operands[0] + ", {%tfh0, %tfh1};\n";
    out += ins.indent + "}\n";
    return true;
}

inline bool LowerMinMax(const Instruction& ins, std::string& out)
{
    const bool vector = ins.opcode == "max.f16x2" || ins.opcode == "min.f16x2";
    const bool scalar = ins.opcode == "max.f16" || ins.opcode == "min.f16";
    if ((!vector && !scalar) || ins.operands.size() != 3) return false;
    const std::string f32op = ins.opcode.substr(0, 3) + ".f32";
    const std::string g = ins.indent + ins.guard;
    out += ins.indent + "{\n";
    out += ins.indent + ".reg .f32 %tff<3>;\n";
    if (scalar)
    {
        out += g + "cvt.f32.f16 %tff0, " + ins.operands[1] + ";\n";
        out += g + "cvt.f32.f16 %tff1, " + ins.operands[2] + ";\n";
        out += g + f32op + " %tff2, %tff0, %tff1;\n";
        out += g + "cvt.rn.f16.f32 " + ins.operands[0] + ", %tff2;\n";
    }
    else
    {
        out += ins.indent + ".reg .b16 %tfa<2>, %tfb<2>, %tfd<2>;\n";
        out += g + "mov.b32 {%tfa0, %tfa1}, " + ins.operands[1] + ";\n";
        out += g + "mov.b32 {%tfb0, %tfb1}, " + ins.operands[2] + ";\n";
        for (const char* half : {"0", "1"})
        {
            out += g + "cvt.f32.f16 %tff0, %tfa" + half + ";\n";
            out += g + "cvt.f32.f16 %tff1, %tfb" + half + ";\n";
            out += g + f32op + " %tff2, %tff0, %tff1;\n";
            out += g + "cvt.rn.f16.f32 %tfd" + half + ", %tff2;\n";
        }
        out += g + "mov.b32 " + ins.operands[0] + ", {%tfd0, %tfd1};\n";
    }
    out += ins.indent + "}\n";
    return true;
}
} // namespace detail

// True when the PTX uses an instruction this pass lowers.
inline bool NeedsSm75Lowering(std::string_view ptx)
{
    return ptx.find("m16n8k16") != std::string_view::npos
        || ptx.find("f16x2.f32") != std::string_view::npos
        || ptx.find("max.f16") != std::string_view::npos
        || ptx.find("min.f16") != std::string_view::npos;
}

// Rewrites the text in place. Returns false if an sm_80 form was recognized
// by opcode but could not be parsed; the caller must then keep the original.
inline bool LowerToSm75(std::string& ptx, Stats& stats)
{
    std::string out;
    out.reserve(ptx.size() + ptx.size() / 4);
    size_t start = 0;
    while (start < ptx.size())
    {
        size_t end = ptx.find('\n', start);
        if (end == std::string::npos) end = ptx.size();
        const std::string_view line(ptx.data() + start, end - start);
        const std::string_view body = detail::Trim(line);
        const bool candidate = body.find("m16n8k16") != std::string_view::npos
            || body.find("cvt.rn.f16x2.f32") != std::string_view::npos
            || body.find("max.f16") != std::string_view::npos
            || body.find("min.f16") != std::string_view::npos;
        detail::Instruction ins;
        if (candidate && !detail::StartsWith(body, "//"))
        {
            // Inline-asm wrappers: "{ instr; }" on one line, or "{instr;" whose
            // closing brace is on the next line. Keep the braces around the
            // replacement, which is itself a scope.
            std::string_view inner = body;
            const bool open = !inner.empty() && inner.front() == '{';
            if (open) inner = detail::Trim(inner.substr(1));
            const bool close = !inner.empty() && inner.back() == '}';
            if (close) inner = detail::Trim(inner.substr(0, inner.size() - 1));
            const std::string indent(line.substr(0, line.find_first_not_of(" \t")));
            if (!detail::Parse(indent + std::string(inner), ins)) return false;
            if (open) out += indent + "{\n";
            if (detail::LowerMma(ins, out)) ++stats.mma;
            else if (detail::LowerCvt(ins, out)) ++stats.cvt;
            else if (detail::LowerMinMax(ins, out)) ++stats.minmax;
            else return false;
            if (close) out += indent + "}\n";
        }
        else
        {
            out.append(line);
            if (end < ptx.size()) out.push_back('\n');
        }
        start = end + 1;
    }
    ptx.swap(out);
    return true;
}
}
