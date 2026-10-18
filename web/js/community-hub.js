/**
 * Lab Developer Community - Discussion Hub (Reddit-style Ecosystem Board)
 * Strict technical architecture, zero AI slop, zero emojis.
 * Full integration with Supabase Auth (lab_supabase_session) & PostgreSQL RLS.
 */

(function () {
  'use strict';

  const STORAGE_VOTES_KEY = 'lab_community_votes_v1';
  const STORAGE_LOCAL_POSTS_KEY = 'lab_community_local_posts_v1';
  const STORAGE_LOCAL_COMMENTS_KEY = 'lab_community_local_comments_v1';

  let activeFlair = 'ALL';
  let activeSort = 'HOT';
  let postsData = [];
  let userVotes = {};

  try {
    userVotes = JSON.parse(localStorage.getItem(STORAGE_VOTES_KEY)) || {};
  } catch (e) {
    userVotes = {};
  }

  // Pre-seeded high-quality ecosystem discussions (baseline technical dataset)
  const SEED_POSTS = [
    {
      id: 'seed-announcement-1',
      title: 'Lab Engine Architecture & Project Hub Roadmap',
      content: 'Official discussion thread for Lab Engine C++20 core, Direct State Access (OpenGL 4.5+ DSA), and the decoupled VFS architecture separating `engine://` assets from `Documents/LabGames/`.\n\n### Modular Subsystems\nAll subsystem headers are compiled under `/include` and exported via 11 modular dynamic libraries:\n- `Engine.dll` - Core loop and memory management\n- `render.dll` - OpenGL 4.5+ DSA pipeline\n- `audio.dll` - Spatial 3D sound synthesizer\n- `physics.dll` - Rigid body simulation\n\n```powershell\n# Build the full modular engine from source\ncmake -B build -S . -DCMAKE_BUILD_TYPE=Release\ncmake --build build --config Release\n```',
      flair: 'ANNOUNCEMENT',
      author_name: 'YoungJasiek',
      author_role: 'admin',
      upvotes: 42,
      is_pinned: true,
      created_at: new Date(Date.now() - 86400000 * 2).toISOString(),
      comments_count: 3
    },
    {
      id: 'seed-engine-1',
      title: 'GPU Bindless Textures & Vertex Buffer Storage Performance Benchmarks',
      content: 'Comparing `glNamedBufferStorage` with persistent mapped flags vs `glBufferSubData` across AMD and NVIDIA hardware.\n\nIn our tests with 10,000 dynamic drawcalls, persistent DSA mapping reduced render-thread CPU driver overhead by 4.2x:\n\n```cpp\n// Direct State Access Persistent Mapping Example\nGLuint vbo = 0;\nglCreateBuffers(1, &vbo);\nGLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;\nglNamedBufferStorage(vbo, bufferSize, nullptr, flags);\nvoid* ptr = glMapNamedBufferRange(vbo, 0, bufferSize, flags);\n```',
      flair: 'ENGINE',
      author_name: 'GordonFreeman',
      author_role: 'admin',
      upvotes: 28,
      is_pinned: false,
      created_at: new Date(Date.now() - 86400000).toISOString(),
      comments_count: 2
    },
    {
      id: 'seed-fl-1',
      title: 'Frozen-Life: Tactical Bot AI State Machine Integration',
      content: 'The 5-state AI finite state machine (Patrol, Alert, Chase, Flank, Retreat) is now fully integrated with skeletal hitboxes and raycast line-of-sight checks.\n\n### How to test:\n1. Open `LabEditor`\n2. Load map `test_multi_bot_waypoints.LABMAP`\n3. Press F5 to simulate bot waypoint navigation with combat decision trees.',
      flair: 'FROZEN-LIFE',
      author_name: 'DevTeam',
      author_role: 'dev',
      upvotes: 19,
      is_pinned: false,
      created_at: new Date(Date.now() - 3600000 * 14).toISOString(),
      comments_count: 1
    },
    {
      id: 'seed-hammer-1',
      title: 'LabHammer 3D CSG: Exporting Solid Brushes to .labmap Format',
      content: 'How are texture coordinates aligned across non-axis-aligned brush planes during CSG carving? Is there a planar projection matrix calculated per polygon face or per brush entity?',
      flair: 'LABHAMMER',
      author_name: 'Mapper_X',
      author_role: 'user',
      upvotes: 11,
      is_pinned: false,
      created_at: new Date(Date.now() - 3600000 * 4).toISOString(),
      comments_count: 1
    }
  ];

  const SEED_COMMENTS = {
    'seed-announcement-1': [
      {
        id: 'c-1',
        author_name: 'GordonFreeman',
        author_role: 'admin',
        content: 'Binary shader cache serialization is confirmed working on both AMD and Intel GPUs. Shader linking time dropped to near zero on warm restart.',
        upvotes: 9,
        created_at: new Date(Date.now() - 86400000).toISOString()
      },
      {
        id: 'c-2',
        author_name: 'Modder_01',
        author_role: 'user',
        content: 'Is there an export script for Blender 4.x skeletal models with multiple animation clips?',
        upvotes: 4,
        created_at: new Date(Date.now() - 3600000 * 18).toISOString(),
        replies: [
          {
            id: 'c-2-1',
            author_name: 'YoungJasiek',
            author_role: 'admin',
            content: 'Yes, check `scripts/export_t800_animations.py`. It exports bone hierarchies and named keyframe clips directly into the engine format.',
            upvotes: 6,
            created_at: new Date(Date.now() - 3600000 * 16).toISOString()
          }
        ]
      }
    ],
    'seed-engine-1': [
      {
        id: 'c-3',
        author_name: 'YoungJasiek',
        author_role: 'admin',
        content: 'Make sure `GL_MAP_COHERENT_BIT` and `GL_MAP_PERSISTENT_BIT` are strictly coupled with `glMemoryBarrier` before dispatch.',
        upvotes: 7,
        created_at: new Date(Date.now() - 3600000 * 10).toISOString()
      }
    ]
  };

  document.addEventListener('DOMContentLoaded', () => {
    initCommunityHub();
  });

  function initCommunityHub() {
    const hubRoot = document.getElementById('community-hub-root');
    if (!hubRoot) return;

    window.addEventListener('hashchange', () => handleRoute());
    handleRoute();
  }

  function handleRoute() {
    const hubRoot = document.getElementById('community-hub-root');
    if (!hubRoot) return;

    const hash = window.location.hash || '';

    if (hash === '#new') {
      const session = getCurrentSession();
      if (!session || !session.user) {
        showNotification('Please log in with your developer account to create a thread.');
        if (typeof window.VdcAuth?.openLogin === 'function') {
          window.VdcAuth.openLogin();
        }
      }
      renderStudioPage(hubRoot);
      window.scrollTo({ top: 0, behavior: 'smooth' });
    } else if (hash.startsWith('#post/')) {
      const postId = hash.slice(6);
      renderThreadPage(hubRoot, postId);
      window.scrollTo({ top: 0, behavior: 'smooth' });
    } else {
      renderFeedPage(hubRoot);
    }
  }

  function renderFeedPage(hubRoot) {
    hubRoot.innerHTML = `
      <div class="hub-container">
        <!-- Top Toolbar: Flair Pills, Sorting, and New Post Button -->
        <div class="hub-toolbar">
          <div class="hub-flairs" id="hub-flair-bar">
            <button type="button" class="hub-flair-btn ${activeFlair === 'ALL' ? 'active' : ''}" data-flair="ALL">All Topics</button>
            <button type="button" class="hub-flair-btn ${activeFlair === 'ANNOUNCEMENT' ? 'active' : ''}" data-flair="ANNOUNCEMENT">Announcements</button>
            <button type="button" class="hub-flair-btn ${activeFlair === 'ENGINE' ? 'active' : ''}" data-flair="ENGINE">Engine &amp; C++20</button>
            <button type="button" class="hub-flair-btn ${activeFlair === 'FROZEN-LIFE' ? 'active' : ''}" data-flair="FROZEN-LIFE">Frozen-Life SDK</button>
            <button type="button" class="hub-flair-btn ${activeFlair === 'LABHAMMER' ? 'active' : ''}" data-flair="LABHAMMER">LabHammer &amp; Maps</button>
            <button type="button" class="hub-flair-btn ${activeFlair === 'SHOWCASE' ? 'active' : ''}" data-flair="SHOWCASE">Showcase</button>
            <button type="button" class="hub-flair-btn ${activeFlair === 'HELP' ? 'active' : ''}" data-flair="HELP">Help &amp; Bugs</button>
          </div>

          <div class="hub-toolbar-right">
            <div class="hub-sort-group">
              <span class="hub-sort-label">Sort:</span>
              <button type="button" class="hub-sort-btn ${activeSort === 'HOT' ? 'active' : ''}" data-sort="HOT">Hot</button>
              <button type="button" class="hub-sort-btn ${activeSort === 'NEW' ? 'active' : ''}" data-sort="NEW">New</button>
              <button type="button" class="hub-sort-btn ${activeSort === 'TOP' ? 'active' : ''}" data-sort="TOP">Top</button>
            </div>
            <a href="#new" id="hub-btn-new-post" class="hub-btn-primary" style="text-decoration:none; display:inline-flex; align-items:center;">+ Create Thread</a>
          </div>
        </div>

        <!-- Main Layout: 2 Columns (Feed + Info Sidebar) -->
        <div class="hub-grid">
          <!-- Left Column: Posts Feed -->
          <div class="hub-feed" id="hub-posts-feed">
            <div class="hub-loading">Loading community discussions...</div>
          </div>

          <!-- Right Column: Sidebar Info & Rules -->
          <aside class="hub-sidebar">
            <div class="hub-card">
              <div class="hub-card-header">Community Hub</div>
              <div class="hub-card-body">
                <p style="font-size:12px; color:var(--wiki-text-secondary); margin-bottom:10px; line-height:1.45;">
                  The technical forum for Lab Engine developers, Frozen-Life modders, and level designers.
                </p>
                <div class="hub-stat-row">
                  <span class="hub-stat-label">Architecture</span>
                  <span class="hub-stat-val">C++20 / DSA</span>
                </div>
                <div class="hub-stat-row">
                  <span class="hub-stat-label">Database Sync</span>
                  <span class="hub-stat-val" id="hub-db-status">PostgreSQL RLS</span>
                </div>
                <div class="hub-stat-row">
                  <span class="hub-stat-label">Modding Target</span>
                  <span class="hub-stat-val">Frozen-Life</span>
                </div>
              </div>
            </div>

            <div class="hub-card">
              <div class="hub-card-header">Posting Guidelines</div>
              <div class="hub-card-body">
                <ul class="hub-rules-list">
                  <li>Keep threads focused strictly on Lab Engine, SDKs, and tools.</li>
                  <li>Include code snippets or stack traces for bug reports.</li>
                  <li>Verify engine version and build flags before submitting performance issues.</li>
                  <li>Administrative decisions on core architecture are maintained per PLAN.md.</li>
                </ul>
              </div>
            </div>
          </aside>
        </div>
      </div>
    `;

    wireHubEventListeners();
    if (postsData.length === 0) {
      fetchPosts();
    } else {
      renderFilteredPosts();
    }
  }

  function wireHubEventListeners() {
    // Flair Filter Buttons
    document.querySelectorAll('.hub-flair-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('.hub-flair-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        activeFlair = btn.getAttribute('data-flair') || 'ALL';
        renderFilteredPosts();
      });
    });

    // Sort Buttons
    document.querySelectorAll('.hub-sort-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('.hub-sort-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        activeSort = btn.getAttribute('data-sort') || 'HOT';
        renderFilteredPosts();
      });
    });
  }

  // Local storage management for offline / initial migration resilience
  function getLocalPosts() {
    try {
      return JSON.parse(localStorage.getItem(STORAGE_LOCAL_POSTS_KEY)) || [];
    } catch (e) {
      return [];
    }
  }

  function saveLocalPost(post) {
    try {
      const posts = getLocalPosts().filter(p => p.id !== post.id);
      posts.unshift(post);
      localStorage.setItem(STORAGE_LOCAL_POSTS_KEY, JSON.stringify(posts));
    } catch (e) {}
  }

  function removeLocalPost(postId) {
    try {
      const posts = getLocalPosts().filter(p => p.id !== postId);
      localStorage.setItem(STORAGE_LOCAL_POSTS_KEY, JSON.stringify(posts));
    } catch (e) {}
  }

  function getLocalComments(postId) {
    try {
      const all = JSON.parse(localStorage.getItem(STORAGE_LOCAL_COMMENTS_KEY)) || {};
      return all[postId] || [];
    } catch (e) {
      return [];
    }
  }

  function saveLocalComment(postId, comment) {
    try {
      const all = JSON.parse(localStorage.getItem(STORAGE_LOCAL_COMMENTS_KEY)) || {};
      if (!all[postId]) all[postId] = [];
      all[postId].push(comment);
      localStorage.setItem(STORAGE_LOCAL_COMMENTS_KEY, JSON.stringify(all));
    } catch (e) {}
  }

  async function fetchPosts(shouldRender = true) {
    const localPosts = getLocalPosts();
    const config = getSupabaseConfig();

    if (config.url && config.anonKey) {
      try {
        const res = await fetch(`${config.url}/rest/v1/community_posts?select=*&order=is_pinned.desc,upvotes.desc,created_at.desc`, {
          headers: {
            'apikey': config.anonKey,
            'Authorization': `Bearer ${getCurrentToken() || config.anonKey}`
          }
        });

        if (res.ok) {
          const dbPosts = await res.json();
          if (Array.isArray(dbPosts)) {
            const dbIds = new Set(dbPosts.map(p => p.id));
            const unsyncedLocal = localPosts.filter(lp => !dbIds.has(lp.id));
            postsData = [...unsyncedLocal, ...dbPosts];
            if (shouldRender && document.getElementById('hub-posts-feed')) renderFilteredPosts();
            return postsData;
          }
        }
      } catch (err) {
        console.warn('Supabase community_posts fetch failed, falling back to local dataset.', err);
      }
    }

    postsData = [...localPosts, ...SEED_POSTS];
    if (shouldRender && document.getElementById('hub-posts-feed')) renderFilteredPosts();
    return postsData;
  }

  function renderFilteredPosts() {
    const feed = document.getElementById('hub-posts-feed');
    if (!feed) return;

    let filtered = postsData.filter(p => {
      if (activeFlair === 'ALL') return true;
      return String(p.flair).toUpperCase() === activeFlair;
    });

    if (activeSort === 'NEW') {
      filtered.sort((a, b) => new Date(b.created_at) - new Date(a.created_at));
    } else if (activeSort === 'TOP') {
      filtered.sort((a, b) => (b.upvotes || 0) - (a.upvotes || 0));
    } else {
      // HOT algorithm: pinned first, then upvotes weighted by recency
      filtered.sort((a, b) => {
        if (a.is_pinned && !b.is_pinned) return -1;
        if (!a.is_pinned && b.is_pinned) return 1;
        const scoreA = (a.upvotes || 0) * 100000 + new Date(a.created_at).getTime();
        const scoreB = (b.upvotes || 0) * 100000 + new Date(b.created_at).getTime();
        return scoreB - scoreA;
      });
    }

    if (filtered.length === 0) {
      feed.innerHTML = `
        <div class="hub-empty">
          <p>No discussions found under this category.</p>
          <button type="button" class="hub-btn-primary" id="hub-empty-new-btn">Start the first thread</button>
        </div>
      `;
      document.getElementById('hub-empty-new-btn')?.addEventListener('click', () => {
        document.getElementById('hub-btn-new-post')?.click();
      });
      return;
    }

    feed.innerHTML = filtered.map(post => renderPostCardHtml(post)).join('');
    wirePostCardInteractions();
  }

  function renderPostCardHtml(post) {
    const userVote = userVotes[post.id] || 0;
    const authorRole = (post.author_role || 'user').toLowerCase();
    const timeAgo = formatTimeAgo(new Date(post.created_at));
    const commentsCount = post.comments_count !== undefined 
      ? post.comments_count 
      : ((SEED_COMMENTS[post.id]?.length || 0) + (getLocalComments(post.id).length));

    return `
      <article class="hub-post-card ${post.is_pinned ? 'is-pinned' : ''}" data-post-id="${escapeHtml(post.id)}">
        <!-- Vote Column -->
        <div class="hub-vote-col">
          <button type="button" class="hub-vote-btn upvote ${userVote === 1 ? 'voted' : ''}" data-dir="1" title="Upvote">&#9650;</button>
          <span class="hub-vote-count ${userVote === 1 ? 'voted-up' : userVote === -1 ? 'voted-down' : ''}">${post.upvotes || 0}</span>
          <button type="button" class="hub-vote-btn downvote ${userVote === -1 ? 'voted' : ''}" data-dir="-1" title="Downvote">&#9660;</button>
        </div>

        <!-- Post Content Main Area -->
        <div class="hub-post-main">
          <div class="hub-post-meta">
            ${post.is_pinned ? '<span class="hub-pin-badge">PINNED</span>' : ''}
            <span class="hub-flair-tag flair-${escapeHtml(String(post.flair).toLowerCase())}">${escapeHtml(post.flair)}</span>
            <span class="hub-meta-sep">&bull;</span>
            <span class="hub-author-label">Posted by</span>
            <span class="hub-author-name">u/${escapeHtml(post.author_name || 'Developer')}</span>
            <span class="role-badge role-${escapeHtml(authorRole)}">[${escapeHtml(authorRole.toUpperCase())}]</span>
            <span class="hub-meta-sep">&bull;</span>
            <span class="hub-post-time">${timeAgo}</span>
          </div>

          <h2 class="hub-post-title">
            <a href="#post/${escapeHtml(post.id)}" class="hub-post-link" data-post-id="${escapeHtml(post.id)}">${escapeHtml(post.title)}</a>
          </h2>

          <div class="hub-post-snippet">
            ${formatPostContentPreview(post.content)}
          </div>

          <div class="hub-post-footer">
            <a href="#post/${escapeHtml(post.id)}" class="hub-action-btn hub-btn-comments" style="text-decoration:none;" data-post-id="${escapeHtml(post.id)}">
              Comments (${commentsCount})
            </a>
            <button type="button" class="hub-action-btn hub-btn-share" data-post-id="${escapeHtml(post.id)}">
              Copy Link
            </button>
            ${canDeletePost(post) ? `
              <button type="button" class="hub-action-btn hub-btn-delete text-danger" data-post-id="${escapeHtml(post.id)}">
                Delete
              </button>
            ` : ''}
          </div>
        </div>
      </article>
    `;
  }

  function wirePostCardInteractions() {
    // Vote Buttons
    document.querySelectorAll('.hub-vote-btn').forEach(btn => {
      btn.addEventListener('click', (e) => {
        e.stopPropagation();
        const postCard = btn.closest('.hub-post-card');
        const postId = postCard?.getAttribute('data-post-id');
        const dir = parseInt(btn.getAttribute('data-dir') || '0', 10);
        handleVote(postId, dir);
      });
    });

    // Click on Post Title or Comments Button opens thread view via hash
    document.querySelectorAll('.hub-post-link, .hub-btn-comments').forEach(el => {
      el.addEventListener('click', (e) => {
        const postId = el.getAttribute('data-post-id');
        if (postId) {
          window.location.hash = '#post/' + postId;
        }
      });
    });

    // Copy Link Button
    document.querySelectorAll('.hub-btn-share').forEach(btn => {
      btn.addEventListener('click', (e) => {
        e.stopPropagation();
        const postId = btn.getAttribute('data-post-id');
        const shareUrl = `${window.location.origin}${window.location.pathname}#post/${postId}`;
        navigator.clipboard.writeText(shareUrl).then(() => {
          showNotification('Thread link copied to clipboard.');
        }).catch(() => {});
      });
    });

    // Delete Button
    document.querySelectorAll('.hub-btn-delete').forEach(btn => {
      btn.addEventListener('click', async (e) => {
        e.stopPropagation();
        const postId = btn.getAttribute('data-post-id');
        if (!confirm('Are you sure you want to delete this thread?')) return;
        await deletePost(postId);
      });
    });
  }

  function handleVote(postId, dir) {
    const post = postsData.find(p => p.id === postId);
    if (!post) return;

    const currentVote = userVotes[postId] || 0;
    let delta = 0;

    if (currentVote === dir) {
      userVotes[postId] = 0;
      delta = -dir;
    } else {
      userVotes[postId] = dir;
      delta = dir - currentVote;
    }

    post.upvotes = (post.upvotes || 0) + delta;
    localStorage.setItem(STORAGE_VOTES_KEY, JSON.stringify(userVotes));

    renderFilteredPosts();

    // Async sync with Supabase if configured and not local seed
    const config = getSupabaseConfig();
    const session = getCurrentSession();
    if (config.url && config.anonKey && session?.access_token && !String(postId).startsWith('seed-') && !String(postId).startsWith('post-')) {
      fetch(`${config.url}/rest/v1/community_posts?id=eq.${postId}`, {
        method: 'PATCH',
        headers: {
          'Content-Type': 'application/json',
          'apikey': config.anonKey,
          'Authorization': `Bearer ${session.access_token}`
        },
        body: JSON.stringify({ upvotes: post.upvotes })
      }).catch(() => {});
    }
  }

  /* ========================================================================
     REDDIT-STYLE THREAD VIEW & HIERARCHICAL COMMENT TREE (Variant 1: Full Page)
     ======================================================================== */
  async function renderThreadPage(hubRoot, postId) {
    if (!hubRoot) return;

    if (postsData.length === 0) {
      await fetchPosts(false);
    }

    const post = postsData.find(p => String(p.id) === String(postId));
    if (!post) {
      hubRoot.innerHTML = `
        <div class="hub-page-view">
          <div class="hub-back-nav">
            <a href="#feed" class="hub-back-link">&larr; Back to Discussions</a>
          </div>
          <div class="hub-view-card" style="padding:48px 24px; text-align:center;">
            <h2 style="color:#ffffff; font-size:18px; margin-bottom:8px;">Thread Not Found</h2>
            <p style="color:#8da4be; font-size:13px; margin-bottom:18px;">The requested technical discussion thread does not exist or has been removed.</p>
            <a href="#feed" class="hub-btn-primary" style="display:inline-block; text-decoration:none; padding:8px 18px;">Return to Discussions Feed</a>
          </div>
        </div>
      `;
      return;
    }

    const authorRole = (post.author_role || 'user').toLowerCase();
    const timeAgo = formatTimeAgo(new Date(post.created_at));
    const userVote = userVotes[post.id] || 0;

    hubRoot.innerHTML = `
      <div class="hub-page-view">
        <div class="hub-back-nav">
          <a href="#feed" class="hub-back-link">&larr; Back to Discussions</a>
          <div style="display:flex; align-items:center; gap:8px;">
            <span class="hub-flair-tag flair-${escapeHtml(String(post.flair).toLowerCase())}">${escapeHtml(post.flair)}</span>
            <span style="font-size:11.5px; color:#67788c;">Thread #${escapeHtml(post.id)}</span>
          </div>
        </div>

        <article class="hub-view-card">
          <div class="hub-view-header">
            <div class="hub-view-title" style="flex-direction:column; align-items:flex-start; gap:4px; width:100%;">
              <div style="font-size:11.5px; color:#8da4be; display:flex; align-items:center; gap:6px; flex-wrap:wrap;">
                ${post.is_pinned ? '<span class="hub-pin-badge">PINNED</span>' : ''}
                <span>Posted by</span>
                <strong style="color:#ffffff;">u/${escapeHtml(post.author_name || 'Developer')}</strong>
                <span class="role-badge role-${escapeHtml(authorRole)}">[${escapeHtml(authorRole.toUpperCase())}]</span>
                <span class="hub-meta-sep">&bull;</span>
                <span class="hub-post-time">${timeAgo}</span>
              </div>
              <h1 style="font-size:22px; font-weight:700; color:#ffffff; margin:6px 0 0 0; line-height:1.35; letter-spacing:-0.01em;">
                ${escapeHtml(post.title)}
              </h1>
            </div>
          </div>

          <div class="hub-page-body">
            <!-- Full Article Content Area -->
            <div class="hub-thread-full-content">
              ${formatPostContentFull(post.content)}
            </div>

            <!-- Action Bar (Vote Pill, Comment count, Share, Delete) -->
            <div class="hub-thread-actions-bar">
              <div class="hub-thread-vote-pill">
                <button type="button" class="hub-vote-btn upvote ${userVote === 1 ? 'voted' : ''}" id="page-vote-up" title="Upvote" style="padding:4px 6px;">&#9650;</button>
                <span class="hub-vote-count ${userVote === 1 ? 'voted-up' : userVote === -1 ? 'voted-down' : ''}" id="page-vote-val" style="margin:0 6px;">${post.upvotes || 0}</span>
                <button type="button" class="hub-vote-btn downvote ${userVote === -1 ? 'voted' : ''}" id="page-vote-down" title="Downvote" style="padding:4px 6px;">&#9660;</button>
              </div>

              <span style="font-size:12px; color:#8da4be; font-weight:600;">
                Technical Discussion
              </span>

              <button type="button" class="hub-action-btn" id="page-share-btn">
                Copy Thread Link
              </button>

              ${canDeletePost(post) ? `
                <button type="button" class="hub-action-btn text-danger" id="page-delete-btn">
                  Delete Thread
                </button>
              ` : ''}
            </div>

            <!-- Main Comment Input Box -->
            <div class="hub-comment-box-wrap">
              <div class="hub-comment-box-header">
                Comment as <strong>${escapeHtml(getCurrentSession()?.user?.user_metadata?.username || getCurrentSession()?.user?.email?.split('@')[0] || 'Guest')}</strong>
              </div>
              <textarea id="hub-new-comment-text" class="hub-textarea" placeholder="What are your technical thoughts or constructive feedback?" rows="3"></textarea>
              <div style="display:flex; justify-content:flex-end; margin-top:8px;">
                <button type="button" id="hub-submit-comment-btn" class="hub-btn-primary">Comment</button>
              </div>
            </div>

            <!-- Hierarchical Reddit Comments Tree -->
            <div class="hub-comments-section">
              <div class="hub-comments-title" id="thread-comments-heading">All Responses</div>
              <div class="hub-comments-list" id="thread-comments-list">
                <div class="hub-loading">Loading comments...</div>
              </div>
            </div>
          </div>
        </article>
      </div>
    `;

    // In-page voting
    document.getElementById('page-vote-up')?.addEventListener('click', () => {
      handleVote(post.id, 1);
      const valEl = document.getElementById('page-vote-val');
      if (valEl) valEl.textContent = post.upvotes || 0;
    });
    document.getElementById('page-vote-down')?.addEventListener('click', () => {
      handleVote(post.id, -1);
      const valEl = document.getElementById('page-vote-val');
      if (valEl) valEl.textContent = post.upvotes || 0;
    });

    document.getElementById('page-share-btn')?.addEventListener('click', () => {
      const shareUrl = `${window.location.origin}${window.location.pathname}#post/${post.id}`;
      navigator.clipboard.writeText(shareUrl).then(() => {
        showNotification('Thread link copied to clipboard.');
      }).catch(() => {});
    });

    document.getElementById('page-delete-btn')?.addEventListener('click', async () => {
      if (!confirm('Are you sure you want to delete this thread?')) return;
      await deletePost(post.id);
      window.location.hash = '#feed';
    });

    document.getElementById('hub-submit-comment-btn')?.addEventListener('click', async () => {
      const textarea = document.getElementById('hub-new-comment-text');
      const text = textarea?.value?.trim();
      if (!text) return;

      const session = getCurrentSession();
      if (!session || !session.user) {
        showNotification('Please log in with your developer account to comment.');
        if (typeof window.VdcAuth?.openLogin === 'function') {
          window.VdcAuth.openLogin();
        }
        return;
      }

      await submitComment(postId, text, null);
      textarea.value = '';
      loadCommentsForThread(postId, post.author_name);
    });

    loadCommentsForThread(postId, post.author_name);
  }

  async function loadCommentsForThread(postId, opAuthorName) {
    const listEl = document.getElementById('thread-comments-list');
    if (!listEl) return;

    let comments = [...(SEED_COMMENTS[postId] || []), ...getLocalComments(postId)];
    const config = getSupabaseConfig();

    if (config.url && config.anonKey && !String(postId).startsWith('seed-') && !String(postId).startsWith('post-')) {
      try {
        const res = await fetch(`${config.url}/rest/v1/community_comments?post_id=eq.${postId}&order=created_at.asc`, {
          headers: {
            'apikey': config.anonKey,
            'Authorization': `Bearer ${getCurrentToken() || config.anonKey}`
          }
        });
        if (res.ok) {
          const dbComments = await res.json();
          if (Array.isArray(dbComments) && dbComments.length > 0) {
            comments = dbComments;
          }
        }
      } catch (err) {
        console.warn('Could not load comments from Supabase:', err);
      }
    }

    if (comments.length === 0) {
      listEl.innerHTML = '<div class="hub-no-comments">No responses yet. Be the first to comment.</div>';
      return;
    }

    listEl.innerHTML = comments.map(c => renderCommentNodeHtml(c, opAuthorName, postId)).join('');
    wireCommentReplyButtons(postId, opAuthorName);
  }

  function renderCommentNodeHtml(comment, opAuthorName, postId) {
    const role = (comment.author_role || 'user').toLowerCase();
    const timeAgo = formatTimeAgo(new Date(comment.created_at));
    const isOp = comment.author_name && opAuthorName && (comment.author_name.toLowerCase() === opAuthorName.toLowerCase());
    const initial = (comment.author_name || 'D').charAt(0).toUpperCase();

    const replies = comment.replies || [];

    return `
      <div class="hub-comment-node" data-comment-id="${escapeHtml(comment.id)}">
        <!-- Vertical Thread Line (Reddit Style) -->
        <div class="hub-comment-thread-line"></div>

        <!-- Avatar Circle -->
        <div class="hub-comment-avatar role-${escapeHtml(role)}">${escapeHtml(initial)}</div>

        <!-- Content Box -->
        <div class="hub-comment-content-block">
          <div class="hub-comment-header">
            <span class="hub-comment-author">u/${escapeHtml(comment.author_name || 'Developer')}</span>
            ${isOp ? '<span class="hub-badge-op">OP</span>' : ''}
            <span class="role-badge role-${escapeHtml(role)}">[${escapeHtml(role.toUpperCase())}]</span>
            <span class="hub-meta-sep">&bull;</span>
            <span class="hub-comment-time">${timeAgo}</span>
          </div>

          <div class="hub-comment-body">
            ${formatPostContentFull(comment.content)}
          </div>

          <div class="hub-comment-actions">
            <button type="button" class="hub-comment-action-btn hub-btn-reply" data-comment-id="${escapeHtml(comment.id)}">Reply</button>
            <span class="hub-meta-sep">&bull;</span>
            <span style="font-family:var(--font-mono); font-size:11px; color:#67788c;">${comment.upvotes || 1} upvotes</span>
          </div>

          <!-- Inline Reply Box (Hidden by default) -->
          <div class="hub-comment-reply-box" id="reply-box-${escapeHtml(comment.id)}" style="display:none;">
            <textarea class="hub-textarea" id="reply-text-${escapeHtml(comment.id)}" placeholder="Write a reply..." rows="2"></textarea>
            <div style="display:flex; justify-content:flex-end; gap:6px; margin-top:6px;">
              <button type="button" class="hub-btn-secondary" onclick="document.getElementById('reply-box-${escapeHtml(comment.id)}').style.display='none'">Cancel</button>
              <button type="button" class="hub-btn-primary hub-btn-send-reply" data-comment-id="${escapeHtml(comment.id)}">Send</button>
            </div>
          </div>
        </div>

        <!-- Nested Replies (Reddit-style tree) -->
        ${replies.length > 0 ? `
          <div class="hub-comment-replies">
            ${replies.map(r => renderCommentNodeHtml(r, opAuthorName, postId)).join('')}
          </div>
        ` : ''}
      </div>
    `;
  }

  function wireCommentReplyButtons(postId, opAuthorName) {
    document.querySelectorAll('.hub-btn-reply').forEach(btn => {
      btn.addEventListener('click', () => {
        const commentId = btn.getAttribute('data-comment-id');
        const box = document.getElementById(`reply-box-${commentId}`);
        if (box) {
          box.style.display = box.style.display === 'none' ? 'block' : 'none';
          if (box.style.display === 'block') {
            document.getElementById(`reply-text-${commentId}`)?.focus();
          }
        }
      });
    });

    document.querySelectorAll('.hub-btn-send-reply').forEach(btn => {
      btn.addEventListener('click', async () => {
        const commentId = btn.getAttribute('data-comment-id');
        const text = document.getElementById(`reply-text-${commentId}`)?.value?.trim();
        if (!text) return;

        const session = getCurrentSession();
        if (!session || !session.user) {
          showNotification('Please log in with your developer account to reply.');
          if (typeof window.VdcAuth?.openLogin === 'function') {
            window.VdcAuth.openLogin();
          }
          return;
        }

        await submitComment(postId, text, commentId);
        loadCommentsForThread(postId, opAuthorName);
      });
    });
  }

  async function submitComment(postId, content, parentId = null) {
    const session = getCurrentSession();
    const config = getSupabaseConfig();
    const authorName = session?.user?.user_metadata?.username || session?.user?.email?.split('@')[0] || 'Developer';
    const authorRole = session?.user?.role || session?.user?.user_metadata?.role || 'user';
    const authorId = session?.user?.id || null;

    const newCommentObj = {
      id: 'c-' + Date.now(),
      post_id: postId,
      parent_id: parentId,
      author_id: authorId,
      author_name: authorName,
      author_role: authorRole,
      content: content,
      upvotes: 1,
      created_at: new Date().toISOString()
    };

    let synced = false;
    if (config.url && config.anonKey && session?.access_token && !String(postId).startsWith('seed-') && !String(postId).startsWith('post-')) {
      try {
        const res = await fetch(`${config.url}/rest/v1/community_comments`, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
            'apikey': config.anonKey,
            'Authorization': `Bearer ${session.access_token}`
          },
          body: JSON.stringify({
            post_id: postId,
            parent_id: parentId,
            author_id: authorId,
            author_name: authorName,
            author_role: authorRole,
            content: content
          })
        });
        if (res.ok) synced = true;
      } catch (err) {}
    }

    if (!synced) {
      saveLocalComment(postId, newCommentObj);
      if (!SEED_COMMENTS[postId]) SEED_COMMENTS[postId] = [];
      SEED_COMMENTS[postId].push(newCommentObj);
    }

    const post = postsData.find(p => p.id === postId);
    if (post) post.comments_count = (post.comments_count || 0) + 1;
    renderFilteredPosts();
    showNotification('Comment published.');
  }

  /* ========================================================================
     STUDIO-GRADE ARTICLE & THREAD CREATOR (Variant 1: Full Page View)
     Features: Toolbar (H2, H3, B, I, Code, Pre, Link, List), Live Preview Tab
     ======================================================================== */
  function renderStudioPage(hubRoot) {
    if (!hubRoot) return;

    const session = getCurrentSession();
    const displayName = session?.user?.user_metadata?.username || session?.user?.email?.split('@')[0] || 'Developer';
    const displayRole = session?.user?.role || session?.user?.user_metadata?.role || 'user';

    hubRoot.innerHTML = `
      <div class="hub-page-view">
        <div class="hub-back-nav">
          <a href="#feed" class="hub-back-link">&larr; Back to Discussions</a>
          <span style="font-size:11.5px; color:#67788c;">Lab Studio &bull; Article Creator</span>
        </div>

        <div class="hub-view-card">
          <div class="hub-view-header">
            <div class="hub-view-title">
              <span class="hub-logo-tag" style="background:#66c0f4; color:#0e1218; font-size:10px; font-weight:800; padding:2px 6px; border-radius:2px;">LAB STUDIO</span>
              <span>Create Technical Thread / Article</span>
            </div>
          </div>

          <div class="hub-page-body">
            <form id="hub-studio-form" class="hub-studio-body" style="padding:0;">
              <!-- Author & Category Meta Bar -->
              <div style="display:flex; align-items:center; justify-content:space-between; flex-wrap:wrap; gap:10px; background:#10141b; border:1px solid #2a475e; border-radius:4px; padding:10px 14px;">
                <div>
                  <span style="font-size:11.5px; color:#8da4be;">Author: </span>
                  <strong style="color:#ffffff; font-size:12px;">u/${escapeHtml(displayName)}</strong>
                  <span class="role-badge role-${escapeHtml(displayRole.toLowerCase())}">[${escapeHtml(displayRole.toUpperCase())}]</span>
                </div>
                <div style="font-size:11px; color:#67788c;">
                  Format: GitHub Markdown &amp; Wikitext Supported
                </div>
              </div>

              <!-- Category / Flair Picker Pills -->
              <div>
                <label class="hub-form-label" style="display:block; margin-bottom:6px; font-size:12px; font-weight:700; color:#8da4be; text-transform:uppercase; letter-spacing:0.04em;">Select Technical Category / Flair</label>
                <div class="hub-flair-picker" id="studio-flair-picker">
                  <button type="button" class="hub-flair-choice selected" data-flair="ENGINE">Engine &amp; C++20</button>
                  <button type="button" class="hub-flair-choice" data-flair="FROZEN-LIFE">Frozen-Life SDK</button>
                  <button type="button" class="hub-flair-choice" data-flair="LABHAMMER">LabHammer &amp; Maps</button>
                  <button type="button" class="hub-flair-choice" data-flair="SHOWCASE">Showcase &amp; Demos</button>
                  <button type="button" class="hub-flair-choice" data-flair="HELP">Help &amp; Bugs</button>
                  ${canPostAnnouncement() ? '<button type="button" class="hub-flair-choice" data-flair="ANNOUNCEMENT">Official Announcement</button>' : ''}
                </div>
                <input type="hidden" id="studio-selected-flair" value="ENGINE">
              </div>

              <!-- Article Title -->
              <div>
                <label class="hub-form-label" for="studio-title-input" style="display:block; margin-bottom:6px; font-size:12px; font-weight:700; color:#8da4be; text-transform:uppercase; letter-spacing:0.04em;">Article Title</label>
                <input type="text" id="studio-title-input" class="hub-studio-title-input" placeholder="e.g. Creating Custom Weapon Viewmodels &amp; CSG Brushes..." required maxlength="150" autofocus autocomplete="off">
              </div>

              <!-- Editor Section with Toolbar & Tabs -->
              <div>
                <div class="hub-studio-toolbar-wrap">
                  <!-- Toolbar Buttons (Matching editor.html style) -->
                  <div class="hub-studio-toolbar">
                    <button type="button" class="hub-tool-btn" data-act="h2" title="Section Heading">H2</button>
                    <button type="button" class="hub-tool-btn" data-act="h3" title="Subsection Heading">H3</button>
                    <button type="button" class="hub-tool-btn" data-act="bold" title="Bold text">B</button>
                    <button type="button" class="hub-tool-btn" data-act="italic" title="Italic text">I</button>
                    <button type="button" class="hub-tool-btn" data-act="code" title="Inline code">&lt;code&gt;</button>
                    <button type="button" class="hub-tool-btn" data-act="pre" title="C++ / Code block">[Code Block]</button>
                    <button type="button" class="hub-tool-btn" data-act="quote" title="Blockquote">&gt; Quote</button>
                    <button type="button" class="hub-tool-btn" data-act="list" title="Bullet List">&bull; List</button>
                    <button type="button" class="hub-tool-btn" data-act="numlist" title="Numbered List">1. List</button>
                    <button type="button" class="hub-tool-btn" data-act="link" title="Hyperlink">[Link]</button>
                  </div>

                  <!-- Write / Live Preview Mode Tabs -->
                  <div class="hub-studio-tabs">
                    <button type="button" class="hub-tab-btn active" id="tab-btn-write">Write Mode</button>
                    <button type="button" class="hub-tab-btn" id="tab-btn-preview">Live Preview</button>
                  </div>
                </div>

                <!-- Editor Textarea -->
                <textarea id="studio-content-textarea" class="hub-studio-textarea" rows="14" placeholder="Write your guide, code examples, or bug report here... (Supports Markdown and Wikitext)" required></textarea>

                <!-- Live Preview Pane (Hidden in write mode) -->
                <div id="studio-preview-pane" class="hub-studio-preview hub-thread-full-content" style="display:none;"></div>
              </div>

              <!-- Footer Actions -->
              <div style="display:flex; justify-content:space-between; align-items:center; border-top:1px solid #2a475e; padding-top:14px;">
                <span style="font-size:11px; color:#67788c;">Tip: Press Ctrl+Enter to publish article</span>
                <div style="display:flex; gap:10px;">
                  <a href="#feed" class="hub-btn-secondary" style="text-decoration:none; display:inline-flex; align-items:center;">Cancel</a>
                  <button type="submit" class="hub-btn-primary" id="studio-submit-btn" style="padding:8px 24px; font-size:13px;">Publish Article</button>
                </div>
              </div>
            </form>
          </div>
        </div>
      </div>
    `;

    // Flair choice pill selection
    document.querySelectorAll('.hub-flair-choice').forEach(btn => {
      btn.addEventListener('click', () => {
        document.querySelectorAll('.hub-flair-choice').forEach(b => b.classList.remove('selected'));
        btn.classList.add('selected');
        document.getElementById('studio-selected-flair').value = btn.getAttribute('data-flair') || 'ENGINE';
      });
    });

    // Toolbar formatting actions
    const textarea = document.getElementById('studio-content-textarea');
    document.querySelectorAll('.hub-tool-btn').forEach(btn => {
      btn.addEventListener('click', () => {
        const act = btn.getAttribute('data-act');
        insertStudioFormatting(textarea, act);
      });
    });

    // Write / Live Preview tab toggling
    const tabWrite = document.getElementById('tab-btn-write');
    const tabPreview = document.getElementById('tab-btn-preview');
    const previewPane = document.getElementById('studio-preview-pane');

    tabWrite?.addEventListener('click', () => {
      tabWrite.classList.add('active');
      tabPreview.classList.remove('active');
      textarea.style.display = 'block';
      previewPane.style.display = 'none';
      textarea.focus();
    });

    tabPreview?.addEventListener('click', () => {
      tabPreview.classList.add('active');
      tabWrite.classList.remove('active');
      textarea.style.display = 'none';
      previewPane.style.display = 'block';
      const textVal = textarea.value.trim();
      previewPane.innerHTML = textVal ? formatPostContentFull(textVal) : '<p style="color:#67788c; font-style:italic;">No content written yet. Switch to Write Mode to compose your article.</p>';
    });

    // Ctrl+Enter shortcut to submit
    textarea.addEventListener('keydown', (e) => {
      if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
        e.preventDefault();
        document.getElementById('hub-studio-form')?.requestSubmit();
      }
    });

    // Form submit
    document.getElementById('hub-studio-form')?.addEventListener('submit', async (e) => {
      e.preventDefault();
      const submitBtn = document.getElementById('studio-submit-btn');
      submitBtn.disabled = true;
      submitBtn.textContent = 'Publishing...';

      const flair = document.getElementById('studio-selected-flair').value;
      const title = document.getElementById('studio-title-input').value.trim();
      const content = textarea.value.trim();

      const created = await createNewPost(title, flair, content);
      if (created && created.id) {
        window.location.hash = '#post/' + created.id;
      } else {
        window.location.hash = '#feed';
      }
    });
  }

  function insertStudioFormatting(textarea, act) {
    if (!textarea) return;
    const start = textarea.selectionStart;
    const end = textarea.selectionEnd;
    const text = textarea.value;
    const sel = text.substring(start, end);

    let before = '', after = '', defaultText = '';

    switch (act) {
      case 'h2':
        before = '\n## ';
        after = '\n';
        defaultText = 'Section Title';
        break;
      case 'h3':
        before = '\n### ';
        after = '\n';
        defaultText = 'Subsection Title';
        break;
      case 'bold':
        before = '**';
        after = '**';
        defaultText = 'bold text';
        break;
      case 'italic':
        before = '*';
        after = '*';
        defaultText = 'italic text';
        break;
      case 'code':
        before = '`';
        after = '`';
        defaultText = 'variable_or_flag';
        break;
      case 'pre':
        before = '\n```cpp\n';
        after = '\n```\n';
        defaultText = '// C++20 code snippet\nvoid InitializeSubsystem() {\n    // Code here\n}';
        break;
      case 'quote':
        before = '\n> ';
        after = '\n';
        defaultText = 'Quoted technical requirement';
        break;
      case 'list':
        before = '\n- ';
        after = '\n';
        defaultText = 'List item';
        break;
      case 'numlist':
        before = '\n1. ';
        after = '\n';
        defaultText = 'First step';
        break;
      case 'link':
        before = '[';
        after = '](https://labenginedev.vercel.app/)';
        defaultText = 'Link title';
        break;
    }

    const insertText = sel || defaultText;
    textarea.value = text.substring(0, start) + before + insertText + after + text.substring(end);
    textarea.focus();
    textarea.selectionStart = start + before.length;
    textarea.selectionEnd = start + before.length + insertText.length;
  }

  async function createNewPost(title, flair, content) {
    const session = getCurrentSession();
    const authorName = session?.user?.user_metadata?.username || session?.user?.email?.split('@')[0] || 'Developer';
    const authorRole = session?.user?.role || session?.user?.user_metadata?.role || 'user';
    const authorId = session?.user?.id || null;
    const isPinned = flair === 'ANNOUNCEMENT' && canPostAnnouncement();

    const localId = 'post-' + Date.now();
    const newPost = {
      id: localId,
      title: title,
      flair: flair,
      content: content,
      author_id: authorId,
      author_name: authorName,
      author_role: authorRole,
      upvotes: 1,
      is_pinned: isPinned,
      created_at: new Date().toISOString(),
      comments_count: 0
    };

    let syncedToDb = false;
    const config = getSupabaseConfig();
    if (config.url && config.anonKey && session?.access_token) {
      try {
        const res = await fetch(`${config.url}/rest/v1/community_posts`, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
            'apikey': config.anonKey,
            'Authorization': `Bearer ${session.access_token}`,
            'Prefer': 'return=representation'
          },
          body: JSON.stringify({
            title: title,
            flair: flair,
            content: content,
            author_id: authorId,
            author_name: authorName,
            author_role: authorRole,
            upvotes: 1,
            is_pinned: isPinned
          })
        });

        if (res.ok) {
          const inserted = await res.json();
          if (Array.isArray(inserted) && inserted[0]) {
            newPost = inserted[0];
            postsData.unshift(inserted[0]);
            syncedToDb = true;
            showNotification('Article published to Supabase community database.');
          }
        } else {
          const errData = await res.json().catch(() => ({}));
          console.warn('Supabase post insert non-OK:', res.status, errData);
          showNotification(`Database note: ${errData.message || 'Table not migrated yet'}. Saved locally.`);
        }
      } catch (err) {
        console.warn('Network error posting to Supabase, saving locally.', err);
      }
    }

    if (!syncedToDb) {
      saveLocalPost(newPost);
      if (!postsData.some(p => p.id === newPost.id)) {
        postsData.unshift(newPost);
      }
      showNotification('Article created and saved to local community feed.');
    }

    renderFilteredPosts();
    return newPost;
  }

  async function deletePost(postId) {
    postsData = postsData.filter(p => p.id !== postId);
    removeLocalPost(postId);
    renderFilteredPosts();

    const config = getSupabaseConfig();
    const session = getCurrentSession();
    if (config.url && config.anonKey && session?.access_token && !String(postId).startsWith('seed-') && !String(postId).startsWith('post-')) {
      try {
        await fetch(`${config.url}/rest/v1/community_posts?id=eq.${postId}`, {
          method: 'DELETE',
          headers: {
            'apikey': config.anonKey,
            'Authorization': `Bearer ${session.access_token}`
          }
        });
      } catch (err) {}
    }

    showNotification('Thread deleted.');
  }

  function canPostAnnouncement() {
    const session = getCurrentSession();
    if (!session || !session.user) return false;
    const role = (session.user.role || session.user.user_metadata?.role || 'user').toLowerCase();
    return ['admin', 'dev', 'moderator'].includes(role);
  }

  function canDeletePost(post) {
    const session = getCurrentSession();
    if (!session || !session.user) return false;
    const role = (session.user.role || session.user.user_metadata?.role || '').toLowerCase();
    if (['admin', 'dev', 'moderator'].includes(role)) return true;
    return session.user.id && post.author_id && session.user.id === post.author_id;
  }

  function getCurrentSession() {
    try {
      if (typeof window.VdcAuth?.getSession === 'function') {
        const authSession = window.VdcAuth.getSession();
        if (authSession && (authSession.user || authSession.access_token)) {
          return authSession;
        }
      }
      const raw = localStorage.getItem('lab_supabase_session') || localStorage.getItem('vdc_session_v1');
      if (raw) {
        const parsed = JSON.parse(raw);
        if (parsed && (parsed.user || parsed.access_token)) return parsed;
      }
    } catch (e) {}
    return null;
  }

  function getCurrentToken() {
    return getCurrentSession()?.access_token || '';
  }

  function getSupabaseConfig() {
    try {
      if (typeof window.VdcAuth?.getConfig === 'function') {
        const cfg = window.VdcAuth.getConfig();
        if (cfg && cfg.url && cfg.anonKey) return cfg;
      }
      const stored = JSON.parse(localStorage.getItem('lab_supabase_config') || '{}');
      if (stored && stored.url && stored.anonKey) return stored;
    } catch (e) {}

    return {
      url: window.LAB_SUPABASE_URL || '',
      anonKey: window.LAB_SUPABASE_ANON_KEY || ''
    };
  }

  function formatTimeAgo(date) {
    const seconds = Math.floor((new Date() - date) / 1000);
    if (seconds < 60) return 'just now';
    const minutes = Math.floor(seconds / 60);
    if (minutes < 60) return `${minutes}m ago`;
    const hours = Math.floor(minutes / 60);
    if (hours < 24) return `${hours}h ago`;
    const days = Math.floor(hours / 24);
    if (days < 30) return `${days}d ago`;
    return date.toLocaleDateString();
  }

  function formatPostContentPreview(text) {
    if (!text) return '';
    // Strip code blocks and markdown symbols for a clean card snippet
    const clean = text
      .replace(/```[\s\S]*?```/g, ' [Code Block] ')
      .replace(/`([^`]+)`/g, '$1')
      .replace(/^#{1,6}\s+/gm, '')
      .replace(/\*\*([^*]+)\*\*/g, '$1')
      .replace(/\*([^*]+)\*/g, '$1')
      .replace(/\r?\n+/g, ' ')
      .trim();
    const plain = clean.substring(0, 240);
    return escapeHtml(plain) + (clean.length > 240 ? '...' : '');
  }

  /* ========================================================================
     ROBUST GITHUB MARKDOWN & WIKITEXT CONTENT PARSER
     Solves the broken code blocks and formatting shown in Image 1
     ======================================================================== */
  function formatPostContentFull(rawText) {
    if (!rawText) return '';

    // Step 1: Extract and protect fenced code blocks FIRST
    const codeBlocks = [];
    let text = rawText.replace(/```([a-zA-Z0-9_\-]*)\r?\n?([\s\S]*?)```/g, (match, lang, code) => {
      const idx = codeBlocks.length;
      codeBlocks.push({ lang: (lang || '').trim(), code: code.replace(/^\r?\n|\r?\n$/g, '') });
      return `%%HUB_CODE_BLOCK_${idx}%%`;
    });

    // Step 2: Extract and protect inline code
    const inlineCodes = [];
    text = text.replace(/`([^`\r\n]+)`/g, (match, code) => {
      const idx = inlineCodes.length;
      inlineCodes.push(code);
      return `%%HUB_INLINE_CODE_${idx}%%`;
    });

    // Step 3: Escape HTML for security
    text = escapeHtml(text);

    // Step 4: Parse Headings (# H1, ## H2, ### H3, == H2 ==, === H3 ===)
    text = text.replace(/^###\s+(.+)$/gm, '<h3 class="hub-content-h3">$1</h3>');
    text = text.replace(/^##\s+(.+)$/gm, '<h2 class="hub-content-h2">$1</h2>');
    text = text.replace(/^#\s+(.+)$/gm, '<h1 class="hub-content-h1">$1</h1>');
    text = text.replace(/^===\s*(.+?)\s*===$/gm, '<h3 class="hub-content-h3">$1</h3>');
    text = text.replace(/^==\s*(.+?)\s*==$/gm, '<h2 class="hub-content-h2">$1</h2>');

    // Step 5: Parse Bold & Italic
    text = text.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');
    text = text.replace(/'''([^']+)'''/g, '<strong>$1</strong>');
    text = text.replace(/\*([^*\r\n]+)\*/g, '<em>$1</em>');
    text = text.replace(/''([^'\r\n]+)''/g, '<em>$1</em>');

    // Step 6: Parse Blockquotes (> text)
    text = text.replace(/^&gt;\s+(.+)$/gm, '<blockquote class="hub-quote">$1</blockquote>');

    // Step 7: Parse Links
    text = text.replace(/\[([^\]]+)\]\(([^)]+)\)/g, '<a href="$2" target="_blank" rel="noopener noreferrer" class="hub-content-link">$1</a>');
    text = text.replace(/\[\[(.+?)\|(.+?)\]\]/g, '<a href="$1" class="hub-content-link">$2</a>');
    text = text.replace(/\[\[(.+?)\]\]/g, '<a href="$1" class="hub-content-link">$1</a>');

    // Step 8: Parse Lists (ordered & unordered)
    const lines = text.split('\n');
    const processedLines = [];
    let inUl = false;
    let inOl = false;

    for (let i = 0; i < lines.length; i++) {
      const line = lines[i];
      const trimmed = line.trim();

      const mUl = trimmed.match(/^[-*]\s+(.+)$/);
      const mOl = trimmed.match(/^(\d+)\.\s+(.+)$/);

      if (mUl) {
        if (!inUl) {
          if (inOl) { processedLines.push('</ol>'); inOl = false; }
          processedLines.push('<ul class="hub-content-ul">');
          inUl = true;
        }
        processedLines.push(`<li>${mUl[1]}</li>`);
      } else if (mOl) {
        if (!inOl) {
          if (inUl) { processedLines.push('</ul>'); inUl = false; }
          processedLines.push('<ol class="hub-content-ol">');
          inOl = true;
        }
        processedLines.push(`<li>${mOl[2]}</li>`);
      } else {
        if (inUl) { processedLines.push('</ul>'); inUl = false; }
        if (inOl) { processedLines.push('</ol>'); inOl = false; }
        processedLines.push(line);
      }
    }
    if (inUl) processedLines.push('</ul>');
    if (inOl) processedLines.push('</ol>');

    text = processedLines.join('\n');

    // Step 9: Paragraph separation
    const paragraphs = text.split(/\n{2,}/);
    const outputParas = [];

    for (let i = 0; i < paragraphs.length; i++) {
      const p = paragraphs[i].trim();
      if (!p) continue;

      if (/^<(h[1-6]|ul|ol|blockquote|div|pre)/i.test(p) || p.startsWith('%%HUB_CODE_BLOCK_')) {
        outputParas.push(p);
      } else {
        const inner = p.replace(/\n/g, '<br>');
        outputParas.push(`<p class="hub-content-p">${inner}</p>`);
      }
    }

    text = outputParas.join('\n');

    // Step 10: Restore Fenced Code Blocks with Container & Copy button
    for (let i = 0; i < codeBlocks.length; i++) {
      const block = codeBlocks[i];
      const langLabel = block.lang ? block.lang.toUpperCase() : 'CODE';
      const escapedCode = escapeHtml(block.code);

      const blockHtml = `
        <div class="hub-code-container">
          <div class="hub-code-header">
            <span class="hub-code-lang">${langLabel}</span>
            <button type="button" class="hub-code-copy-btn" onclick="navigator.clipboard.writeText(this.closest('.hub-code-container').querySelector('code').innerText); this.textContent='Copied!'; setTimeout(() => this.textContent='Copy', 1500);">Copy</button>
          </div>
          <pre class="hub-code-pre"><code>${escapedCode}</code></pre>
        </div>
      `;
      text = text.replace(`%%HUB_CODE_BLOCK_${i}%%`, blockHtml);
    }

    // Step 11: Restore Inline Code with crisp styling
    for (let i = 0; i < inlineCodes.length; i++) {
      const escapedCode = escapeHtml(inlineCodes[i]);
      const inlineHtml = `<code class="hub-inline-code">${escapedCode}</code>`;
      text = text.replace(`%%HUB_INLINE_CODE_${i}%%`, inlineHtml);
    }

    return text;
  }

  function escapeHtml(str) {
    return String(str || '')
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#39;');
  }

  function showNotification(msg) {
    let t = document.getElementById('hub-toast');
    if (!t) {
      t = document.createElement('div');
      t.id = 'hub-toast';
      t.style.cssText = `
        position: fixed; bottom: 20px; right: 20px;
        background: #141c26; border: 1px solid #2a475e; border-left: 4px solid #66c0f4;
        color: #ffffff; padding: 10px 16px; border-radius: 3px; font-size: 12px;
        box-shadow: 0 4px 16px rgba(0,0,0,0.6); z-index: 100000;
        pointer-events: none; max-width: 380px; line-height: 1.4;
      `;
      document.body.appendChild(t);
    }
    t.textContent = msg;
    t.style.display = 'block';
    clearTimeout(t._timer);
    t._timer = setTimeout(() => { t.style.display = 'none'; }, 3400);
  }
})();
