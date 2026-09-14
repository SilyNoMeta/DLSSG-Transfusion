import json
from pathlib import Path
import numpy as np
import cv2

def render_comparisons():
    cap_dir = Path("capture-output")
    out_dir = Path("capture-review/policy_comparison")
    out_dir.mkdir(parents=True, exist_ok=True)
    
    W, H = 2560, 1440
    qf0, qf1 = 0.5 / W, 0.5 / H
    qf2, qf3 = 1.0 - qf0, 1.0 - qf1

    def to_bgr(arr):
        return (np.clip(arr[..., :3], 0, 1)[..., ::-1] * 255).astype(np.uint8)

    dispatches = [2989, 2992, 3099, 3103, 3231, 3410]
    
    for disp in dispatches:
        raw_files = list(cap_dir.glob(f"*-{disp}.rgba32f"))
        if not raw_files:
            continue
        raw_file = raw_files[0]
        data = np.fromfile(raw_file, dtype=np.float32).reshape((9, 512, 512, 4))
        
        raw0 = data[0]
        raw1 = data[1]
        cor0_captured = data[6] # Old corrected0 with p16 bug
        cor1_captured = data[7]
        uv = data[5]
        flags = data[8]
        
        u0, v0 = uv[..., 0], uv[..., 1]
        u1, v1 = uv[..., 2], uv[..., 3]
        b0 = (u0 >= qf0) & (u0 <= qf2) & (v0 >= qf1) & (v0 <= qf3)
        b1 = (u1 >= qf0) & (u1 <= qf2) & (v1 >= qf1) & (v1 <= qf3)
        
        l1_raw0 = np.sum(np.abs(raw0[..., :3]), axis=-1)
        l1_raw1 = np.sum(np.abs(raw1[..., :3]), axis=-1)
        f0 = np.isfinite(l1_raw0) & (l1_raw0 < 1e30)
        f1 = np.isfinite(l1_raw1) & (l1_raw1 < 1e30)
        pure_v3 = b0 & b1 & f0 & f1
        
        diff = np.abs(raw0[..., :3] - raw1[..., :3])
        motion_err = np.sum(diff, axis=-1)
        chroma_err = (np.abs(diff[..., 0] - diff[..., 1]) +
                      np.abs(diff[..., 1] - diff[..., 2]) +
                      np.abs(diff[..., 2] - diff[..., 0]))
                      
        # Shadow veto
        sig = (l1_raw0 > 0.01) & (l1_raw1 > 0.01)
        n0 = raw0[..., :3] / np.maximum(l1_raw0[..., None], 0.01)
        n1 = raw1[..., :3] / np.maximum(l1_raw1[..., None], 0.01)
        dot01 = np.sum(n0 * n1, axis=-1)
        dot00 = np.sum(n0 * n0, axis=-1)
        dot11 = np.sum(n1 * n1, axis=-1)
        norm_sq = dot00 * dot11
        cos_sq = dot01 * dot01
        res = norm_sq - cos_sq
        is_shadow = sig & (dot01 > 0) & (res <= 0.01 * norm_sq)
        
        # Proposed fix:
        conflict = (motion_err > 0.25) & (chroma_err > 0.15) & pure_v3
        new_qv8 = conflict & (~is_shadow)
        
        new_cor0 = raw0.copy()
        new_cor0[new_qv8] = raw1[new_qv8]
        
        # Crop to the siren area (y: 30..250, x: 20..420)
        c_raw0 = to_bgr(raw0)[30:250, 20:420]
        c_raw1 = to_bgr(raw1)[30:250, 20:420]
        c_old = to_bgr(cor0_captured)[30:250, 20:420]
        c_new = to_bgr(new_cor0)[30:250, 20:420]
        
        mask = np.zeros_like(c_new)
        mask_crop = new_qv8[30:250, 20:420]
        mask[mask_crop] = [0, 255, 0] # Green for overwritten pixels
        
        comp = np.hstack([c_raw0, c_raw1, c_old, c_new, mask])
        
        cv2.imwrite(str(out_dir / f"compare_{disp}.png"), comp)
        cv2.imwrite(str(out_dir / f"full_new_cor0_{disp}.png"), to_bgr(new_cor0))
        print(f"Rendered comparison for dispatch {disp}")

if __name__ == "__main__":
    render_comparisons()
