from pathlib import Path
import numpy as np

raw_file = Path("capture-output/26816-2989.rgba32f")
data = np.fromfile(raw_file, dtype=np.float32).reshape((9, 512, 512, 4))
flags = data[8]

qv0 = flags[..., 0] > 0.5
qv1 = flags[..., 1] > 0.5
qv4 = flags[..., 2] > 0.5
qv8 = flags[..., 3] > 0.5

print(f"Total pixels: {512*512}")
print(f"qv0: {np.sum(qv0)} ({np.sum(qv0)/(512*512)*100:.1f}%)")
print(f"qv1: {np.sum(qv1)} ({np.sum(qv1)/(512*512)*100:.1f}%)")
print(f"qv4: {np.sum(qv4)} ({np.sum(qv4)/(512*512)*100:.1f}%)")
print(f"qv8: {np.sum(qv8)} ({np.sum(qv8)/(512*512)*100:.1f}%)")

# Let's check the values of flags on the uncollapsed pixels from before
raw0 = data[0]
raw1 = data[1]
is_red0 = (raw0[..., 0] > 0.4) & (raw0[..., 1] < 0.2) & (raw0[..., 2] < 0.2)
not_red1 = (raw1[..., 0] < 0.4) | (raw1[..., 1] > 0.3)
uncollapsed = is_red0 & not_red1 & (~qv8)

print("\nOn uncollapsed pixels (total:", np.sum(uncollapsed), "):")
print("flags[..., 0] (qv0) mean:", np.mean(flags[uncollapsed, 0]))
print("flags[..., 1] (qv1) mean:", np.mean(flags[uncollapsed, 1]))
print("flags[..., 2] (qv4) mean:", np.mean(flags[uncollapsed, 2]))
print("flags[..., 3] (qv8) mean:", np.mean(flags[uncollapsed, 3]))

# Where does qv1 == 0 in that region?
bad_qv1 = uncollapsed & (~qv1)
print(f"Number of uncollapsed where qv1 == 0: {np.sum(bad_qv1)}")
# Print sample coordinates of bad_qv1
coords = np.argwhere(bad_qv1)
for i in range(min(5, len(coords))):
    y, x = coords[i]
    print(f"Pixel ({x}, {y}):")
    print(f"  raw0 RGB: {raw0[y, x, :3]}")
    print(f"  raw1 RGB: {raw1[y, x, :3]}")
    print(f"  weights:  {data[4, y, x]}")
    print(f"  uv:       {data[5, y, x]}")
    print(f"  flags:    {flags[y, x]}")
