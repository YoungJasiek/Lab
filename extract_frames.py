import os
import subprocess

videos = [
    ('cs-source.mp4', 'cs_source'),
    ('garrys-mod.mp4', 'garrys_mod'),
    ('hl2.mp4', 'hl2')
]

base_out = os.path.join('insp', 'fl', 'menu', 'extracted')
os.makedirs(base_out, exist_ok=True)

for vid_name, folder_name in videos:
    vid_path = os.path.join('insp', 'fl', 'menu', vid_name)
    out_dir = os.path.join(base_out, folder_name)
    os.makedirs(out_dir, exist_ok=True)
    
    print(f"Extracting frames from {vid_name} to {out_dir}...")
    # Extract 1 frame every 2.5 seconds, 1280x720 JPG (high quality q:v 3)
    cmd = [
        'ffmpeg', '-y', '-i', vid_path,
        '-vf', 'fps=0.4,scale=1280:720',
        '-q:v', '3',
        os.path.join(out_dir, 'frame_%03d.jpg')
    ]
    res = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if res.returncode == 0:
        count = len([f for f in os.listdir(out_dir) if f.endswith('.jpg')])
        print(f"  -> Extracted {count} frames for {folder_name}")
    else:
        print(f"  -> Error extracting {vid_name}: {res.stderr.decode('utf-8', errors='ignore')[:200]}")

print("Extraction complete!")
