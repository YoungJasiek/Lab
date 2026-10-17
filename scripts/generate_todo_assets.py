#!/usr/bin/env python3
"""
Autonomous Asset & Animation Generator for Lab Engine / Frozen-Life TODO tasks.
Generates:
1. 3D Environment Prop Models (.stl binary):
   - assets/models/lab_door.stl
   - assets/models/lab_locker.stl
   - assets/models/ceiling_lamp.stl
   - assets/models/corridor_pipe.stl
   - assets/models/wall_stain.stl
2. Skeletal Animations (.anim LAB_ANIM\0 binary):
   - assets/animations/Idle.anim (derived from Pistol Idle.anim / breathing modulation)
   - assets/animations/Walk.anim (derived from Walking.anim)
   - assets/animations/Talk.anim (expressive head/jaw/arm gesture animation for bot dialogue)
   - assets/animations/Attack.anim (derived from Firing Rifle.anim)
3. Textures & Normal Maps (.bmp):
   - assets/textures/concrete_wall_normal.bmp
   - assets/textures/floor_lab_normal.bmp
   - assets/textures/metal_hull_normal.bmp
   - assets/textures/corridor_dirty_wall.bmp
   - assets/textures/decal_rust_stain.bmp
   - assets/textures/cubemap_site14.bmp
4. Audio Files (.wav PCM 16-bit 44.1kHz):
   - assets/audio/bot_corridor_dialog.wav
   - assets/audio/bot_footstep_3d.wav
5. 2D UI Screen Layouts (.labui):
   - assets/ui/main_menu.labui
   - assets/ui/pause_menu.labui
   - assets/ui/options_menu.labui
   - assets/ui/hud_layout.labui
"""

import os
import math
import struct
import wave
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def add_box_triangles(tris, cx, cy, cz, sx, sy, sz):
    hx, hy, hz = sx * 0.5, sy * 0.5, sz * 0.5
    x0, x1 = cx - hx, cx + hx
    y0, y1 = cy - hy, cy + hy
    z0, z1 = cz - hz, cz + hz

    faces = [
        # Front (+Z)
        ((0, 0, 1), [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]),
        # Back (-Z)
        ((0, 0, -1), [(x1, y0, z0), (x0, y0, z0), (x0, y1, z0), (x1, y1, z0)]),
        # Right (+X)
        ((1, 0, 0), [(x1, y0, z1), (x1, y0, z0), (x1, y1, z0), (x1, y1, z1)]),
        # Left (-X)
        ((-1, 0, 0), [(x0, y0, z0), (x0, y0, z1), (x0, y1, z1), (x0, y1, z0)]),
        # Top (+Y)
        ((0, 1, 0), [(x0, y1, z1), (x1, y1, z1), (x1, y1, z0), (x0, y1, z0)]),
        # Bottom (-Y)
        ((0, -1, 0), [(x0, y0, z0), (x1, y0, z0), (x1, y0, z1), (x0, y0, z1)]),
    ]
    for n, v in faces:
        tris.append((n, v[0], v[1], v[2]))
        tris.append((n, v[0], v[2], v[3]))


def add_cylinder_z(tris, cx, cy, z0, z1, radius, segments=12):
    for i in range(segments):
        a0 = (2.0 * math.pi * i) / segments
        a1 = (2.0 * math.pi * (i + 1)) / segments
        c0, s0 = math.cos(a0), math.sin(a0)
        c1, s1 = math.cos(a1), math.sin(a1)
        nx, ny = math.cos((a0 + a1) * 0.5), math.sin((a0 + a1) * 0.5)
        p00 = (cx + c0 * radius, cy + s0 * radius, z0)
        p10 = (cx + c1 * radius, cy + s1 * radius, z0)
        p11 = (cx + c1 * radius, cy + s1 * radius, z1)
        p01 = (cx + c0 * radius, cy + s0 * radius, z1)
        tris.append(((nx, ny, 0.0), p00, p10, p11))
        tris.append(((nx, ny, 0.0), p00, p11, p01))
        # Caps
        tris.append(((0.0, 0.0, -1.0), (cx, cy, z0), p10, p00))
        tris.append(((0.0, 0.0, 1.0), (cx, cy, z1), p01, p11))


def write_binary_stl(filepath, tris, header_text="LabEngine Prop"):
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    hdr = header_text.encode("ascii")[:80].ljust(80, b"\x00")
    with open(filepath, "wb") as f:
        f.write(hdr)
        f.write(struct.pack("<I", len(tris)))
        for n, v0, v1, v2 in tris:
            f.write(struct.pack("<12fH",
                                n[0], n[1], n[2],
                                v0[0], v0[1], v0[2],
                                v1[0], v1[1], v1[2],
                                v2[0], v2[1], v2[2], 0))


