"""Execute the injected RGB policy on synthetic GPU inputs and JIT the full patched kernel.
Does not launch a game or write outside the specified workspace.
Run after the quality_ptx_profile CTest has exported quality-patched.ptx.
"""
import ctypes as C
import math
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[1]
build = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else (root / 'tests/startup_hooks/build-siren' if (root / 'tests/startup_hooks/build-siren/quality-patched.ptx').exists() else root / 'tests/startup_hooks/build-shadow')
header = (root / 'source/native/quality_fix.h').read_text()
registers = re.search(r'kRegisters = R"ptx\((.*?)\)ptx"', header, re.S)[1]
exported = (build / 'quality-patched.ptx').read_text()
policy = exported[exported.index('// QUALITY_VALID_WARP_V4_E2'):exported.index('ld.param.u8 %rs8, [%rd6+220];')]
assert 'QUALITY_SHADOW_SCALE_V1' in policy
siren_mode = 'QUALITY_SIREN_CHROMA_025_V1' in policy
diagnostic = 1 if 'QUALITY_DIAGNOSTIC_SOURCE0' in policy else 2 if 'QUALITY_DIAGNOSTIC_SOURCE1' in policy else 0
policy = policy.replace('ld.param.u8 %qrs0, [%rd6+220];', 'mov.u16 %qrs0, 0;')
slots = [115,116,117,119,120,121,123,124,129,130,125,126,127,131,132,133,
         148,149,39,38,37,43,42,41,36,40]
outputs = [39,38,37,43,42,41,36,40]
shader = '''.version 8.7
.target sm_89
.address_size 64
.visible .entry quality_policy(.param .u64 src, .param .u64 dst,
 .param .u32 missingA, .param .u32 missingB) {
.reg .f32 %f<560>;
.reg .pred %p<260>;
.reg .b32 %r<20>;
.reg .b64 %rd<4>;
''' + registers + '''
ld.param.u64 %rd0, [src];
ld.param.u64 %rd1, [dst];
ld.param.u32 %r0, [missingA];
ld.param.u32 %r1, [missingB];
setp.ne.u32 %p17, %r0, 0;
setp.ne.u32 %p16, %r1, 0;
mov.u32 %r10, 1920;
mov.u32 %r11, 1080;
''' + '\n'.join(f'ld.global.f32 %f{r}, [%rd0+{i*4}];' for i,r in enumerate(slots)) + '''
mov.f32 %f139, %f125;
mov.f32 %f140, %f126;
mov.f32 %f141, %f127;
mov.f32 %f145, %f131;
mov.f32 %f146, %f132;
mov.f32 %f147, %f133;
'''
shader += policy + '\n' + '\n'.join(f'st.global.f32 [%rd1+{i*4}], %f{r};' for i,r in enumerate(outputs)) + '\nret;\n}\n'

cuda = C.WinDLL('nvcuda.dll')
ptr = C.c_void_p
u64 = C.c_uint64
signatures = {
 'cuInit':[C.c_uint], 'cuDeviceGet':[C.POINTER(C.c_int),C.c_int],
 'cuCtxCreate_v2':[C.POINTER(ptr),C.c_uint,C.c_int], 'cuCtxDestroy_v2':[ptr],
 'cuModuleLoadData':[C.POINTER(ptr),ptr], 'cuModuleUnload':[ptr],
 'cuModuleGetFunction':[C.POINTER(ptr),ptr,C.c_char_p],
 'cuMemAlloc_v2':[C.POINTER(u64),C.c_size_t], 'cuMemFree_v2':[u64],
 'cuMemcpyHtoD_v2':[u64,ptr,C.c_size_t], 'cuMemcpyDtoH_v2':[ptr,u64,C.c_size_t],
 'cuLaunchKernel':[ptr]+[C.c_uint]*7+[ptr,ptr,ptr], 'cuCtxSynchronize':[]}
for name, args in signatures.items():
    getattr(cuda,name).argtypes = args
    getattr(cuda,name).restype = C.c_int


def call(name,*args):
    result = getattr(cuda,name)(*args)
    if result: raise RuntimeError(f'{name}: CUDA error {result}')


