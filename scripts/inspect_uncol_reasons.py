from pathlib import Path
import numpy as np

raw_file = Path("capture-output/26816-2989.rgba32f")
data = np.fromfile(raw_file, dtype=np.float32).reshape((9, 512, 512, 4))
raw0 = data[0]
raw1 = data[1]

is_red0 = (raw0[..., 0] > 0.4) & (raw0[..., 1] < 0.2) & (raw0[..., 2] < 0.2)
not_red1 = (raw1[..., 0] < 0.4) | (raw1[..., 1] > 0.3)
ghost_red = is_red0 & not_red1

is_blue0 = (raw0[..., 2] > 0.4) & (raw0[..., 0] < 0.25)
not_blue1 = (raw1[..., 2] < 0.4) | (raw1[..., 0] > 0.3)
ghost_blue = is_blue0 & not_blue1

ghost = ghost_red | ghost_blue

diff = np.abs(raw0[..., :3] - raw1[..., :3])
motion_err = np.sum(diff, axis=-1)
chroma_err = (np.abs(diff[..., 0] - diff[..., 1]) +
              np.abs(diff[..., 1] - diff[..., 2]) +
              np.abs(diff[..., 2] - diff[..., 0]))

# Shadow veto
l1_raw0 = np.sum(np.abs(raw0[..., :3]), axis=-1)
l1_raw1 = np.sum(np.abs(raw1[..., :3]), axis=-1)
sig = (l1_raw0 > 0.01) & (l1_raw1 > 0.01)
n0 = raw0[..., :3] / np.maximum(l1_raw0[..., None], 0.01)
n1 = raw1[..., :3] / np.maximum(l1_raw1[..., None], 0.01)
dot01 = np.sum(n0 * n1, axis=-1)
dot00 = np.sum(n0 * n0, axis=-1)
dot11 = np.sum(n1 * n1, axis=-1)
norm_sq = dot00 * dot11
cos_sq = dot01 * dot01
res = norm_sq - cos_sq
is_shadow = sig & (dot01 > 0) & (res <= 0.01 * norm_sq)

uv = data[5]
W, H = 2560, 1440
qf0, qf1 = 0.5 / W, 0.5 / H
qf2, qf3 = 1.0 - qf0, 1.0 - qf1
u0, v0 = uv[..., 0], uv[..., 1]
u1, v1 = uv[..., 2], uv[..., 3]
b0 = (u0 >= qf0) & (u0 <= qf2) & (v0 >= qf1) & (v0 <= qf3)
b1 = (u1 >= qf0) & (u1 <= qf2) & (v1 >= qf1) & (v1 <= qf3)
f0 = np.isfinite(l1_raw0) & (l1_raw0 < 1e30)
f1 = np.isfinite(l1_raw1) & (l1_raw1 < 1e30)
pure_v3 = b0 & b1 & f0 & f1

conflict = (motion_err > 0.25) & (chroma_err > 0.15) & pure_v3
new_qv8 = conflict & (~is_shadow)

uncol = ghost & (~new_qv8)

print(f"Total uncollapsed ghost pixels in 2989: {np.sum(uncol)}")
# Break down why uncol didn't trigger:
print(f"  Failed motion_err > 0.25: {np.sum(uncol & (motion_err <= 0.25))}")
print(f"  Failed chroma_err > 0.15: {np.sum(uncol & (chroma_err <= 0.15))}")
print(f"  Failed pure_v3: {np.sum(uncol & (~pure_v3))}")
print(f"  Vetoed by is_shadow: {np.sum(uncol & is_shadow)}")

# Sample a few that failed motion or chroma
failed_mc = uncol & ((motion_err <= 0.25) | (chroma_err <= 0.15))
idx = np.argwhere(failed_mc)
for i in range(min(5, len(idx))):
    y, x = idx[i]
    print(f"  y={y}, x={x}: raw0={raw0[y,x,:3]}, raw1={raw1[y,x,:3]}, motion={motion_err[y,x]:.3f}, chroma={chroma_err[y,x]:.3f}, res/norm={res[y,x]/max(1e-6,norm_sq[y,x]):.4f}")
