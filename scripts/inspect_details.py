import json
from pathlib import Path
import numpy as np
import cv2

def inspect_details():
    cap_dir = Path("capture-output")
    out_dir = Path("capture-review")
    
    # Inspect a few key captures: 3904, 4066, 4155, 4316, 4317
    target_stems = ["25240-3904", "25240-4066", "25240-4155", "25240-4316", "25240-4317"]
    
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
        ref0 = data[2]
        ref1 = data[3]
        weights = data[4]
        uv = data[5]
        cor0 = data[6]
        cor1 = data[7]
        flags = data[8]
        
        w0 = weights[..., 0]
        w1 = weights[..., 1]
        
        qv8 = flags[..., 3] > 0.5
        c0_wins = (w0 >= 0.50) & (w1 <= 0.35) & qv8
        c1_wins = (w1 >= 0.50) & (w0 <= 0.35) & qv8
        
        # Save a diagnostic map:
        # Red = C0->C1 (qv14, C0 overwrote C1)
        # Blue = C1->C0 (qv13, C1 overwrote C0)
        # Green = qv8 triggered but neither met confidence threshold
        diag = np.zeros((side, side, 3), dtype=np.uint8)
        diag[qv8] = [0, 100, 0]
        diag[c1_wins] = [255, 0, 0] # Blue in BGR
        diag[c0_wins] = [0, 0, 255] # Red in BGR
        
        stem_out = out_dir / stem
        stem_out.mkdir(exist_ok=True)
        cv2.imwrite(str(stem_out / "diag_reconciliation_map.png"), diag)
        
        # Also let's inspect what colors raw0 and raw1 have at c0_wins pixels:
        if np.any(c0_wins):
            c0_raw0 = raw0[c0_wins, :3]
            c0_raw1 = raw1[c0_wins, :3]
            print(f"\n{stem} - c0_wins ({np.sum(c0_wins)} pixels):")
            print(f"  raw0 (past) mean RGB: {np.mean(c0_raw0, axis=0)}")
            print(f"  raw1 (future) mean RGB: {np.mean(c0_raw1, axis=0)}")
            print(f"  mean w0: {np.mean(w0[c0_wins]):.3f}, mean w1: {np.mean(w1[c0_wins]):.3f}")
            
        if np.any(c1_wins):
            c1_raw0 = raw0[c1_wins, :3]
            c1_raw1 = raw1[c1_wins, :3]
            print(f"\n{stem} - c1_wins ({np.sum(c1_wins)} pixels):")
            print(f"  raw0 (past) mean RGB: {np.mean(c1_raw0, axis=0)}")
            print(f"  raw1 (future) mean RGB: {np.mean(c1_raw1, axis=0)}")
            print(f"  mean w0: {np.mean(w0[c1_wins]):.3f}, mean w1: {np.mean(w1[c1_wins]):.3f}")

if __name__ == "__main__":
    inspect_details()