context, full, module, function = ptr(),ptr(),ptr(),ptr()
src, dst = u64(),u64()
try:
    call('cuInit',0)
    device = C.c_int()
    call('cuDeviceGet',C.byref(device),0)
    call('cuCtxCreate_v2',C.byref(context),0,device)
    full_text = C.create_string_buffer((build/'quality-patched.ptx').read_bytes())
    call('cuModuleLoadData',C.byref(full),full_text)
    print('FULL_PATCHED_KERNEL_JIT_OK')
    call('cuModuleUnload',full)
    full = ptr()
    fatbin = C.create_string_buffer((build/'quality-patched.fatbin').read_bytes())
    call('cuModuleLoadData',C.byref(full),fatbin)
    print('PRODUCTION_REBUILT_FATBIN_JIT_OK')
    text = C.create_string_buffer(shader.encode())
    call('cuModuleLoadData',C.byref(module),text)
    call('cuModuleGetFunction',C.byref(function),module,b'quality_policy')
    call('cuMemAlloc_v2',C.byref(src),len(slots)*4)
    call('cuMemAlloc_v2',C.byref(dst),len(outputs)*4)
    baseline = {**dict.fromkeys([115,116,117],.1), **dict.fromkeys([119,120,121],.2),
                **dict.fromkeys([123,124,129,130],.5), **dict.fromkeys([125,126,127],.6),
                **dict.fromkeys([131,132,133],.61),148:.5,149:.5,
                **dict.fromkeys([39,38,37],.2), **dict.fromkeys([43,42,41],.3),36:.7,40:.8}
    def execute(changes, missing_a=0, missing_b=0):
        values = baseline | changes
        data = (C.c_float*len(slots))(*(values[r] for r in slots))
        call('cuMemcpyHtoD_v2',src,data,C.sizeof(data))
        ma,mb = C.c_uint(missing_a),C.c_uint(missing_b)
        params = (ptr*4)(*(C.cast(C.byref(x),ptr) for x in [src,dst,ma,mb]))
        call('cuLaunchKernel',function,1,1,1,1,1,1,0,None,params,None)
        call('cuCtxSynchronize')
        result = (C.c_float*len(outputs))()
        call('cuMemcpyDtoH_v2',result,dst,C.sizeof(result))
        return list(result)
    count = 0
    def check(name, a, b, copy=False, missing_a=0, missing_b=0, changes=None):
        global count
        inputs = dict(zip([125,126,127,131,132,133], a+b)) | (changes or {})
        result = execute(inputs, missing_a, missing_b)
        expected = ([.2]*3 + [.3]*3 + [.7,.8]) if copy else (a + b + [.7,.8])
        if diagnostic and not missing_a and not missing_b:
            expected[:6] = (a if diagnostic == 1 else b) * 2
        if missing_a or inputs.get(123, 0.5) < 0: expected[:3] = [.2]*3
        if missing_b or inputs.get(129, 0.5) > 1: expected[3:6] = [.3]*3
        if not all(math.isclose(x,y,abs_tol=3e-6) for x,y in zip(result,expected)):
            raise AssertionError(f'{name}: {result} != {expected}')
        count += 1
    # Wide brightness and saturation sweep: includes counterexamples to the old ratio claim.
    for color in [[1.,0.,0.],[0.,1.,0.],[0.,0.,1.],[.8,.61,.55],[.5,.5,.5],[.01,.5,1.]]:
        for exposure in [.1,1.,4.,16.]:
            for attenuation in [.1,.4,.7,.95]:
                a=[v*exposure*attenuation for v in color]; b=[v*exposure for v in color]
                check('scaled shadow',a,b)
                check('reverse scaled shadow',b,a)
    check('blue siren',[.2,.45,.2],[.2,.55,1.],True)
    check('red siren',[.2,.45,.2],[1.,.2,.2],True)
    # Conflicts missed by 0.70 but admitted by 0.25. Test both temporal directions.
    for color in [[.25,.55,.47],[.47,.55,.25]]:
        check('moderate siren',[.2,.45,.2],color,siren_mode)
        check('moderate trailing siren',color,[.2,.45,.2],siren_mode)
    check('small chromatic fluctuation',[.2,.45,.2],[.2,.45,.35])
    # Approximate brightness scaling: preserve mild chromatic noise on colored ground.
    check('near scalar brick',[.40,.30,.275],[.8,.61,.55])
    check('achromatic edge',[.1]*3,[.9]*3)
    check('missing past',[.2,.45,.2],[.2,.55,1.],missing_a=1)
    check('missing current',[.2,.45,.2],[.2,.55,1.],missing_b=1)
    # Candidate Conflict Firewall:
    # Under chromatic conflict passing shadow veto, geometric warp is bypassed.
    # Stock DLSS-G candidates are preserved untouched (no candidate overwrite):
    result_conflict = execute({125:.2,126:.45,127:.2,131:.2,132:.55,133:1.,148:.7,149:.3})
    assert all(math.isclose(x,y,abs_tol=3e-6) for x,y in zip(result_conflict,[.2,.2,.2,.3,.3,.3,.7,.8]))
    result_conflict2 = execute({125:.2,126:.45,127:.2,131:.2,132:.55,133:1.,148:.3,149:.7})
    assert all(math.isclose(x,y,abs_tol=3e-6) for x,y in zip(result_conflict2,[.2,.2,.2,.3,.3,.3,.7,.8]))
    print(f'GPU_SHADOW_SCALE_OK (siren_mode={siren_mode}, diagnostic={diagnostic}): {count+2} cases; exported experimental policy executed')
finally:
    if src.value: call('cuMemFree_v2',src)
    if dst.value: call('cuMemFree_v2',dst)
    if module.value: call('cuModuleUnload',module)
    if full.value: call('cuModuleUnload',full)
    if context.value: call('cuCtxDestroy_v2',context)
