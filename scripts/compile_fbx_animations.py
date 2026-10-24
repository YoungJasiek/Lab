import os
import struct
import zlib
import math

def euler_to_quat(pitch_rad, yaw_rad, roll_rad):
    # FBX Mixamo default rotation order: Euler XYZ (X * Y * Z)
    cp = math.cos(pitch_rad * 0.5)
    sp = math.sin(pitch_rad * 0.5)
    cy = math.cos(yaw_rad * 0.5)
    sy = math.sin(yaw_rad * 0.5)
    cr = math.cos(roll_rad * 0.5)
    sr = math.sin(roll_rad * 0.5)
    return (
        sp * cy * cr + cp * sy * sr,
        cp * sy * cr - sp * cy * sr,
        cp * cy * sr + sp * sy * cr,
        cp * cy * cr - sp * sy * sr
    )

def quat_mul(q1, q2):
    x1, y1, z1, w1 = q1
    x2, y2, z2, w2 = q2
    return (
        w1*x2 + x1*w2 + y1*z2 - z1*y2,
        w1*y2 - x1*z2 + y1*w2 + z1*x2,
        w1*z2 + x1*y2 - y1*x2 + z1*w2,
        w1*w2 - x1*x2 - y1*y2 - z1*z2
    )

def quat_inv(q):
    return (-q[0], -q[1], -q[2], q[3])

