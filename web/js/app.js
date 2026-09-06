/**
 * LAB DEVELOPER COMMUNITY – CLIENT SCRIPT
 * Multi-page MediaWiki/Wikipedia Architecture
 * Lab Engine, LabHammer 3D Level Editor & Frozen-Life (FL)
 */

document.addEventListener('DOMContentLoaded', () => {
  initThemeToggle();
  initMobileSidebar();
  initTocToggle();
  initActiveNav();
  initWikiSearch();
  initCodeCopy();
  initWikiLightbox();
  initSourceTabs();
});

/* ==========================================================================
   1. THEME SWITCHER (DARK / LIGHT) – SYNCHRONIZED ACROSS PAGES
   ========================================================================== */
function initThemeToggle() {
  const toggleBtn = document.getElementById('theme-toggle-btn');
  if (!toggleBtn) return;

  const currentTheme = localStorage.getItem('lab_doc_theme') || 'dark';
  document.documentElement.setAttribute('data-theme', currentTheme);
  updateThemeBtnText(currentTheme);

  toggleBtn.addEventListener('click', () => {
    const isDark = document.documentElement.getAttribute('data-theme') === 'dark';
    const newTheme = isDark ? 'light' : 'dark';
    document.documentElement.setAttribute('data-theme', newTheme);
    localStorage.setItem('lab_doc_theme', newTheme);
    updateThemeBtnText(newTheme);
  });

  function updateThemeBtnText(theme) {
    toggleBtn.innerHTML = theme === 'dark' ? '☀️ Light Mode' : '🌙 Dark Mode';
  }
}

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
  { title: "Featured Subject: LabHammer", url: "labhammer.html", cat: "Portal", snippet: "The official standalone 3D level editor for the Lab Engine and Frozen-Life." },
  { title: "Latest Release: Frozen-Life Alpha", url: "frozen-life.html", cat: "Portal", snippet: "First playable test build v0.1.0-alpha featuring 2 maps and multiplayer foundation." },
  
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
  { title: "Troubleshooting Build Errors", url: "building.html#sec-troubleshooting", cat: "Compiling", snippet: "Solutions for missing OpenGL 4.5 DSA entry points, GLFW linking, and asset paths." }
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

