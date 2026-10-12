/**
 * LAB DEVELOPER COMMUNITY – CLIENT SCRIPT
 * Multi-page MediaWiki/Wikipedia Architecture
 * Lab Engine, LabHammer 3D Level Editor & Frozen-Life (FL)
 */

document.addEventListener('DOMContentLoaded', () => {
  // Permanently enforce authentic 2007 dark VDC theme
  try {
    localStorage.removeItem('lab_doc_theme');
    document.documentElement.setAttribute('data-theme', 'dark');
  } catch (e) {}

  initMobileSidebar();
  initTocToggle();
  initActiveNav();
  initWikiSearch();
  initCodeCopy();
  initWikiLightbox();
  initSourceTabs();
  initTalkDiscussionSystem();
});

/* ==========================================================================
   2. MOBILE SIDEBAR DRAWER
   ========================================================================== */
function initMobileSidebar() {
  const burger = document.querySelector('.vdc-mobile-burger');
  const sidebar = document.querySelector('.vdc-sidebar');
  if (!burger || !sidebar) return;

  burger.addEventListener('click', () => {
    sidebar.classList.toggle('open');
  });

  sidebar.querySelectorAll('a').forEach(link => {
    link.addEventListener('click', () => {
      sidebar.classList.remove('open');
    });
  });
}

/* ==========================================================================
   3. TABLE OF CONTENTS (TOC) COLLAPSIBLE TOGGLE
   ========================================================================== */
function initTocToggle() {
  const toggleBtn = document.getElementById('toctogglelink');
  const tocList = document.getElementById('toc-list');
  if (!toggleBtn || !tocList) return;

  toggleBtn.addEventListener('click', (e) => {
    e.preventDefault();
    const isHidden = tocList.style.display === 'none';
    tocList.style.display = isHidden ? 'block' : 'none';
    toggleBtn.textContent = isHidden ? 'hide' : 'show';
  });
}

/* ==========================================================================
   4. ACTIVE NAVIGATION LINK DETECTION
   ========================================================================== */
function initActiveNav() {
  const currentPath = window.location.pathname.split('/').pop() || 'index.html';
  
  // Highlight active sidebar item
  document.querySelectorAll('.sidebar-links a').forEach(link => {
    const href = link.getAttribute('href');
    if (!href) return;
    const linkFile = href.split('#')[0].split('/').pop();
    if (linkFile === currentPath || (currentPath === '' && linkFile === 'index.html')) {
      link.classList.add('active');
    } else {
      link.classList.remove('active');
    }
  });
}

/* ==========================================================================
   5. MULTI-PAGE WIKIPEDIA SEARCH INDEX & AUTOCOMPLETE DROPDOWN
   ========================================================================== */
