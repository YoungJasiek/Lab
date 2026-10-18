/**
 * Lab Developer Community - Discussion Hub (Reddit-style Ecosystem Board)
 * Strict technical architecture, zero AI slop, zero emojis.
 * Full integration with Supabase Auth (lab_supabase_session) & PostgreSQL RLS.
 */

(function () {
  'use strict';

  const STORAGE_VOTES_KEY = 'lab_community_votes_v1';
  const STORAGE_COMMENT_VOTES_KEY = 'lab_community_comment_votes_v1';
  const STORAGE_LOCAL_POSTS_KEY = 'lab_community_local_posts_v1';
  const STORAGE_LOCAL_COMMENTS_KEY = 'lab_community_local_comments_v1';

  let activeFlair = 'ALL';
  let activeSort = 'HOT';
  let postsData = [];
  let userVotes = {};
  let userCommentVotes = {};
  let activeCommentMap = new Map();

  try {
    userVotes = JSON.parse(localStorage.getItem(STORAGE_VOTES_KEY)) || {};
  } catch (e) {
    userVotes = {};
  }

  try {
    userCommentVotes = JSON.parse(localStorage.getItem(STORAGE_COMMENT_VOTES_KEY)) || {};
  } catch (e) {
    userCommentVotes = {};
  }

  // Community hub discussions are persisted in PostgreSQL Supabase (community_posts & community_comments)
  const SEED_POSTS = [];
  const SEED_COMMENTS = {};

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
      const list = all[postId] || [];
      const seenIds = new Set();
      const deduped = [];
      for (const c of list) {
        if (!c || !c.content) continue;
        if (seenIds.has(c.id)) continue;
        // Filter out rapid accidental duplicate submissions (same author, content, parent within 4s)
        const isDuplicateSubmission = deduped.some(prev => 
          prev.author_name === c.author_name &&
          prev.content === c.content &&
          prev.parent_id === c.parent_id &&
          Math.abs(new Date(prev.created_at).getTime() - new Date(c.created_at).getTime()) < 4000
        );
        if (isDuplicateSubmission) continue;

        seenIds.add(c.id);
        deduped.push(c);
      }
      return deduped;
    } catch (e) {
      return [];
    }
  }

  function saveLocalComment(postId, comment) {
    try {
      const all = JSON.parse(localStorage.getItem(STORAGE_LOCAL_COMMENTS_KEY)) || {};
      if (!all[postId]) all[postId] = [];
      const list = all[postId];
      const isDuplicate = list.some(c => 
        c.id === comment.id || 
        (c.author_name === comment.author_name && 
         c.content === comment.content && 
         c.parent_id === comment.parent_id &&
         Math.abs(new Date(c.created_at).getTime() - new Date(comment.created_at).getTime()) < 4000)
      );
      if (!isDuplicate) {
        list.push(comment);
      }
      localStorage.setItem(STORAGE_LOCAL_COMMENTS_KEY, JSON.stringify(all));
    } catch (e) {}
  }

  function removeLocalComment(postId, commentId) {
    try {
      const all = JSON.parse(localStorage.getItem(STORAGE_LOCAL_COMMENTS_KEY)) || {};
      if (all[postId]) {
        all[postId] = all[postId].filter(c => c.id !== commentId && c.parent_id !== commentId);
        localStorage.setItem(STORAGE_LOCAL_COMMENTS_KEY, JSON.stringify(all));
      }
    } catch (e) {}
  }

  async function fetchPosts(shouldRender = true) {
    const localPosts = getLocalPosts();
    const config = await ensureSupabaseConfig();

    if (config.url && config.anonKey) {
      try {
        const res = await fetch(`${config.url}/rest/v1/community_posts?select=*,community_comments(count)&order=is_pinned.desc,upvotes.desc,created_at.desc`, {
          headers: {
            'apikey': config.anonKey,
            'Authorization': `Bearer ${getCurrentToken() || config.anonKey}`
          }
        });

        if (res.ok) {
          let dbPosts = await res.json();
          if (Array.isArray(dbPosts)) {
            // Filter out dummy test discussions
            dbPosts = dbPosts.filter(p => {
              const t = (p.title || '').trim().toLowerCase();
              const c = (p.content || '').trim().toLowerCase();
              return t !== 'test' && c !== 'test' && !t.startsWith('test_');
            });

            // Map PostgREST relation count to post.comments_count
            dbPosts.forEach(p => {
              if (Array.isArray(p.community_comments) && p.community_comments[0] && typeof p.community_comments[0].count === 'number') {
                p.comments_count = p.community_comments[0].count;
              } else if (p.comments_count === undefined || p.comments_count === null) {
                p.comments_count = getLocalComments(p.id).length;
              }
            });

            const dbIds = new Set(dbPosts.map(p => p.id));
            const unsyncedLocal = localPosts.filter(lp => !dbIds.has(lp.id));

            // Auto-sync pending local posts created when DB was offline or unmigrated
            for (const lp of unsyncedLocal) {
              if (String(lp.id).startsWith('post-')) {
                const alreadyInDb = dbPosts.some(dbp => dbp.title === lp.title && dbp.author_name === lp.author_name);
                if (alreadyInDb) {
                  removeLocalPost(lp.id);
                  continue;
                }
                try {
                  const syncRes = await fetch(`${config.url}/rest/v1/community_posts`, {
                    method: 'POST',
                    headers: {
                      'Content-Type': 'application/json',
                      'apikey': config.anonKey,
                      'Authorization': `Bearer ${getCurrentToken() || config.anonKey}`,
                      'Prefer': 'return=representation'
                    },
                    body: JSON.stringify({
                      title: lp.title,
                      flair: lp.flair || 'GENERAL',
                      content: lp.content,
                      author_id: lp.author_id || null,
                      author_name: lp.author_name || 'Developer',
                      author_role: lp.author_role || 'user',
                      upvotes: lp.upvotes || 1,
                      is_pinned: lp.is_pinned || false
                    })
                  });
                  if (syncRes.ok) {
                    const syncedRows = await syncRes.json();
                    if (Array.isArray(syncedRows) && syncedRows[0]) {
                      const newDbPost = syncedRows[0];
                      removeLocalPost(lp.id);
                      dbPosts.unshift(newDbPost);
                      dbIds.add(newDbPost.id);

                      // Migrate local comments from local ID to Supabase UUID
                      try {
                        const localCommentsMap = JSON.parse(localStorage.getItem(STORAGE_LOCAL_COMMENTS_KEY)) || {};
                        if (localCommentsMap[lp.id] && localCommentsMap[lp.id].length > 0) {
                          localCommentsMap[newDbPost.id] = localCommentsMap[lp.id];
                          delete localCommentsMap[lp.id];
                          localStorage.setItem(STORAGE_LOCAL_COMMENTS_KEY, JSON.stringify(localCommentsMap));
                        }
                      } catch (cErr) {}
                    }
                  }
                } catch (syncErr) {
                  console.warn('Could not auto-sync local post:', syncErr);
                }
              }
            }

            const remainingUnsynced = getLocalPosts().filter(lp => !dbIds.has(lp.id) && (lp.title || '').trim().toLowerCase() !== 'test');
            postsData = [...remainingUnsynced, ...dbPosts];
            if (shouldRender && document.getElementById('hub-posts-feed')) renderFilteredPosts();
            return postsData;
          }
        }
      } catch (err) {
        console.warn('Supabase community_posts fetch failed, falling back to local dataset.', err);
      }
    }

    postsData = [...localPosts.filter(lp => (lp.title || '').trim().toLowerCase() !== 'test')];
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
    const commentsCount = (post.comments_count !== undefined && post.comments_count !== null)
      ? post.comments_count 
      : getLocalComments(post.id).length;

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

              <span style="font-size:12px; color:#8da4be; font-weight:600;" id="page-comments-counter">
                Comments (${(post.comments_count !== undefined && post.comments_count !== null) ? post.comments_count : 0})
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
      const btn = document.getElementById('hub-submit-comment-btn');
      if (btn?.disabled) return;

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

      btn.disabled = true;
      btn.textContent = 'Posting...';
      try {
        await submitComment(postId, text, null);
        textarea.value = '';
      } finally {
        btn.disabled = false;
        btn.textContent = 'Comment';
      }
      loadCommentsForThread(postId, post.author_name);
    });

    loadCommentsForThread(postId, post.author_name);
  }

  async function loadCommentsForThread(postId, opAuthorName) {
    const listEl = document.getElementById('thread-comments-list');
    if (!listEl) return;

    const commentMap = new Map();

    function ingestComment(c, parentId = null) {
      if (!c || !c.id) return;
      const effectiveParentId = c.parent_id !== undefined ? c.parent_id : parentId;
      if (!commentMap.has(c.id)) {
        commentMap.set(c.id, {
          id: c.id,
          post_id: c.post_id || postId,
          parent_id: effectiveParentId,
          author_id: c.author_id || null,
          author_name: c.author_name || 'Developer',
          author_role: c.author_role || 'user',
          content: c.content || '',
          upvotes: c.upvotes || 1,
          created_at: c.created_at || new Date().toISOString(),
          replies: []
        });
      }

      if (Array.isArray(c.replies)) {
        c.replies.forEach(r => ingestComment(r, c.id));
      }
    }

    // 1. Ingest baseline seed comments
    (SEED_COMMENTS[postId] || []).forEach(c => ingestComment(c, null));

    // 2. Ingest local comments
    getLocalComments(postId).forEach(c => ingestComment(c, c.parent_id || null));

    // 3. Ingest Supabase comments if configured
    const config = await ensureSupabaseConfig();
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
          if (Array.isArray(dbComments)) {
            const dbCommentIds = new Set(dbComments.map(c => c.id));
            const pendingComments = getLocalComments(postId).filter(lc => !dbCommentIds.has(lc.id));
            for (const pc of pendingComments) {
              if (String(pc.id).startsWith('c-')) {
                try {
                  const cRes = await fetch(`${config.url}/rest/v1/community_comments`, {
                    method: 'POST',
                    headers: {
                      'Content-Type': 'application/json',
                      'apikey': config.anonKey,
                      'Authorization': `Bearer ${getCurrentToken() || config.anonKey}`,
                      'Prefer': 'return=representation'
                    },
                    body: JSON.stringify({
                      post_id: postId,
                      parent_id: pc.parent_id || null,
                      author_id: pc.author_id || null,
                      author_name: pc.author_name || 'Developer',
                      author_role: pc.author_role || 'user',
                      content: pc.content
                    })
                  });
                  if (cRes.ok) {
                    const cRows = await cRes.json();
                    if (Array.isArray(cRows) && cRows[0]) {
                      removeLocalComment(postId, pc.id);
                      dbComments.push(cRows[0]);
                      dbCommentIds.add(cRows[0].id);
                    }
                  }
                } catch (cErr) {}
              }
            }
            dbComments.forEach(c => ingestComment(c, c.parent_id || null));
          }
        }
      } catch (err) {
        console.warn('Could not load comments from Supabase:', err);
      }
    }

    if (commentMap.size === 0) {
      listEl.innerHTML = '<div class="hub-no-comments">No responses yet. Be the first to comment.</div>';
      const heading = document.getElementById('thread-comments-heading');
      if (heading) heading.textContent = 'All Responses (0)';
      const pageCounter = document.getElementById('page-comments-counter');
      if (pageCounter) pageCounter.textContent = 'Comments (0)';
      const pObj = postsData.find(p => String(p.id) === String(postId));
      if (pObj) pObj.comments_count = 0;
      return;
    }

    // 4. Assemble hierarchical Reddit-style comment tree
    const rootComments = [];
    const allComments = Array.from(commentMap.values());

    allComments.forEach(c => {
      c.replies = [];
    });

    allComments.forEach(c => {
      if (c.parent_id && commentMap.has(c.parent_id)) {
        const parent = commentMap.get(c.parent_id);
        if (!parent.replies.some(r => r.id === c.id)) {
          parent.replies.push(c);
        }
      } else {
        rootComments.push(c);
      }
    });

    activeCommentMap = commentMap;
    const totalCount = commentMap.size;
    const heading = document.getElementById('thread-comments-heading');
    if (heading) heading.textContent = `All Responses (${totalCount})`;
    const pageCounter = document.getElementById('page-comments-counter');
    if (pageCounter) pageCounter.textContent = `Comments (${totalCount})`;
    const pObj = postsData.find(p => String(p.id) === String(postId));
    if (pObj) pObj.comments_count = totalCount;

    listEl.innerHTML = rootComments.map(c => renderCommentNodeHtml(c, opAuthorName, postId)).join('');
    wireCommentReplyButtons(postId, opAuthorName);
  }

  function renderCommentNodeHtml(comment, opAuthorName, postId) {
    const role = (comment.author_role || 'user').toLowerCase();
    const timeAgo = formatTimeAgo(new Date(comment.created_at));
    const isOp = comment.author_name && opAuthorName && (comment.author_name.toLowerCase() === opAuthorName.toLowerCase());
    const initial = (comment.author_name || 'D').charAt(0).toUpperCase();
    const userCommentVote = userCommentVotes[comment.id] || 0;

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
            <!-- Comment Vote Cluster -->
            <div class="hub-comment-vote-cluster" data-comment-id="${escapeHtml(comment.id)}">
              <button type="button" class="hub-comment-vote-btn upvote ${userCommentVote === 1 ? 'voted' : ''}" data-dir="1" title="Upvote">&#9650;</button>
              <span class="hub-comment-vote-count ${userCommentVote === 1 ? 'voted-up' : userCommentVote === -1 ? 'voted-down' : ''}" id="comment-vote-count-${escapeHtml(comment.id)}">${comment.upvotes || 0}</span>
              <button type="button" class="hub-comment-vote-btn downvote ${userCommentVote === -1 ? 'voted' : ''}" data-dir="-1" title="Downvote">&#9660;</button>
            </div>
            <span class="hub-meta-sep">&bull;</span>
            <button type="button" class="hub-comment-action-btn hub-btn-reply" data-comment-id="${escapeHtml(comment.id)}">Reply</button>
            ${canDeleteComment(comment, opAuthorName) ? `
              <span class="hub-meta-sep">&bull;</span>
              <button type="button" class="hub-comment-action-btn hub-btn-delete-comment text-danger" data-comment-id="${escapeHtml(comment.id)}">Delete</button>
            ` : ''}
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
        if (btn.disabled) return;
        const commentId = btn.getAttribute('data-comment-id');
        const textarea = document.getElementById(`reply-text-${commentId}`);
        const text = textarea?.value?.trim();
        if (!text) return;

        const session = getCurrentSession();
        if (!session || !session.user) {
          showNotification('Please log in with your developer account to reply.');
          if (typeof window.VdcAuth?.openLogin === 'function') {
            window.VdcAuth.openLogin();
          }
          return;
        }

        btn.disabled = true;
        btn.textContent = 'Sending...';
        try {
          await submitComment(postId, text, commentId);
          const box = document.getElementById(`reply-box-${commentId}`);
          if (box) box.style.display = 'none';
        } finally {
          btn.disabled = false;
          btn.textContent = 'Send';
        }
        loadCommentsForThread(postId, opAuthorName);
      });
    });

    document.querySelectorAll('.hub-btn-delete-comment').forEach(btn => {
      btn.addEventListener('click', async (e) => {
        e.stopPropagation();
        const commentId = btn.getAttribute('data-comment-id');
        if (!confirm('Are you sure you want to delete this comment?')) return;
        await deleteComment(postId, commentId, opAuthorName);
      });
    });

    document.querySelectorAll('.hub-comment-vote-btn').forEach(btn => {
      btn.addEventListener('click', (e) => {
        e.stopPropagation();
        const cluster = btn.closest('.hub-comment-vote-cluster');
        const commentId = cluster?.getAttribute('data-comment-id');
        const dir = parseInt(btn.getAttribute('data-dir'), 10);
        if (commentId && (dir === 1 || dir === -1)) {
          handleCommentVote(commentId, dir, postId);
        }
      });
    });
  }

  async function handleCommentVote(commentId, dir, postId) {
    const session = getCurrentSession();
    if (!session || !session.user) {
      showNotification('Please log in with your developer account to vote on comments.');
      if (typeof window.VdcAuth?.openLogin === 'function') {
        window.VdcAuth.openLogin();
      }
      return;
    }

    const currentVote = userCommentVotes[commentId] || 0;
    let delta = 0;

    if (currentVote === dir) {
      userCommentVotes[commentId] = 0;
      delta = -dir;
    } else {
      userCommentVotes[commentId] = dir;
      delta = dir - currentVote;
    }

    localStorage.setItem(STORAGE_COMMENT_VOTES_KEY, JSON.stringify(userCommentVotes));

    const cObj = activeCommentMap.get(commentId);
    if (cObj) {
      cObj.upvotes = Math.max(0, (cObj.upvotes || 0) + delta);
    }

    // Instant zero-flicker DOM update
    const cluster = document.querySelector(`.hub-comment-vote-cluster[data-comment-id="${commentId}"]`);
    if (cluster) {
      const upBtn = cluster.querySelector('.hub-comment-vote-btn.upvote');
      const downBtn = cluster.querySelector('.hub-comment-vote-btn.downvote');
      const countEl = cluster.querySelector('.hub-comment-vote-count');

      const newVote = userCommentVotes[commentId] || 0;
      if (upBtn) upBtn.classList.toggle('voted', newVote === 1);
      if (downBtn) downBtn.classList.toggle('voted', newVote === -1);
      if (countEl) {
        const newCount = cObj ? cObj.upvotes : Math.max(0, parseInt(countEl.textContent || '0', 10) + delta);
        countEl.textContent = newCount;
        countEl.classList.toggle('voted-up', newVote === 1);
        countEl.classList.toggle('voted-down', newVote === -1);
      }
    }

    try {
      const allLocal = JSON.parse(localStorage.getItem(STORAGE_LOCAL_COMMENTS_KEY)) || {};
      if (allLocal[postId]) {
        const found = allLocal[postId].find(c => c.id === commentId);
        if (found) {
          found.upvotes = Math.max(0, (found.upvotes || 0) + delta);
          localStorage.setItem(STORAGE_LOCAL_COMMENTS_KEY, JSON.stringify(allLocal));
        }
      }
    } catch (e) {}

    const config = await ensureSupabaseConfig();
    const token = getCurrentToken() || session?.access_token || config.anonKey;
    if (config.url && config.anonKey && token && !String(commentId).startsWith('c-')) {
      const targetUpvotes = cObj ? cObj.upvotes : undefined;
      if (targetUpvotes !== undefined) {
        fetch(`${config.url}/rest/v1/community_comments?id=eq.${commentId}`, {
          method: 'PATCH',
          headers: {
            'Content-Type': 'application/json',
            'apikey': config.anonKey,
            'Authorization': `Bearer ${token}`
          },
          body: JSON.stringify({ upvotes: targetUpvotes })
        }).catch(() => {});
      }
    }
  }

  async function submitComment(postId, content, parentId = null) {
    const session = getCurrentSession();
    const config = await ensureSupabaseConfig();
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
    const token = getCurrentToken() || session?.access_token || config.anonKey;
    if (config.url && config.anonKey && token && !String(postId).startsWith('seed-') && !String(postId).startsWith('post-')) {
      try {
        const res = await fetch(`${config.url}/rest/v1/community_comments`, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
            'apikey': config.anonKey,
            'Authorization': `Bearer ${token}`,
            'Prefer': 'return=representation'
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
        if (res.ok) {
          const inserted = await res.json();
          if (Array.isArray(inserted) && inserted[0]) {
            newCommentObj.id = inserted[0].id;
            synced = true;
          }
        }
      } catch (err) {}
    }

    // Save locally into localStorage (deduplicated automatically)
    saveLocalComment(postId, newCommentObj);

    const post = postsData.find(p => String(p.id) === String(postId));
    if (post) {
      post.comments_count = (post.comments_count || 0) + 1;
      const pageCounter = document.getElementById('page-comments-counter');
      if (pageCounter) pageCounter.textContent = `Comments (${post.comments_count})`;
    }
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
    const config = await ensureSupabaseConfig();
    const token = getCurrentToken() || session?.access_token || config.anonKey;
    if (config.url && config.anonKey && token) {
      try {
        const res = await fetch(`${config.url}/rest/v1/community_posts`, {
          method: 'POST',
          headers: {
            'Content-Type': 'application/json',
            'apikey': config.anonKey,
            'Authorization': `Bearer ${token}`,
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

  async function deleteComment(postId, commentId, opAuthorName) {
    removeLocalComment(postId, commentId);

    const config = await ensureSupabaseConfig();
    const session = getCurrentSession();
    const token = getCurrentToken() || session?.access_token || config.anonKey;
    if (config.url && config.anonKey && token && !String(commentId).startsWith('c-')) {
      try {
        await fetch(`${config.url}/rest/v1/community_comments?id=eq.${commentId}`, {
          method: 'DELETE',
          headers: {
            'apikey': config.anonKey,
            'Authorization': `Bearer ${token}`
          }
        });
      } catch (err) {}
    }

    const post = postsData.find(p => String(p.id) === String(postId));
    if (post && post.comments_count > 0) {
      post.comments_count--;
      const pageCounter = document.getElementById('page-comments-counter');
      if (pageCounter) pageCounter.textContent = `Comments (${post.comments_count})`;
      renderFilteredPosts();
    }

    showNotification('Comment deleted.');
    loadCommentsForThread(postId, opAuthorName);
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
    if (session.user.id && post.author_id && String(session.user.id) === String(post.author_id)) return true;
    const currentName = session.user.user_metadata?.username || session.user.email?.split('@')[0];
    if (currentName && post.author_name && currentName.toLowerCase() === post.author_name.toLowerCase()) return true;
    return false;
  }

  function canDeleteComment(comment, opAuthorName = null) {
    const session = getCurrentSession();
    if (!session || !session.user) return false;
    const role = (session.user.role || session.user.user_metadata?.role || '').toLowerCase();
    // 1. Staff (admin, dev, moderator) can always delete
    if (['admin', 'dev', 'moderator'].includes(role)) return true;

    // 2. The author who wrote the comment can always delete it
    if (session.user.id && comment.author_id && String(session.user.id) === String(comment.author_id)) return true;
    const currentName = session.user.user_metadata?.username || session.user.email?.split('@')[0];
    if (currentName && comment.author_name && currentName.toLowerCase() === comment.author_name.toLowerCase()) return true;

    // 3. Thread OP can moderate/delete comments in their thread
    if (currentName && opAuthorName && currentName.toLowerCase() === opAuthorName.toLowerCase()) return true;

    return false;
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

  async function ensureSupabaseConfig() {
    let cfg = getSupabaseConfig();
    if (cfg.url && cfg.anonKey) return cfg;

    try {
      const res = await fetch('/api/config');
      if (res.ok) {
        const data = await res.json();
        if (data.url && data.anonKey) {
          window.LAB_SUPABASE_URL = data.url;
          window.LAB_SUPABASE_ANON_KEY = data.anonKey;
          localStorage.setItem('lab_supabase_config', JSON.stringify({ url: data.url, anonKey: data.anonKey }));
          return data;
        }
      }
    } catch (e) {}

    return cfg;
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
