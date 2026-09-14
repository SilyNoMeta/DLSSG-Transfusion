import json
from pathlib import Path
import numpy as np

def inspect_ghost():
    raw_file = Path("capture-output/26816-2989.rgba32f")
    data = np.fromfile(raw_file, dtype=np.float32).reshape((9, 512, 512, 4))
    
    raw0 = data[0]
    raw1 = data[1]
    cor0 = data[6]
    cor1 = data[7]
    flags = data[8]
    
    qv0 = flags[..., 0] > 0.5
    qv1 = flags[..., 1] > 0.5
    qv4 = flags[..., 2] > 0.5
    qv8 = flags[..., 3] > 0.5
    
    # In cor0, find where the old red dome is still present:
    # That is: raw0 is reddish (R > 0.4, G < 0.2, B < 0.2), but raw1 is NOT reddish (R < 0.4 or G > 0.3),
    # yet cor0 is STILL reddish (R > 0.4)! (i.e. it was NOT overwritten!)
    is_red0 = (raw0[..., 0] > 0.4) & (raw0[..., 1] < 0.2) & (raw0[..., 2] < 0.2)
    not_red1 = (raw1[..., 0] < 0.4) | (raw1[..., 1] > 0.3)
    uncollapsed_ghost = is_red0 & not_red1 & (~qv8)
    
    print(f"Total uncollapsed red siren ghost pixels: {np.sum(uncollapsed_ghost)}")
    
    if np.any(uncollapsed_ghost):
        # Inspect why qv8 did not fire on these pixels:
        # Check motion error
        diff = np.abs(raw0[..., :3] - raw1[..., :3])
        motion_err = np.sum(diff, axis=-1)
        chroma_err = (np.abs(diff[..., 0] - diff[..., 1]) +
                      np.abs(diff[..., 1] - diff[..., 2]) +
                      np.abs(diff[..., 2] - diff[..., 0]))
        
        # Shadow veto check
        l1_0 = np.sum(np.abs(raw0[..., :3]), axis=-1)
        l1_1 = np.sum(np.abs(raw1[..., :3]), axis=-1)
        sig = (l1_0 > 0.01) & (l1_1 > 0.01)
        n0 = raw0[..., :3] / np.maximum(l1_0[..., None], 0.01)
        n1 = raw1[..., :3] / np.maximum(l1_1[..., None], 0.01)
        dot01 = np.sum(n0 * n1, axis=-1)
        dot00 = np.sum(n0 * n0, axis=-1)
        dot11 = np.sum(n1 * n1, axis=-1)
        norm_sq = dot00 * dot11
        cos_sq = dot01 * dot01
        res = norm_sq - cos_sq
        is_shadow = sig & (dot01 > 0) & (res <= 0.01 * norm_sq)
        
        m_ghost = motion_err[uncollapsed_ghost]
        c_ghost = chroma_err[uncollapsed_ghost]
        v0_ghost = qv0[uncollapsed_ghost]
        v1_ghost = qv1[uncollapsed_ghost]
        c4_ghost = qv4[uncollapsed_ghost]
        sh_ghost = is_shadow[uncollapsed_ghost]
        
        print(f"  motion_err: min={np.min(m_ghost):.3f}, median={np.median(m_ghost):.3f}, max={np.max(m_ghost):.3f}")
        print(f"  motion_err > 0.35: {np.sum(m_ghost > 0.35)} / {len(m_ghost)}")
        print(f"  chroma_err: min={np.min(c_ghost):.3f}, median={np.median(c_ghost):.3f}, max={np.max(c_ghost):.3f}")
        print(f"  chroma_err > 0.25: {np.sum(c_ghost > 0.25)} / {len(c_ghost)}")
        print(f"  qv0 (C0 valid): {np.sum(v0_ghost)} / {len(m_ghost)}")
        print(f"  qv1 (C1 valid): {np.sum(v1_ghost)} / {len(m_ghost)}")
        print(f"  qv4 (conflict true): {np.sum(c4_ghost)} / {len(m_ghost)}")
        print(f"  is_shadow (vetoed): {np.sum(sh_ghost)} / {len(m_ghost)}")

if __name__ == "__main__":
    inspect_ghost()
