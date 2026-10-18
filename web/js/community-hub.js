/**
 * Lab Developer Community - Discussion Hub (Reddit-style Ecosystem Board)
 * Strict technical architecture, zero AI slop, zero emojis.
 */

(function () {
  'use strict';

  const STORAGE_VOTES_KEY = 'lab_community_votes_v1';
  let activeFlair = 'ALL';
  let activeSort = 'HOT';
  let postsData = [];
  let commentsCache = {};
  let userVotes = {};

  try {
    userVotes = JSON.parse(localStorage.getItem(STORAGE_VOTES_KEY)) || {};
  } catch (e) {
    userVotes = {};
  }

  // Pre-seeded high-quality ecosystem discussions (fallback and baseline)
  const SEED_POSTS = [
    {
      id: 'seed-announcement-1',
      title: 'Lab Engine Architecture & Project Hub Roadmap',
      content: 'Official discussion thread for Lab Engine C++20 core, Direct State Access (OpenGL 4.5+ DSA), and the decoupled VFS architecture separating engine:// assets from Documents/LabGames/.\n\nAll subsystem headers are compiled under /include and exported via 11 modular dynamic libraries. Review the project hub documentation for packaging standalone builds.',
      flair: 'ANNOUNCEMENT',
      author_name: 'YoungJasiek',
      author_role: 'admin',
      upvotes: 42,
      is_pinned: true,
      created_at: new Date(Date.now() - 86400000 * 2).toISOString(),
      comments_count: 5
    },
    {
      id: 'seed-engine-1',
      title: 'GPU Bindless Textures & Vertex Buffer Storage Performance Benchmarks',
      content: 'Comparing glNamedBufferStorage with persistent mapped flags vs glBufferSubData across AMD and NVIDIA hardware. In our tests with 10,000 dynamic drawcalls, persistent DSA mapping reduced render-thread CPU driver overhead by 4.2x.',
      flair: 'ENGINE',
      author_name: 'GordonFreeman',
      author_role: 'admin',
      upvotes: 28,
      is_pinned: false,
      created_at: new Date(Date.now() - 86400000).toISOString(),
      comments_count: 3
    },
    {
      id: 'seed-fl-1',
      title: 'Frozen-Life: Tactical Bot AI State Machine Integration',
      content: 'The 5-state AI finite state machine (Patrol, Alert, Chase, Flank, Retreat) is now fully integrated with skeletal hitboxes and raycast line-of-sight checks. You can test waypoint navigation using test_multi_bot_waypoints.LABMAP in the editor.',
      flair: 'FROZEN-LIFE',
      author_name: 'DevTeam',
      author_role: 'dev',
      upvotes: 19,
      is_pinned: false,
      created_at: new Date(Date.now() - 3600000 * 14).toISOString(),
      comments_count: 2
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
      comments_count: 4
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
        created_at: new Date(Date.now() - 3600000 * 18).toISOString()
      }
    ],
    'seed-engine-1': [
      {
        id: 'c-3',
        author_name: 'YoungJasiek',
        author_role: 'admin',
        content: 'Make sure GL_MAP_COHERENT_BIT and GL_MAP_PERSISTENT_BIT are strictly coupled with glMemoryBarrier before dispatch.',
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

    renderHubLayout(hubRoot);
    wireHubEventListeners();
    fetchPosts();
  }

  function renderHubLayout(container) {
    container.innerHTML = `
      <div class="hub-container">
        <!-- Top Toolbar: Flair Pills, Sorting, and New Post Button -->
        <div class="hub-toolbar">
          <div class="hub-flairs" id="hub-flair-bar">
            <button type="button" class="hub-flair-btn active" data-flair="ALL">All Topics</button>
            <button type="button" class="hub-flair-btn" data-flair="ANNOUNCEMENT">Announcements</button>
            <button type="button" class="hub-flair-btn" data-flair="ENGINE">Engine & C++20</button>
            <button type="button" class="hub-flair-btn" data-flair="FROZEN-LIFE">Frozen-Life SDK</button>
            <button type="button" class="hub-flair-btn" data-flair="LABHAMMER">LabHammer & Maps</button>
            <button type="button" class="hub-flair-btn" data-flair="SHOWCASE">Showcase</button>
            <button type="button" class="hub-flair-btn" data-flair="HELP">Help & Bugs</button>
          </div>

          <div class="hub-toolbar-right">
            <div class="hub-sort-group">
              <span class="hub-sort-label">Sort:</span>
              <button type="button" class="hub-sort-btn active" data-sort="HOT">Hot</button>
              <button type="button" class="hub-sort-btn" data-sort="NEW">New</button>
              <button type="button" class="hub-sort-btn" data-sort="TOP">Top</button>
            </div>
            <button type="button" id="hub-btn-new-post" class="hub-btn-primary">+ Create Thread</button>
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

      <!-- New Thread Modal -->
      <div id="hub-modal-container"></div>
    `;
  }

  function wireHubEventListeners() {
    // Flair Filter Buttons
    document.querySelectorAll('.hub-flair-btn').forEach(btn => {
      btn.addEventListener('click', (e) => {
        document.querySelectorAll('.hub-flair-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        activeFlair = btn.getAttribute('data-flair') || 'ALL';
        renderFilteredPosts();
      });
    });

    // Sort Buttons
    document.querySelectorAll('.hub-sort-btn').forEach(btn => {
      btn.addEventListener('click', (e) => {
        document.querySelectorAll('.hub-sort-btn').forEach(b => b.classList.remove('active'));
        btn.classList.add('active');
        activeSort = btn.getAttribute('data-sort') || 'HOT';
        renderFilteredPosts();
      });
    });

    // Create Thread Button
    document.getElementById('hub-btn-new-post')?.addEventListener('click', () => {
      const session = getCurrentSession();
      if (!session || !session.user) {
        if (typeof window.VdcAuth?.openLogin === 'function') {
          window.VdcAuth.openLogin();
        } else {
          document.getElementById('vdc-login-link')?.click();
        }
        return;
      }
      openNewPostModal();
    });
  }

  async function fetchPosts() {
    const feed = document.getElementById('hub-posts-feed');
    if (!feed) return;

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
          if (Array.isArray(dbPosts) && dbPosts.length > 0) {
            postsData = dbPosts;
            renderFilteredPosts();
            return;
          }
        }
      } catch (err) {
        console.warn('Supabase community_posts fetch failed, falling back to local dataset.', err);
      }
    }

    // Baseline fallback to seed posts
    postsData = [...SEED_POSTS];
    renderFilteredPosts();
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
          <button type="button" class="hub-btn-primary" onclick="document.getElementById('hub-btn-new-post').click()">Start the first thread</button>
        </div>
      `;
      return;
    }

    feed.innerHTML = filtered.map(post => renderPostCardHtml(post)).join('');
    wirePostCardInteractions();
  }

  function renderPostCardHtml(post) {
    const userVote = userVotes[post.id] || 0;
    const authorRole = (post.author_role || 'user').toLowerCase();
    const timeAgo = formatTimeAgo(new Date(post.created_at));
    const commentsCount = post.comments_count !== undefined ? post.comments_count : (SEED_COMMENTS[post.id]?.length || 0);

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
            <span class="hub-author-name">${escapeHtml(post.author_name || 'Developer')}</span>
            <span class="role-badge role-${escapeHtml(authorRole)}">${escapeHtml(authorRole.toUpperCase())}</span>
            <span class="hub-meta-sep">&bull;</span>
            <span class="hub-post-time">${timeAgo}</span>
          </div>

          <h2 class="hub-post-title">
            <a href="#" class="hub-post-link" data-post-id="${escapeHtml(post.id)}">${escapeHtml(post.title)}</a>
          </h2>

          <div class="hub-post-snippet">
            ${formatPostContentPreview(post.content)}
          </div>

          <div class="hub-post-footer">
            <button type="button" class="hub-action-btn hub-btn-comments" data-post-id="${escapeHtml(post.id)}">
              Comments (${commentsCount})
            </button>
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

    // Click on Post Title or Comments Button opens thread view
    document.querySelectorAll('.hub-post-link, .hub-btn-comments').forEach(el => {
      el.addEventListener('click', (e) => {
        e.preventDefault();
        const postId = el.getAttribute('data-post-id');
        openThreadView(postId);
      });
    });

    // Copy Link Button
    document.querySelectorAll('.hub-btn-share').forEach(btn => {
      btn.addEventListener('click', (e) => {
        e.stopPropagation();
        const postId = btn.getAttribute('data-post-id');
        const shareUrl = `${window.location.origin}${window.location.pathname}#post-${postId}`;
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
      // Toggle off
      userVotes[postId] = 0;
      delta = -dir;
    } else {
      userVotes[postId] = dir;
      delta = dir - currentVote;
    }

    post.upvotes = (post.upvotes || 0) + delta;
    localStorage.setItem(STORAGE_VOTES_KEY, JSON.stringify(userVotes));

    renderFilteredPosts();

    // Async sync with Supabase if configured
    const config = getSupabaseConfig();
    if (config.url && config.anonKey && !String(postId).startsWith('seed-')) {
      fetch(`${config.url}/rest/v1/community_posts?id=eq.${postId}`, {
        method: 'PATCH',
        headers: {
          'Content-Type': 'application/json',
          'apikey': config.anonKey,
          'Authorization': `Bearer ${getCurrentToken() || config.anonKey}`
        },
        body: JSON.stringify({ upvotes: post.upvotes })
      }).catch(() => {});
    }
  }

  async function openThreadView(postId) {
    const post = postsData.find(p => p.id === postId);
    if (!post) return;

    const modalContainer = document.getElementById('hub-modal-container');
    if (!modalContainer) return;

    modalContainer.innerHTML = `
      <div class="hub-modal-backdrop" id="thread-modal-backdrop">
        <div class="hub-thread-modal">
          <div class="hub-modal-header">
            <div class="hub-modal-title">
              <span class="hub-flair-tag flair-${escapeHtml(String(post.flair).toLowerCase())}">${escapeHtml(post.flair)}</span>
              <span>${escapeHtml(post.title)}</span>
            </div>
            <button type="button" class="hub-modal-close" id="thread-modal-close">&times;</button>
          </div>

          <div class="hub-thread-body">
            <div class="hub-post-meta" style="margin-bottom:12px;">
              <span>Posted by <strong>${escapeHtml(post.author_name || 'Developer')}</strong></span>
              <span class="role-badge role-${escapeHtml((post.author_role || 'user').toLowerCase())}">${escapeHtml((post.author_role || 'user').toUpperCase())}</span>
              <span class="hub-meta-sep">&bull;</span>
              <span>${formatTimeAgo(new Date(post.created_at))}</span>
            </div>

            <div class="hub-thread-full-content">
              ${formatPostContentFull(post.content)}
            </div>

            <!-- Comment Input Box -->
            <div class="hub-comment-box-wrap">
              <div class="hub-comment-box-header">Add a comment:</div>
              <textarea id="hub-new-comment-text" class="hub-textarea" placeholder="Write a constructive response or technical inquiry..." rows="3"></textarea>
              <div style="display:flex; justify-content:flex-end; margin-top:8px;">
                <button type="button" id="hub-submit-comment-btn" class="hub-btn-primary">Post Comment</button>
              </div>
            </div>

            <!-- Comments List -->
            <div class="hub-comments-section">
              <div class="hub-comments-title" id="thread-comments-heading">Discussion</div>
              <div class="hub-comments-list" id="thread-comments-list">
                <div class="hub-loading">Loading comments...</div>
              </div>
            </div>
          </div>
        </div>
      </div>
    `;

    document.getElementById('thread-modal-close')?.addEventListener('click', () => {
      modalContainer.innerHTML = '';
    });

    document.getElementById('thread-modal-backdrop')?.addEventListener('click', (e) => {
      if (e.target.id === 'thread-modal-backdrop') modalContainer.innerHTML = '';
    });

    document.getElementById('hub-submit-comment-btn')?.addEventListener('click', async () => {
      const textarea = document.getElementById('hub-new-comment-text');
      const text = textarea?.value?.trim();
      if (!text) return;

      const session = getCurrentSession();
      if (!session || !session.user) {
        if (typeof window.VdcAuth?.openLogin === 'function') {
          window.VdcAuth.openLogin();
        } else {
          document.getElementById('vdc-login-link')?.click();
        }
        return;
      }

      await submitComment(postId, text);
      textarea.value = '';
      loadCommentsForThread(postId);
    });

    loadCommentsForThread(postId);
  }

  async function loadCommentsForThread(postId) {
    const listEl = document.getElementById('thread-comments-list');
    if (!listEl) return;

    let comments = SEED_COMMENTS[postId] || [];
    const config = getSupabaseConfig();

    if (config.url && config.anonKey && !String(postId).startsWith('seed-')) {
      try {
        const res = await fetch(`${config.url}/rest/v1/community_comments?post_id=eq.${postId}&order=created_at.asc`, {
          headers: {
            'apikey': config.anonKey,
            'Authorization': `Bearer ${getCurrentToken() || config.anonKey}`
          }
        });
        if (res.ok) {
          comments = await res.json();
        }
      } catch (err) {
        console.warn('Could not load comments from Supabase:', err);
      }
    }

    if (comments.length === 0) {
      listEl.innerHTML = '<div class="hub-no-comments">No responses yet. Be the first to comment.</div>';
      return;
    }

    listEl.innerHTML = comments.map(c => `
      <div class="hub-comment-item">
        <div class="hub-comment-meta">
          <span class="hub-author-name">${escapeHtml(c.author_name || 'Developer')}</span>
          <span class="role-badge role-${escapeHtml((c.author_role || 'user').toLowerCase())}">${escapeHtml((c.author_role || 'user').toUpperCase())}</span>
          <span class="hub-meta-sep">&bull;</span>
          <span class="hub-post-time">${formatTimeAgo(new Date(c.created_at))}</span>
        </div>
        <div class="hub-comment-body">${formatPostContentFull(c.content)}</div>
      </div>
    `).join('');
  }

  async function submitComment(postId, content) {
    const session = getCurrentSession();
    const config = getSupabaseConfig();
    const authorName = session.user.user_metadata?.username || session.user.email?.split('@')[0] || 'Developer';
    const authorRole = session.user.role || 'user';

    const newCommentObj = {
      id: 'c-' + Date.now(),
      post_id: postId,
      author_id: session.user.id,
      author_name: authorName,
      author_role: authorRole,
      content: content,
      upvotes: 1,
      created_at: new Date().toISOString()
    };

    if (config.url && config.anonKey && !String(postId).startsWith('seed-')) {
      try {
        await fetch(`${config.url}/rest/v1/community_comments`, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
            'apikey': config.anonKey,
            'Authorization': `Bearer ${session.access_token}`
          },
          body: JSON.stringify({
            post_id: postId,
            author_id: session.user.id,
            author_name: authorName,
            author_role: authorRole,
            content: content
          })
        });
      } catch (err) {}
    } else {
      if (!SEED_COMMENTS[postId]) SEED_COMMENTS[postId] = [];
      SEED_COMMENTS[postId].push(newCommentObj);
    }

    const post = postsData.find(p => p.id === postId);
    if (post) post.comments_count = (post.comments_count || 0) + 1;
    renderFilteredPosts();
    showNotification('Comment published.');
  }

  function openNewPostModal() {
    const modalContainer = document.getElementById('hub-modal-container');
    if (!modalContainer) return;

    modalContainer.innerHTML = `
      <div class="hub-modal-backdrop" id="new-post-modal-backdrop">
        <div class="hub-thread-modal">
          <div class="hub-modal-header">
            <h3 style="margin:0; font-size:14px; color:#ffffff;">Create Discussion Thread</h3>
            <button type="button" class="hub-modal-close" id="new-post-modal-close">&times;</button>
          </div>

          <form id="hub-new-post-form" style="padding:16px;">
            <div style="margin-bottom:12px;">
              <label class="hub-form-label">Category / Flair</label>
              <select id="hub-post-flair-select" class="hub-select" required>
                <option value="ENGINE">Engine & C++20</option>
                <option value="FROZEN-LIFE">Frozen-Life SDK</option>
                <option value="LABHAMMER">LabHammer & Maps</option>
                <option value="SHOWCASE">Showcase & Demos</option>
                <option value="HELP">Help & Bugs</option>
                ${canPostAnnouncement() ? '<option value="ANNOUNCEMENT">Announcement (Dev Only)</option>' : ''}
              </select>
            </div>

            <div style="margin-bottom:12px;">
              <label class="hub-form-label">Thread Title</label>
              <input type="text" id="hub-post-title-input" class="hub-input" placeholder="Clear, concise title summarizing your topic..." required maxlength="150">
            </div>

            <div style="margin-bottom:14px;">
              <label class="hub-form-label">Body Content (Supports code blocks and wikitext)</label>
              <textarea id="hub-post-content-input" class="hub-textarea" rows="8" placeholder="Provide background details, steps to reproduce, or code snippets..." required></textarea>
            </div>

            <div style="display:flex; justify-content:flex-end; gap:8px;">
              <button type="button" class="hub-btn-secondary" id="new-post-cancel-btn">Cancel</button>
              <button type="submit" class="hub-btn-primary" id="new-post-submit-btn">Publish Thread</button>
            </div>
          </form>
        </div>
      </div>
    `;

    document.getElementById('new-post-modal-close')?.addEventListener('click', () => modalContainer.innerHTML = '');
    document.getElementById('new-post-cancel-btn')?.addEventListener('click', () => modalContainer.innerHTML = '');
    document.getElementById('new-post-modal-backdrop')?.addEventListener('click', (e) => {
      if (e.target.id === 'new-post-modal-backdrop') modalContainer.innerHTML = '';
    });

    document.getElementById('hub-new-post-form')?.addEventListener('submit', async (e) => {
      e.preventDefault();
      const submitBtn = document.getElementById('new-post-submit-btn');
      submitBtn.disabled = true;
      submitBtn.textContent = 'Publishing...';

      const flair = document.getElementById('hub-post-flair-select').value;
      const title = document.getElementById('hub-post-title-input').value.trim();
      const content = document.getElementById('hub-post-content-input').value.trim();

      await createNewPost(title, flair, content);
      modalContainer.innerHTML = '';
      fetchPosts();
    });
  }

  async function createNewPost(title, flair, content) {
    const session = getCurrentSession();
    if (!session || !session.user) return;

    const authorName = session.user.user_metadata?.username || session.user.email?.split('@')[0] || 'Developer';
    const authorRole = session.user.role || 'user';
    const config = getSupabaseConfig();

    const newPost = {
      id: 'local-' + Date.now(),
      title: title,
      flair: flair,
      content: content,
      author_id: session.user.id,
      author_name: authorName,
      author_role: authorRole,
      upvotes: 1,
      is_pinned: flair === 'ANNOUNCEMENT' && canPostAnnouncement(),
      created_at: new Date().toISOString(),
      comments_count: 0
    };

    if (config.url && config.anonKey) {
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
            author_id: session.user.id,
            author_name: authorName,
            author_role: authorRole,
            upvotes: 1,
            is_pinned: newPost.is_pinned
          })
        });

        if (res.ok) {
          const inserted = await res.json();
          if (Array.isArray(inserted) && inserted[0]) {
            postsData.unshift(inserted[0]);
            showNotification('Discussion thread published to live Supabase.');
            return;
          }
        }
      } catch (err) {
        console.warn('Could not post to live database, storing locally.', err);
      }
    }

    postsData.unshift(newPost);
    showNotification('Discussion thread created.');
  }

  async function deletePost(postId) {
    postsData = postsData.filter(p => p.id !== postId);
    renderFilteredPosts();

    const config = getSupabaseConfig();
    const session = getCurrentSession();
    if (config.url && config.anonKey && session?.access_token && !String(postId).startsWith('seed-')) {
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
    const role = session.user.role || (session.user.user_metadata?.role) || 'user';
    return ['admin', 'dev', 'moderator'].includes(role.toLowerCase());
  }

  function canDeletePost(post) {
    const session = getCurrentSession();
    if (!session || !session.user) return false;
    const role = (session.user.role || '').toLowerCase();
    if (['admin', 'dev', 'moderator'].includes(role)) return true;
    return session.user.id && post.author_id && session.user.id === post.author_id;
  }

  function getCurrentSession() {
    try {
      return JSON.parse(localStorage.getItem('vdc_session_v1')) || null;
    } catch (e) {
      return null;
    }
  }

  function getCurrentToken() {
    return getCurrentSession()?.access_token || '';
  }

  function getSupabaseConfig() {
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
    const plain = text.substring(0, 240);
    return escapeHtml(plain) + (text.length > 240 ? '...' : '');
  }

  function formatPostContentFull(text) {
    if (!text) return '';
    return text
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
      .replace(/'''(.+?)'''/g, '<strong>$1</strong>')
      .replace(/''(.+?)''/g, '<em>$1</em>')
      .replace(/\[\[(.+?)\|(.+?)\]\]/g, '<a href="$1" style="color:var(--wiki-accent);">$2</a>')
      .replace(/\[\[(.+?)\]\]/g, '<a href="$1" style="color:var(--wiki-accent);">$1</a>')
      .replace(/`([^`]+)`/g, '<code class="hub-inline-code">$1</code>')
      .replace(/```([a-zA-Z]*)\n([\s\S]*?)```/g, '<pre class="hub-code-block"><code>$2</code></pre>')
      .replace(/\n\n/g, '<p></p>')
      .replace(/\n/g, '<br>');
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
    t._timer = setTimeout(() => { t.style.display = 'none'; }, 3200);
  }
})();