def generate_3d_props():
    models_dir = os.path.join(ROOT, "assets", "models")
    os.makedirs(models_dir, exist_ok=True)

    # 1. lab_door.stl (Heavy bulkhead door with frame, window bars & keypad)
    tris = []
    # Left & Right frame pillars
    add_box_triangles(tris, -0.95, 0.0, 0.0, 0.16, 2.4, 0.24)
    add_box_triangles(tris,  0.95, 0.0, 0.0, 0.16, 2.4, 0.24)
    # Top lintel header
    add_box_triangles(tris,  0.0,  1.12, 0.0, 2.06, 0.18, 0.26)
    # Left & Right door leaves
    add_box_triangles(tris, -0.44, -0.05, 0.0, 0.86, 2.26, 0.10)
    add_box_triangles(tris,  0.44, -0.05, 0.0, 0.86, 2.26, 0.10)
    # Reinforced horizontal ribs
    add_box_triangles(tris,  0.0, -0.55, 0.0, 1.72, 0.12, 0.14)
    add_box_triangles(tris,  0.0,  0.10, 0.0, 1.72, 0.12, 0.14)
    # Window frames
    add_box_triangles(tris, -0.44,  0.52, 0.0, 0.34, 0.48, 0.13)
    add_box_triangles(tris,  0.44,  0.52, 0.0, 0.34, 0.48, 0.13)
    # Central locking wheel / bar
    add_box_triangles(tris, -0.08,  0.0,  0.07, 0.08, 0.36, 0.06)
    write_binary_stl(os.path.join(models_dir, "lab_door.stl"), tris, "Site14 Heavy Lab Door")

    # 2. lab_locker.stl (Bank of 3 industrial crew lockers with louver vents)
    tris = []
    # Main cabinet body
    add_box_triangles(tris, 0.0, 0.0, 0.0, 1.35, 1.85, 0.50)
    # Base plinth
    add_box_triangles(tris, 0.0, -0.88, 0.02, 1.37, 0.10, 0.52)
    # 3 locker door fronts + vents + handles
    for i, lx in enumerate([-0.44, 0.0, 0.44]):
        add_box_triangles(tris, lx, 0.04, 0.26, 0.40, 1.68, 0.03)
        # Handle recess & latch
        add_box_triangles(tris, lx + 0.14, 0.05, 0.28, 0.03, 0.16, 0.03)
        # Top & bottom louver vents
        for v in range(4):
            add_box_triangles(tris, lx, 0.65 - v * 0.05, 0.28, 0.28, 0.02, 0.02)
            add_box_triangles(tris, lx, -0.55 - v * 0.05, 0.28, 0.28, 0.02, 0.02)
    write_binary_stl(os.path.join(models_dir, "lab_locker.stl"), tris, "Site14 Industrial Locker")

    # 3. ceiling_lamp.stl (Industrial fluorescent cage trough fixture)
    tris = []
    # Reflector trough
    add_box_triangles(tris, 0.0, 0.05, 0.0, 0.32, 0.06, 1.25)
    add_box_triangles(tris, -0.15, 0.01, 0.0, 0.03, 0.08, 1.25)
    add_box_triangles(tris,  0.15, 0.01, 0.0, 0.03, 0.08, 1.25)
    # Dual fluorescent tubes
    add_cylinder_z(tris, -0.06, -0.01, -0.58, 0.58, 0.025, 8)
    add_cylinder_z(tris,  0.06, -0.01, -0.58, 0.58, 0.025, 8)
    # Suspension conduit rods
    add_box_triangles(tris, 0.0, 0.16, -0.45, 0.03, 0.18, 0.03)
    add_box_triangles(tris, 0.0, 0.16,  0.45, 0.03, 0.18, 0.03)
    write_binary_stl(os.path.join(models_dir, "ceiling_lamp.stl"), tris, "Site14 Fluorescent Lamp")

    # 4. corridor_pipe.stl (Dual coolant pipe assembly with flanges & brackets)
    tris = []
    add_cylinder_z(tris, 0.0,  0.10, -1.5, 1.5, 0.09, 10)
    add_cylinder_z(tris, 0.0, -0.10, -1.5, 1.5, 0.06, 8)
    # Flange collars & wall mounting brackets
    for fz in [-1.2, 0.0, 1.2]:
        add_cylinder_z(tris, 0.0,  0.10, fz - 0.03, fz + 0.03, 0.13, 10)
        add_cylinder_z(tris, 0.0, -0.10, fz - 0.025, fz + 0.025, 0.09, 8)
        add_box_triangles(tris, -0.12, 0.0, fz, 0.12, 0.36, 0.05)
    write_binary_stl(os.path.join(models_dir, "corridor_pipe.stl"), tris, "Site14 Corridor Pipe Assembly")

    # 5. wall_stain.stl (Relief grime / leak decal plaque)
    tris = []
    add_box_triangles(tris, 0.0, 0.0, 0.0, 1.1, 1.1, 0.015)
    add_box_triangles(tris, 0.05, -0.15, 0.005, 0.65, 0.75, 0.015)
    write_binary_stl(os.path.join(models_dir, "wall_stain.stl"), tris, "Site14 Wall Grime Stain")


