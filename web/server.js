/**
 * Lab Engine Wiki - Development Server
 * Zero-dependency Node.js HTTP server with live reload and clean URLs.
 */

import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { exec } from 'node:child_process';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

// Port resolution: CLI args (--port, -p) > npm config (--port) > env (PORT) > default (8990)
function resolvePort() {
  for (let i = 0; i < process.argv.length; i++) {
    const arg = process.argv[i];
    if (arg === '--port' || arg === '-p') {
      const val = process.argv[i + 1];
      if (val && !isNaN(Number(val))) return Number(val);
    }
    if (arg.startsWith('--port=')) {
      const val = arg.split('=')[1];
      if (val && !isNaN(Number(val))) return Number(val);
    }
  }
  if (process.env.npm_config_port && !isNaN(Number(process.env.npm_config_port))) {
    return Number(process.env.npm_config_port);
  }
  if (process.env.PORT && !isNaN(Number(process.env.PORT))) {
    return Number(process.env.PORT);
  }
  return 8990;
}

const PORT = resolvePort();
const WEB_ROOT = __dirname;

const MIME_TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.htm': 'text/html; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.mjs': 'application/javascript; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.png': 'image/png',
  '.jpg': 'image/jpeg',
  '.jpeg': 'image/jpeg',
  '.webp': 'image/webp',
  '.gif': 'image/gif',
  '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon',
  '.bmp': 'image/bmp',
  '.xml': 'application/xml',
  '.webmanifest': 'application/manifest+json',
  '.txt': 'text/plain; charset=utf-8',
  '.woff': 'font/woff',
  '.woff2': 'font/woff2',
  '.ttf': 'font/ttf'
};

// Live reload clients
const liveReloadClients = new Set();

function broadcastReload() {
  for (const res of liveReloadClients) {
    try {
      res.write('data: reload\n\n');
    } catch {
      liveReloadClients.delete(res);
    }
  }
}

// Watch directory for changes (debounced)
let reloadTimer = null;
fs.watch(WEB_ROOT, { recursive: true }, (eventType, filename) => {
  if (!filename) return;
  // Ignore git, node_modules, temp files
  if (filename.includes('node_modules') || filename.includes('.git') || filename.endsWith('.tmp')) return;

  clearTimeout(reloadTimer);
  reloadTimer = setTimeout(() => {
    console.log(`[live-reload] File changed: ${filename} -> refreshing browser`);
    broadcastReload();
  }, 120);
});

const LIVE_RELOAD_SNIPPET = `
<!-- Lab Wiki Live Reload -->
<script>
  (() => {
    try {
      const es = new EventSource('/__livereload');
      es.onmessage = () => {
        console.log('[live-reload] Reloading page...');
        location.reload();
      };
      es.onerror = () => {
        // Retry connection after pause
        setTimeout(() => new EventSource('/__livereload'), 2000);
      };
    } catch (e) {}
  })();
</script>
`;

function openBrowser(url) {
  const cmd = process.platform === 'win32' ? `start "" "${url}"` :
              process.platform === 'darwin' ? `open "${url}"` : `xdg-open "${url}"`;
  exec(cmd, () => {});
}

const server = http.createServer((req, res) => {
  const reqUrl = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  let pathname = decodeURIComponent(reqUrl.pathname);

  // SSE endpoint for live reload
  if (pathname === '/__livereload') {
    res.writeHead(200, {
      'Content-Type': 'text/event-stream',
      'Cache-Control': 'no-cache',
      'Connection': 'keep-alive',
      'Access-Control-Allow-Origin': '*'
    });
    res.write(': connected\n\n');
    liveReloadClients.add(res);

    req.on('close', () => {
      liveReloadClients.delete(res);
    });
    return;
  }

  // Prevent directory traversal attacks
  const safePath = path.normalize(pathname).replace(/^(\.\.[\/\\])+/, '');
  let filePath = path.join(WEB_ROOT, safePath);

  // Directory request default to index.html
  if (fs.existsSync(filePath) && fs.statSync(filePath).isDirectory()) {
    filePath = path.join(filePath, 'index.html');
  }

  // Clean URLs support: if /architecture requested, try /architecture.html
  if (!fs.existsSync(filePath) && fs.existsSync(filePath + '.html')) {
    filePath = filePath + '.html';
  }

  // 404 fallback
  if (!fs.existsSync(filePath) || !fs.statSync(filePath).isFile()) {
    const notFoundPath = path.join(WEB_ROOT, '404.html');
    if (fs.existsSync(notFoundPath)) {
      res.writeHead(404, { 'Content-Type': 'text/html; charset=utf-8' });
      let html = fs.readFileSync(notFoundPath, 'utf8');
      if (html.includes('</body>')) {
        html = html.replace('</body>', `${LIVE_RELOAD_SNIPPET}</body>`);
      }
      res.end(html);
    } else {
      res.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
      res.end('404 Not Found');
    }
    return;
  }

  const ext = path.extname(filePath).toLowerCase();
  const contentType = MIME_TYPES[ext] || 'application/octet-stream';

  // Inject live reload script into HTML files
  if (ext === '.html' || ext === '.htm') {
    try {
      let html = fs.readFileSync(filePath, 'utf8');
      if (html.includes('</body>')) {
        html = html.replace('</body>', `${LIVE_RELOAD_SNIPPET}</body>`);
      } else {
        html += LIVE_RELOAD_SNIPPET;
      }
      res.writeHead(200, {
        'Content-Type': contentType,
        'Cache-Control': 'no-cache, no-store, must-revalidate'
      });
      res.end(html);
      return;
    } catch (err) {
      res.writeHead(500, { 'Content-Type': 'text/plain; charset=utf-8' });
      res.end('500 Internal Server Error');
      return;
    }
  }

  // Non-HTML static assets
  const stat = fs.statSync(filePath);
  res.writeHead(200, {
    'Content-Type': contentType,
    'Content-Length': stat.size,
    'Cache-Control': ext === '.css' || ext === '.js' ? 'public, max-age=0, must-revalidate' : 'public, max-age=86400'
  });

  const readStream = fs.createReadStream(filePath);
  readStream.pipe(res);
});

server.on('error', (err) => {
  if (err.code === 'EADDRINUSE') {
    console.log(`\n[!] Port ${PORT} jest juz zajety.`);
    console.log(`[>] Otwieranie istniejacej instancji: http://localhost:${PORT}\n`);
    openBrowser(`http://localhost:${PORT}`);
    process.exit(0);
  } else {
    console.error('Server error:', err);
    process.exit(1);
  }
});

server.listen(PORT, () => {
  console.log('========================================================');
  console.log('  Lab Wiki - Web Server Preview (Node.js / npm)');
  console.log('========================================================');
  console.log(`[✓] Serwer aktywny na: http://localhost:${PORT}`);
  console.log(`[✓] Automatyczne odswiezanie (live-reload): Wlaczone`);
  console.log(`[✓] Aby zakonczyc, nacisnij Ctrl+C.\n`);

  openBrowser(`http://localhost:${PORT}`);
});
