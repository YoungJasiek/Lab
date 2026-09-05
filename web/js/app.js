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
  { title: "LabHammer Overview", url: "labhammer.html#overview", cat: "LabHammer", snippet: "Standalone 3D world creation suite linked directly to LabEngineLib." },
  { title: "LabHammer: 3D Viewports & Camera", url: "labhammer.html#navigation", cat: "LabHammer", snippet: "Fly camera controls, RMB+WASD navigation, and Shift speed acceleration (15 to 35 m/s)." },
  { title: "LabHammer: 6 Core Editor Tools", url: "labhammer.html#tools", cat: "LabHammer", snippet: "Raycast Picker, Brush Cube, 3D STL Prop, Texture Pipette, Airlock Door, and Spawn Points." },
  { title: "LabHammer: Tool 0 (Raycast Picker)", url: "labhammer.html#tool-0", cat: "LabHammer", snippet: "Direct 3D object selection with real-time bounding box wireframe highlight." },
  { title: "LabHammer: Tool 1 (Block Brush)", url: "labhammer.html#tool-1", cat: "LabHammer", snippet: "3D textured cube primitive spawner with configurable world-space dimensions." },
  { title: "LabHammer: Tool 2 (3D STL Prop)", url: "labhammer.html#tool-2", cat: "LabHammer", snippet: "External 3D mesh placement with scale, rotation, and custom material assignment." },
  { title: "LabHammer: Tool 3 (Texture Pipette)", url: "labhammer.html#tool-3", cat: "LabHammer", snippet: "LMB applies active material to hovered face, RMB samples existing texture into active palette." },
  { title: "LabHammer: Tool 4 (Airlock Door Entity)", url: "labhammer.html#tool-4", cat: "LabHammer", snippet: "Interactive sliding sector barrier with configurable trigger proximity radius." },
  { title: "LabHammer: Tool 5 (Spawn Point Entity)", url: "labhammer.html#tool-5", cat: "LabHammer", snippet: "Player spawn markers supporting FFA and Team Deathmatch orientations." },
  { title: "LabHammer: .labmap File Format", url: "labhammer.html#labmap-format", cat: "LabHammer", snippet: "Plain-text human-readable format for level geometry, entities, and lighting parameters." },
  { title: "LabHammer: Keyboard Shortcuts", url: "labhammer.html#shortcuts", cat: "LabHammer", snippet: "Complete keybindings cheat sheet including F9 hot-launch testing." },
  { title: "LabHammer: Screenshots Gallery", url: "labhammer.html#gallery", cat: "LabHammer", snippet: "In-editor UI, properties inspector, and entity placement screenshots." },

  // frozen-life.html
  { title: "Frozen-Life Overview", url: "frozen-life.html#overview", cat: "Frozen-Life", snippet: "Tactical retro-modern FPS built on top of the Lab Engine." },
  { title: "Frozen-Life: Playable Test Build", url: "frozen-life.html#build-spec", cat: "Frozen-Life", snippet: "Official v0.1.0-alpha standalone test build specifications and executable Lab.exe." },
  { title: "Frozen-Life: Playable Maps", url: "frozen-life.html#maps", cat: "Frozen-Life", snippet: "Detailed map breakdown of cryo_outpost and facility_alpha environments." },
  { title: "Frozen-Life: Controls & Mechanics", url: "frozen-life.html#controls", cat: "Frozen-Life", snippet: "WASD, Space, Shift sprint, LMB fire, RMB ads, Tab scoreboard, T global chat." },
  { title: "Frozen-Life: Menus & UI", url: "frozen-life.html#menus", cat: "Frozen-Life", snippet: "Host Game server menu, map selector dropdown, HUD ammo counter, and scoreboard." },
  { title: "Frozen-Life: Performance Benchmarks", url: "frozen-life.html#benchmarks", cat: "Frozen-Life", snippet: "144+ FPS frame timings, 0.08ms ray-AABB collision pass, and GPU metrics." },
  { title: "Frozen-Life: Screenshots Gallery", url: "frozen-life.html#gallery", cat: "Frozen-Life", snippet: "In-game combat, host menu, scoreboard, ammo HUD, and weapon spawner screenshots." },

  // architecture.html
  { title: "Engine Architecture Overview", url: "architecture.html#overview", cat: "Architecture", snippet: "Modern C++20 design principles, zero-cost abstractions, and strict RAII." },
  { title: "C++20 & Memory Safety", url: "architecture.html#cpp20-raii", cat: "Architecture", snippet: "Elimination of raw new/delete, Rule of Five, std::unique_ptr and std::span." },
  { title: "Engine Subsystems & Lifecycle", url: "architecture.html#subsystems", cat: "Architecture", snippet: "Renderer, Physics, Input, Audio, and UI subsystem orchestration." },
  { title: "Slab Ray-AABB Collision Math", url: "architecture.html#collision", cat: "Architecture", snippet: "Optimized slab method for bullet and raycast intersection against Axis-Aligned Bounding Boxes." },
  { title: "C++ Source Code Inspector", url: "architecture.html#source-code", cat: "Architecture", snippet: "Interactive browser for Engine.h, Renderer.cpp, RayAABB.cpp, and MapLoader.cpp." },

  // opengl-dsa.html
  { title: "Direct State Access (DSA) Overview", url: "opengl-dsa.html#overview", cat: "OpenGL DSA", snippet: "Modern OpenGL 4.5+ Direct State Access bypassing global context binding." },
  { title: "Classic vs Direct State Access", url: "opengl-dsa.html#comparison", cat: "OpenGL DSA", snippet: "Side-by-side comparison table between legacy bind-to-edit and modern DSA." },
  { title: "DSA Texture Storage", url: "opengl-dsa.html#textures", cat: "OpenGL DSA", snippet: "glCreateTextures and glTextureStorage2D for immutable mipmapped textures." },
  { title: "DSA Vertex Arrays & Buffers", url: "opengl-dsa.html#buffers", cat: "OpenGL DSA", snippet: "glCreateBuffers and glNamedBufferStorage for efficient GPU memory allocation." },
  { title: "Source-Engine Visual Aesthetic", url: "opengl-dsa.html#aesthetics", cat: "OpenGL DSA", snippet: "Blinn-Phong lighting, sharp normals, high contrast, subtle bloom, and MSAA." },
  { title: "Driver Overhead & Performance", url: "opengl-dsa.html#performance", cat: "OpenGL DSA", snippet: "Eliminating driver state validation overhead with DSA calls." },

  // building.html
  { title: "Building from Source Overview", url: "building.html#overview", cat: "Compiling", snippet: "How to clone, configure, and compile Lab Engine and LabHammer using CMake." },
  { title: "Toolchain Prerequisites", url: "building.html#prerequisites", cat: "Compiling", snippet: "C++20 compiler (MSVC 2022 / Clang 16+), CMake 3.22+, and OpenGL 4.5+ GPU drivers." },
  { title: "CMake Build Commands", url: "building.html#commands", cat: "Compiling", snippet: "PowerShell build script using CMake and MSVC Release configuration." },
  { title: "Repository Directory Structure", url: "building.html#structure", cat: "Compiling", snippet: "Directory tree showing src/, assets/, maps/, and third-party libraries." },
  { title: "Troubleshooting Build Errors", url: "building.html#troubleshooting", cat: "Compiling", snippet: "Solutions for missing OpenGL 4.5 DSA entry points, GLFW linking, and asset paths." }
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