const WIKI_SEARCH_INDEX = [
  // index.html
  { title: "Main Page", url: "index.html", cat: "Portal", snippet: "Lab Developer Community central documentation hub and overview." },
  { title: "Project Hub & Main Menu", url: "project-hub.html", cat: "Portal", snippet: "Engine main menu, project launcher, asset decoupling (engine:// vs game://), and standalone exporter." },
  { title: "Modular Drop-in Plugins", url: "plugins.html", cat: "Portal", snippet: "Decentralized single-file Lua plugins with in-engine Package Manager GUI." },
  { title: "Featured Subject: LabHammer", url: "labhammer.html", cat: "Portal", snippet: "The official standalone 3D level editor for the Lab Engine and Frozen-Life." },
  { title: "Latest Release: Frozen-Life Alpha", url: "frozen-life.html", cat: "Portal", snippet: "First playable test build v0.1.0-alpha featuring 2 maps and multiplayer foundation." },

  // plugins.html
  { title: "Modular Plugins: Single-File Architecture", url: "plugins.html#sec-philosophy", cat: "Plugins", snippet: "Decentralized single-file Lua plugin architecture without monolithic script merges." },
  { title: "Modular Plugins: Drop-in Discovery", url: "plugins.html#sec-discovery", cat: "Plugins", snippet: "Filesystem scan of plugins/*.dll with standard metadata headers." },
  { title: "Modular Plugins: Package Manager GUI", url: "plugins.html#sec-package-manager", cat: "Plugins", snippet: "In-engine Package Manager modal with search, category filters, and live toggle switches." },
  { title: "Modular Plugins: 9 Showcase Plugins", url: "plugins.html#sec-showcase", cat: "Plugins", snippet: "Showcase plugins including AI Patrol, Vehicle Physics, Dialogue Quest, CRT Shaders, and Voxel Terrain." },
  { title: "Modular Plugins: Life Cycle & API", url: "plugins.html#sec-api", cat: "Plugins", snippet: "onLoad(), onUnload(), onUpdate(dt), onRender(), and onEditorGui() life cycle callbacks." },

  // project-hub.html
  { title: "Project Hub: Main Menu & Launcher", url: "project-hub.html#sec-overview", cat: "Project Hub", snippet: "Engine main menu workspace for launching, switching, and creating independent game projects." },
  { title: "Project Hub: Asset Decoupling", url: "project-hub.html#sec-decoupling", cat: "Project Hub", snippet: "Decoupled engine assets in engine/assets/ vs game assets in Documents/LabGames/." },
  { title: "Project Hub: Virtual URI Resolution", url: "project-hub.html#sec-virtual-uri", cat: "Project Hub", snippet: "Unified virtual URI system resolving engine:// and game:// with automatic fallback." },
  { title: "Project Hub: Standalone Exporter", url: "project-hub.html#sec-exporter", cat: "Project Hub", snippet: "One-click build pipeline bundling standalone demo distributions with LabPlayer.exe." },
  { title: "Project Hub: Frozen-Life Showcase", url: "project-hub.html#sec-frozen-life", cat: "Project Hub", snippet: "Flagship 3D showcase game located in games/frozen_life/ with standalone demo and native HUD." },
  
  // labhammer.html
  { title: "LabHammer Overview", url: "labhammer.html#sec-overview", cat: "LabHammer", snippet: "Standalone 3D world creation suite linked directly to LabEngineLib." },
  { title: "LabHammer: 3D Viewports & Camera", url: "labhammer.html#sec-navigation", cat: "LabHammer", snippet: "Fly camera controls, RMB+WASD navigation, and Shift speed acceleration (15 to 35 m/s)." },
  { title: "LabHammer: 6 Core Editor Tools", url: "labhammer.html#sec-tools", cat: "LabHammer", snippet: "Raycast Picker, Brush Cube, 3D STL Prop, Texture Pipette, Airlock Door, and Spawn Points." },
  { title: "LabHammer: CSG Geometry Clip Tool", url: "labhammer.html#sec-csg", cat: "LabHammer", snippet: "Interactive Sutherland-Hodgman brush slicing with Shift+X, KeepFront/Back/Both, and cap triangulation." },
  { title: "LabHammer: Physical Props (Rigid Bodies)", url: "labhammer.html#sec-props", cat: "LabHammer", snippet: "Placing destructible wooden crates (prop_crate) and explosive red barrels (prop_barrel) in Hammer." },
  { title: "LabHammer: Entity Properties Inspector", url: "labhammer.html#sec-properties", cat: "LabHammer", snippet: "UV scaling, texture assignment, door speeds, spawn modes, and prop parameters." },
  { title: "LabHammer: .labmap File Format", url: "labhammer.html#sec-labmap-format", cat: "LabHammer", snippet: "Plain-text human-readable format for level geometry, poly_brush polyhedra, entities, and props." },
  { title: "LabHammer: Keyboard Shortcuts", url: "labhammer.html#sec-shortcuts", cat: "LabHammer", snippet: "Complete keybindings cheat sheet including F9 hot-launch testing and Shift+X CSG clipping." },
  { title: "LabHammer: Screenshots Gallery", url: "labhammer.html#sec-gallery", cat: "LabHammer", snippet: "In-editor UI, CSG cutting plane visualization, properties inspector, and entity placement." },

  // frozen-life.html
  { title: "Frozen-Life Overview", url: "frozen-life.html#sec-overview", cat: "Frozen-Life", snippet: "Tactical retro-modern FPS built on top of the Lab Engine." },
  { title: "Frozen-Life: Playable Test Build", url: "frozen-life.html#sec-overview", cat: "Frozen-Life", snippet: "Official v0.1.0-alpha standalone test build specifications and executable Lab.exe." },
  { title: "Frozen-Life: Playable Maps", url: "frozen-life.html#sec-maps", cat: "Frozen-Life", snippet: "Detailed map breakdown of cryo_outpost and facility_alpha environments." },
  { title: "Frozen-Life: Tactical FPP Arms & Animations", url: "frozen-life.html#sec-systems", cat: "Frozen-Life", snippet: "Visible glove suit, socket bone attachments, full reload sequence, and weapon inspection (V key)." },
  { title: "Frozen-Life: Half-Life 2 Flashlight & Shadows", url: "frozen-life.html#sec-systems", cat: "Frozen-Life", snippet: "Tactical flashlight on F key with dynamic 2048x2048 shadow mapping and 3x3 PCF filter." },
  { title: "Frozen-Life: 3D Rigid Body Physics & Destruction", url: "frozen-life.html#sec-physics", cat: "Frozen-Life", snippet: "Destructible wooden crates breaking into 7 planks, explosive barrels with 6.5m blast, and bot kinetic knockback." },
  { title: "Frozen-Life: Projective Decal System", url: "frozen-life.html#sec-decals", cat: "Frozen-Life", snippet: "Bullet holes on concrete and metal, bot blood splatters, and explosion scorch marks with depth bias." },
  { title: "Frozen-Life: Interactive Terminals & Triggers", url: "frozen-life.html#sec-interactive", cat: "Frozen-Life", snippet: "In-world 3D CRT screens with scanlines, status LEDs, and E key (+use) airlock door override logic." },
  { title: "Frozen-Life: 3D Spatial Audio Engine", url: "frozen-life.html#sec-audio", cat: "Frozen-Life", snippet: "miniaudio integration with 26 procedural 16-bit PCM WAV synthesizers and 3D listener attenuation." },
  { title: "Frozen-Life: Controls & Mechanics", url: "frozen-life.html#sec-controls", cat: "Frozen-Life", snippet: "WASD, Space jump, Shift sprint, F flashlight, V inspect, E interact, LMB fire, Tab scoreboard." },
  { title: "Frozen-Life: Screenshots Gallery", url: "frozen-life.html#sec-gallery", cat: "Frozen-Life", snippet: "In-game combat, flashlight, destruction, terminals, decals, HUD, and scoreboard." },

  // architecture.html
  { title: "Engine Architecture Overview", url: "architecture.html#sec-philosophy", cat: "Architecture", snippet: "Modern C++20 design principles, zero-cost abstractions, and strict RAII." },
  { title: "C++20 & Memory Safety", url: "architecture.html#sec-memory", cat: "Architecture", snippet: "Elimination of raw new/delete, Rule of Five, std::unique_ptr and std::span." },
  { title: "Fixed Timestep & Subsystems", url: "architecture.html#sec-loop", cat: "Architecture", snippet: "Deterministic 60Hz physics accumulator and subsystem lifecycle orchestration." },
  { title: "LabPhysics: 3D Rigid Body Dynamics", url: "architecture.html#sec-physics-engine", cat: "Architecture", snippet: "Newtonian rigid bodies, inertia tensors, semi-implicit Euler, OBB/AABB contact manifolds." },
  { title: "LabAudio: 3D Spatial Audio & Synths", url: "architecture.html#sec-audio-engine", cat: "Architecture", snippet: "miniaudio v0.11.25, 26 procedural WAV synthesizers, 64-voice memory-safe audio pool." },
  { title: "LabCSG: Sutherland-Hodgman Polygon Clipping", url: "architecture.html#sec-csg-math", cat: "Architecture", snippet: "Convex polyhedron clipping, cap triangulation, and planar UV calculation." },
  { title: "LabDecals: Projective Decal System", url: "architecture.html#sec-decals-engine", cat: "Architecture", snippet: "Depth bias glPolygonOffset(-2.0, -2.0), 256 circular instance buffer, and smooth alpha fade." },
  { title: "LabSkeletal: glTF 2.0 & GPU Vertex Skinning", url: "architecture.html#sec-skeletal-engine", cat: "Architecture", snippet: "Quaternions with slerp, 20-bone rig, 6 animation clips, and bone socket attachments." },
  { title: "Slab Ray-AABB Collision Math", url: "architecture.html#sec-collision", cat: "Architecture", snippet: "Optimized branchless slab method for bullet and raycast intersection against AABBs." },
  { title: "C++ Source Code Inspector", url: "architecture.html#sec-source-code", cat: "Architecture", snippet: "Interactive browser for LabCombat.h, LabRenderer.h, and engine headers." },

  // opengl-dsa.html
  { title: "Direct State Access (DSA) Overview", url: "opengl-dsa.html#sec-overview", cat: "OpenGL DSA", snippet: "Modern OpenGL 4.5+ Direct State Access bypassing global context binding." },
  { title: "Dynamic Shadow Mapping Pipeline", url: "opengl-dsa.html#sec-shadows", cat: "OpenGL DSA", snippet: "2048x2048 GL_DEPTH_COMPONENT24 FBO, two-pass rendering, and 3x3 PCF filter." },
  { title: "GPU Vertex Skinning in DSA", url: "opengl-dsa.html#sec-skinning", cat: "OpenGL DSA", snippet: "Hardware vertex skinning with aBoneIDs, aBoneWeights, and uBoneMatrices[64]." },
  { title: "Classic vs Direct State Access", url: "opengl-dsa.html#sec-comparison", cat: "OpenGL DSA", snippet: "Side-by-side comparison table between legacy bind-to-edit and modern DSA." },
  { title: "DSA Texture Storage", url: "opengl-dsa.html#sec-textures", cat: "OpenGL DSA", snippet: "glCreateTextures and glTextureStorage2D for immutable mipmapped textures." },
  { title: "DSA Vertex Arrays & Buffers", url: "opengl-dsa.html#sec-buffers", cat: "OpenGL DSA", snippet: "glCreateBuffers and glNamedBufferStorage for efficient GPU memory allocation." },
  { title: "Source-Engine Visual Aesthetic", url: "opengl-dsa.html#sec-aesthetics", cat: "OpenGL DSA", snippet: "Blinn-Phong lighting, sharp normals, high contrast, subtle bloom, and MSAA." },
  { title: "Driver Overhead & Performance", url: "opengl-dsa.html#sec-performance", cat: "OpenGL DSA", snippet: "Eliminating driver state validation overhead with DSA calls." },

  // building.html
  { title: "Building from Source Overview", url: "building.html#sec-prerequisites", cat: "Compiling", snippet: "How to clone, configure, and compile Lab Engine and LabHammer using CMake." },
  { title: "Toolchain Prerequisites", url: "building.html#sec-prerequisites", cat: "Compiling", snippet: "C++20 compiler (MSVC 2022 / Clang 16+), CMake 3.22+, and OpenGL 4.5+ GPU drivers." },
  { title: "CMake Build Commands", url: "building.html#sec-commands", cat: "Compiling", snippet: "PowerShell build script using CMake and MSVC Release configuration." },
  { title: "Repository Directory Structure", url: "building.html#sec-hierarchy", cat: "Compiling", snippet: "Directory tree showing src/, assets/, maps/, and third-party libraries." },
  { title: "Troubleshooting Build Errors", url: "building.html#sec-troubleshooting", cat: "Compiling", snippet: "Solutions for missing OpenGL 4.5 DSA entry points, GLFW linking, and asset paths." },

  // modules.html & Source Subsystem DLLs
  { title: "Engine Subsystem Modules (DLLs)", url: "modules.html", cat: "Architecture", snippet: "Source-style decoupled architecture: server.dll, client.dll, physics.dll, Engine.dll via pure virtual interfaces." },
  { title: "server.dll: Authoritative Server Subsystem", url: "modules.html#sec-source-arch", cat: "Subsystems", snippet: "IServerSubsystem 64/128Hz fixed tick, authoritative ECS entity spawning, delta compression, headless runner." },
  { title: "client.dll: Client Presentation Subsystem", url: "modules.html#sec-source-arch", cat: "Subsystems", snippet: "IClientSubsystem presentation layer, circular snapshot history, state interpolation, prediction, zero GPU coupling." },
  { title: "physics.dll: Vehicles, Water & Ragdolls", url: "module-physics.html", cat: "Subsystems", snippet: "Raycast suspension vehicles, Archimedes fluid buoyancy, Gerstner waves, articulated ragdolls, and SAT 3D contact manifold." },
  { title: "Engine.dll / Core: Kernel Subsystem", url: "module-core.html", cat: "Subsystems", snippet: "Engine kernel lifecycle, fixed timestep timing, input polling, Vec2/Vec3/Mat4 math, and camera transforms." },
  { title: "render.dll: DSA Rendering Subsystem", url: "module-render.html", cat: "Subsystems", snippet: "OpenGL 4.5+ DSA pipeline, CSG brush clipping, projective decals, PBR lighting, and skeletal skinning." },
  { title: "audio.dll: Spatial Audio Subsystem", url: "module-audio.html", cat: "Subsystems", snippet: "miniaudio hardware backend, 3D spatial listener attenuation, 26 procedural PCM sound synthesizers." },
  { title: "network.dll: UDP Network Subsystem", url: "module-network.html", cat: "Subsystems", snippet: "Ultra-low latency UDP client-server networking, client prediction, reconciliation, and LAN discovery." },

  // multiplayer.html & Generic ECS Networking
  { title: "Generic ECS Networking & Replication", url: "multiplayer.html", cat: "Networking", snippet: "Data-driven binary state replication via NetworkIdentityComponent and NetworkTransformComponent." },
  { title: "ECS Networking: EntitySpawn & Destroy", url: "multiplayer.html#sec-protocol", cat: "Networking", snippet: "NetMsgType::EntitySpawn and NetMsgType::EntityDestroy binary packet protocol." },
  { title: "ECS Networking: EntitySync & Delta Snapshots", url: "multiplayer.html#sec-protocol", cat: "Networking", snippet: "NetMsgType::EntitySync state interpolation, tick sequence tracking, and delta payload packing." },
  { title: "ECS Networking: Prediction & Reconciliation", url: "multiplayer.html#sec-prediction", cat: "Networking", snippet: "Client input buffer replay, zero-desync server reconciliation, and ground clamping." },

  // Cook-Torrance PBR & Lighting
  { title: "Cook-Torrance Microfacet PBR BRDF", url: "opengl-dsa.html#sec-pbr", cat: "Graphics", snippet: "Physically based rendering: GGX normal distribution (D), Smith geometry shadowing (G), and Fresnel-Schlick (F)." },
  { title: "Unified Shadow Atlas (1024x1024)", url: "opengl-dsa.html#sec-shadows", cat: "Graphics", snippet: "Single 4-tile depth FBO shadow atlas eliminating framebuffer switching and driver state churn." },
  { title: "Dynamic Lighting & Forward Multi-Light", url: "lighting.html", cat: "Graphics", snippet: "Forward rendering up to 16 point lights, quadratic attenuation, Blinn-Phong specular, and Kelvin presets." },

  // Modern .labscene & LabHammer
  { title: "Modern .labscene JSON Scene Graph", url: "labhammer.html#sec-labscene-format", cat: "Editor", snippet: "Universal structured JSON scene graph format replacing legacy text tokens, with automatic migration." },
  { title: "Scene Migration & .backup Modal", url: "labhammer.html#sec-migration", cat: "Editor", snippet: "Non-destructive automatic migration modal prompting user to upgrade .labmap to .labscene with safety backups." },
  { title: "Decoupled Camera & ScreenToWorld", url: "architecture.html", cat: "Architecture", snippet: "Decoupled CameraComponent with bidirectional ScreenToWorld and WorldToScreen coordinate projections." },
  { title: "Object-Centered Pivot Gizmo", url: "labhammer.html#sec-gizmo", cat: "Editor", snippet: "3D translation and rotation gizmo centered on entity bounding box, with bracket hotkeys ([ / ])." },

  // QA & Verification
  { title: "Automated QA: 87/87 Tests Passed", url: "index.html#sec-qa-matrix", cat: "QA & Verification", snippet: "Zero-regression test suite passing 87/87 unit and integration tests via TestVerify.exe (100% pass rate)." },
  { title: "VFS Sandbox Isolation (0 Leakage)", url: "project-hub.html#sec-decoupling", cat: "Architecture", snippet: "Strict virtual filesystem isolation separating engine:// from game:// in Documents/LabGames with 0 leakage." },

  // Specialized Subsystems & Tools
  { title: "Combat Bot AI & 5-State FSM", url: "bot-ai.html", cat: "AI", snippet: "5-state finite state machine (Patrol, Alert, Engage, Cover, Retreat), slab ray-AABB line of sight, and human recoil cadence." },
  { title: "3D Audio & Synthesizers", url: "audio-synthesizer.html", cat: "Audio", snippet: "26 procedural 16-bit PCM sound synthesizers, real-time sliding window RMS lip-sync, and miniaudio 3D spatial attenuation." },
  { title: "Character & Weapon Studio", url: "character-studio.html", cat: "Studio", snippet: "Dual-socket weapon grip poser, independent weapon translation, FBX 23-clip preview grid, and PBR skins." },
  { title: "Lab HUD: 2D Canvas & GUI Editor", url: "labhud.html", cat: "HUD", snippet: "Standalone 2D canvas layout authoring suite, responsive anchors, widget hierarchy, and .labhud format." },
  { title: "Skeletal FBX & Mixamo Animations", url: "skeletal-animations.html", cat: "Animation", snippet: "Mixamo 52-bone humanoid retargeting, LAB_ANIM 0.05ms binary cache, and GPU Linear Blend Skinning." },
  { title: "Tactical Weapons & Viewmodel Dynamics", url: "weapons-viewmodel.html", cat: "Weapons", snippet: "9-weapon ballistics table, dual-spring damper inertia sway, and tactical walking roll sway (~1.45 deg)." },
  { title: "Lua 5.4 Scripting & Balance", url: "lua-scripting.html", cat: "Scripting", snippet: "Decoupled balance tables (scripts/balance.lua), developer console (~), C-API bindings, and hot reloading." },
  { title: "LabEditor Unified Multi-Workspace Suite", url: "labeditor.html", cat: "Editor", snippet: "5 unified workspaces: 3D Level, 2D Tilemap, Studio Posing, HUD Designer, Full-Viewport Scripting IDE, and PIE." }
];

