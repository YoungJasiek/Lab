import urllib.request
import urllib.parse
import json

SUPABASE_URL = 'https://mbcfimyumbiftnkbwprc.supabase.co'
ANON_KEY = 'sb_publishable_JS8HiEj-4tmn2ksUbeQGkw_Y6DoQD6W'

posts = [
    {
        'title': 'Lab Engine Architecture & Project Hub Roadmap',
        'flair': 'ANNOUNCEMENT',
        'content': '''Official discussion thread for Lab Engine C++20 core, Direct State Access (OpenGL 4.5+ DSA), and the decoupled VFS architecture separating `engine://` assets from `Documents/LabGames/`.

### Modular Subsystems
All subsystem headers are compiled under `/include` and exported via 11 modular dynamic libraries:
- `Engine.dll` - Core loop and memory management
- `render.dll` - OpenGL 4.5+ DSA pipeline
- `audio.dll` - Spatial 3D sound synthesizer
- `physics.dll` - Rigid body simulation

```powershell
# Build the full modular engine from source
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```''',
        'author_name': 'YoungJasiek',
        'author_role': 'admin',
        'upvotes': 42,
        'is_pinned': True,
        'comments': [
            {
                'author_name': 'GordonFreeman',
                'author_role': 'admin',
                'content': 'Binary shader cache serialization is confirmed working on both AMD and Intel GPUs. Shader linking time dropped to near zero on warm restart.',
                'upvotes': 9
            },
            {
                'author_name': 'Modder_01',
                'author_role': 'user',
                'content': 'Is there an export script for Blender 4.x skeletal models with multiple animation clips?',
                'upvotes': 4,
                'reply': {
                    'author_name': 'YoungJasiek',
                    'author_role': 'admin',
                    'content': 'Yes, check `scripts/export_t800_animations.py`. It exports bone hierarchies and named keyframe clips directly into the engine format.',
                    'upvotes': 6
                }
            }
        ]
    },
    {
        'title': 'GPU Bindless Textures & Vertex Buffer Storage Performance Benchmarks',
        'flair': 'ENGINE',
        'content': '''Comparing `glNamedBufferStorage` with persistent mapped flags vs `glBufferSubData` across AMD and NVIDIA hardware.

In our tests with 10,000 dynamic drawcalls, persistent DSA mapping reduced render-thread CPU driver overhead by 4.2x:

```cpp
// Direct State Access Persistent Mapping Example
GLuint vbo = 0;
glCreateBuffers(1, &vbo);
GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
glNamedBufferStorage(vbo, bufferSize, nullptr, flags);
void* ptr = glMapNamedBufferRange(vbo, 0, bufferSize, flags);
```''',
        'author_name': 'GordonFreeman',
        'author_role': 'admin',
        'upvotes': 28,
        'is_pinned': False,
        'comments': [
            {
                'author_name': 'YoungJasiek',
                'author_role': 'admin',
                'content': 'Make sure `GL_MAP_COHERENT_BIT` and `GL_MAP_PERSISTENT_BIT` are strictly coupled with `glMemoryBarrier` before dispatch.',
                'upvotes': 7
            }
        ]
    },
    {
        'title': 'Frozen-Life: Tactical Bot AI State Machine Integration',
        'flair': 'FROZEN-LIFE',
        'content': '''The 5-state AI finite state machine (Patrol, Alert, Chase, Flank, Retreat) is now fully integrated with skeletal hitboxes and raycast line-of-sight checks.

### How to test:
1. Open `LabEditor`
2. Load map `test_multi_bot_waypoints.LABMAP`
3. Press F5 to simulate bot waypoint navigation with combat decision trees.''',
        'author_name': 'DevTeam',
        'author_role': 'dev',
        'upvotes': 19,
        'is_pinned': False,
        'comments': []
    }
]

for p in posts:
    # Check if exists
    chk_url = f"{SUPABASE_URL}/rest/v1/community_posts?title=eq.{urllib.parse.quote(p['title'])}"
    chk_req = urllib.request.Request(chk_url, headers={'apikey': ANON_KEY})
    existing = json.loads(urllib.request.urlopen(chk_req).read().decode())
    if existing:
        post_id = existing[0]['id']
        print(f"Post already exists: {p['title']} ({post_id})")
    else:
        post_data = {
            'title': p['title'],
            'flair': p['flair'],
            'content': p['content'],
            'author_name': p['author_name'],
            'author_role': p['author_role'],
            'upvotes': p['upvotes'],
            'is_pinned': p['is_pinned']
        }
        ins_req = urllib.request.Request(
            f"{SUPABASE_URL}/rest/v1/community_posts",
            data=json.dumps(post_data).encode(),
            headers={
                'apikey': ANON_KEY,
                'Content-Type': 'application/json',
                'Prefer': 'return=representation'
            },
            method='POST'
        )
        created = json.loads(urllib.request.urlopen(ins_req).read().decode())
        post_id = created[0]['id']
        print(f"Created real post: {p['title']} -> {post_id}")

    # Insert comments
    for c in p.get('comments', []):
        c_url = f"{SUPABASE_URL}/rest/v1/community_comments?post_id=eq.{post_id}&content=eq.{urllib.parse.quote(c['content'])}"
        c_req = urllib.request.Request(c_url, headers={'apikey': ANON_KEY})
        c_exist = json.loads(urllib.request.urlopen(c_req).read().decode())
        if not c_exist:
            c_data = {
                'post_id': post_id,
                'author_name': c['author_name'],
                'author_role': c['author_role'],
                'content': c['content'],
                'upvotes': c['upvotes']
            }
            ins_c = urllib.request.Request(
                f"{SUPABASE_URL}/rest/v1/community_comments",
                data=json.dumps(c_data).encode(),
                headers={
                    'apikey': ANON_KEY,
                    'Content-Type': 'application/json',
                    'Prefer': 'return=representation'
                },
                method='POST'
            )
            c_created = json.loads(urllib.request.urlopen(ins_c).read().decode())
            parent_id = c_created[0]['id']
            print(f"  Created comment from {c['author_name']} -> {parent_id}")
        else:
            parent_id = c_exist[0]['id']

        if 'reply' in c:
            rep = c['reply']
            rep_url = f"{SUPABASE_URL}/rest/v1/community_comments?post_id=eq.{post_id}&parent_id=eq.{parent_id}"
            rep_req = urllib.request.Request(rep_url, headers={'apikey': ANON_KEY})
            rep_exist = json.loads(urllib.request.urlopen(rep_req).read().decode())
            if not rep_exist:
                rep_data = {
                    'post_id': post_id,
                    'parent_id': parent_id,
                    'author_name': rep['author_name'],
                    'author_role': rep['author_role'],
                    'content': rep['content'],
                    'upvotes': rep['upvotes']
                }
                ins_rep = urllib.request.Request(
                    f"{SUPABASE_URL}/rest/v1/community_comments",
                    data=json.dumps(rep_data).encode(),
                    headers={
                        'apikey': ANON_KEY,
                        'Content-Type': 'application/json',
                        'Prefer': 'return=representation'
                    },
                    method='POST'
                )
                urllib.request.urlopen(ins_rep)
                print(f"    Created reply from {rep['author_name']}")
