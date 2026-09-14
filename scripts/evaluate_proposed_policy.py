import json
from pathlib import Path
import numpy as np
import cv2

def evaluate_all():
    cap_dir = Path("capture-output")
    json_files = sorted(cap_dir.glob("*.json"))
    
    print(f"Found {len(json_files)} capture metadata files.")
    
    W, H = 2560, 1440
    qf0, qf1 = 0.5 / W, 0.5 / H
    qf2, qf3 = 1.0 - qf0, 1.0 - qf1

    total_ghost_pixels_old = 0
    total_ghost_pixels_new = 0
    total_new_qv8 = 0
    total_old_qv8 = 0

    results = []

    for jf in json_files:
        stem = jf.stem
        raw_file = cap_dir / f"{stem}.rgba32f"
        if not raw_file.exists():
            continue
        meta = json.loads(jf.read_text())
        side = meta.get("side", 512)
        planes = meta.get("planes", 9)
        data = np.fromfile(raw_file, dtype=np.float32).reshape((planes, side, side, 4))
        
        raw0 = data[0]
        raw1 = data[1]
        uv = data[5]
        flags = data[8]
        
        old_qv8 = flags[..., 3] > 0.5
        
        u0, v0 = uv[..., 0], uv[..., 1]
        u1, v1 = uv[..., 2], uv[..., 3]
        
        b0 = (u0 >= qf0) & (u0 <= qf2) & (v0 >= qf1) & (v0 <= qf3)
        b1 = (u1 >= qf0) & (u1 <= qf2) & (v1 >= qf1) & (v1 <= qf3)
        
        l1_raw0 = np.sum(np.abs(raw0[..., :3]), axis=-1)
        l1_raw1 = np.sum(np.abs(raw1[..., :3]), axis=-1)
        f0 = np.isfinite(l1_raw0) & (l1_raw0 < 1e30)
        f1 = np.isfinite(l1_raw1) & (l1_raw1 < 1e30)
        
        # Pure validity: screen bounds + finite floats (no p16/p17 kill switch)
        pure_v0 = b0 & f0
        pure_v1 = b1 & f1
        pure_v3 = pure_v0 & pure_v1
        
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
        
        # Proposed policy: motion > 0.25, chroma > 0.15, pure_v3, !is_shadow
        conflict = (motion_err > 0.25) & (chroma_err > 0.15) & pure_v3
        new_qv8 = conflict & (~is_shadow)
        
        # Check red siren ghost: raw0 is saturated red, raw1 is not
        is_red0 = (raw0[..., 0] > 0.4) & (raw0[..., 1] < 0.2) & (raw0[..., 2] < 0.2)
        not_red1 = (raw1[..., 0] < 0.4) | (raw1[..., 1] > 0.3)
        ghost_red = is_red0 & not_red1
        
        # Check blue siren ghost: raw0 is blue, raw1 is not
        is_blue0 = (raw0[..., 2] > 0.4) & (raw0[..., 0] < 0.25)
        not_blue1 = (raw1[..., 2] < 0.4) | (raw1[..., 0] > 0.3)
        ghost_blue = is_blue0 & not_blue1
        
        ghost = ghost_red | ghost_blue
        
        n_ghost = np.sum(ghost)
        uncol_old = np.sum(ghost & (~old_qv8))
        uncol_new = np.sum(ghost & (~new_qv8))
        
        total_ghost_pixels_old += uncol_old
        total_ghost_pixels_new += uncol_new
        total_old_qv8 += np.sum(old_qv8)
        total_new_qv8 += np.sum(new_qv8)
        
        disp = meta.get("dispatch", 0)
        results.append((disp, n_ghost, uncol_old, uncol_new, np.sum(old_qv8), np.sum(new_qv8)))
        
    print(f"{'Dispatch':<10} | {'Ghost Tot':<10} | {'Old Uncol':<10} | {'New Uncol':<10} | {'Old qv8':<10} | {'New qv8':<10}")
    print("-" * 70)
    for r in results:
        print(f"{r[0]:<10} | {r[1]:<10} | {r[2]:<10} | {r[3]:<10} | {r[4]:<10} | {r[5]:<10}")
    print("-" * 70)
    print(f"Total uncollapsed ghost pixels: OLD={total_ghost_pixels_old} -> NEW={total_ghost_pixels_new} (reduction: {(1 - total_ghost_pixels_new/max(1,total_ghost_pixels_old))*100:.1f}%)")
    print(f"Total active qv8 pixels: OLD={total_old_qv8} -> NEW={total_new_qv8}")

if __name__ == "__main__":
    evaluate_all()
