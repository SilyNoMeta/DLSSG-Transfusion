from pathlib import Path
import numpy as np
import cv2

def test_p16_fix():
    raw_file = Path("capture-output/26816-2989.rgba32f")
    data = np.fromfile(raw_file, dtype=np.float32).reshape((9, 512, 512, 4))
    
    raw0 = data[0]
    raw1 = data[1]
    uv = data[5]
    flags = data[8]
    
    # Check bounds
    W, H = 2560, 1440
    qf0, qf1 = 0.5 / W, 0.5 / H
    qf2, qf3 = 1.0 - qf0, 1.0 - qf1
    
    u0, v0 = uv[..., 0], uv[..., 1]
    u1, v1 = uv[..., 2], uv[..., 3]
    
    b0 = (u0 >= qf0) & (u0 <= qf2) & (v0 >= qf1) & (v0 <= qf3)
    b1 = (u1 >= qf0) & (u1 <= qf2) & (v1 >= qf1) & (v1 <= qf3)
    
    f0 = np.isfinite(raw0[..., :3].sum(axis=-1)) & (raw0[..., :3].sum(axis=-1) < 1e30)
    f1 = np.isfinite(raw1[..., :3].sum(axis=-1)) & (raw1[..., :3].sum(axis=-1) < 1e30)
    
    # Pure geometric validity (without p16/p17 motion vector failure kill switch):
    pure_v0 = b0 & f0
    pure_v1 = b1 & f1
    pure_v3 = pure_v0 & pure_v1
    
    # Motion & chroma error
    diff = np.abs(raw0[..., :3] - raw1[..., :3])
    motion_err = np.sum(diff, axis=-1)
    chroma_err = (np.abs(diff[..., 0] - diff[..., 1]) +
                  np.abs(diff[..., 1] - diff[..., 2]) +
                  np.abs(diff[..., 2] - diff[..., 0]))
    
    conflict = (motion_err > 0.35) & (chroma_err > 0.25) & pure_v3
    
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
    
    new_qv8 = conflict & (~is_shadow)
    
    old_qv8 = flags[..., 3] > 0.5
    
    print(f"Old qv8 active pixels: {np.sum(old_qv8)}")
    print(f"New qv8 active pixels: {np.sum(new_qv8)}")
    
    # Check uncollapsed ghost pixels from before:
    is_red0 = (raw0[..., 0] > 0.4) & (raw0[..., 1] < 0.2) & (raw0[..., 2] < 0.2)
    not_red1 = (raw1[..., 0] < 0.4) | (raw1[..., 1] > 0.3)
    uncollapsed_old = is_red0 & not_red1 & (~old_qv8)
    uncollapsed_new = is_red0 & not_red1 & (~new_qv8)
    
    print(f"Old uncollapsed ghost pixels: {np.sum(uncollapsed_old)}")
    print(f"New uncollapsed ghost pixels: {np.sum(uncollapsed_new)}")
    
    # Generate visual comparison:
    # 1. raw0
    # 2. raw1
    # 3. old cor0 (with old double red dome)
    # 4. new cor0 (with p16 fix)
    # 5. new copy mask
    def to_bgr(arr):
        return (np.clip(arr[..., :3], 0, 1)[..., ::-1] * 255).astype(np.uint8)
        
    old_cor0 = data[6].copy()
    new_cor0 = raw0.copy()
    new_cor0[new_qv8] = raw1[new_qv8]
    
    mask_vis = (to_bgr(new_cor0)).copy()
    mask_vis[new_qv8] = [0, 255, 0] # Green
    
    panel = np.hstack([
        to_bgr(raw0),
        to_bgr(raw1),
        to_bgr(old_cor0),
        to_bgr(new_cor0),
        mask_vis
    ])
    
    cv2.imwrite("capture-review/test_p16_fix_panel.png", panel)
    print("Saved capture-review/test_p16_fix_panel.png")

if __name__ == "__main__":
    test_p16_fix()