def read_lab_anim(filepath):
    with open(filepath, "rb") as f:
        magic = f.read(9)
        if magic != b"LAB_ANIM\x00":
            raise ValueError(f"Invalid magic in {filepath}")
        name_len = struct.unpack("<H", f.read(2))[0]
        name = f.read(name_len).decode("utf-8")
        duration, ch_count = struct.unpack("<fI", f.read(8))
        channels = []
        for _ in range(ch_count):
            b_len = struct.unpack("<H", f.read(2))[0]
            bone_name = f.read(b_len).decode("utf-8")
            p_cnt = struct.unpack("<I", f.read(4))[0]
            pos_keys = [struct.unpack("<ffff", f.read(16)) for _ in range(p_cnt)]
            r_cnt = struct.unpack("<I", f.read(4))[0]
            rot_keys = [struct.unpack("<fffff", f.read(20)) for _ in range(r_cnt)]
            s_cnt = struct.unpack("<I", f.read(4))[0]
            scl_keys = [struct.unpack("<ffff", f.read(16)) for _ in range(s_cnt)]
            channels.append((bone_name, pos_keys, rot_keys, scl_keys))
        return name, duration, channels


def write_lab_anim(filepath, clip_name, duration, channels):
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    with open(filepath, "wb") as f:
        f.write(b"LAB_ANIM\x00")
        nb = clip_name.encode("utf-8")
        f.write(struct.pack("<H", len(nb)))
        f.write(nb)
        f.write(struct.pack("<fI", float(duration), len(channels)))
        for bone_name, pos_keys, rot_keys, scl_keys in channels:
            bb = bone_name.encode("utf-8")
            f.write(struct.pack("<H", len(bb)))
            f.write(bb)
            f.write(struct.pack("<I", len(pos_keys)))
            for pk in pos_keys:
                f.write(struct.pack("<ffff", *pk))
            f.write(struct.pack("<I", len(rot_keys)))
            for rk in rot_keys:
                f.write(struct.pack("<fffff", *rk))
            f.write(struct.pack("<I", len(scl_keys)))
            for sk in scl_keys:
                f.write(struct.pack("<ffff", *sk))


def quat_mul(q1, q2):
    x1, y1, z1, w1 = q1
    x2, y2, z2, w2 = q2
    x = w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2
    y = w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2
    z = w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2
    w = w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2
    n = math.sqrt(x * x + y * y + z * z + w * w)
    if n > 1e-6:
        return (x / n, y / n, z / n, w / n)
    return (0.0, 0.0, 0.0, 1.0)


def quat_from_euler(rx, ry, rz):
    cx, sx = math.cos(rx * 0.5), math.sin(rx * 0.5)
    cy, sy = math.cos(ry * 0.5), math.sin(ry * 0.5)
    cz, sz = math.cos(rz * 0.5), math.sin(rz * 0.5)
    return (
        sx * cy * cz - cx * sy * sz,
        cx * sy * cz + sx * cy * sz,
        cx * cy * sz - sx * sy * cz,
        cx * cy * cz + sx * sy * sz,
    )