function initWikiSearch() {
  const searchBar = document.querySelector('.vdc-search-bar');
  const searchInput = document.getElementById('vdc-search-input');
  if (!searchBar || !searchInput) return;

  // Create Dropdown Container
  let dropdown = document.querySelector('.vdc-search-dropdown');
  if (!dropdown) {
    dropdown = document.createElement('div');
    dropdown.className = 'vdc-search-dropdown';
    dropdown.setAttribute('role', 'listbox');
    searchBar.appendChild(dropdown);
  }

  let selectedIdx = -1;

  searchInput.addEventListener('input', (e) => {
    const query = e.target.value.toLowerCase().trim();
    if (!query) {
      dropdown.classList.remove('active');
      dropdown.innerHTML = '';
      selectedIdx = -1;
      return;
    }

    const matches = WIKI_SEARCH_INDEX.filter(item => 
      item.title.toLowerCase().includes(query) ||
      item.snippet.toLowerCase().includes(query) ||
      item.cat.toLowerCase().includes(query)
    ).slice(0, 8);

    if (matches.length === 0) {
      dropdown.innerHTML = `<div class="search-res-empty">No wiki articles found for "<strong>${escapeHtml(query)}</strong>"</div>`;
      dropdown.classList.add('active');
      selectedIdx = -1;
      return;
    }

    dropdown.innerHTML = matches.map((item, idx) => `
      <a href="${item.url}" class="search-result-item" data-index="${idx}">
        <div class="search-res-title">
          <span>${highlightMatch(item.title, query)}</span>
          <span class="search-res-badge">${item.cat}</span>
        </div>
        <div class="search-res-snippet">${highlightMatch(item.snippet, query)}</div>
      </a>
    `).join('');

    dropdown.classList.add('active');
    selectedIdx = -1;
  });

  // Keyboard navigation
  searchInput.addEventListener('keydown', (e) => {
    const items = dropdown.querySelectorAll('.search-result-item');
    if (!items.length || !dropdown.classList.contains('active')) return;

    if (e.key === 'ArrowDown') {
      e.preventDefault();
      selectedIdx = (selectedIdx + 1) % items.length;
      updateSelection(items);
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      selectedIdx = (selectedIdx - 1 + items.length) % items.length;
      updateSelection(items);
    } else if (e.key === 'Enter') {
      if (selectedIdx >= 0 && items[selectedIdx]) {
        e.preventDefault();
        items[selectedIdx].click();
      }
    } else if (e.key === 'Escape') {
      dropdown.classList.remove('active');
      selectedIdx = -1;
    }
  });

  function updateSelection(items) {
    items.forEach((item, idx) => {
      if (idx === selectedIdx) {
        item.classList.add('selected');
        item.scrollIntoView({ block: 'nearest' });
      } else {
        item.classList.remove('selected');
      }
    });
  }

  // Close dropdown when clicking outside
  document.addEventListener('click', (e) => {
    if (!searchBar.contains(e.target)) {
      dropdown.classList.remove('active');
      selectedIdx = -1;
    }
  });

  function highlightMatch(text, query) {
    const idx = text.toLowerCase().indexOf(query);
    if (idx === -1) return escapeHtml(text);
    const before = text.substring(0, idx);
    const matched = text.substring(idx, idx + query.length);
    const after = text.substring(idx + query.length);
    return `${escapeHtml(before)}<mark style="background: rgba(56,189,248,0.25); color: inherit; padding: 0 1px;">${escapeHtml(matched)}</mark>${escapeHtml(after)}`;
  }

  function escapeHtml(str) {
    return str.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }
}

