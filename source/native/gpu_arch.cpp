#include "gpu_arch.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace gpu_arch
{
namespace
{
Family gTarget = Family::Ada;
wchar_t gDescription[160] = L"not initialized";

// Minimal D3DKMT declarations (shared/d3dkmthk.h). The thunks live in gdi32
// and never enter the user-mode driver, so they are usable in DllMain.
using KmtHandle = UINT;
struct KmtAdapterInfo { KmtHandle adapter; LUID luid; ULONG sources; BOOL precisePresentRegions; };
struct KmtEnumAdapters2 { ULONG count; KmtAdapterInfo* adapters; };
struct KmtQueryAdapterInfo { KmtHandle adapter; UINT type; void* data; UINT size; };
struct KmtQueryDeviceIds
{
    UINT physicalAdapterIndex;
    UINT vendorId, deviceId, subVendorId, subSystemId, revisionId, busType;
};
struct KmtCloseAdapter { KmtHandle adapter; };
constexpr UINT kQueryPhysicalAdapterDeviceIds = 31; // KMTQAITYPE_PHYSICALADAPTERDEVICEIDS
constexpr UINT kNvidiaVendorId = 0x10DE;

Family Classify(UINT id) noexcept
{
    // PCI device ID blocks by generation. GTX 16 (TU116/TU117) lacks tensor
    // cores and will still be refused by NGX; Hopper/GA100 are excluded.
    if ((id >= 0x1E00 && id <= 0x1FFF) || (id >= 0x2180 && id <= 0x21FF)) return Family::Turing;
    if ((id >= 0x2200 && id <= 0x22FF) || (id >= 0x2400 && id <= 0x25FF)) return Family::Ampere;
    if (id >= 0x2600 && id <= 0x28FF) return Family::Ada;
    if (id >= 0x2900 && id <= 0x2FFF) return Family::Blackwell;
    return Family::Unknown;
}

const wchar_t* Name(Family family) noexcept
{
    switch (family)
    {
    case Family::Turing: return L"Turing (SM75)";
    case Family::Ampere: return L"Ampere (SM86)";
    case Family::Ada: return L"Ada (SM89)";
    case Family::Blackwell: return L"Blackwell (SM120)";
    default: return L"unknown";
    }
}

// The newest NVIDIA adapter wins: that is the one a DLSS-G game renders on.
Family Survey(UINT& deviceId) noexcept
{
    HMODULE gdi = GetModuleHandleW(L"gdi32.dll");
    if (!gdi) gdi = LoadLibraryExW(L"gdi32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!gdi) return Family::Unknown;
    using EnumFn = LONG (APIENTRY*)(KmtEnumAdapters2*);
    using QueryFn = LONG (APIENTRY*)(const KmtQueryAdapterInfo*);
    using CloseFn = LONG (APIENTRY*)(const KmtCloseAdapter*);
    const auto enumAdapters = reinterpret_cast<EnumFn>(GetProcAddress(gdi, "D3DKMTEnumAdapters2"));
    const auto query = reinterpret_cast<QueryFn>(GetProcAddress(gdi, "D3DKMTQueryAdapterInfo"));
    const auto close = reinterpret_cast<CloseFn>(GetProcAddress(gdi, "D3DKMTCloseAdapter"));
    if (!enumAdapters || !query || !close) return Family::Unknown;

    KmtEnumAdapters2 request{};
    if (enumAdapters(&request) < 0 || !request.count) return Family::Unknown;
    std::vector<KmtAdapterInfo> adapters(request.count);
    request.adapters = adapters.data();
    if (enumAdapters(&request) < 0) return Family::Unknown;

    Family best = Family::Unknown;
    for (ULONG i = 0; i < request.count && i < adapters.size(); ++i)
    {
        KmtQueryDeviceIds ids{};
        const KmtQueryAdapterInfo info{adapters[i].adapter, kQueryPhysicalAdapterDeviceIds, &ids, sizeof(ids)};
        const bool known = query(&info) >= 0;
        const KmtCloseAdapter closing{adapters[i].adapter};
        close(&closing);
        if (!known || ids.vendorId != kNvidiaVendorId) continue;
        const Family family = Classify(ids.deviceId);
        if (static_cast<int>(family) > static_cast<int>(best))
        {
            best = family;
            deviceId = ids.deviceId;
        }
    }
    return best;
}
} // namespace

void Initialize(Family forced) noexcept
{
    UINT deviceId = 0;
    const Family detected = Survey(deviceId);
    const Family chosen = forced != Family::Unknown ? forced : detected;
    // Blackwell and unknown adapters keep Transfusion's original Ada target.
    gTarget = (chosen == Family::Turing || chosen == Family::Ampere) ? chosen : Family::Ada;
    swprintf_s(gDescription, L"adapter %s 0x%04X, provider patches target %s%s",
        Name(detected), deviceId, Name(gTarget), forced != Family::Unknown ? L" (forced by gpuArchitecture)" : L"");
}

bool TryParse(const char* text, size_t length, Family& family) noexcept
{
    const auto is = [&](const char* name) { return strlen(name) == length && _strnicmp(text, name, length) == 0; };
    if (is("auto")) family = Family::Unknown;
    else if (is("ada") || is("sm89")) family = Family::Ada;
    else if (is("ampere") || is("sm86")) family = Family::Ampere;
    else if (is("turing") || is("sm75")) family = Family::Turing;
    else return false;
    return true;
}

const char* ConfigName(Family family) noexcept
{
    switch (family)
    {
    case Family::Ada: return "ada";
    case Family::Ampere: return "ampere";
    case Family::Turing: return "turing";
    default: return "auto";
    }
}

Family Target() noexcept { return gTarget; }
bool PreAda() noexcept { return gTarget == Family::Ampere || gTarget == Family::Turing; }

uint32_t NgxArchitecture() noexcept
{
    return gTarget == Family::Turing ? 0x160u : gTarget == Family::Ampere ? 0x170u : 0x190u;
}

uint32_t SmVersion() noexcept
{
    return gTarget == Family::Turing ? 75u : gTarget == Family::Ampere ? 86u : 89u;
}

const wchar_t* Describe() noexcept { return gDescription; }
}
