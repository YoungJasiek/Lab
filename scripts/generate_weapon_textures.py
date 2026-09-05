import os
import struct
import math

def create_bmp(filepath, width, height, pixel_func):
    row_bytes = width * 3
    padding = (4 - (row_bytes % 4)) % 4
    image_size = (row_bytes + padding) * height
    file_size = 54 + image_size

    # BMP Header
    bmp_header = struct.pack('<2sIHHI', b'BM', file_size, 0, 0, 54)
    # DIB Header (BITMAPINFOHEADER)
    dib_header = struct.pack('<IIIHHIIIIII', 40, width, height, 1, 24, 0, image_size, 2835, 2835, 0, 0)

    data = bytearray()
    for y in range(height):
        # BMP stores rows bottom-to-top
        row = bytearray()
        for x in range(width):
            r, g, b = pixel_func(x, y, width, height)
            r = max(0, min(255, int(r)))
            g = max(0, min(255, int(g)))
            b = max(0, min(255, int(b)))
            # BGR order
            row.extend(struct.pack('BBB', b, g, r))
        row.extend(b'\x00' * padding)
        data.extend(row)

    os.makedirs(os.path.dirname(os.path.abspath(filepath)), exist_ok=True)
    with open(filepath, 'wb') as f:
        f.write(bmp_header)
        f.write(dib_header)
        f.write(data)
    print(f"Generated: {filepath} ({width}x{height})")

def noise2d(x, y, seed=42):
    n = x * 374761393 + y * 668265263 + seed * 3266489917
    n = (n ^ (n >> 13)) * 1274126177
    return ((n ^ (n >> 16)) & 0x7fffffff) / float(0x7fffffff)

def smooth_noise(x, y, scale, seed=42):
    sx = x / scale
    sy = y / scale
    x0 = int(math.floor(sx))
    y0 = int(math.floor(sy))
    fx = sx - x0
    fy = sy - y0
    fx = fx * fx * (3.0 - 2.0 * fx)
    fy = fy * fy * (3.0 - 2.0 * fy)
    v00 = noise2d(x0, y0, seed)
    v10 = noise2d(x0 + 1, y0, seed)
    v01 = noise2d(x0, y0 + 1, seed)
    v11 = noise2d(x0 + 1, y0 + 1, seed)
    return (1.0 - fy) * ((1.0 - fx) * v00 + fx * v10) + fy * ((1.0 - fx) * v01 + fx * v11)

# 1. PIPE (Steel Pipe) - Industrial oxidized steel, scratches, rust streaks, screw thread bands
def tex_pipe(x, y, w, h):
    base_steel = 110 + int(smooth_noise(x, y, 16.0, 101) * 35)
    rust_val = smooth_noise(x, y, 28.0, 303)
    groove = 1.0 - 0.35 * abs(math.sin(y * 0.15))
    thread = 1.0
    if x < 40 or x > w - 40:
        thread = 0.7 if (x % 6 < 2) else 1.15
    
    r = base_steel * groove * thread
    g = base_steel * groove * thread * 0.95
    b = base_steel * groove * thread * 0.92

    if rust_val > 0.62:
        factor = (rust_val - 0.62) / 0.38
        r = r * (1.0 - factor) + 165 * factor
        g = g * (1.0 - factor) + 75 * factor
        b = b * (1.0 - factor) + 35 * factor

    sc = noise2d(x, y * 3, 505)
    if sc > 0.96:
        r += 50; g += 50; b += 50

    return (r, g, b)

