"""Build an observation-only probe from the exact exported mode-9 PTX prefix.

The original game kernel and its ABI are not modified. The probe re-evaluates
the same reads/arithmetic on the same bound inputs, writing only its own atlas.
Run after the mode-9 QualityPtxHarness exports quality-patched.ptx.
"""
import ctypes as C
import hashlib
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
source = Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'tests/startup_hooks/build-siren/quality-patched.ptx'
out = root / 'build-capture'
out.mkdir(exist_ok=True)
text = source.read_text().rstrip('\0')
assert 'QUALITY_SIREN_CHROMA_025_V1' in text
assert 'QUALITY_SHADOW_SCALE_V1' in text
assert 'QUALITY_DIAGNOSTIC_SOURCE' not in text
prefix = text[:text.index('ld.param.u8 %rs8, [%rd6+220];')]
prefix = prefix.replace('Kernel_BlendCandidatesFused', 'TransfusionCapture')
assert prefix.count('param_0[240]') == 1
prefix = prefix.replace('param_0[240]', 'param_0[256]')
prefix = prefix.replace('$L__BB0_29', 'CaptureExit').replace('$L__BB0_9', 'CaptureExit')
registers = '''
.reg .b64 %crd;
.reg .b32 %cr<6>;
.reg .pred %cp<4>;
.reg .f32 %cf<4>;
'''
prefix = prefix.replace('.reg .pred %p<260>;', registers + '.reg .pred %p<260>;')
roi = '''
ld.param.u64 %crd, [TransfusionCapture_param_0+240];
ld.param.u32 %cr0, [TransfusionCapture_param_0+248];
ld.param.u32 %cr1, [TransfusionCapture_param_0+252];
sub.u32 %cr2, %r7, %cr0;
sub.u32 %cr3, %r30, %cr1;
setp.ge.u32 %cp0, %cr2, 256;
setp.ge.u32 %cp1, %cr3, 256;
or.pred %cp0, %cp0, %cp1;
@%cp0 bra CaptureExit;
'''
# Reject ROI before any global memory or texture reads. Unsigned subtraction
# also rejects coordinates below the origin.
prefix = prefix.replace('ld.param.u64 %rd2, [%rd6+16];', roi + 'ld.param.u64 %rd2, [%rd6+16];')
planes = [
    [125,126,127,128], [131,132,133,134],
    [115,116,117,118], [119,120,121,122],
    [148,149,150,151], [123,124,129,130],
    [39,38,37,36], [43,42,41,40],
]
stores = ''
for i, regs in enumerate(planes):
    stores += f'add.u32 %cr4, %cr3, {i*256};\n'
    stores += 'sust.p.2d.v4.b32.zero [%crd, {%cr2,%cr4}], {' + ','.join(f'%f{r}' for r in regs) + '};\n'
stores += '''
selp.f32 %cf0, 0f3F800000, 0f00000000, %qv0;
selp.f32 %cf1, 0f3F800000, 0f00000000, %qv1;
selp.f32 %cf2, 0f3F800000, 0f00000000, %qv4;
selp.f32 %cf3, 0f3F800000, 0f00000000, %qv8;
add.u32 %cr4, %cr3, 2048;
sust.p.2d.v4.b32.zero [%crd, {%cr2,%cr4}], {%cf0,%cf1,%cf2,%cf3};
CaptureExit:
ret;
}
'''
probe = prefix + stores
assert len(re.findall(r'sust\.', probe)) == 9
assert not re.search(r'\bst\.(global|shared)|\batom\.', probe)
(out / 'capture-probe.ptx').write_text(probe)

# A deterministic second entry exercises the identical ROI + atlas stores via
# real NVAPI/D3D12 launch, resource transitions, queue fence and readback.
smoke = '''.version 8.7
.target sm_89
.address_size 64
.visible .entry TransfusionCapture(.param .align 8 .b8 TransfusionCapture_param_0[256])
.maxntid 256,1,1
{
.reg .f32 %f<559>;
.reg .b32 %r<31>;
.reg .pred %qv<16>;
''' + registers + '''
mov.u32 %r7, %tid.x;
mov.u32 %r30, %ctaid.y;
''' + roi
for r in sorted({r for plane in planes for r in plane}):
    smoke += f'mov.f32 %f{r}, 0f3F000000;\n'
for r in [0,1,4,8]:
    smoke += f'setp.eq.u32 %qv{r}, %r7, %r7;\n'
smoke += stores
(out / 'capture-smoke.ptx').write_text(smoke)

cuda = C.WinDLL('nvcuda.dll')
ptr = C.c_void_p
def setup(name, args):
    fn = getattr(cuda, name); fn.argtypes = args; fn.restype = C.c_int
    return fn
init = setup('cuInit',[C.c_uint])
device_get = setup('cuDeviceGet',[C.POINTER(C.c_int),C.c_int])
create = setup('cuCtxCreate_v2',[C.POINTER(ptr),C.c_uint,C.c_int])
destroy = setup('cuCtxDestroy_v2',[ptr])
link_create = setup('cuLinkCreate_v2',[C.c_uint,ptr,ptr,C.POINTER(ptr)])
link_add = setup('cuLinkAddData_v2',[ptr,C.c_int,ptr,C.c_size_t,C.c_char_p,C.c_uint,ptr,ptr])
link_complete = setup('cuLinkComplete',[ptr,C.POINTER(ptr),C.POINTER(C.c_size_t)])
link_destroy = setup('cuLinkDestroy',[ptr])
def checked(result):
    if result: raise RuntimeError(f'CUDA error {result}')
context = ptr()
checked(init(0)); device = C.c_int(); checked(device_get(C.byref(device),0))
checked(create(C.byref(context),0,device))
try:
    headers = ['#pragma once\n#include <cstddef>\nnamespace capture_binary {\n']
    for name, program in [('probe',probe),('smoke',smoke)]:
        link = ptr()
        checked(link_create(0,None,None,C.byref(link)))
        try:
            data = C.create_string_buffer(program.encode())
            checked(link_add(link,1,data,len(data),name.encode(),0,None,None))
            binary, size = ptr(), C.c_size_t()
            checked(link_complete(link,C.byref(binary),C.byref(size)))
            cubin = C.string_at(binary,size.value)
            (out / f'capture-{name}.cubin').write_bytes(cubin)
            headers.append(f'inline constexpr unsigned char {name}[] = {{\n')
            headers += [','.join(f'0x{x:02x}' for x in cubin[i:i+24])+',\n' for i in range(0,len(cubin),24)]
            headers.append('};\n')
            print(name, len(cubin), hashlib.sha256(cubin).hexdigest())
        finally:
            checked(link_destroy(link))
    headers.append('}\n')
    (out / 'capture_binary.generated.h').write_text(''.join(headers))
finally:
    checked(destroy(context))
