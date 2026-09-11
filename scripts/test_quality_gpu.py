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
header = (Path(sys.argv[1]) if len(sys.argv) > 1 else root / 'source/native/quality_fix.h').read_text()
registers = re.search(r'kRegisters = R"ptx\((.*?)\)ptx"', header, re.S)[1]
policy = re.search(r'kPolicy = R"ptx\((.*?)\)ptx"', header, re.S)[1]
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
''' + '\n'.join(f'ld.global.f32 %f{r}, [%rd0+{i*4}];' for i,r in enumerate(slots))
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
    full_text = C.create_string_buffer((root/'tests/startup_hooks/build/quality-patched.ptx').read_bytes())
    call('cuModuleLoadData',C.byref(full),full_text)
    print('FULL_PATCHED_KERNEL_JIT_OK')
    call('cuModuleUnload',full)
    full = ptr()
    fatbin = C.create_string_buffer((root/'tests/startup_hooks/build/quality-patched.fatbin').read_bytes())
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
    cases = [
      ('coherent motion',{},0,0,.525,.5485),
      ('left exit',{129:-.1},0,0,.525,.525),
      ('right exit',{129:1.1},0,0,.525,.525),
      ('top exit',{124:-.1},0,0,.5485,.5485),
      ('bottom exit',{124:1.1},0,0,.5485,.5485),
      ('both offscreen',{124:-.1,130:1.1},0,0,.2,.3),
      ('both missing',{},1,1,.2,.3),
      ('A missing',{},1,0,.5485,.5485),
      ('B missing',{},0,1,.525,.525),
      ('low confidence',{148:.1,149:.1},0,0,.2,.3),
      ('low A confidence is not missing',{148:.1},0,0,.2,.5485),
      ('disagreeing colors',{131:.95,132:.95,133:.95},0,0,.2,.3),
      ('NaN coordinates',{123:math.nan,129:math.nan},0,0,.2,.3),
      ('infinite coordinates',{124:math.inf,130:-math.inf},0,0,.2,.3),
      ('nonfinite candidate',{125:math.nan,131:math.inf},0,0,.2,.3),
      ('nonfinite confidence',{148:math.nan,149:math.inf},0,0,.2,.3),
      ('preserve higher confidence',{148:.95},0,0,.575,.5485),
      ('half texel outside',{123:0.,129:1.},0,0,.2,.3),
      ('replace invalid A RGB',{39:math.nan,38:math.nan,37:math.nan},1,0,.5485,.5485),
      ('replace invalid B RGB',{43:math.inf,42:math.inf,41:math.inf},0,1,.525,.525),
    ]
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
    for name,changes,missing_a,missing_b,want_a,want_b in cases:
        result = execute(changes,missing_a,missing_b)
        expected = [want_a]*3 + [want_b]*3 + [.7,.8]
        if not all(math.isclose(x,y,abs_tol=2e-6) for x,y in zip(result,expected)):
            raise AssertionError(f'{name}: got {list(result)}, expected {expected}')
        print('PASS:',name)
    print(f'GPU_QUALITY_POLICY_OK: {len(cases)} cases; auxiliary channels preserved')
    # Sweep through the old hard thresholds and all four source-image borders.
    # These measure numerical continuity of the actual injected GPU instructions.
    sweeps = {
        'confidence': [{148:i/1000} for i in range(1001)],
        'color agreement': [{131:v,132:v,133:v} for v in [.60+i*.0005 for i in range(401)]],
    }
    for label,register,size,reverse in [('left',123,1920,False),('right',123,1920,True),
                                        ('top',124,1080,False),('bottom',124,1080,True)]:
        sweeps[label] = [{register: (1-(-.5+i*.01)/size) if reverse else (-.5+i*.01)/size}
                         for i in range(401)]
    for name,inputs in sweeps.items():
        if 'QUALITY_SMOOTH_WARP_V5' not in policy:
            print('CONTINUITY_NOT_REQUIRED: v4 baseline retains hard thresholds; v5 was visually worse')
            break
        results = [execute(values) for values in inputs]
        assert all(math.isfinite(v) for row in results for v in row), name
        worst = max(abs(x-y) for a,b in zip(results,results[1:]) for x,y in zip(a,b))
        assert worst < .004, (name,worst)
        print(f'CONTINUITY_PASS: {name}; max adjacent RGB change={worst:.6f}')
finally:
    if src.value: call('cuMemFree_v2',src)
    if dst.value: call('cuMemFree_v2',dst)
    if module.value: call('cuModuleUnload',module)
    if full.value: call('cuModuleUnload',full)
    if context.value: call('cuCtxDestroy_v2',context)
