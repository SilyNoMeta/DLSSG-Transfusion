import json
from pathlib import Path
import numpy as np
import cv2

def simulate_fixes():
    cap_dir = Path("capture-output")
    out_dir = Path("capture-review/simulations")
    out_dir.mkdir(parents=True, exist_ok=True)
    
    stems = ["25240-3904", "25240-4066", "25240-4155", "25240-4316"]
    
    for stem in stems:
        jf = cap_dir / f"{stem}.json"
        raw_file = cap_dir / f"{stem}.rgba32f"
        if not raw_file.exists():
            continue
        meta = json.loads(jf.read_text())
        side = meta.get("side", 256)
        planes = meta.get("planes", 9)
        data = np.fromfile(raw_file, dtype=np.float32).reshape((planes, side, side, 4))
        
        raw0 = data[0]
        raw1 = data[1]
        w0 = data[4, ..., 0]
        w1 = data[4, ..., 1]
        flags = data[8]
        qv0 = flags[..., 0] > 0.5
        qv1 = flags[..., 1] > 0.5
        
        # Calculate motion & chroma error
        diff = np.abs(raw0[..., :3] - raw1[..., :3])
        motion_err = np.sum(diff, axis=-1)
        chroma_err = (np.abs(diff[..., 0] - diff[..., 1]) +
                      np.abs(diff[..., 1] - diff[..., 2]) +
                      np.abs(diff[..., 2] - diff[..., 0]))
        
        # Conflict: motion > 0.35 and chroma > 0.25 and both valid
        conflict = (motion_err > 0.35) & (chroma_err > 0.25) & qv0 & qv1
        
        # Shadow veto calculation
        l1_0 = np.sum(np.abs(raw0[..., :3]), axis=-1)
        l1_1 = np.sum(np.abs(raw1[..., :3]), axis=-1)
        sig = (l1_0 > 0.01) & (l1_1 > 0.01)
        
        l1_0_clamped = np.maximum(l1_0, 0.01)
        l1_1_clamped = np.maximum(l1_1, 0.01)
        
        n0 = raw0[..., :3] / l1_0_clamped[..., None]
        n1 = raw1[..., :3] / l1_1_clamped[..., None]
        
        dot01 = np.sum(n0 * n1, axis=-1)
        dot00 = np.sum(n0 * n0, axis=-1)
        dot11 = np.sum(n1 * n1, axis=-1)
        
        norm_sq = dot00 * dot11
        cos_sq = dot01 * dot01
        res = norm_sq - cos_sq
        
        is_shadow = sig & (dot01 > 0) & (res <= 0.01 * norm_sq)
        
        # qv8 = conflict & not is_shadow
        qv8 = conflict & (~is_shadow)
        
        # Scenario A: Current broken bidirectional (qv13 and qv14)
        c0_wins = qv8 & (w0 >= 0.50) & (w1 <= 0.35)
        c1_wins = qv8 & (w1 >= 0.50) & (w0 <= 0.35)
        simA_c0 = raw0.copy()
        simA_c1 = raw1.copy()
        simA_c0[c1_wins] = raw1[c1_wins]
        simA_c1[c0_wins] = raw0[c0_wins]
        
        # Scenario B: TestBuild 13 + Shadow Veto (ONLY C1 overwrites C0, C1 is NEVER modified)
        # With qv8:
        simB_c0 = raw0.copy()
        simB_c1 = raw1.copy() # C1 stays untouched!
        # C1 overwrites C0 when qv8 fires:
        simB_c0[qv8] = raw1[qv8]
        
        # Scenario C: Only C1 overwrites C0, but gating on C1 confidence (w1 >= 0.40):
        c1_confident = qv8 & (w1 >= 0.40)
        simC_c0 = raw0.copy()
        simC_c1 = raw1.copy()
        simC_c0[c1_confident] = raw1[c1_confident]
        
        # Save visualization of Scenario B vs Scenario A for cor0 and cor1
        # Panel: raw0 | raw1 | simA_c1 (broken) | simB_c0 (restored) | simB_c1 (pristine)
        def to_bgr(arr):
            return (np.clip(arr[..., :3], 0, 1)[..., ::-1] * 255).astype(np.uint8)
        
        panel = np.hstack([
            to_bgr(raw0),
            to_bgr(raw1),
            to_bgr(simA_c1),
            to_bgr(simB_c0),
            to_bgr(simB_c1)
        ])
        cv2.imwrite(str(out_dir / f"sim_{stem}.png"), panel)
        print(f"Saved sim_{stem}.png: raw0 | raw1 | simA_c1(broken) | simB_c0(restored) | simB_c1(pristine)")

if __name__ == "__main__":
    simulate_fixes()