/* ==========================================================================
   6. CODE SNIPPET COPY TO CLIPBOARD
   ========================================================================== */
function initCodeCopy() {
  document.querySelectorAll('.btn-wiki-copy').forEach(btn => {
    btn.addEventListener('click', async () => {
      const codeId = btn.getAttribute('data-target');
      const codeEl = document.getElementById(codeId);
      if (!codeEl) return;

      try {
        await navigator.clipboard.writeText(codeEl.innerText);
        const originalText = btn.textContent;
        btn.textContent = 'Copied!';
        btn.style.color = 'var(--wiki-success)';
        setTimeout(() => { 
          btn.textContent = originalText; 
          btn.style.color = '';
        }, 1800);
      } catch (err) {
        btn.textContent = 'Error';
      }
    });
  });
}

/* ==========================================================================
   7. INTERACTIVE SOURCE CODE TABS (FOR ARCHITECTURE INSPECTOR)
   ========================================================================== */
function initSourceTabs() {
  const codeTabs = document.querySelectorAll('.source-tab-btn');
  const codePanels = document.querySelectorAll('.source-code-panel');
  if (!codeTabs.length || !codePanels.length) return;

  codeTabs.forEach(btn => {
    btn.addEventListener('click', () => {
      const targetPanelId = btn.getAttribute('data-tab');
      codeTabs.forEach(b => b.classList.remove('active'));
      codePanels.forEach(p => p.style.display = 'none');

      btn.classList.add('active');
      const targetPanel = document.getElementById(targetPanelId);
      if (targetPanel) {
        targetPanel.style.display = 'block';
      }
    });
  });
}