# 2. PISTOLET (Tactical Pistol) - Matte tactical polymer, grip crosshatch, gunmetal slide
def tex_pistol(x, y, w, h):
    is_slide = (y > h * 0.55)
    if is_slide:
        grain = smooth_noise(x * 4, y, 8.0, 202)
        base = 55 + int(grain * 25)
        if x < 70 and (x % 8 < 3):
            base -= 25
        r, g, b = base, base + 2, base + 6
        if 140 < x < 190 and y > h * 0.75:
            r, g, b = 25, 27, 30
    else:
        check = ((x // 6 + y // 6) % 2 == 0)
        base = 35 + (12 if check else -8)
        grain = int(noise2d(x, y, 707) * 10)
        base += grain
        r, g, b = base, base + 1, base + 2
        if 210 < x < 225 and 110 < y < 125:
            r, g, b = 220, 25, 25

    return (r, g, b)

# 3. STRZELBA (Tactical Shotgun) - Heavy steel heat shield, ribbed forend, brass shell ejection
def tex_shotgun(x, y, w, h):
    is_pump = (y < h * 0.45 and 40 < x < 210)
    if is_pump:
        rib = abs(math.sin(x * 0.3))
        val = 30 + int(rib * 45)
        return (val, val, val + 2)
    
    is_barrel = (y >= h * 0.65)
    if is_barrel:
        val = 70 + int(smooth_noise(x, y, 12.0, 404) * 20)
        col = (x // 24) * 24 + 12
        row = ((y - int(h * 0.65)) // 20) * 20 + int(h * 0.65) + 10
        dist = math.sqrt((x - col)**2 + (y - row)**2)
        if dist < 6.0:
            return (15, 18, 22)
        return (val, val + 2, val + 5)
    
    base = 45 + int(smooth_noise(x, y, 18.0, 909) * 15)
    if 160 < x < 210 and 120 < y < 155:
        return (185, 150, 45)
    return (base, base + 3, base + 7)

# 4. M4A4-S (Silenced Assault Rifle) - Urban midnight camo, carbon fiber suppressor, picatinny rail
def tex_m4a4s(x, y, w, h):
    if x < 80:
        twill = ((x // 4 + y // 4) % 2 == 0)
        c_val = 28 + (16 if twill else 0)
        return (c_val, c_val + 2, c_val + 4)

    if y > h - 30:
        tooth = (x % 14 < 7)
        r_val = 30 if tooth else 70
        return (r_val, r_val + 2, r_val + 5)

    n1 = smooth_noise(x, y, 32.0, 111)
    n2 = smooth_noise(x, y, 14.0, 222)
    camo = (n1 + n2 * 0.5) / 1.5
    if camo > 0.65:
        return (42, 52, 65)
    elif camo > 0.40:
        return (32, 38, 35)
    else:
        return (22, 23, 26)

# 5. SG553 (Scoped Battle Rifle) - Swiss military tactical green, amber scope reticle, receiver vents
def tex_sg553(x, y, w, h):
    if 180 < x < 240 and 180 < y < 240:
        cx, cy = 210, 210
        d = math.sqrt((x - cx)**2 + (y - cy)**2)
        if d < 26.0:
            if d < 3.0 or abs(x - cx) < 1.5 or abs(y - cy) < 1.5:
                return (255, 180, 20)
            lens = 30 + int(d * 2.5)
            return (15, 35 + lens, 65 + lens)
        elif d < 29.0:
            return (15, 15, 15)

    gn = smooth_noise(x, y, 20.0, 616)
    r = 38 + int(gn * 15)
    g = 52 + int(gn * 20)
    b = 35 + int(gn * 12)

    if 60 < x < 140 and 110 < y < 145:
        return (20, 22, 25)
    if 30 < x < 45 and 70 < y < 85:
        return (210, 30, 30)

    return (r, g, b)

# 6. MINIGUN (Rotary Chaingun) - 6 titanium black barrels, yellow-black hazard feeding, heavy ammo belt
def tex_minigun(x, y, w, h):
    if y < 65:
        stripe = ((x + y) // 16) % 2
        return (220, 180, 15) if stripe == 0 else (25, 25, 28)

    if 70 <= y < 120 and 40 < x < 220:
        link = (x % 16 < 12)
        if link:
            br = 195 + int(smooth_noise(x, y, 6.0, 808) * 35)
            bg = 155 + int(smooth_noise(x, y, 6.0, 808) * 25)
            return (br, bg, 35)
        else:
            return (35, 35, 40)

    bar_x = x % 42
    cylinder_shade = math.sin((bar_x / 42.0) * math.pi)
    base = 35 + int(cylinder_shade * 75)
    return (base, base + 2, base + 6)

# 7. PLASMA GUN (Pulse Energy Cannon) - Sci-Fi cyan glow conduits, dark alloy armor, hex power core
def tex_plasma(x, y, w, h):
    conduit_y = abs(y - 128)
    if conduit_y < 22:
        glow = 1.0 - (conduit_y / 22.0)
        pulse = 0.8 + 0.2 * math.sin(x * 0.1)
        return (int(20 * glow * pulse), int(210 * glow * pulse), int(255 * glow * pulse))

    hx = (x % 32) - 16
    hy = (y % 28) - 14
    hd = math.sqrt(hx*hx + hy*hy)
    if abs(hd - 12.0) < 2.0:
        return (40, 160, 220)

    base = 28 + int(smooth_noise(x, y, 16.0, 777) * 18)
    return (base, base + 4, base + 12)

# 8. RAILGUN (Electromagnetic Accelerator) - Dual magnetic copper rails, high-voltage coils, blue arc
def tex_railgun(x, y, w, h):
    if 115 < y < 141:
        arc = noise2d(x * 4, y, 333)
        return (210, 240, 255) if arc > 0.7 else (30, 120, 240)

    if (95 <= y <= 115) or (141 <= y <= 160):
        c_shade = 170 + int(smooth_noise(x, y, 8.0, 555) * 45)
        return (c_shade, int(c_shade * 0.62), int(c_shade * 0.25))

    if 50 < x < 210:
        wire = (x % 10 < 5)
        return (150, 95, 35) if wire else (20, 25, 30)

    panel = smooth_noise(x, y, 32.0, 999)
    val = 60 + int(panel * 40)
    return (val, val + 5, val + 10)

# 9. RPG (Rocket Propelled Grenade) - Military OD green tube, wooden heat wrap, olive warhead with fuse
def tex_rpg(x, y, w, h):
    if x > 190:
        wh_base = 65 + int(smooth_noise(x, y, 12.0, 111) * 25)
        if x > 242:
            return (220, 220, 225)
        if 205 < x < 215:
            return (200, 25, 25)
        return (wh_base - 10, wh_base + 15, wh_base - 15)

    if 70 < x < 160:
        grain = smooth_noise(x * 0.5, y * 3.0, 14.0, 444)
        wr = 135 + int(grain * 45)
        wg = 75 + int(grain * 25)
        wb = 25 + int(grain * 10)
        return (wr, wg, wb)

    tube = 45 + int(smooth_noise(x, y, 24.0, 888) * 20)
    if 25 < x < 65 and 115 < y < 140:
        st = noise2d(x * 2, y * 2, 999)
        if st > 0.65:
            return (230, 230, 220)
    return (tube, tube + 18, tube - 5)

def main():
    target_dirs = [
        "assets/textures",
        "build/Release/assets/textures"
    ]
    weapons = [
        ("weapon_pipe.bmp", tex_pipe),
        ("weapon_pistol.bmp", tex_pistol),
        ("weapon_shotgun.bmp", tex_shotgun),
        ("weapon_m4a4s.bmp", tex_m4a4s),
        ("weapon_sg553.bmp", tex_sg553),
        ("weapon_minigun.bmp", tex_minigun),
        ("weapon_plasma.bmp", tex_plasma),
        ("weapon_railgun.bmp", tex_railgun),
        ("weapon_rpg.bmp", tex_rpg)
    ]

    for d in target_dirs:
        for fname, func in weapons:
            fpath = os.path.join(d, fname)
            create_bmp(fpath, 256, 256, func)

if __name__ == "__main__":
    main()
