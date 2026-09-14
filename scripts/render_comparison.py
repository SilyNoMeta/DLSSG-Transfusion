import json
from pathlib import Path
import numpy as np
import cv2

def render_comparison():
    cap_dir = Path("capture-output")
    out_dir = Path("capture-review")
    
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
        
        raw0 = np.clip(data[0, ..., :3], 0, 1)[..., ::-1] # BGR
        raw1 = np.clip(data[1, ..., :3], 0, 1)[..., ::-1]
        cor0 = np.clip(data[6, ..., :3], 0, 1)[..., ::-1]
        cor1 = np.clip(data[7, ..., :3], 0, 1)[..., ::-1]
        
        flags = data[8]
        qv8 = flags[..., 3] > 0.5
        w0 = data[4, ..., 0]
        w1 = data[4, ..., 1]
        
        c0_wins = (w0 >= 0.50) & (w1 <= 0.35) & qv8
        c1_wins = (w1 >= 0.50) & (w0 <= 0.35) & qv8
        
        # Overlay reconciliation on raw1 to see what happened to Candidate 1:
        cor1_marked = (cor1 * 255).astype(np.uint8)
        # Highlight in magenta where C0 overwrote C1
        cor1_marked[c0_wins] = [255, 0, 255] # Magenta
        
        # Combine side-by-side: raw0 | raw1 | cor0 | cor1 | cor1_marked
        row = np.hstack([
            (raw0 * 255).astype(np.uint8),
            (raw1 * 255).astype(np.uint8),
            (cor0 * 255).astype(np.uint8),
            (cor1 * 255).astype(np.uint8),
            cor1_marked
        ])
        
        cv2.imwrite(str(out_dir / f"compare_{stem}.png"), row)
        print(f"Saved compare_{stem}.png: raw0 | raw1 | cor0 | cor1 | cor1_marked")

if __name__ == "__main__":
    render_comparison()
