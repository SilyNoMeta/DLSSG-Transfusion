from pathlib import Path
import numpy as np
import cv2

def test_thresholds():
    raw_file = Path("capture-output/26816-2989.rgba32f")
    data = np.fromfile(raw_file, dtype=np.float32).reshape((9, 512, 512, 4))
    
    raw0 = data[0]
    raw1 = data[1]
    
    diff = np.abs(raw0[..., :3] - raw1[..., :3])
    motion_err = np.sum(diff, axis=-1)
    chroma_err = (np.abs(diff[..., 0] - diff[..., 1]) +
                  np.abs(diff[..., 1] - diff[..., 2]) +
                  np.abs(diff[..., 2] - diff[..., 0]))
    
    # Shadow veto
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
    
    # Check different chroma thresholds: 0.25, 0.15, 0.10, 0.05
    for th in [0.25, 0.15, 0.12, 0.10]:
        conflict = (motion_err > 0.25) & (chroma_err > th)
        qv8 = conflict & (~is_shadow)
        
        # Test on the red siren ghost
        # raw0 is red, raw1 is not red
        siren_raw0 = raw0[100:200, 50:200, :3]
        siren_raw1 = raw1[100:200, 50:200, :3]
        siren_qv8 = qv8[100:200, 50:200]
        
        is_red0 = (siren_raw0[..., 0] > 0.4) & (siren_raw0[..., 1] < 0.2) & (siren_raw0[..., 2] < 0.2)
        not_red1 = (siren_raw1[..., 0] < 0.4) | (siren_raw1[..., 1] > 0.3)
        ghost = is_red0 & not_red1
        
        n_ghost_total = np.sum(ghost)
        n_ghost_collapsed = np.sum(ghost & siren_qv8)
        
        # Also check on car hood/windshield (should NOT trigger copy)
        hood_qv8 = qv8[250:400, 100:400]
        
        print(f"Threshold chroma > {th:.2f}, motion > 0.25:")
        print(f"  Total qv8 pixels: {np.sum(qv8)}")
        print(f"  Ghost pixels collapsed: {n_ghost_collapsed} / {n_ghost_total} ({n_ghost_collapsed/n_ghost_total*100:.1f}%)")
        print(f"  Hood/windshield pixels copied: {np.sum(hood_qv8)}")
        
        # Save a rendered image of cor0
        cor0 = raw0.copy()
        cor0[qv8] = raw1[qv8]
        bgr = (np.clip(cor0[..., :3], 0, 1)[..., ::-1] * 255).astype(np.uint8)
        cv2.imwrite(f"capture-review/test_th_{int(th*100)}.png", bgr)

if __name__ == "__main__":
    test_thresholds()