/* ==========================================================================
   8. WIKI THUMBNAIL LIGHTBOX
   ========================================================================== */
function initWikiLightbox() {
  const modal = document.getElementById('vdc-lightbox');
  const imgEl = document.getElementById('lightbox-img');
  const titleEl = document.getElementById('lightbox-title');
  const closeBtn = document.getElementById('lightbox-close');
  const prevBtn = document.getElementById('lightbox-prev');
  const nextBtn = document.getElementById('lightbox-next');

  if (!modal || !imgEl) return;

  const thumbs = Array.from(document.querySelectorAll('.thumbinner'));
  let currentIdx = 0;

  thumbs.forEach((thumb, idx) => {
    thumb.style.cursor = 'pointer';
    thumb.addEventListener('click', () => {
      currentIdx = idx;
      openModal();
    });
  });

  function openModal() {
    const thumb = thumbs[currentIdx];
    if (!thumb) return;
    const img = thumb.querySelector('.thumbimage');
    const caption = thumb.querySelector('.thumbcaption');

    imgEl.src = img.getAttribute('data-full') || img.src;
    imgEl.alt = img.alt;
    titleEl.textContent = caption ? caption.innerText.replace('🔍', '').trim() : img.alt;

    modal.classList.add('active');
    modal.setAttribute('aria-hidden', 'false');
  }

  function closeModal() {
    modal.classList.remove('active');
    modal.setAttribute('aria-hidden', 'true');
  }

  if (closeBtn) closeBtn.addEventListener('click', closeModal);
  modal.addEventListener('click', (e) => {
    if (e.target === modal) closeModal();
  });

  if (nextBtn) {
    nextBtn.addEventListener('click', () => {
      currentIdx = (currentIdx + 1) % thumbs.length;
      openModal();
    });
  }

  if (prevBtn) {
    prevBtn.addEventListener('click', () => {
      currentIdx = (currentIdx - 1 + thumbs.length) % thumbs.length;
      openModal();
    });
  }

  window.addEventListener('keydown', (e) => {
    if (!modal.classList.contains('active')) return;
    if (e.key === 'Escape') closeModal();
    else if (e.key === 'ArrowRight') { currentIdx = (currentIdx + 1) % thumbs.length; openModal(); }
    else if (e.key === 'ArrowLeft') { currentIdx = (currentIdx - 1 + thumbs.length) % thumbs.length; openModal(); }
  });
}

/* ==========================================================================
   9. INTERACTIVE MEDIAWIKI TALK / DISCUSSION SYSTEM (talk.html)
   Supports:
   - Creating new discussion topics (== Section ==) with TOC auto-update
   - Replying inline to any existing thread ([reply | edit]) with ':' indentation
   - MediaWiki markup parsing ('''bold''', ''italic'', [[Link]], <code>, <pre>, ~~~~)
   - Live Preview & automatic UTC timestamp signature generation
   - Persistent storage in localStorage
   ========================================================================== */
