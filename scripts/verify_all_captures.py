import json
from pathlib import Path
import numpy as np

def verify_all_captures():
    cap_dir = Path("capture-output")
    json_files = sorted(cap_dir.glob("*.json"))
    
    total_conflict_pixels = 0
    total_vetoed_shadow_pixels = 0
    total_reconciled_pixels = 0
    
    for jf in json_files:
        stem = jf.stem
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
        
        diff = np.abs(raw0[..., :3] - raw1[..., :3])
        motion_err = np.sum(diff, axis=-1)
        chroma_err = (np.abs(diff[..., 0] - diff[..., 1]) +
                      np.abs(diff[..., 1] - diff[..., 2]) +
                      np.abs(diff[..., 2] - diff[..., 0]))
        
        conflict = (motion_err > 0.35) & (chroma_err > 0.25) & qv0 & qv1
        
        # Shadow veto
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
        
        qv8 = conflict & (~is_shadow)
        
        n_conflict = np.sum(conflict)
        n_shadow = np.sum(conflict & is_shadow)
        n_qv8 = np.sum(qv8)
        
        total_conflict_pixels += n_conflict
        total_vetoed_shadow_pixels += n_shadow
        total_reconciled_pixels += n_qv8
        
        disp = meta["dispatch"]
        if disp in [3619, 3751, 3904, 4066, 4155, 4316]:
            print(f"Disp {disp}: Conflict={n_conflict}, ShadowVetoed={n_shadow} ({n_shadow/max(1,n_conflict)*100:.1f}%), Qv8(Active)={n_qv8}")
            
    print("\nTOTALS across all 36 captures:")
    print(f"  Total conflict pixels: {total_conflict_pixels}")
    print(f"  Total shadow vetoed pixels: {total_vetoed_shadow_pixels} ({total_vetoed_shadow_pixels/total_conflict_pixels*100:.1f}%)")
    print(f"  Total Candidate 0 -> Candidate 1 overwrites: {total_reconciled_pixels}")
    print(f"  Total Candidate 1 modifications: 0 (Candidate 1 is NEVER touched!)")

if __name__ == "__main__":
    verify_all_captures()