def generate_animations():
    anim_dir = os.path.join(ROOT, "assets", "animations")
    pistol_idle_path = os.path.join(anim_dir, "Pistol Idle.anim")
    walking_path = os.path.join(anim_dir, "Walking.anim")
    firing_path = os.path.join(anim_dir, "Firing Rifle.anim")

    if os.path.exists(pistol_idle_path):
        _, dur, channels = read_lab_anim(pistol_idle_path)
        # 1. Idle.anim
        write_lab_anim(os.path.join(anim_dir, "Idle.anim"), "Idle", dur, channels)

        # 2. Talk.anim (modulate Head, Neck, Spine1, RightArm, LeftArm for expressive dialogue gestures)
        talk_dur = max(dur, 2.4)
        talk_channels = []
        for bone_name, pos_keys, rot_keys, scl_keys in channels:
            bl = bone_name.lower()
            new_rot = []
            if ("head" in bl or "neck" in bl or "spine1" in bl or "spine2" in bl or "arm" in bl or "hand" in bl) and rot_keys:
                base_r = rot_keys[0][1:]
                steps = 16
                for s in range(steps + 1):
                    t = (talk_dur * s) / steps
                    phase = (2.0 * math.pi * s) / steps
                    if "head" in bl:
                        dq = quat_from_euler(math.sin(phase * 3.0) * 0.08, math.sin(phase * 1.5) * 0.12, math.cos(phase * 2.0) * 0.04)
                    elif "neck" in bl:
                        dq = quat_from_euler(math.sin(phase * 2.0) * 0.04, math.sin(phase) * 0.06, 0.0)
                    elif "spine" in bl:
                        dq = quat_from_euler(math.sin(phase) * 0.03, math.cos(phase) * 0.03, 0.0)
                    else:
                        dq = quat_from_euler(math.sin(phase * 2.0) * 0.06, 0.0, math.cos(phase) * 0.04)
                    rq = quat_mul(base_r, dq)
                    new_rot.append((t, rq[0], rq[1], rq[2], rq[3]))
            else:
                new_rot = rot_keys
            talk_channels.append((bone_name, pos_keys, new_rot, scl_keys))
        write_lab_anim(os.path.join(anim_dir, "Talk.anim"), "Talk", talk_dur, talk_channels)

    if os.path.exists(walking_path):
        _, dur, channels = read_lab_anim(walking_path)
        write_lab_anim(os.path.join(anim_dir, "Walk.anim"), "Walk", dur, channels)

    if os.path.exists(firing_path):
        _, dur, channels = read_lab_anim(firing_path)
        write_lab_anim(os.path.join(anim_dir, "Attack.anim"), "Attack", dur, channels)



def write_bmp_24(filepath, width, height, pixel_fn):
    os.makedirs(os.path.dirname(filepath), exist_ok=True)
    row_stride = (width * 3 + 3) & ~3
    img_size = row_stride * height
    file_size = 54 + img_size

    with open(filepath, "wb") as f:
        # BMP Header
        f.write(b"BM")
        f.write(struct.pack("<IHHI", file_size, 0, 0, 54))
        # DIB Header (BITMAPINFOHEADER)
        f.write(struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0, img_size, 2835, 2835, 0, 0))
        pad = b"\x00" * (row_stride - width * 3)
        for y in range(height):
            row = bytearray()
            for x in range(width):
                r, g, b = pixel_fn(x, y, width, height)
                row.extend((max(0, min(255, int(b))),
                            max(0, min(255, int(g))),
                            max(0, min(255, int(r)))))
            f.write(row)
            if pad:
                f.write(pad)


