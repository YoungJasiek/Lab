import subprocess
import json
import os

files = ['cs-source.mp4', 'garrys-mod.mp4', 'hl2.mp4']
for name in files:
    path = os.path.join('insp', 'fl', 'menu', name)
    if not os.path.exists(path):
        print(f"File not found: {path}")
        continue
    cmd = [
        'ffprobe', '-v', 'error',
        '-show_entries', 'format=duration,size:stream=width,height,r_frame_rate',
        '-of', 'json', path
    ]
    res = subprocess.run(cmd, capture_output=True, text=True)
    try:
        data = json.loads(res.stdout)
        dur = float(data['format']['duration'])
        size_mb = float(data['format']['size']) / (1024 * 1024)
        stream = data['streams'][0]
        w = stream.get('width')
        h = stream.get('height')
        fps = stream.get('r_frame_rate')
        print(f"{name}: {dur:.1f}s, {size_mb:.2f} MB, {w}x{h} @ {fps}")
    except Exception as e:
        print(f"{name}: error {e}")
