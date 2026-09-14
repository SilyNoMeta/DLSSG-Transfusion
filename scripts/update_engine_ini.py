import re
from pathlib import Path

ini_path = Path(r"C:\Users\TonyJoaca\AppData\Local\HT\Saved_GlobalSteam\Config\Windows\Engine.ini")

if not ini_path.exists():
    print(f"Error: {ini_path} does not exist!")
    exit(1)

content = ini_path.read_text(encoding="utf-8", errors="ignore")

# Define replacements
replacements = [
    (r"r\.Streamline\.DilateMotionVectors\s*=\s*0", "r.Streamline.DilateMotionVectors=1"),
    (r"r\.NGX\.DLSS\.DilateMotionVectors\s*=\s*0", "r.NGX.DLSS.DilateMotionVectors=1"),
    (r"r\.MotionBlur\.Amount\s*=\s*[0-9\.]+", "r.MotionBlur.Amount=0"),
    (r"r\.MotionBlur\.Max\s*=\s*[0-9\.]+", "r.MotionBlur.Max=0"),
    (r"r\.MotionBlurQuality\s*=\s*[0-9]+", "r.MotionBlurQuality=0"),
    (r"r\.MotionBlur\.Separate\s*=\s*[0-9]+", "r.MotionBlur.Separate=0"),
    (r"r\.MotionBlur\.Scale\s*=\s*[0-9\.]+", "r.MotionBlur.Scale=0"),
    (r"r\.VRS\.Translucency\s*=\s*[0-9]+", "r.VRS.Translucency=0"),
    (r"r\.Tonemapper\.Sharpen\s*=\s*[0-9\.]+", "r.Tonemapper.Sharpen=0.5"),
]

updated = content
for pattern, repl in replacements:
    matches = len(re.findall(pattern, updated))
    updated, count = re.subn(pattern, repl, updated)
    print(f"Replaced {count} instances of {pattern} -> {repl}")

ini_path.write_text(updated, encoding="utf-8")
print("\nSuccessfully updated Engine.ini!")