function initTalkDiscussionSystem() {
  const talkBody = document.querySelector('.talk-body');
  if (!talkBody) return;

  const STORAGE_TOPICS_KEY = 'lab_wiki_talk_topics_v1';
  const STORAGE_REPLIES_KEY = 'lab_wiki_talk_replies_v1';
  const STORAGE_AUTHOR_KEY = 'lab_wiki_talk_author_v1';

  function getSavedAuthor() {
    try {
      return localStorage.getItem(STORAGE_AUTHOR_KEY) || 'YoungJasiek';
    } catch (e) {
      return 'YoungJasiek';
    }
  }

  function setSavedAuthor(name) {
    try {
      if (name && name.trim()) {
        localStorage.setItem(STORAGE_AUTHOR_KEY, name.trim());
      }
    } catch (e) {}
  }

  function formatUtcTimestamp(dateObj) {
    const d = dateObj || new Date();
    const months = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];
    const hh = String(d.getUTCHours()).padStart(2, '0');
    const mm = String(d.getUTCMinutes()).padStart(2, '0');
    const day = d.getUTCDate();
    const mon = months[d.getUTCMonth()];
    const yr = d.getUTCFullYear();
    return `${hh}:${mm}, ${day} ${mon} ${yr} (UTC)`;
  }

  function escapeBasicHtml(str) {
    return str
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;');
  }

  function buildSignatureHtml(author, timestampStr) {
    const safeAuthor = escapeBasicHtml((author || 'YoungJasiek').trim() || 'YoungJasiek');
    const ts = timestampStr || formatUtcTimestamp(new Date());
    return `<span class="talk-sig">&mdash;<a href="https://github.com/YoungJasiek/Lab" target="_blank" rel="noopener noreferrer" class="mw-userlink">${safeAuthor}</a> (<a href="#editor-section" class="mw-usertalk">talk</a>) ${ts}</span>`;
  }

  function parseWikitext(rawText, author, timestampStr) {
    if (!rawText) return '';
    const sigFull = buildSignatureHtml(author, timestampStr);
    const safeAuthor = escapeBasicHtml((author || 'YoungJasiek').trim() || 'YoungJasiek');
    const sigNameOnly = `<a href="https://github.com/YoungJasiek/Lab" target="_blank" rel="noopener noreferrer" class="mw-userlink">${safeAuthor}</a> (<a href="#editor-section" class="mw-usertalk">talk</a>)`;

    const hasTildeSig = /~~~/.test(rawText);

    // Extract <pre>...</pre> and <code>...</code> blocks before escaping
    const preBlocks = [];
    let text = rawText.replace(/<pre>([\s\S]*?)<\/pre>/gi, (_, code) => {
      const idx = preBlocks.length;
      preBlocks.push(`<pre>${escapeBasicHtml(code.replace(/^\n+|\n+$/g, ''))}</pre>`);
      return `@@PRE_BLOCK_${idx}@@`;
    });

    const codeBlocks = [];
    text = text.replace(/<code>([\s\S]*?)<\/code>/gi, (_, code) => {
      const idx = codeBlocks.length;
      codeBlocks.push(`<code>${escapeBasicHtml(code)}</code>`);
      return `@@CODE_BLOCK_${idx}@@`;
    });

    // Escape remaining HTML
    text = escapeBasicHtml(text);

    // MediaWiki bold '''...''' and italic ''...''
    text = text.replace(/'''([^']+?)'''/g, '<strong>$1</strong>');
    text = text.replace(/''([^']+?)''/g, '<em>$1</em>');

    // MediaWiki links [[target|label]] or [[target]]
    text = text.replace(/\[\[([^\]|]+)\|([^\]]+)\]\]/g, (_, target, label) => {
      const href = target.endsWith('.html') || target.startsWith('#') ? target : 'index.html';
      return `<a href="${href}">${label}</a>`;
    });
    text = text.replace(/\[\[([^\]]+)\]\]/g, (_, target) => {
      const href = target.endsWith('.html') || target.startsWith('#') ? target : 'index.html';
      return `<a href="${href}">${target}</a>`;
    });

    // Replace ~~~~ and ~~~
    text = text.replace(/~~~~/g, sigFull);
    text = text.replace(/~~~/g, sigNameOnly);

    // Restore inline code and pre blocks
    text = text.replace(/@@CODE_BLOCK_(\d+)@@/g, (_, i) => codeBlocks[Number(i)]);
    text = text.replace(/@@PRE_BLOCK_(\d+)@@/g, (_, i) => preBlocks[Number(i)]);

    // Convert line breaks
    text = text.replace(/\n\n+/g, '</p><p>').replace(/\n/g, '<br>');

    if (!hasTildeSig) {
      text += ' ' + sigFull;
    }
    return text;
  }

  function insertAtCursor(textarea, before, after, sampleText) {
    if (!textarea) return;
    const start = textarea.selectionStart || 0;
    const end = textarea.selectionEnd || 0;
    const selected = textarea.value.substring(start, end) || sampleText || '';
    const replacement = before + selected + (after || '');
    textarea.value = textarea.value.substring(0, start) + replacement + textarea.value.substring(end);
    textarea.focus();
    textarea.selectionStart = start + before.length;
    textarea.selectionEnd = start + before.length + selected.length;
  }

  function appendReplyDomToSection(headingEl, replyObj) {
    if (!headingEl) return;
    // Find the end of this section (right before the next h2/h3 or end of .talk-body)
    let cursor = headingEl.nextElementSibling;
    let lastInSection = headingEl;
    while (cursor && cursor.tagName !== 'H2' && cursor.tagName !== 'H3' && !cursor.classList.contains('mw-inline-reply-form')) {
      lastInSection = cursor;
      cursor = cursor.nextElementSibling;
    }

    const depth = Math.max(1, Math.min(4, Number(replyObj.indent) || 1));
    const parsedHtml = parseWikitext(replyObj.body, replyObj.author, replyObj.timestamp);

    let wrapper = document.createElement('dl');
    let inner = wrapper;
    for (let i = 1; i < depth; i++) {
      const dd = document.createElement('dd');
      const nextDl = document.createElement('dl');
      dd.appendChild(nextDl);
      inner.appendChild(dd);
      inner = nextDl;
    }
    const finalDd = document.createElement('dd');
    finalDd.innerHTML = parsedHtml;
    if (replyObj.isCustom) {
      finalDd.setAttribute('data-custom-reply', 'true');
    }
    inner.appendChild(finalDd);

    lastInSection.insertAdjacentElement('afterend', wrapper);
    return wrapper;
  }

  function openInlineReplyBox(headingEl, sectionId, sectionTitle) {
    // Remove any existing open inline reply form
    const existingForm = talkBody.querySelector('.mw-inline-reply-form');
    if (existingForm) {
      const prevSec = existingForm.getAttribute('data-for-section');
      existingForm.remove();
      if (prevSec === sectionId) return; // toggle off if clicking same section
    }

    // Find insertion point at bottom of section
    let cursor = headingEl.nextElementSibling;
    let lastInSection = headingEl;
    while (cursor && cursor.tagName !== 'H2' && cursor.tagName !== 'H3') {
      lastInSection = cursor;
      cursor = cursor.nextElementSibling;
    }

    const formBox = document.createElement('div');
    formBox.className = 'mw-inline-reply-form';
    formBox.setAttribute('data-for-section', sectionId);
    formBox.innerHTML = `
      <div style="font-weight:bold; color:#e6ebf2; margin-bottom:8px; display:flex; justify-content:space-between; align-items:center;">
        <span>Replying to section: <em>${escapeBasicHtml(sectionTitle)}</em></span>
        <span style="font-size:11px; color:#8a96a8; font-weight:normal;">MediaWiki Wikitext enabled</span>
      </div>
      <div class="mw-editor-row">
        <div class="mw-editor-field" style="max-width:240px;">
          <label>Username / Signature:</label>
          <input type="text" class="mw-input reply-author-input" value="${escapeBasicHtml(getSavedAuthor())}" placeholder="Your username">
        </div>
        <div class="mw-editor-field" style="max-width:220px;">
          <label>Indentation level (<code>:</code>):</label>
          <select class="mw-select reply-indent-select">
            <option value="1">: (1st level reply)</option>
            <option value="2">:: (2nd level nested)</option>
            <option value="3">::: (3rd level nested)</option>
          </select>
        </div>
      </div>
      <div class="mw-edit-toolbar">
        <button type="button" class="mw-tool-btn" data-act="bold" title="Bold text">B</button>
        <button type="button" class="mw-tool-btn" data-act="italic" title="Italic text"><em>I</em></button>
        <button type="button" class="mw-tool-btn" data-act="link" title="Internal wiki link">[[Link]]</button>
        <button type="button" class="mw-tool-btn" data-act="code" title="Inline code">&lt;code&gt;</button>
        <button type="button" class="mw-tool-btn" data-act="pre" title="Code block">&lt;pre&gt;</button>
        <button type="button" class="mw-tool-btn" data-act="sig" title="Insert signature and timestamp">--~~~~</button>
      </div>
      <textarea class="mw-textarea reply-body-textarea" rows="4" placeholder="Write your response here... (Your comment will be signed automatically with --~~~~)"></textarea>
      <div class="mw-preview-box reply-preview-box">
        <div class="mw-preview-title">Live Wikitext Preview</div>
        <div class="reply-preview-content"></div>
      </div>
      <div class="mw-editor-actions">
        <button type="button" class="mw-btn-save btn-submit-reply">Post reply</button>
        <button type="button" class="mw-btn-secondary btn-preview-reply">Show preview</button>
        <button type="button" class="mw-btn-secondary btn-cancel-reply">Cancel</button>
      </div>
    `;

    lastInSection.insertAdjacentElement('afterend', formBox);

    const authorInput = formBox.querySelector('.reply-author-input');
    const indentSelect = formBox.querySelector('.reply-indent-select');
    const bodyTextarea = formBox.querySelector('.reply-body-textarea');
    const previewBox = formBox.querySelector('.reply-preview-box');
    const previewContent = formBox.querySelector('.reply-preview-content');

    bodyTextarea.focus();

    // Toolbar buttons
    formBox.querySelectorAll('.mw-tool-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const act = btn.getAttribute('data-act');
        if (act === 'bold') insertAtCursor(bodyTextarea, "'''", "'''", 'bold text');
        else if (act === 'italic') insertAtCursor(bodyTextarea, "''", "''", 'italic text');
        else if (act === 'link') insertAtCursor(bodyTextarea, '[[', ']]', 'modules.html|Lab SDK');
        else if (act === 'code') insertAtCursor(bodyTextarea, '<code>', '</code>', 'render.dll');
        else if (act === 'pre') insertAtCursor(bodyTextarea, '<pre>\n', '\n</pre>', '// C++20 code');
        else if (act === 'sig') insertAtCursor(bodyTextarea, ' --~~~~', '', '');
      });
    });

    // Preview button
    formBox.querySelector('.btn-preview-reply').addEventListener('click', () => {
      const raw = bodyTextarea.value.trim();
      if (!raw) return;
      previewContent.innerHTML = parseWikitext(raw, authorInput.value, formatUtcTimestamp(new Date()));
      previewBox.classList.add('active');
    });

    // Cancel button
    formBox.querySelector('.btn-cancel-reply').addEventListener('click', () => {
      formBox.remove();
    });

    // Submit reply
    formBox.querySelector('.btn-submit-reply').addEventListener('click', () => {
      const raw = bodyTextarea.value.trim();
      if (!raw) {
        bodyTextarea.focus();
        return;
      }
      const author = (authorInput.value || 'YoungJasiek').trim();
      setSavedAuthor(author);
      const replyObj = {
        sectionId,
        author,
        indent: Number(indentSelect.value) || 1,
        body: raw,
        timestamp: formatUtcTimestamp(new Date()),
        isCustom: true
      };

      // Save to localStorage
      try {
        const existing = JSON.parse(localStorage.getItem(STORAGE_REPLIES_KEY) || '[]');
        existing.push(replyObj);
        localStorage.setItem(STORAGE_REPLIES_KEY, JSON.stringify(existing));
      } catch (e) {}

      formBox.remove();
      const inserted = appendReplyDomToSection(headingEl, replyObj);
      if (inserted) {
        inserted.scrollIntoView({ behavior: 'smooth', block: 'center' });
      }
    });
  }

  function wireHeadingEditLinks(headingEl) {
    const headline = headingEl.querySelector('.mw-headline');
    const editSpan = headingEl.querySelector('.mw-editsection');
    if (!headline || !editSpan) return;

    const sectionId = headline.id || headline.textContent.trim().replace(/\s+/g, '_');
    const sectionTitle = headline.textContent.trim();

    editSpan.innerHTML = `[<a href="#${sectionId}" class="mw-reply-trigger">reply</a> | <a href="#${sectionId}" class="mw-edit-trigger">edit</a>]`;

    editSpan.querySelectorAll('a').forEach(link => {
      link.addEventListener('click', (e) => {
        e.preventDefault();
        openInlineReplyBox(headingEl, sectionId, sectionTitle);
      });
    });
  }

  // Wire all existing h2 and h3 headings inside .talk-body
  talkBody.querySelectorAll('h2, h3').forEach(wireHeadingEditLinks);

  // Helper to append a complete new topic (h2 + p) to .talk-body and update TOC
  function renderNewTopicDom(topicObj, scrollToIt) {
    const safeId = topicObj.id || ('Topic_' + Date.now());
    const h2 = document.createElement('h2');
    h2.innerHTML = `<span class="mw-headline" id="${escapeBasicHtml(safeId)}">${escapeBasicHtml(topicObj.subject)}</span> <span class="mw-editsection">[<a href="#${escapeBasicHtml(safeId)}">edit</a>]</span>`;
    if (topicObj.isCustom) h2.setAttribute('data-custom-topic', 'true');

    const p = document.createElement('p');
    p.innerHTML = parseWikitext(topicObj.body, topicObj.author, topicObj.timestamp);
    if (topicObj.isCustom) p.setAttribute('data-custom-topic', 'true');

    talkBody.appendChild(h2);
    talkBody.appendChild(p);
    wireHeadingEditLinks(h2);

    // Also add to TOC (.toc-list)
    const tocList = document.querySelector('.toc .toc-list');
    if (tocList) {
      const topLevelCount = tocList.children.length + 1;
      const li = document.createElement('li');
      if (topicObj.isCustom) li.setAttribute('data-custom-topic', 'true');
      li.innerHTML = `<a href="#${escapeBasicHtml(safeId)}"><span class="tocnumber">${topLevelCount}</span> <span class="toctext">${escapeBasicHtml(topicObj.subject)}</span></a>`;
      tocList.appendChild(li);
    }

    if (scrollToIt) {
      h2.scrollIntoView({ behavior: 'smooth', block: 'center' });
    }
  }

  // Load saved custom topics from localStorage
  try {
    const savedTopics = JSON.parse(localStorage.getItem(STORAGE_TOPICS_KEY) || '[]');
    savedTopics.forEach(t => renderNewTopicDom(t, false));
  } catch (e) {}

  // Load saved custom replies from localStorage
  try {
    const savedReplies = JSON.parse(localStorage.getItem(STORAGE_REPLIES_KEY) || '[]');
    savedReplies.forEach(r => {
      const targetHeadline = document.getElementById(r.sectionId);
      if (targetHeadline) {
        const headingEl = targetHeadline.closest('h2, h3');
        if (headingEl) appendReplyDomToSection(headingEl, r);
      }
    });
  } catch (e) {}

  // Wire bottom New Topic Editor Form (#talk-new-topic-form)
  const newTopicForm = document.getElementById('talk-new-topic-form');
  if (newTopicForm) {
    const subjInput = document.getElementById('talk-new-subject');
    const authInput = document.getElementById('talk-new-author');
    const bodyInput = document.getElementById('talk-new-body');
    const previewBox = document.getElementById('talk-new-preview-box');
    const previewContent = document.getElementById('talk-new-preview-content');
    const previewBtn = document.getElementById('btn-talk-preview');
    const resetBtn = document.getElementById('btn-talk-reset-local');

    if (authInput) authInput.value = getSavedAuthor();

    // Wire toolbar buttons in the main New Topic Editor
    newTopicForm.querySelectorAll('.mw-tool-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const act = btn.getAttribute('data-act');
        if (act === 'bold') insertAtCursor(bodyInput, "'''", "'''", 'bold text');
        else if (act === 'italic') insertAtCursor(bodyInput, "''", "''", 'italic text');
        else if (act === 'link') insertAtCursor(bodyInput, '[[', ']]', 'modules.html|Lab SDK');
        else if (act === 'code') insertAtCursor(bodyInput, '<code>', '</code>', 'Engine.dll');
        else if (act === 'pre') insertAtCursor(bodyInput, '<pre>\n', '\n</pre>', '// code block');
        else if (act === 'sig') insertAtCursor(bodyInput, ' --~~~~', '', '');
      });
    });

    if (previewBtn) {
      previewBtn.addEventListener('click', () => {
        const subj = (subjInput.value || 'Untitled Topic').trim();
        const raw = bodyInput.value.trim();
        if (!raw) {
          bodyInput.focus();
          return;
        }
        previewContent.innerHTML = `<h2 style="margin-top:0;">${escapeBasicHtml(subj)}</h2><p>${parseWikitext(raw, authInput.value, formatUtcTimestamp(new Date()))}</p>`;
        previewBox.classList.add('active');
      });
    }

    newTopicForm.addEventListener('submit', (e) => {
      e.preventDefault();
      const subj = subjInput.value.trim();
      const raw = bodyInput.value.trim();
      if (!subj) {
        subjInput.focus();
        return;
      }
      if (!raw) {
        bodyInput.focus();
        return;
      }
      const author = (authInput.value || 'YoungJasiek').trim();
      setSavedAuthor(author);

      const safeId = subj.replace(/[^a-zA-Z0-9_-]/g, '_') + '_' + Math.floor(Date.now() / 1000);
      const topicObj = {
        id: safeId,
        subject: subj,
        author,
        body: raw,
        timestamp: formatUtcTimestamp(new Date()),
        isCustom: true
      };

      try {
        const existing = JSON.parse(localStorage.getItem(STORAGE_TOPICS_KEY) || '[]');
        existing.push(topicObj);
        localStorage.setItem(STORAGE_TOPICS_KEY, JSON.stringify(existing));
      } catch (err) {}

      subjInput.value = '';
      bodyInput.value = '';
      if (previewBox) previewBox.classList.remove('active');

      renderNewTopicDom(topicObj, true);
    });

    if (resetBtn) {
      resetBtn.addEventListener('click', () => {
        try {
          localStorage.removeItem(STORAGE_TOPICS_KEY);
          localStorage.removeItem(STORAGE_REPLIES_KEY);
        } catch (err) {}
        document.querySelectorAll('[data-custom-topic], [data-custom-reply]').forEach(el => {
          const parentDl = el.tagName === 'DD' ? el.closest('dl') : null;
          if (parentDl) parentDl.remove();
          else el.remove();
        });
      });
    }
  }

  // Smooth focus when clicking "+ Add topic" links
  document.querySelectorAll('a[href="#new-topic"], a[href="#editor-section"]').forEach(link => {
    link.addEventListener('click', (e) => {
      const subjInput = document.getElementById('talk-new-subject');
      if (subjInput) {
        e.preventDefault();
        subjInput.scrollIntoView({ behavior: 'smooth', block: 'center' });
        setTimeout(() => subjInput.focus(), 250);
      }
    });
  });
}

