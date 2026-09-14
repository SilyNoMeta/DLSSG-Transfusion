from pathlib import Path
import numpy as np

raw_file = Path("capture-output/26816-2989.rgba32f")
data = np.fromfile(raw_file, dtype=np.float32).reshape((9, 512, 512, 4))
raw0 = data[0]
raw1 = data[1]

# In panel 4/5, the red dome is around y=100..200, x=50..200
# Let's crop to the red siren:
siren_raw0 = raw0[100:200, 50:200, :3]
siren_raw1 = raw1[100:200, 50:200, :3]

diff = np.abs(siren_raw0 - siren_raw1)
motion_err = np.sum(diff, axis=-1)
chroma_err = (np.abs(diff[..., 0] - diff[..., 1]) +
              np.abs(diff[..., 1] - diff[..., 2]) +
              np.abs(diff[..., 2] - diff[..., 0]))

# Check where both raw0 and raw1 are red:
both_red = (siren_raw0[..., 0] > 0.4) & (siren_raw1[..., 0] > 0.4)
print(f"Pixels where BOTH raw0 and raw1 are red: {np.sum(both_red)}")

# On pixels where BOTH are red:
print(f"Motion error on overlapping red region: min={np.min(motion_err[both_red]):.3f}, median={np.median(motion_err[both_red]):.3f}, max={np.max(motion_err[both_red]):.3f}")
print(f"Chroma error on overlapping red region: min={np.min(chroma_err[both_red]):.3f}, median={np.median(chroma_err[both_red]):.3f}, max={np.max(chroma_err[both_red]):.3f}")
print(f"Motion error > 0.35: {np.sum(motion_err[both_red] > 0.35)} / {np.sum(both_red)}")
print(f"Chroma error > 0.25: {np.sum(chroma_err[both_red] > 0.25)} / {np.sum(both_red)}")

# Where raw0 is red but raw1 is NOT red:
raw0_only_red = (siren_raw0[..., 0] > 0.4) & (siren_raw1[..., 0] <= 0.4)
print(f"\nPixels where raw0 is red but raw1 is NOT red: {np.sum(raw0_only_red)}")
print(f"Motion error: min={np.min(motion_err[raw0_only_red]):.3f}, median={np.median(motion_err[raw0_only_red]):.3f}, max={np.max(motion_err[raw0_only_red]):.3f}")
print(f"Chroma error: min={np.min(chroma_err[raw0_only_red]):.3f}, median={np.median(chroma_err[raw0_only_red]):.3f}, max={np.max(chroma_err[raw0_only_red]):.3f}")
print(f"Motion error > 0.35: {np.sum(motion_err[raw0_only_red] > 0.35)} / {np.sum(raw0_only_red)}")
print(f"Chroma error > 0.25: {np.sum(chroma_err[raw0_only_red] > 0.25)} / {np.sum(raw0_only_red)}")

# Why did some raw0_only_red NOT have chroma > 0.25 or motion > 0.35?
low_chroma = raw0_only_red & (chroma_err <= 0.25)
print(f"raw0_only_red with chroma <= 0.25: {np.sum(low_chroma)}")
if np.any(low_chroma):
    idx = np.argwhere(low_chroma)
    for i in range(min(5, len(idx))):
        y, x = idx[i]
        print(f"  Pixel: raw0={siren_raw0[y,x]}, raw1={siren_raw1[y,x]}, diff={diff[y,x]}, chroma_err={chroma_err[y,x]:.3f}, motion_err={motion_err[y,x]:.3f}")
