"""Read-only PTX audit. Optional CUDA JIT checks compilation, never launches kernels.

Usage: python scripts/audit_quality_dumps.py ../ptx_dumps --output report.json [--jit]
The old regexes are reproduced only to inventory the removed bypass's scope.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import re

BACKWARD = re.compile(r'and\.pred\s+(%p\d+),\s+(%p\d+),\s+(%p\d+);[\r\n]+(\s*)@\1\s+bra\s+(\$L__BB\d+_\d+);')
FORWARD = re.compile(r'setp\.geu\.ftz\.f32\s+(%p\d+),\s+(%f\d+),\s+(%f\d+);[\r\n]+(\s*)@\1\s+bra\s+(\$L__BB\d+_\d+);')


def check(result, name):
    if result:
        raise RuntimeError(f'{name}: CUDA error {result}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('dumps', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--jit', action='store_true')
    args = parser.parse_args()
    files = sorted(args.dumps.glob('*_sm120.ptx'))
    if not files:
        parser.error('No sm120 PTX dumps found')
    report = {'scope': 'Static dump audit; no game execution or image-quality validation', 'kernels': []}
    cuda, context = None, C.c_void_p()
    try:
        if args.jit:
            cuda = C.WinDLL('nvcuda.dll')
            cuda.cuInit.argtypes = [C.c_uint]
            cuda.cuDeviceGet.argtypes = [C.POINTER(C.c_int), C.c_int]
            cuda.cuDeviceGetName.argtypes = [C.c_char_p, C.c_int, C.c_int]
            cuda.cuCtxCreate_v2.argtypes = [C.POINTER(C.c_void_p), C.c_uint, C.c_int]
            cuda.cuModuleLoadData.argtypes = [C.POINTER(C.c_void_p), C.c_void_p]
            cuda.cuModuleUnload.argtypes = [C.c_void_p]
            cuda.cuCtxDestroy_v2.argtypes = [C.c_void_p]
            check(cuda.cuInit(0), 'cuInit')
            device = C.c_int()
            check(cuda.cuDeviceGet(C.byref(device), 0), 'cuDeviceGet')
            name = C.create_string_buffer(256)
            check(cuda.cuDeviceGetName(name, len(name), device), 'cuDeviceGetName')
            report['jit_device'] = name.value.decode()
            check(cuda.cuCtxCreate_v2(C.byref(context), 0, device), 'cuCtxCreate')
        for path in files:
            data = path.read_bytes()
            text = data.decode('latin1')
            item = {'file': path.name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}
            if 'EstimateIntermMvecsScatter' in path.name:
                item['removed_bypass_matches'] = {
                    label: [{'line': text.count('\n', 0, m.start()) + 1,
                             'predicate': m[1], 'destination': m[5]} for m in pattern.finditer(text)]
                    for label, pattern in [('backward_pattern', BACKWARD), ('forward_pattern', FORWARD)]}
            if cuda:
                # Only change the target directive; keep all instructions and predicates.
                retargeted, count = re.subn(r'\.target sm_120[\r\n ]*', '.target sm_89\n', text)
                if count != 1:
                    raise ValueError(f'{path}: expected one target, found {count}')
                module = C.c_void_p()
                payload = C.create_string_buffer(retargeted.encode('latin1'))
                item['jit_result'] = cuda.cuModuleLoadData(C.byref(module), payload)
                if item['jit_result'] == 0:
                    check(cuda.cuModuleUnload(module), 'cuModuleUnload')
            report['kernels'].append(item)
    finally:
        if context.value:
            check(cuda.cuCtxDestroy_v2(context), 'cuCtxDestroy')
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'Audited {len(files)} kernels; report: {args.output}')
    for item in report['kernels']:
        if 'removed_bypass_matches' in item:
            print('Removed bypass:', {k: len(v) for k, v in item['removed_bypass_matches'].items()})
    if cuda:
        failed = [item['file'] for item in report['kernels'] if item['jit_result'] != 0]
        print(f'CUDA JIT: {len(files) - len(failed)}/{len(files)} loaded on {report["jit_device"]}; no kernels executed')
        if failed:
            raise SystemExit('JIT failures: ' + ', '.join(failed))


if __name__ == '__main__':
    main()