def parse_and_export_fbx(fbx_path, anim_out_path):
    with open(fbx_path, 'rb') as f:
        magic = f.read(23)
        if not magic.startswith(b'Kaydara FBX Binary'):
            return False
        version = struct.unpack('<I', f.read(4))[0]

        models = {}
        pre_rotations = {}
        post_rotations = {}
        curve_nodes = {}
        curves = {}
        connections = []

        def read_node():
            header = f.read(25)
            if len(header) < 25: return None
            end_offset, num_props, prop_len, name_len = struct.unpack('<QQQB', header)
            if end_offset == 0: return None
            name = f.read(name_len).decode('ascii', 'replace')
            props = []
            for _ in range(num_props):
                tc = f.read(1).decode('ascii')
                if tc == 'Y': props.append(struct.unpack('<h', f.read(2))[0])
                elif tc == 'C': props.append(struct.unpack('<?', f.read(1))[0])
                elif tc == 'I': props.append(struct.unpack('<i', f.read(4))[0])
                elif tc == 'F': props.append(struct.unpack('<f', f.read(4))[0])
                elif tc == 'D': props.append(struct.unpack('<d', f.read(8))[0])
                elif tc == 'L': props.append(struct.unpack('<q', f.read(8))[0])
                elif tc == 'R':
                    l = struct.unpack('<I', f.read(4))[0]
                    props.append(f.read(l))
                elif tc == 'S':
                    l = struct.unpack('<I', f.read(4))[0]
                    props.append(f.read(l).decode('utf-8', 'replace'))
                elif tc in ('f', 'd', 'l', 'i', 'b'):
                    arr_len, enc, comp_len = struct.unpack('<III', f.read(12))
                    data = f.read(comp_len)
                    if enc == 1: data = zlib.decompress(data)
                    props.append((tc, arr_len, data))
            return end_offset, name, props

        while True:
            n = read_node()
            if not n: break
            end_offset, name, props = n
            if name == 'Objects':
                while f.tell() < end_offset:
                    c = read_node()
                    if not c: continue
                    c_end, c_name, c_props = c
                    if c_name == 'Model' and len(c_props) >= 3:
                        m_id = c_props[0]
                        m_name = c_props[1].split('\x00')[0].replace('Model::', '')
                        models[m_id] = m_name
                        pre = (0.0, 0.0, 0.0)
                        post = (0.0, 0.0, 0.0)
                        while f.tell() < c_end:
                            p = read_node()
                            if not p: continue
                            p_end, p_name, p_props = p
                            if p_name == 'Properties70':
                                while f.tell() < p_end:
                                    pr = read_node()
                                    if not pr: continue
                                    pr_end, pr_name, pr_props = pr
                                    if len(pr_props) >= 7:
                                        if pr_props[0] == 'PreRotation':
                                            pre = (float(pr_props[4]), float(pr_props[5]), float(pr_props[6]))
                                        elif pr_props[0] == 'PostRotation':
                                            post = (float(pr_props[4]), float(pr_props[5]), float(pr_props[6]))
                                    f.seek(pr_end)
                            f.seek(p_end)
                        pre_rotations[m_name] = pre
                        post_rotations[m_name] = post
                    elif c_name == 'AnimationCurveNode' and len(c_props) >= 2:
                        cn_id = c_props[0]
                        cn_type = c_props[1].split('\x00')[0]
                        curve_nodes[cn_id] = {'type': cn_type, 'curves': {}}
                    elif c_name == 'AnimationCurve' and len(c_props) >= 1:
                        cv_id = c_props[0]
                        times = []
                        values = []
                        while f.tell() < c_end:
                            k = read_node()
                            if not k: continue
                            k_end, k_name, k_props = k
                            if k_name == 'KeyTime' and len(k_props) >= 1:
                                tc, count, data = k_props[0]
                                raw_times = struct.unpack(f'<{count}q', data)
                                times = [t / 46186158000.0 for t in raw_times]
                            elif k_name == 'KeyValueFloat' and len(k_props) >= 1:
                                tc, count, data = k_props[0]
                                values = list(struct.unpack(f'<{count}f', data))
                            f.seek(k_end)
                        curves[cv_id] = (times, values)
                    f.seek(c_end)
            elif name == 'Connections':
                while f.tell() < end_offset:
                    c = read_node()
                    if not c: continue
                    c_end, c_name, c_props = c
                    if c_name == 'C' and len(c_props) >= 3:
                        connections.append((c_props[0], c_props[1], c_props[2], c_props[3] if len(c_props) > 3 else ''))
                    f.seek(c_end)
            f.seek(end_offset)

        for rel, child, parent, prop in connections:
            if child in curves and parent in curve_nodes:
                channel = prop.replace('d|', '')
                curve_nodes[parent]['curves'][channel] = curves[child]
            elif child in curve_nodes and parent in models:
                curve_nodes[child]['model_name'] = models[parent]

        # Group by bone
        bones = {} # bone_name -> {'T': ..., 'R': ..., 'S': ...}
        max_duration = 0.0

        for cn_id, cn in curve_nodes.items():
            b_name = cn.get('model_name')
            if not b_name: continue
            c_type = cn['type'] # 'T', 'R', 'S'
            if b_name not in bones: bones[b_name] = {}
            bones[b_name][c_type] = cn['curves']

            for ch, (times, vals) in cn['curves'].items():
                if times and times[-1] > max_duration:
                    max_duration = times[-1]

        # Generate output tracks
        tracks = []
        for b_name, data in bones.items():
            t_keys = []
            r_keys = []
            s_keys = []

            # Translation
            if 'T' in data:
                c_data = data['T']
                if 'X' in c_data and 'Y' in c_data and 'Z' in c_data:
                    tx, vx = c_data['X']
                    ty, vy = c_data['Y']
                    tz, vz = c_data['Z']
                    count = min(len(tx), len(ty), len(tz))
                    scale_trans = 5.0775775  # Mixamo FBX cm (~100.12) to T-800 GLB bind units (~508.37)
                    for i in range(count):
                        t_keys.append((tx[i], vx[i] * scale_trans, vy[i] * scale_trans, vz[i] * scale_trans))

            # Rotation (Euler degrees -> Quat with PreRotation / PostRotation)
            if 'R' in data:
                c_data = data['R']
                if 'X' in c_data and 'Y' in c_data and 'Z' in c_data:
                    tx, vx = c_data['X']
                    ty, vy = c_data['Y']
                    tz, vz = c_data['Z']
                    count = min(len(tx), len(ty), len(tz))
                    pre = pre_rotations.get(b_name, (0.0, 0.0, 0.0))
                    post = post_rotations.get(b_name, (0.0, 0.0, 0.0))
                    q_pre = euler_to_quat(pre[0] * math.pi / 180.0, pre[1] * math.pi / 180.0, pre[2] * math.pi / 180.0)
                    q_post = euler_to_quat(post[0] * math.pi / 180.0, post[1] * math.pi / 180.0, post[2] * math.pi / 180.0)
                    q_post_inv = quat_inv(q_post)
                    for i in range(count):
                        pitch = vx[i] * math.pi / 180.0
                        yaw = vy[i] * math.pi / 180.0
                        roll = vz[i] * math.pi / 180.0
                        q_anim = euler_to_quat(pitch, yaw, roll)
                        qx, qy, qz, qw = quat_mul(quat_mul(q_pre, q_anim), q_post_inv)
                        r_keys.append((tx[i], qx, qy, qz, qw))

            # Scale
            if 'S' in data:
                c_data = data['S']
                if 'X' in c_data and 'Y' in c_data and 'Z' in c_data:
                    tx, vx = c_data['X']
                    ty, vy = c_data['Y']
                    tz, vz = c_data['Z']
                    count = min(len(tx), len(ty), len(tz))
                    for i in range(count):
                        s_keys.append((tx[i], vx[i], vy[i], vz[i]))

            if t_keys or r_keys or s_keys:
                tracks.append((b_name, t_keys, r_keys, s_keys))

        # Binary Serialization (.anim)
        clip_name = os.path.splitext(os.path.basename(fbx_path))[0]
        with open(anim_out_path, 'wb') as out:
            # Magic: LAB_ANIM\0 (8 bytes)
            out.write(b'LAB_ANIM\0')
            
            # Clip Name (length + utf8)
            enc_name = clip_name.encode('utf-8')
            out.write(struct.pack('<H', len(enc_name)))
            out.write(enc_name)

            # Duration (float)
            out.write(struct.pack('<f', max_duration))

            # Track Count
            out.write(struct.pack('<I', len(tracks)))

            for b_name, t_keys, r_keys, s_keys in tracks:
                enc_bname = b_name.encode('utf-8')
                out.write(struct.pack('<H', len(enc_bname)))
                out.write(enc_bname)

                # Translations
                out.write(struct.pack('<I', len(t_keys)))
                for t, x, y, z in t_keys:
                    out.write(struct.pack('<ffff', t, x, y, z))

                # Rotations
                out.write(struct.pack('<I', len(r_keys)))
                for t, x, y, z, w in r_keys:
                    out.write(struct.pack('<fffff', t, x, y, z, w))

                # Scales
                out.write(struct.pack('<I', len(s_keys)))
                for t, x, y, z in s_keys:
                    out.write(struct.pack('<ffff', t, x, y, z))

        return True

def main():
    anim_dir = 'assets/animations'
    files = [f for f in os.listdir(anim_dir) if f.endswith('.fbx')]
    print(f'Compiling {len(files)} FBX animations to binary .anim cache...')
    for f in sorted(files):
        fbx_p = os.path.join(anim_dir, f)
        base = os.path.splitext(f)[0]
        anim_p = os.path.join(anim_dir, base + '.anim')
        success = parse_and_export_fbx(fbx_p, anim_p)
        if success:
            sz = os.path.getsize(anim_p)
            print(f'  [OK] {base:30} -> {base}.anim ({sz // 1024} KB)')
        else:
            print(f'  [FAIL] {f}')

if __name__ == '__main__':
    main()