/* ====================================================================
   FORM ERROR STATES & CLIENT-SIDE VALIDATION
   ==================================================================== */
function initFormValidation() {
  const forms = document.querySelectorAll('form');
  forms.forEach(form => {
    // Add real-time input validation cleanup
    const inputs = form.querySelectorAll('input, textarea');
    inputs.forEach(input => {
      input.addEventListener('input', () => {
        if (input.classList.contains('is-invalid')) {
          input.classList.remove('is-invalid');
          const errorMsg = input.parentNode.querySelector('.form-error-msg');
          if (errorMsg) errorMsg.style.display = 'none';
        }
      });
    });

    form.addEventListener('submit', (e) => {
      let hasError = false;
      inputs.forEach(input => {
        if (input.hasAttribute('required') && !input.value.trim()) {
          hasError = true;
          input.classList.add('is-invalid');
          input.setAttribute('aria-invalid', 'true');

          let errorMsg = input.parentNode.querySelector('.form-error-msg');
          if (!errorMsg) {
            errorMsg = document.createElement('div');
            errorMsg.className = 'form-error-msg';
            errorMsg.innerHTML = '&#9888; This field is required.';
            input.parentNode.appendChild(errorMsg);
          }
          errorMsg.style.display = 'flex';
        }
      });

      if (hasError) {
        e.preventDefault();
        e.stopPropagation();
        form.classList.remove('form-shake');
        void form.offsetWidth; // Force reflow
        form.classList.add('form-shake');
        return false;
      }

      // Add loading state to submit button if form is valid
      const submitBtn = form.querySelector('button[type="submit"], input[type="submit"]');
      if (submitBtn && !form.dataset.noLoadingState) {
        submitBtn.classList.add('btn-loading');
        setTimeout(() => {
          submitBtn.classList.remove('btn-loading');
        }, 600);
      }
    });
  });
}

// Global bootstrap for the web components
document.addEventListener('DOMContentLoaded', () => {
  initFormValidation();
});



