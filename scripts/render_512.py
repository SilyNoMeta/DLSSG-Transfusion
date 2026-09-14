import json
from pathlib import Path
import numpy as np
import cv2

def render_512():
    cap_dir = Path("capture-output")
    out_dir = Path("capture-review")
    
    # Check dispatches: 2989, 3099, 3101, 3231, 3410
    sample_stems = ["26816-2989", "26816-2992", "26816-3099", "26816-3101", "26816-3231", "26816-3410"]
    
    for stem in sample_stems:
        jf = cap_dir / f"{stem}.json"
        raw_file = cap_dir / f"{stem}.rgba32f"
        if not raw_file.exists():
            continue
        meta = json.loads(jf.read_text())
        side = meta.get("side", 512)
        planes = meta.get("planes", 9)
        data = np.fromfile(raw_file, dtype=np.float32).reshape((planes, side, side, 4))
        
        raw0 = np.clip(data[0, ..., :3], 0, 1)[..., ::-1] # BGR
        raw1 = np.clip(data[1, ..., :3], 0, 1)[..., ::-1]
        cor0 = np.clip(data[6, ..., :3], 0, 1)[..., ::-1]
        cor1 = np.clip(data[7, ..., :3], 0, 1)[..., ::-1]
        flags = data[8]
        qv8 = flags[..., 3] > 0.5
        
        # Color difference between raw0 and raw1
        diff = np.abs(data[0, ..., :3] - data[1, ..., :3])
        diff_vis = np.clip(diff * 3.0, 0, 1)[..., ::-1] # boosted difference
        
        # Copy mask visual: green where qv8 fired
        copy_vis = (cor0 * 255).astype(np.uint8)
        copy_vis[qv8] = [0, 255, 0]
        
        # Save side-by-side row:
        # raw0 | raw1 | diff | cor0 | cor1 | copy_mask
        row = np.hstack([
            (raw0 * 255).astype(np.uint8),
            (raw1 * 255).astype(np.uint8),
            (diff_vis * 255).astype(np.uint8),
            (cor0 * 255).astype(np.uint8),
            (cor1 * 255).astype(np.uint8),
            copy_vis
        ])
        
        cv2.imwrite(str(out_dir / f"compare512_{stem}.png"), row)
        print(f"Saved compare512_{stem}.png, qv8 pixels = {np.sum(qv8)} ({np.sum(qv8)/(side*side)*100:.1f}%)")

if __name__ == "__main__":
    render_512()
