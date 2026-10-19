import json
import struct
import math
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GLB_PATH = os.path.join(ROOT, "assets", "models", "t-800_run.glb")
ANIM_DIR = os.path.join(ROOT, "assets", "animations")

def write_lab_anim(filepath, name, duration, tracks):
    with open(filepath, "wb") as f:
        # Magic: LAB_ANIM\0 (9 bytes)
        f.write(b"LAB_ANIM\0")
        enc_name = name.encode("utf-8")
        f.write(struct.pack("<H", len(enc_name)))
        f.write(enc_name)
        f.write(struct.pack("<f", float(duration)))
        f.write(struct.pack("<I", len(tracks)))

        for b_name, t_keys, r_keys, s_keys in tracks:
            enc_bname = b_name.encode("utf-8")
            f.write(struct.pack("<H", len(enc_bname)))
            f.write(enc_bname)

            f.write(struct.pack("<I", len(t_keys)))
            for t, x, y, z in t_keys:
                f.write(struct.pack("<ffff", float(t), float(x), float(y), float(z)))

            f.write(struct.pack("<I", len(r_keys)))
            for t, x, y, z, w in r_keys:
                f.write(struct.pack("<fffff", float(t), float(x), float(y), float(z), float(w)))

            f.write(struct.pack("<I", len(s_keys)))
            for t, x, y, z in s_keys:
                f.write(struct.pack("<ffff", float(t), float(x), float(y), float(z)))

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

def quat_mul(q1, q2):
    x1, y1, z1, w1 = q1; x2, y2, z2, w2 = q2
    return (
        w1*x2 + x1*w2 + y1*z2 - z1*y2,
        w1*y2 - x1*z2 + y1*w2 + z1*x2,
        w1*z2 + x1*y2 - y1*x2 + z1*w2,
        w1*w2 - x1*x2 - y1*y2 - z1*z2
    )