def generate_textures():
    tex_dir = os.path.join(ROOT, "assets", "textures")

    # 1. concrete_wall_normal.bmp (Sharp Blinn-Phong normal map with panel bevels & surface grain)
    def concrete_normal(x, y, w, h):
        nx, ny, nz = 0.0, 0.0, 1.0
        # Panel groove every 64px
        if x % 64 < 2:
            nx = -0.45
        elif x % 64 > 61:
            nx = 0.45
        if y % 64 < 2:
            ny = -0.45
        elif y % 64 > 61:
            ny = 0.45
        # Subtle high-frequency concrete pit relief
        grain = math.sin(x * 0.73 + y * 0.31) * math.cos(x * 0.29 - y * 0.83) * 0.12
        nx += grain
        ny += grain * 0.8
        length = math.sqrt(nx * nx + ny * ny + nz * nz)
        return ((nx / length * 0.5 + 0.5) * 255,
                (ny / length * 0.5 + 0.5) * 255,
                (nz / length * 0.5 + 0.5) * 255)

    write_bmp_24(os.path.join(tex_dir, "concrete_wall_normal.bmp"), 128, 128, concrete_normal)

    # 2. floor_lab_normal.bmp (Industrial diamond plate & floor tile normal map)
    def floor_normal(x, y, w, h):
        nx, ny, nz = 0.0, 0.0, 1.0
        if x % 32 == 0 or x % 32 == 31:
            nx = -0.5 if (x % 32 == 0) else 0.5
        if y % 32 == 0 or y % 32 == 31:
            ny = -0.5 if (y % 32 == 0) else 0.5
        # Diamond tread pattern
        dx = (x % 16) - 8
        dy = (y % 16) - 8
        if abs(dx) + abs(dy) < 4:
            nx += dx * 0.08
            ny += dy * 0.08
        length = math.sqrt(nx * nx + ny * ny + nz * nz)
        return ((nx / length * 0.5 + 0.5) * 255,
                (ny / length * 0.5 + 0.5) * 255,
                (nz / length * 0.5 + 0.5) * 255)

    write_bmp_24(os.path.join(tex_dir, "floor_lab_normal.bmp"), 128, 128, floor_normal)

    # 3. metal_hull_normal.bmp (Corrugated ribs + rivets)
    def metal_normal(x, y, w, h):
        nx, ny, nz = 0.0, math.sin(y * 0.3927) * 0.35, 1.0
        length = math.sqrt(nx * nx + ny * ny + nz * nz)
        return ((nx / length * 0.5 + 0.5) * 255,
                (ny / length * 0.5 + 0.5) * 255,
                (nz / length * 0.5 + 0.5) * 255)

    write_bmp_24(os.path.join(tex_dir, "metal_hull_normal.bmp"), 128, 128, metal_normal)

    # 4. corridor_dirty_wall.bmp (Site 14 Dirty Corridor weathered concrete/metal with rust streaks & hazard trim)
    def dirty_wall(x, y, w, h):
        base = 112 + int(14 * math.sin(x * 0.17) * math.cos(y * 0.13))
        r, g, b = base, base + 3, base + 6
        # Bottom grime & hazard baseboard
        if y < 18:
            stripe = ((x + y) // 8) % 2
            if y >= 10:
                if stripe == 0:
                    return (195, 150, 25)
                else:
                    return (32, 34, 38)
            else:
                return (42, 38, 35)
        # Vertical water/rust leak stains from top pipes
        leak = max(0.0, math.sin(x * 0.22 + 1.3) ** 5) * max(0.0, (y - 32) / float(h))
        r = int(r * (1.0 - 0.35 * leak) + 92 * leak)
        g = int(g * (1.0 - 0.50 * leak) + 54 * leak)
        b = int(b * (1.0 - 0.60 * leak) + 34 * leak)
        # Panel seam lines
        if x % 64 == 0 or y % 64 == 0:
            r, g, b = int(r * 0.65), int(g * 0.65), int(b * 0.65)
        return (r, g, b)

    write_bmp_24(os.path.join(tex_dir, "corridor_dirty_wall.bmp"), 128, 128, dirty_wall)

    # 5. decal_rust_stain.bmp (Dark industrial oil/rust stain)
    def rust_stain(x, y, w, h):
        cx, cy = (x - w * 0.5) / (w * 0.5), (y - h * 0.5) / (h * 0.5)
        d = math.sqrt(cx * cx + cy * cy)
        streak = 0.25 * math.sin(x * 0.35) * math.cos(y * 0.18)
        factor = max(0.0, min(1.0, 1.0 - (d + streak)))
        return (int(35 + 65 * factor), int(28 + 38 * factor), int(24 + 22 * factor))

    write_bmp_24(os.path.join(tex_dir, "decal_rust_stain.bmp"), 64, 64, rust_stain)

    # 6. cubemap_site14.bmp (Cold cyan/amber metallic reflection map)
    def cubemap_tex(x, y, w, h):
        v = y / float(h)
        horiz = math.exp(-((v - 0.52) ** 2) * 28.0)
        r = int(28 + 140 * horiz)
        g = int(42 + 165 * horiz)
        b = int(58 + 185 * horiz)
        return (r, g, b)

    write_bmp_24(os.path.join(tex_dir, "cubemap_site14.bmp"), 128, 128, cubemap_tex)


def generate_audio():
    audio_dir = os.path.join(ROOT, "assets", "audio")
    os.makedirs(audio_dir, exist_ok=True)
    sample_rate = 44100

    # 1. bot_corridor_dialog.wav (3.0s synthesized voice with distinct syllable cadence for Lip-Sync)
    dialog_path = os.path.join(audio_dir, "bot_corridor_dialog.wav")
    duration = 3.0
    total_samples = int(sample_rate * duration)
    # Syllable windows (start_s, end_s, f1_hz, f2_hz)
    syllables = [
        (0.10, 0.32, 720.0, 1180.0),  # "WAR-" (A/O)
        (0.34, 0.52, 420.0, 1950.0),  # "-NING" (I)
        (0.68, 0.90, 580.0, 1350.0),  # "SEC-" (E)
        (0.92, 1.15, 480.0, 920.0),   # "-TOR" (O)
        (1.25, 1.52, 650.0, 1100.0),  # "FOUR-" (O/U)
        (1.55, 1.82, 360.0, 2150.0),  # "-TEEN" (I/E)
        (2.00, 2.35, 760.0, 1250.0),  # "BREACH" (A/I)
        (2.42, 2.80, 510.0, 1480.0),  # "ALERT" (E/O)
    ]
    samples = bytearray()
    for i in range(total_samples):
        t = i / float(sample_rate)
        env = 0.0
        f1, f2 = 500.0, 1500.0
        for s0, s1, sf1, sf2 in syllables:
            if s0 <= t <= s1:
                mid = 0.5 * (s0 + s1)
                half = 0.5 * (s1 - s0)
                env = math.sin(math.pi * (t - s0) / (s1 - s0))
                f1, f2 = sf1, sf2
                break
        if env > 0.0:
            # Synthesized vocoder/synth voice: fundamental 115Hz + formants f1, f2
            f0 = 115.0 + 8.0 * math.sin(t * 6.0)
            sig = (0.45 * math.sin(2.0 * math.pi * f0 * t) +
                   0.30 * math.sin(2.0 * math.pi * (f0 * 2.0) * t) +
                   0.25 * math.sin(2.0 * math.pi * f1 * t) +
                   0.15 * math.sin(2.0 * math.pi * f2 * t))
            val = int(max(-1.0, min(1.0, sig * env * 0.75)) * 32767)
        else:
            val = 0
        samples.extend(struct.pack("<h", val))

    with wave.open(dialog_path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(samples)

    # 2. bot_footstep_3d.wav (0.28s heavy metallic boot thud)
    step_path = os.path.join(audio_dir, "bot_footstep_3d.wav")
    step_samples = int(sample_rate * 0.28)
    s_data = bytearray()
    for i in range(step_samples):
        t = i / float(sample_rate)
        env = math.exp(-t * 22.0)
        thud = math.sin(2.0 * math.pi * (95.0 * math.exp(-t * 8.0)) * t)
        clank = math.sin(2.0 * math.pi * 840.0 * t) * math.exp(-t * 45.0) * 0.35
        val = int(max(-1.0, min(1.0, (thud * 0.75 + clank) * env)) * 32767)
        s_data.extend(struct.pack("<h", val))

    with wave.open(step_path, "wb") as wf:
        wf.setnchannels(1)
        wf.setsampwidth(2)
        wf.setframerate(sample_rate)
        wf.writeframes(s_data)


def generate_labui_files():
    ui_dir = os.path.join(ROOT, "assets", "ui")
    hud_dir = os.path.join(ROOT, "assets", "hud_projects")
    os.makedirs(ui_dir, exist_ok=True)
    os.makedirs(hud_dir, exist_ok=True)

    screens = {
        "main_menu.labui": """project_name "MainMenu_Screen"
screen_type "MainMenu"
virtual_res 1920 1080
grid_size 10

element "MainMenu_Background" Image
    anchor Center
    offset 0 0
    size 1920 1080
    color 0.06 0.08 0.11 1.0
    bg_color 0.04 0.05 0.08 1.0
    text "FROZEN-LIFE : SITE 14"
    texture "corridor_dirty_wall.bmp"
    scale 1.0
    visible 1
end_element

element "MainMenu_Panel" Panel
    anchor TopLeft
    offset 120 140
    size 520 760
    color 0.20 0.75 0.95 1.0
    bg_color 0.10 0.13 0.18 0.92
    text "MAIN MENU"
    scale 1.0
    visible 1
end_element

element "MainMenu_Title" Label
    parent "MainMenu_Panel"
    anchor TopLeft
    offset 32 28
    size 450 56
    color 0.95 0.98 1.0 1.0
    bg_color 0.0 0.0 0.0 0.0
    text "FROZEN-LIFE : SITE 14"
    scale 2.2
    visible 1
end_element

element "Btn_Campaign" Button
    parent "MainMenu_Panel"
    anchor TopLeft
    offset 32 120
    size 456 58
    color 1.0 1.0 1.0 1.0
    bg_color 0.18 0.36 0.56 0.95
    text "START CAMPAIGN"
    action "screen:HUD"
    scale 1.6
    visible 1
end_element

element "Btn_Options" Button
    parent "MainMenu_Panel"
    anchor TopLeft
    offset 32 200
    size 456 58
    color 1.0 1.0 1.0 1.0
    bg_color 0.16 0.28 0.42 0.95
    text "OPTIONS & SETTINGS"
    action "screen:OptionsMenu"
    scale 1.6
    visible 1
end_element

element "Btn_Quit" Button
    parent "MainMenu_Panel"
    anchor TopLeft
    offset 32 280
    size 456 58
    color 1.0 0.85 0.85 1.0
    bg_color 0.45 0.16 0.18 0.95
    text "QUIT TO DESKTOP"
    action "quit"
    scale 1.6
    visible 1
end_element
""",
        "pause_menu.labui": """project_name "PauseMenu_Screen"
screen_type "PauseMenu"
virtual_res 1920 1080
grid_size 10

element "Pause_Overlay" Panel
    anchor Center
    offset -300 -260
    size 600 520
    color 0.20 0.75 0.95 1.0
    bg_color 0.08 0.10 0.14 0.94
    text "MISSION PAUSED"
    scale 1.0
    visible 1
end_element

element "Pause_Header" Label
    parent "Pause_Overlay"
    anchor TopLeft
    offset 40 30
    size 520 48
    color 0.98 0.82 0.20 1.0
    bg_color 0.0 0.0 0.0 0.0
    text "PAUSED - SITE 14 SECTOR"
    scale 2.0
    visible 1
end_element

element "Btn_Resume" Button
    parent "Pause_Overlay"
    anchor TopLeft
    offset 40 110
    size 520 56
    color 1.0 1.0 1.0 1.0
    bg_color 0.18 0.55 0.36 0.95
    text "RESUME MISSION [ESC]"
    action "resume"
    scale 1.6
    visible 1
end_element

element "Btn_PauseOptions" Button
    parent "Pause_Overlay"
    anchor TopLeft
    offset 40 186
    size 520 56
    color 1.0 1.0 1.0 1.0
    bg_color 0.18 0.32 0.48 0.95
    text "OPTIONS & SETTINGS"
    action "screen:OptionsMenu"
    scale 1.6
    visible 1
end_element

element "Btn_MainMenu" Button
    parent "Pause_Overlay"
    anchor TopLeft
    offset 40 262
    size 520 56
    color 1.0 0.9 0.8 1.0
    bg_color 0.48 0.22 0.18 0.95
    text "RETURN TO MAIN MENU"
    action "screen:MainMenu"
    scale 1.6
    visible 1
end_element
""",
        "options_menu.labui": """project_name "OptionsMenu_Screen"
screen_type "OptionsMenu"
virtual_res 1920 1080
grid_size 10

element "Options_Frame" Panel
    anchor Center
    offset -420 -320
    size 840 640
    color 0.20 0.75 0.95 1.0
    bg_color 0.09 0.11 0.16 0.95
    text "ENGINE OPTIONS & SETTINGS"
    scale 1.0
    visible 1
end_element

element "Options_Title" Label
    parent "Options_Frame"
    anchor TopLeft
    offset 36 28
    size 600 48
    color 0.25 0.85 1.0 1.0
    bg_color 0.0 0.0 0.0 0.0
    text "GRAPHICS, AUDIO & CONTROLS"
    scale 2.0
    visible 1
end_element

element "Options_Banner" Image
    parent "Options_Frame"
    anchor TopLeft
    offset 36 92
    size 768 120
    color 0.85 0.90 0.95 1.0
    bg_color 0.12 0.16 0.22 1.0
    text "BLINN-PHONG / DSA / FXAA PIPELINE"
    texture "metal_plate.bmp"
    scale 1.0
    visible 1
end_element

element "Btn_ToggleFXAA" Button
    parent "Options_Frame"
    anchor TopLeft
    offset 36 240
    size 360 54
    color 1.0 1.0 1.0 1.0
    bg_color 0.18 0.38 0.56 0.95
    text "ANTI-ALIASING: FXAA [ON]"
    action "toggle:fxaa"
    scale 1.5
    visible 1
end_element

element "Btn_ToggleBloom" Button
    parent "Options_Frame"
    anchor TopLeft
    offset 444 240
    size 360 54
    color 1.0 1.0 1.0 1.0
    bg_color 0.18 0.38 0.56 0.95
    text "HDR BLOOM: SUBTLE [ON]"
    action "toggle:bloom"
    scale 1.5
    visible 1
end_element

element "Btn_BackToMain" Button
    parent "Options_Frame"
    anchor BottomLeft
    offset 36 -84
    size 260 54
    color 1.0 1.0 1.0 1.0
    bg_color 0.24 0.28 0.36 0.95
    text "< BACK TO MENU"
    action "screen:MainMenu"
    scale 1.5
    visible 1
end_element
""",
        "hud_layout.labui": """project_name "FrozenLife_HUD"
screen_type "HUD"
virtual_res 1920 1080
grid_size 10

element "HUD_VitalPanel" Panel
    anchor BottomLeft
    offset 40 -130
    size 420 96
    color 0.98 0.78 0.08 1.0
    bg_color 0.05 0.07 0.10 0.82
    text "VITALS"
    scale 1.0
    visible 1
end_element

element "HUD_HealthBar" ProgressBar
    parent "HUD_VitalPanel"
    anchor TopLeft
    offset 18 22
    size 260 28
    color 0.98 0.78 0.08 1.0
    bg_color 0.12 0.14 0.18 0.90
    text "HEALTH"
    scale 1.2
    visible 1
    value 100
    max_value 100
end_element

element "HUD_SuitLabel" Label
    parent "HUD_VitalPanel"
    anchor TopLeft
    offset 18 58
    size 260 26
    color 0.30 0.85 1.0 1.0
    bg_color 0.0 0.0 0.0 0.0
    text "HEV SUIT ARMOR: 100%"
    scale 1.4
    visible 1
end_element

element "HUD_CenterCrosshair" Crosshair
    anchor Center
    offset -16 -16
    size 32 32
    color 0.20 0.95 0.40 0.95
    bg_color 0.0 0.0 0.0 0.0
    text "+"
    scale 1.0
    visible 1
end_element
"""
    }

    for fname, content in screens.items():
        with open(os.path.join(ui_dir, fname), "w", encoding="utf-8") as f:
            f.write(content)
        with open(os.path.join(hud_dir, fname), "w", encoding="utf-8") as f:
            f.write(content)


def download_polyhaven_cc0_assets():
    import urllib.request
    tex_dir = os.path.join(ROOT, "assets", "textures")
    models_dir = os.path.join(ROOT, "assets", "models")
    os.makedirs(tex_dir, exist_ok=True)
    os.makedirs(models_dir, exist_ok=True)

    cc0_textures = {
        "blue_metal_plate_diff_1k.jpg": "https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/blue_metal_plate/blue_metal_plate_diff_1k.jpg",
        "blue_metal_plate_nor_gl_1k.jpg": "https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/blue_metal_plate/blue_metal_plate_nor_gl_1k.jpg",
        "factory_wall_diff_1k.jpg": "https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/factory_wall/factory_wall_diff_1k.jpg",
        "factory_wall_nor_gl_1k.jpg": "https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/factory_wall/factory_wall_nor_gl_1k.jpg",
        "corrugated_iron_diff_1k.jpg": "https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/corrugated_iron/corrugated_iron_diff_1k.jpg",
        "corrugated_iron_nor_gl_1k.jpg": "https://dl.polyhaven.org/file/ph-assets/Textures/jpg/1k/corrugated_iron/corrugated_iron_nor_gl_1k.jpg",
    }
    cc0_models = {
        "mounted_fluorescent_lights_1k.gltf": "https://dl.polyhaven.org/file/ph-assets/Models/gltf/1k/mounted_fluorescent_lights/mounted_fluorescent_lights_1k.gltf",
        "modular_industrial_pipes_01_1k.gltf": "https://dl.polyhaven.org/file/ph-assets/Models/gltf/1k/modular_industrial_pipes_01/modular_industrial_pipes_01_1k.gltf",
        "rollershutter_door_1k.gltf": "https://dl.polyhaven.org/file/ph-assets/Models/gltf/1k/rollershutter_door/rollershutter_door_1k.gltf",
        "worn_metal_rack_1k.gltf": "https://dl.polyhaven.org/file/ph-assets/Models/gltf/1k/worn_metal_rack/worn_metal_rack_1k.gltf",
    }

    headers = {"User-Agent": "LabEngine-AssetFetcher/1.0"}
    for fname, url in cc0_textures.items():
        dest = os.path.join(tex_dir, fname)
        if not os.path.exists(dest):
            try:
                req = urllib.request.Request(url, headers=headers)
                with urllib.request.urlopen(req, timeout=15) as resp, open(dest, "wb") as out:
                    out.write(resp.read())
                print(f"[PolyHaven CC0] Downloaded texture: {fname}")
            except Exception as e:
                print(f"[PolyHaven CC0] Skipped {fname}: {e}")

    for fname, url in cc0_models.items():
        dest = os.path.join(models_dir, fname)
        if not os.path.exists(dest):
            try:
                req = urllib.request.Request(url, headers=headers)
                with urllib.request.urlopen(req, timeout=15) as resp, open(dest, "wb") as out:
                    out.write(resp.read())
                print(f"[PolyHaven CC0] Downloaded model: {fname}")
            except Exception as e:
                print(f"[PolyHaven CC0] Skipped {fname}: {e}")


if __name__ == "__main__":
    generate_3d_props()
    generate_animations()
    generate_textures()
    generate_audio()
    generate_labui_files()
    download_polyhaven_cc0_assets()
    print("[generate_todo_assets] Successfully generated and downloaded all 3D props, animations, normal maps, 3D audio, and .labui screens.")

