#include <sl.h>

namespace { volatile unsigned calls[4]{}; }

SL_API sl::Result slGetFeatureFunction(sl::Feature, const char*, void*& function)
{
    calls[0] = calls[0] + 1;
    function = nullptr;
    return sl::Result::eErrorFeatureMissing;
}

SL_API sl::Result slSetD3DDevice(void*)
{
    calls[1] = calls[1] + 1;
    return sl::Result::eOk;
}

SL_API sl::Result slSetTag(const sl::ViewportHandle&, const sl::ResourceTag*,
    uint32_t, sl::CommandBuffer*)
{
    calls[2] = calls[2] + 1;
    return sl::Result::eOk;
}

SL_API sl::Result slSetTagForFrame(const sl::FrameToken&, const sl::ViewportHandle&,
    const sl::ResourceTag*, uint32_t, sl::CommandBuffer*)
{
    calls[3] = calls[3] + 1;
    return sl::Result::eOk;
}
