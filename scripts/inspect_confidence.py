import json
from pathlib import Path
import numpy as np

def inspect_confidence():
    cap_dir = Path("capture-output")
    target_stems = ["25240-3619", "25240-3751", "25240-3904", "25240-4066", "25240-4155", "25240-4316"]
    
    for stem in target_stems:
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
        
        diff = np.abs(raw0[..., :3] - raw1[..., :3])
        motion_err = np.sum(diff, axis=-1)
        chroma_err = (np.abs(diff[..., 0] - diff[..., 1]) +
                      np.abs(diff[..., 1] - diff[..., 2]) +
                      np.abs(diff[..., 2] - diff[..., 0]))
        
        flags = data[8]
        qv0 = flags[..., 0] > 0.5
        qv1 = flags[..., 1] > 0.5
        qv8 = flags[..., 3] > 0.5
        
        conflict = (motion_err > 0.35) & (chroma_err > 0.25) & qv0 & qv1
        
        print(f"\nCapture {stem}:")
        print(f"  Conflict pixels: {np.sum(conflict)}")
        print(f"  qv8 (after shadow veto) pixels: {np.sum(qv8)}")
        
        # Distribution of w1 on qv8 pixels
        w1_qv8 = w1[qv8]
        w0_qv8 = w0[qv8]
        print(f"  On qv8 pixels:")
        print(f"    w1: min={np.min(w1_qv8):.3f}, 10%={np.percentile(w1_qv8, 10):.3f}, median={np.median(w1_qv8):.3f}, 90%={np.percentile(w1_qv8, 90):.3f}, max={np.max(w1_qv8):.3f}")
        print(f"    w0: min={np.min(w0_qv8):.3f}, 10%={np.percentile(w0_qv8, 10):.3f}, median={np.median(w0_qv8):.3f}, 90%={np.percentile(w0_qv8, 90):.3f}, max={np.max(w0_qv8):.3f}")
        
        # How many qv8 pixels have w1 >= 0.40?
        print(f"    w1 >= 0.40: {np.sum(w1_qv8 >= 0.40)} / {len(w1_qv8)} ({np.sum(w1_qv8 >= 0.40)/len(w1_qv8)*100:.1f}%)")
        print(f"    w1 >= 0.20: {np.sum(w1_qv8 >= 0.20)} / {len(w1_qv8)} ({np.sum(w1_qv8 >= 0.20)/len(w1_qv8)*100:.1f}%)")
        print(f"    w1 < 0.10:  {np.sum(w1_qv8 < 0.10)} / {len(w1_qv8)} ({np.sum(w1_qv8 < 0.10)/len(w1_qv8)*100:.1f}%)")

if __name__ == "__main__":
    inspect_confidence()