def export_native_animations():
    print(f"Loading native T-800 rig from {GLB_PATH}...")
    with open(GLB_PATH, "rb") as f:
        f.read(12)
        cl = int.from_bytes(f.read(4), "little")
        f.read(4)
        d = json.loads(f.read(cl).decode("utf-8"))
        f.read(4) # bin chunk len
        f.read(4) # bin chunk type
        bin_offset = f.tell()

        skin_joints = d["skins"][0]["joints"]
        nodes = d["nodes"]
        anim = d["animations"][0]
        channels = anim["channels"]
        samplers = anim["samplers"]
        accessors = d["accessors"]
        buffer_views = d["bufferViews"]

        def read_accessor_floats(acc_idx, count_floats):
            acc = accessors[acc_idx]
            bv = buffer_views[acc["bufferView"]]
            off = bin_offset + bv.get("byteOffset", 0) + acc.get("byteOffset", 0)
            cnt = acc["count"]
            f.seek(off)
            raw = f.read(cnt * count_floats * 4)
            res = []
            for i in range(cnt):
                res.append(struct.unpack(f"<{count_floats}f", raw[i*count_floats*4 : (i+1)*count_floats*4]))
            return res

        # Extract all channels grouped by joint node
        node_tracks = {}
        for j in skin_joints:
            node_name = nodes[j].get("name", f"Bone_{j}")
            node_tracks[j] = {
                "name": node_name,
                "t": [],
                "r": [],
                "s": []
            }

        for ch in channels:
            target_node = ch["target"]["node"]
            if target_node not in node_tracks:
                continue
            path = ch["target"]["path"]
            sampler = samplers[ch["sampler"]]
            time_floats = [t[0] for t in read_accessor_floats(sampler["input"], 1)]
            val_floats = read_accessor_floats(sampler["output"], 4 if path == "rotation" else 3)

            keys = []
            for t, v in zip(time_floats, val_floats):
                keys.append((t, *v))

            if path == "translation":
                node_tracks[target_node]["t"] = keys
            elif path == "rotation":
                node_tracks[target_node]["r"] = keys
            elif path == "scale":
                node_tracks[target_node]["s"] = keys

        # Fill defaults from node bind pose if track is empty
        for j in skin_joints:
            n = nodes[j]
            t_def = n.get("translation", [0.0, 0.0, 0.0])
            r_def = n.get("rotation", [0.0, 0.0, 0.0, 1.0])
            s_def = n.get("scale", [1.0, 1.0, 1.0])
            if not node_tracks[j]["t"]:
                node_tracks[j]["t"] = [(0.0, *t_def)]
            if not node_tracks[j]["r"]:
                node_tracks[j]["r"] = [(0.0, *r_def)]
            if not node_tracks[j]["s"]:
                node_tracks[j]["s"] = [(0.0, *s_def)]

        # Find key bones
        hips_node = next(j for j in skin_joints if "Hips" in nodes[j].get("name", ""))
        chest_node = next(j for j in skin_joints if "Spine2" in nodes[j].get("name", ""))
        head_node = next(j for j in skin_joints if "Head" in nodes[j].get("name", "") and "Top" not in nodes[j].get("name", ""))
        rarm_node = next(j for j in skin_joints if "RightArm" in nodes[j].get("name", ""))
        rfore_node = next(j for j in skin_joints if "RightForeArm" in nodes[j].get("name", ""))

        # =========================================================================
        # 1. Export Native Walk.anim (stride cycle: 1.25s, in-place root motion)
        # =========================================================================
        walk_dur = 1.25
        walk_tracks = []
        for j in skin_joints:
            tr = node_tracks[j]
            t_k = [k for k in tr["t"] if k[0] <= walk_dur]
            r_k = [k for k in tr["r"] if k[0] <= walk_dur]
            s_k = [k for k in tr["s"] if k[0] <= walk_dur]
            if not t_k: t_k = tr["t"][:1]
            if not r_k: r_k = tr["r"][:1]
            if not s_k: s_k = tr["s"][:1]

            # In-place clamping for hips root motion (preserve vertical Y bob, lock X/Z)
            if j == hips_node and t_k:
                base_x = t_k[0][1]
                base_z = t_k[0][3]
                t_k = [(k[0], base_x, k[2], base_z) for k in t_k]

            walk_tracks.append((tr["name"], t_k, r_k, s_k))

        walk_path = os.path.join(ANIM_DIR, "Walk.anim")
        write_lab_anim(walk_path, "Walk", walk_dur, walk_tracks)
        print(f"  [OK] Exported native Walk.anim ({os.path.getsize(walk_path)} bytes, dur: {walk_dur}s)")

        # =========================================================================
        # 2. Export Native Idle.anim (calm breathing rest pose: 2.4s)
        # =========================================================================
        idle_dur = 2.4
        idle_tracks = []
        for j in skin_joints:
            tr = node_tracks[j]
            base_t = tr["t"][0][1:]
            base_r = tr["r"][0][1:]
            base_s = tr["s"][0][1:]

            t_k = [(0.0, *base_t), (idle_dur, *base_t)]
            s_k = [(0.0, *base_s), (idle_dur, *base_s)]

            if j == chest_node:
                # Subtle spine breathing pitch
                r_mid = quat_mul(base_r, quat_from_euler(-0.035, 0.0, 0.0))
                r_k = [(0.0, *base_r), (idle_dur * 0.5, *r_mid), (idle_dur, *base_r)]
            elif j == head_node:
                # Subtle head glance
                r_g1 = quat_mul(base_r, quat_from_euler(0.0, 0.04, 0.0))
                r_g2 = quat_mul(base_r, quat_from_euler(0.0, -0.04, 0.0))
                r_k = [(0.0, *base_r), (idle_dur * 0.3, *r_g1), (idle_dur * 0.7, *r_g2), (idle_dur, *base_r)]
            else:
                r_k = [(0.0, *base_r), (idle_dur, *base_r)]

            idle_tracks.append((tr["name"], t_k, r_k, s_k))

        idle_path = os.path.join(ANIM_DIR, "Idle.anim")
        write_lab_anim(idle_path, "Idle", idle_dur, idle_tracks)
        print(f"  [OK] Exported native Idle.anim ({os.path.getsize(idle_path)} bytes, dur: {idle_dur}s)")

        # =========================================================================
        # 3. Export Native Attack.anim / Shoot.anim (weapon recoil: 0.32s)
        # =========================================================================
        shoot_dur = 0.32
        attack_tracks = []
        for j in skin_joints:
            tr = node_tracks[j]
            base_t = tr["t"][0][1:]
            base_r = tr["r"][0][1:]
            base_s = tr["s"][0][1:]

            t_k = [(0.0, *base_t), (shoot_dur, *base_t)]
            s_k = [(0.0, *base_s), (shoot_dur, *base_s)]

            if j == rarm_node:
                # Right arm recoil lift
                r_peak = quat_mul(base_r, quat_from_euler(0.28, -0.03, 0.0))
                r_rec = quat_mul(base_r, quat_from_euler(0.10, -0.01, 0.0))
                r_k = [(0.0, *base_r), (0.04, *r_peak), (0.14, *r_rec), (shoot_dur, *base_r)]
            elif j == rfore_node:
                r_peak = quat_mul(base_r, quat_from_euler(0.18, 0.0, 0.0))
                r_k = [(0.0, *base_r), (0.04, *r_peak), (shoot_dur, *base_r)]
            elif j == chest_node:
                r_peak = quat_mul(base_r, quat_from_euler(-0.08, 0.0, 0.0))
                r_k = [(0.0, *base_r), (0.04, *r_peak), (shoot_dur, *base_r)]
            else:
                r_k = [(0.0, *base_r), (shoot_dur, *base_r)]

            attack_tracks.append((tr["name"], t_k, r_k, s_k))

        attack_path = os.path.join(ANIM_DIR, "Attack.anim")
        write_lab_anim(attack_path, "Attack", shoot_dur, attack_tracks)
        print(f"  [OK] Exported native Attack.anim ({os.path.getsize(attack_path)} bytes, dur: {shoot_dur}s)")

        # =========================================================================
        # 4. Export Native Talk.anim (expressive dialogue: 2.8s)
        # =========================================================================
        talk_dur = 2.8
        talk_tracks = []
        for j in skin_joints:
            tr = node_tracks[j]
            base_t = tr["t"][0][1:]
            base_r = tr["r"][0][1:]
            base_s = tr["s"][0][1:]

            t_k = [(0.0, *base_t), (talk_dur, *base_t)]
            s_k = [(0.0, *base_s), (talk_dur, *base_s)]

            if j == head_node:
                # Expressive head nods and emphasis tilts
                r_k = [
                    (0.0, *base_r),
                    (0.5, *quat_mul(base_r, quat_from_euler(0.06, 0.04, 0.0))),
                    (1.1, *quat_mul(base_r, quat_from_euler(-0.04, -0.03, 0.02))),
                    (1.8, *quat_mul(base_r, quat_from_euler(0.05, 0.02, -0.01))),
                    (2.4, *quat_mul(base_r, quat_from_euler(-0.02, 0.0, 0.0))),
                    (talk_dur, *base_r)
                ]
            elif j == chest_node:
                r_k = [
                    (0.0, *base_r),
                    (0.7, *quat_mul(base_r, quat_from_euler(-0.03, 0.02, 0.0))),
                    (1.9, *quat_mul(base_r, quat_from_euler(0.02, -0.02, 0.0))),
                    (talk_dur, *base_r)
                ]
            else:
                r_k = [(0.0, *base_r), (talk_dur, *base_r)]

            talk_tracks.append((tr["name"], t_k, r_k, s_k))

        talk_path = os.path.join(ANIM_DIR, "Talk.anim")
        write_lab_anim(talk_path, "Talk", talk_dur, talk_tracks)
        print(f"  [OK] Exported native Talk.anim ({os.path.getsize(talk_path)} bytes, dur: {talk_dur}s)")

if __name__ == "__main__":
    export_native_animations()
