from pathlib import Path
import numpy as np

def inspect_mvecs():
    f = Path("capture-output/26816-2989.rgba32f")
    data = np.fromfile(f, dtype=np.float32).reshape((9, 512, 512, 4))
    
    uv = data[5]
    u0, v0 = uv[..., 0], uv[..., 1]
    u1, v1 = uv[..., 2], uv[..., 3]
    
    # Calculate screen UV grid:
    # origin is [1024, 464], width=2560, height=1440
    xs = np.arange(1024, 1024 + 512)
    ys = np.arange(464, 464 + 512)
    grid_x, grid_y = np.meshgrid(xs, ys)
    screen_u = (grid_x + 0.5) / 2560.0
    screen_v = (grid_y + 0.5) / 1440.0
    
    # Motion vectors in pixels:
    flow0_x = (u0 - screen_u) * 2560.0
    flow0_y = (v0 - screen_v) * 1440.0
    flow1_x = (u1 - screen_u) * 2560.0
    flow1_y = (v1 - screen_v) * 1440.0
    
    # On the car body (x=200..300, y=300..400):
    body_f0_x = flow0_x[300:400, 200:300]
    body_f0_y = flow0_y[300:400, 200:300]
    body_f1_x = flow1_x[300:400, 200:300]
    body_f1_y = flow1_y[300:400, 200:300]
    print(f"Car body flow0: x={np.median(body_f0_x):.2f}, y={np.median(body_f0_y):.2f}")
    print(f"Car body flow1: x={np.median(body_f1_x):.2f}, y={np.median(body_f1_y):.2f}")
    
    # On the red siren (x=50..150, y=100..200):
    siren_f0_x = flow0_x[100:200, 50:150]
    siren_f0_y = flow0_y[100:200, 50:150]
    siren_f1_x = flow1_x[100:200, 50:150]
    siren_f1_y = flow1_y[100:200, 50:150]
    print(f"Red siren flow0: x={np.median(siren_f0_x):.2f}, y={np.median(siren_f0_y):.2f}")
    print(f"Red siren flow1: x={np.median(siren_f1_x):.2f}, y={np.median(siren_f1_y):.2f}")
    
    # On the background hedge (x=50..150, y=20..70):
    hedge_f0_x = flow0_x[20:70, 50:150]
    hedge_f0_y = flow0_y[20:70, 50:150]
    hedge_f1_x = flow1_x[20:70, 50:150]
    hedge_f1_y = flow1_y[20:70, 50:150]
    print(f"Hedge flow0: x={np.median(hedge_f0_x):.2f}, y={np.median(hedge_f0_y):.2f}")
    print(f"Hedge flow1: x={np.median(hedge_f1_x):.2f}, y={np.median(hedge_f1_y):.2f}")

if __name__ == "__main__":
    inspect_mvecs()
