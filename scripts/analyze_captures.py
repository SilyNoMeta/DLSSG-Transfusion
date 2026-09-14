import json
from pathlib import Path
import numpy as np

def analyze():
    cap_dir = Path("capture-output")
    json_files = sorted(cap_dir.glob("*.json"))
    print(f"Total capture files: {len(json_files)}")
    
    bursts = {}
    for jf in json_files:
        stem = jf.stem
        raw_file = cap_dir / f"{stem}.rgba32f"
        if not raw_file.exists():
            continue
        meta = json.loads(jf.read_text())
        disp = meta["dispatch"]
        # Group into bursts (consecutive dispatches)
        burst_id = disp // 100
        bursts.setdefault(burst_id, []).append((disp, jf, raw_file, meta))
        
    print(f"Found {len(bursts)} bursts:")
    for b_id, items in bursts.items():
        disps = [x[0] for x in items]
        print(f"  Burst {b_id}: {len(items)} captures: {disps}")

    # Now let's analyze the properties of each capture
    for b_id, items in bursts.items():
        print(f"\n================ BURST {b_id} ================")
        for idx, (disp, jf, raw_file, meta) in enumerate(items):
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
            
            qv0 = flags[..., 0] > 0.5
            qv1 = flags[..., 1] > 0.5
            qv4 = flags[..., 2] > 0.5 # chroma conflict
            qv8 = flags[..., 3] > 0.5 # copy executed
            
            # Count pixels
            tot = side * side
            n_qv0 = np.sum(qv0)
            n_qv1 = np.sum(qv1)
            n_qv4 = np.sum(qv4)
            n_qv8 = np.sum(qv8)
            
            # Mean weights
            mean_w0 = np.mean(w0)
            mean_w1 = np.mean(w1)
            
            # Check where w0 > 0.5 and w1 <= 0.35 (C0 wins)
            c0_wins = (w0 >= 0.50) & (w1 <= 0.35) & qv8
            # Check where w1 > 0.5 and w0 <= 0.35 (C1 wins)
            c1_wins = (w1 >= 0.50) & (w0 <= 0.35) & qv8
            
            # Color difference between raw0 and raw1
            diff = np.abs(raw0[..., :3] - raw1[..., :3])
            motion_err = np.sum(diff, axis=-1)
            chroma_err = (np.abs(diff[..., 0] - diff[..., 1]) +
                          np.abs(diff[..., 1] - diff[..., 2]) +
                          np.abs(diff[..., 2] - diff[..., 0]))
            
            # High chroma conflict pixels
            high_chroma = (chroma_err > 0.25) & (motion_err > 0.35)
            
            print(f"Subframe {idx} (disp {disp}): mean w0={mean_w0:.3f}, mean w1={mean_w1:.3f} | "
                  f"qv4(conflict)={n_qv4} ({n_qv4/tot*100:.1f}%), qv8={n_qv8} ({n_qv8/tot*100:.1f}%) | "
                  f"C1->C0(qv13)={np.sum(c1_wins)}, C0->C1(qv14)={np.sum(c0_wins)} | "
                  f"high_chroma={np.sum(high_chroma)}")

if __name__ == "__main__":
    analyze()
