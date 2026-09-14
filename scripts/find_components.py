from pathlib import Path
import numpy as np
import cv2

raw_file = Path("capture-output/26816-2989.rgba32f")
data = np.fromfile(raw_file, dtype=np.float32).reshape((9, 512, 512, 4))
raw0 = data[0]
raw1 = data[1]

# Let's find connected components of red in raw0 and raw1
def get_red_mask(arr):
    # Red is high R, low G and B
    return ((arr[..., 0] > 0.4) & (arr[..., 1] < 0.25) & (arr[..., 2] < 0.25)).astype(np.uint8)

def get_blue_mask(arr):
    return ((arr[..., 2] > 0.4) & (arr[..., 0] < 0.25) & (arr[..., 1] < 0.35)).astype(np.uint8)

mask_r0 = get_red_mask(raw0)
mask_r1 = get_red_mask(raw1)
mask_b0 = get_blue_mask(raw0)
mask_b1 = get_blue_mask(raw1)

num_r0, labels_r0, stats_r0, cents_r0 = cv2.connectedComponentsWithStats(mask_r0)
num_r1, labels_r1, stats_r1, cents_r1 = cv2.connectedComponentsWithStats(mask_r1)

print("Red components in raw0:")
for i in range(1, num_r0):
    if stats_r0[i, cv2.CC_STAT_AREA] > 50:
        print(f"  Component {i}: area={stats_r0[i, cv2.CC_STAT_AREA]}, x={stats_r0[i, cv2.CC_STAT_LEFT]}..{stats_r0[i, cv2.CC_STAT_LEFT]+stats_r0[i, cv2.CC_STAT_WIDTH]}, y={stats_r0[i, cv2.CC_STAT_TOP]}..{stats_r0[i, cv2.CC_STAT_TOP]+stats_r0[i, cv2.CC_STAT_HEIGHT]}, centroid={cents_r0[i]}")

print("\nRed components in raw1:")
for i in range(1, num_r1):
    if stats_r1[i, cv2.CC_STAT_AREA] > 50:
        print(f"  Component {i}: area={stats_r1[i, cv2.CC_STAT_AREA]}, x={stats_r1[i, cv2.CC_STAT_LEFT]}..{stats_r1[i, cv2.CC_STAT_LEFT]+stats_r1[i, cv2.CC_STAT_WIDTH]}, y={stats_r1[i, cv2.CC_STAT_TOP]}..{stats_r1[i, cv2.CC_STAT_TOP]+stats_r1[i, cv2.CC_STAT_HEIGHT]}, centroid={cents_r1[i]}")
