"""Parse and visualize GPU candidate capture files from capture-output/.

Extracts all 9 captured planes (raw0, raw1, reference0, reference1, weights, uv01,
corrected0, corrected1, flags) into side-by-side PNGs for immediate inspection.
"""
import json
from pathlib import Path
import numpy as np
import cv2
import sys

def process_capture(capture_dir="capture-output", out_dir="capture-review"):
    cap_path = Path(capture_dir)
    out_path = Path(out_dir)
    out_path.mkdir(parents=True, exist_ok=True)
    
    json_files = sorted(cap_path.glob("*.json"))
    if not json_files:
        print(f"No capture files found in {capture_dir}")
        return
    
    print(f"Found {len(json_files)} captures in {capture_dir}")
    for jf in json_files:
        stem = jf.stem
        raw_file = cap_path / f"{stem}.rgba32f"
        if not raw_file.exists():
            continue
        
        meta = json.loads(jf.read_text())
        side = meta.get("side", 256)
        planes = meta.get("planes", 9)
        plane_names = meta.get("plane_names", [f"plane_{i}" for i in range(planes)])
        
        data = np.fromfile(raw_file, dtype=np.float32)
        expected_floats = side * side * planes * 4
        if len(data) != expected_floats:
            print(f"Skipping {stem}: size mismatch {len(data)} != {expected_floats}")
            continue
        
        # Reshape: (planes, side, side, 4)
        atlas = data.reshape((planes, side, side, 4))
        
        stem_out = out_path / stem
        stem_out.mkdir(exist_ok=True)
        
        print(f"Exporting capture {stem} (origin: {meta.get('origin')})...")
        for p_idx, name in enumerate(plane_names):
            plane_data = atlas[p_idx] # (256, 256, 4)
            # Clip RGB to [0, 1] for visualization
            rgb = np.clip(plane_data[..., :3], 0.0, 1.0)
            # Convert to BGR uint8
            bgr = (rgb[..., ::-1] * 255.0).astype(np.uint8)
            cv2.imwrite(str(stem_out / f"{p_idx:02d}_{name}.png"), bgr)
            
            # For flags plane (plane 8), save individual predicate masks:
            if "flags" in name:
                v0 = (plane_data[..., 0] * 255.0).clip(0, 255).astype(np.uint8)
                v1 = (plane_data[..., 1] * 255.0).clip(0, 255).astype(np.uint8)
                conflict = (plane_data[..., 2] * 255.0).clip(0, 255).astype(np.uint8)
                copied = (plane_data[..., 3] * 255.0).clip(0, 255).astype(np.uint8)
                cv2.imwrite(str(stem_out / "flag_valid0.png"), v0)
                cv2.imwrite(str(stem_out / "flag_valid1.png"), v1)
                cv2.imwrite(str(stem_out / "flag_chroma_conflict.png"), conflict)
                cv2.imwrite(str(stem_out / "flag_copy_executed.png"), copied)

    print(f"Done! All captures exported to {out_dir}/")

if __name__ == "__main__":
    c_dir = sys.argv[1] if len(sys.argv) > 1 else "capture-output"
    o_dir = sys.argv[2] if len(sys.argv) > 2 else "capture-review"
    process_capture(c_dir, o_dir)
