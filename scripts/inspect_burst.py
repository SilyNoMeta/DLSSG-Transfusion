from pathlib import Path
import numpy as np
import cv2

def inspect_burst():
    disps = [2989, 2990, 2991, 2992, 2993, 2994]
    out_dir = Path("capture-review/burst29")
    out_dir.mkdir(exist_ok=True)
    
    for i, d in enumerate(disps):
        f = Path(f"capture-output/26816-{d}.rgba32f")
        data = np.fromfile(f, dtype=np.float32).reshape((9, 512, 512, 4))
        raw0 = np.clip(data[0, ..., :3], 0, 1)[..., ::-1]
        raw1 = np.clip(data[1, ..., :3], 0, 1)[..., ::-1]
        cor0 = np.clip(data[6, ..., :3], 0, 1)[..., ::-1]
        cor1 = np.clip(data[7, ..., :3], 0, 1)[..., ::-1]
        w0 = data[4, ..., 0]
        w1 = data[4, ..., 1]
        
        # Crop to the siren bar (y=80..230, x=30..480)
        c_raw0 = (raw0[80:230, 30:480] * 255).astype(np.uint8)
        c_raw1 = (raw1[80:230, 30:480] * 255).astype(np.uint8)
        c_cor0 = (cor0[80:230, 30:480] * 255).astype(np.uint8)
        c_cor1 = (cor1[80:230, 30:480] * 255).astype(np.uint8)
        
        # Simulated blend: 0.5 * cor0 + 0.5 * cor1
        blend = ((cor0 * 0.5 + cor1 * 0.5)[80:230, 30:480] * 255).astype(np.uint8)
        
        # Row: raw0 | raw1 | cor0 | cor1 | blend
        row = np.hstack([c_raw0, c_raw1, c_cor0, c_cor1, blend])
        cv2.imwrite(str(out_dir / f"subframe_{i}_disp_{d}.png"), row)
        print(f"Subframe {i} (disp {d}): mean w0={np.mean(w0):.3f}, mean w1={np.mean(w1):.3f}")

if __name__ == "__main__":
    inspect_burst()
