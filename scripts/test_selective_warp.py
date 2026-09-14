from pathlib import Path
import numpy as np
import cv2

def test_selective_warp():
    raw_file = Path("capture-output/26816-2989.rgba32f")
    data = np.fromfile(raw_file, dtype=np.float32).reshape((9, 512, 512, 4))
    
    raw0 = data[0]      # f125..f127
    raw1 = data[1]      # f131..f133
    ref0 = data[2]      # f115..f117
    ref1 = data[3]      # f119..f121
    weights = data[4]   # f148, f149
    w0_stock = weights[..., 0]
    w1_stock = weights[..., 1]
    
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
    
    conflict = (motion_err > 0.35) & (chroma_err > 0.25)
    qv8 = conflict & (~is_shadow)
    
    # Selective warp weight:
    # Normally 1.0 (pure warp on wire fences and solid geometry)
    # Reverts to stock confidence on chromatic conflicts (sirens/bubbles)
    w0_sel = np.where(qv8, w0_stock, 1.0)
    w1_sel = np.where(qv8, w1_stock, 1.0)
    
    # Compute Candidate 0 and Candidate 1:
    c0_sel = ref0[..., :3] + w0_sel[..., None] * (raw0[..., :3] - ref0[..., :3])
    c1_sel = ref1[..., :3] + w1_sel[..., None] * (raw1[..., :3] - ref1[..., :3])
    
    # Compare with forced 100% warp:
    c0_forced = raw0[..., :3]
    c1_forced = raw1[..., :3]
    
    def to_bgr(arr):
        return (np.clip(arr, 0, 1)[..., ::-1] * 255).astype(np.uint8)
        
    crop_forced0 = to_bgr(c0_forced)[30:250, 20:420]
    crop_forced1 = to_bgr(c1_forced)[30:250, 20:420]
    crop_sel0 = to_bgr(c0_sel)[30:250, 20:420]
    crop_sel1 = to_bgr(c1_sel)[30:250, 20:420]
    crop_ref1 = to_bgr(ref1[..., :3])[30:250, 20:420]
    
    panel = np.hstack([crop_forced0, crop_forced1, crop_sel0, crop_sel1, crop_ref1])
    cv2.imwrite("capture-review/test_selective_warp.png", panel)
    print("Saved capture-review/test_selective_warp.png")

if __name__ == "__main__":
    test_selective_warp()
