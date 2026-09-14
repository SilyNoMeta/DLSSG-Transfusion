import json
from pathlib import Path
import numpy as np

def debug_qv1():
    raw_file = Path("capture-output/26816-2989.rgba32f")
    data = np.fromfile(raw_file, dtype=np.float32).reshape((9, 512, 512, 4))
    
    raw0 = data[0]
    raw1 = data[1]
    uv = data[5] # uv0 = [0, 1], uv1 = [2, 3]
    flags = data[8]
    qv1 = flags[..., 1] > 0.5
    
    is_red0 = (raw0[..., 0] > 0.4) & (raw0[..., 1] < 0.2) & (raw0[..., 2] < 0.2)
    not_red1 = (raw1[..., 0] < 0.4) | (raw1[..., 1] > 0.3)
    qv8 = flags[..., 3] > 0.5
    uncollapsed = is_red0 & not_red1 & (~qv8)
    
    u1 = uv[..., 2]
    v1 = uv[..., 3]
    
    print(f"On uncollapsed pixels (total {np.sum(uncollapsed)}):")
    print(f"  u1 min={np.min(u1[uncollapsed]):.4f}, max={np.max(u1[uncollapsed]):.4f}")
    print(f"  v1 min={np.min(v1[uncollapsed]):.4f}, max={np.max(v1[uncollapsed]):.4f}")
    
    # Check width and height from json
    meta = json.loads(Path("capture-output/26816-2989.json").read_text())
    W, H = meta["width"], meta["height"]
    qf0 = 0.5 / W
    qf1 = 0.5 / H
    qf2 = 1.0 - qf0
    qf3 = 1.0 - qf1
    print(f"  W={W}, H={H}")
    print(f"  qf0={qf0:.6f}, qf2={qf2:.6f}")
    print(f"  qf1={qf1:.6f}, qf3={qf3:.6f}")
    
    bounds_ok = (u1 >= qf0) & (u1 <= qf2) & (v1 >= qf1) & (v1 <= qf3)
    print(f"  bounds_ok: {np.sum(bounds_ok[uncollapsed])} / {np.sum(uncollapsed)}")
    
    # Check finite
    sum_abs = np.abs(raw1[..., 0]) + np.abs(raw1[..., 1]) + np.abs(raw1[..., 2])
    finite_ok = np.isfinite(sum_abs) & (sum_abs < 1e30)
    print(f"  finite_ok: {np.sum(finite_ok[uncollapsed])} / {np.sum(uncollapsed)}")
    
    # Check what else could make qv1 false in kPolicyE2:
    # Wait! In kPolicyE2, look at lines 198-228:
    # Does anything else touch qv1 before flags store?
    # Let's check capture-probe.ptx!

if __name__ == "__main__":
    debug_qv1()
