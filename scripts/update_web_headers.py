import re
import os

WEB_DIR = r"c:\Users\jancz\Desktop\Lab\web"

HEADER_REPLACEMENT = """  <!-- ====================================================================
       1. TOP HEADER (EXACT MATCH TO VALVE DEVELOPER COMMUNITY - IMAGE 2)
       ==================================================================== -->
  <header class="vdc-top-header" role="banner">
    <div class="vdc-header-left">
      <button class="vdc-mobile-burger" aria-label="Toggle navigation drawer">&#9776;</button>
      <a href="index.html" class="vdc-brand">
        <span class="vdc-logo-stamp">LAB</span>
        <div class="vdc-brand-text">
          <span class="vdc-brand-title">Developer Community</span>
        </div>
      </a>
    </div>

    <div class="vdc-header-right">
      <div class="vdc-user-tools">
        <a href="#" id="auth-btn-create" class="user-link">Create account</a>
        <span class="pipe-sep" style="color:#505967; margin:0 4px;">&bull;</span>
        <a href="#" id="auth-btn-login" class="user-link">Log in</a>
      </div>
    </div>
  </header>"""

FLAGS_BAR = """      <!-- Language flag row under search bar matching Image 2 -->
      <div class="vdc-flags-bar" aria-label="Language selection">
        <img class="flag-img flag-active" src="assets/flags/us.png" alt="English" title="English">
        <img class="flag-img" src="assets/flags/de.png" alt="Deutsch" title="Deutsch">
        <img class="flag-img" src="assets/flags/es.png" alt="Español" title="Español">
        <img class="flag-img" src="assets/flags/fr.png" alt="Français" title="Français">
        <img class="flag-img" src="assets/flags/fi.png" alt="Suomi" title="Suomi">
        <img class="flag-img" src="assets/flags/it.png" alt="Italiano" title="Italiano">
        <img class="flag-img" src="assets/flags/jp.png" alt="日本語" title="日本語">
        <img class="flag-img" src="assets/flags/nl.png" alt="Nederlands" title="Nederlands">
        <img class="flag-img" src="assets/flags/br.png" alt="Português (Brasil)" title="Português (Brasil)">
        <img class="flag-img" src="assets/flags/pl.png" alt="Polski" title="Polski">
        <img class="flag-img" src="assets/flags/ru.png" alt="Русский" title="Русский">
        <img class="flag-img" src="assets/flags/sk.png" alt="Slovenčina" title="Slovenčina">
        <img class="flag-img" src="assets/flags/se.png" alt="Svenska" title="Svenska">
        <img class="flag-img" src="assets/flags/tr.png" alt="Türkçe" title="Türkçe">
        <img class="flag-img" src="assets/flags/cn.png" alt="中文" title="中文">
      </div>"""

TOOLS_BLOCK = """      <div class="sidebar-block">
        <div class="sidebar-title">Tools</div>
        <ul class="sidebar-links">
          <li><a href="editor.html" style="color:var(--wiki-accent); font-weight:bold;">+ Create New Article</a></li>
          <li><a href="wiki-contents.html">What links here</a></li>
          <li><a href="project-hub.html">Special pages</a></li>
          <li><a href="javascript:window.print();">Printable version</a></li>
        </ul>
      </div>"""

def process_file(filepath):
    filename = os.path.basename(filepath)
    if filename in ["index.html", "editor.html"]:
        print(f"Skipping {filename} (already updated)")
        return

    with open(filepath, "r", encoding="utf-8") as f:
        content = f.read()

    orig_content = content

    # 1. Replace <header class="vdc-top-header" ... </header>
    header_pattern = re.compile(r'(?:<!--\s*=+\s*1\.\s*TOP HEADER[\s\S]*?-->\s*)*<header class="vdc-top-header"[\s\S]*?</header>', re.IGNORECASE)
    if header_pattern.search(content):
        content = header_pattern.sub(HEADER_REPLACEMENT, content, count=1)
    else:
        print(f"Warning: header pattern not found in {filename}")

    # 2. Update Tabs
    tabs_pattern = re.compile(r'(?:<!--\s*Navigation Tabs[\s\S]*?-->\s*)*<nav class="vdc-page-tabs"[\s\S]*?</nav>', re.IGNORECASE)
    tabs_match = tabs_pattern.search(content)
    if tabs_match:
        tabs_text = tabs_match.group(0)
        
        # Check tabs-left
        left_match = re.search(r'<div class="tabs-left">([\s\S]*?)</div>', tabs_text, re.IGNORECASE)
        if left_match:
            left_html = left_match.group(1).strip()
        else:
            left_html = f'<a href="{filename}" class="wiki-tab active">Page</a>\n          <a href="talk.html" class="wiki-tab">Discussion</a>'

        new_tabs = f"""<!-- Navigation Tabs (Exact 2007 Valve Developer Community layout matching Image 2) -->
      <nav class="vdc-page-tabs" aria-label="Namespace tabs">
        <div class="tabs-left">
          {left_html}
        </div>
        <div class="tabs-right">
          <a href="{filename}" class="wiki-tab active">Read</a>
          <a href="#" id="vdc-tab-edit" class="wiki-tab">Edit</a>
          <a href="https://github.com/YoungJasiek/Lab" target="_blank" rel="noopener noreferrer" class="wiki-tab">View source</a>
          <a href="https://github.com/YoungJasiek/Lab/commits/main" target="_blank" rel="noopener noreferrer" class="wiki-tab">View history</a>
          <span class="wiki-tab" style="cursor:default;">More &#9662;</span>

          <!-- Tab-aligned search bar matching Image 2 -->
          <div class="vdc-tab-search" role="search">
            <input type="text" id="vdc-search-input" class="vdc-search-input" placeholder="Search Lab Developer Community" aria-label="Search wiki documentation">
            <span class="vdc-search-icon">&#128269;</span>
          </div>
        </div>
      </nav>"""
        content = content[:tabs_match.start()] + new_tabs + content[tabs_match.end():]

    # 3. Language Flags Bar
    # Check if there is an existing flags bar right after nav
    existing_flags = re.search(r'<div class="vdc-flags-bar"[\s\S]*?</div>', content, re.IGNORECASE)
    if existing_flags:
        content = content[:existing_flags.start()] + FLAGS_BAR.strip() + content[existing_flags.end():]
    else:
        # insert right after </nav>
        nav_end = content.find('</nav>')
        if nav_end != -1:
            insert_pos = nav_end + len('</nav>')
            content = content[:insert_pos] + '\n\n' + FLAGS_BAR + content[insert_pos:]

    # 4. Sidebar Tools block
    aside_match = re.search(r'<aside class="vdc-sidebar"[\s\S]*?</aside>', content, re.IGNORECASE)
    if aside_match:
        aside_text = aside_match.group(0)
        if 'href="editor.html"' not in aside_text:
            if 'sidebar-title">Tools</div>' in aside_text:
                new_aside = re.sub(
                    r'(<div class="sidebar-title">Tools</div>\s*<ul class="sidebar-links">)',
                    r'\1\n          <li><a href="editor.html" style="color:var(--wiki-accent); font-weight:bold;">+ Create New Article</a></li>',
                    aside_text,
                    count=1
                )
                content = content[:aside_match.start()] + new_aside + content[aside_match.end():]
            else:
                aside_end = aside_text.rfind('</aside>')
                new_aside = aside_text[:aside_end] + '\n' + TOOLS_BLOCK + '\n    ' + aside_text[aside_end:]
                content = content[:aside_match.start()] + new_aside + content[aside_match.end():]

    # 5. Supabase Auth script before app.js
    if 'supabase-auth.js' not in content:
        app_js_pos = content.find('<script src="js/app.js')
        if app_js_pos != -1:
            content = content[:app_js_pos] + '<!-- Supabase Auth & VDC MediaWiki Editor -->\n  <script src="js/supabase-auth.js"></script>\n\n  ' + content[app_js_pos:]
        else:
            body_end = content.rfind('</body>')
            if body_end != -1:
                content = content[:body_end] + '  <!-- Supabase Auth & VDC MediaWiki Editor -->\n  <script src="js/supabase-auth.js"></script>\n' + content[body_end:]

    if content != orig_content:
        with open(filepath, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"Updated {filename}")
    else:
        print(f"No changes needed for {filename}")

if __name__ == "__main__":
    import sys
    target = sys.argv[1] if len(sys.argv) > 1 else None
    if target:
        process_file(os.path.join(WEB_DIR, target))
    else:
        for fname in sorted(os.listdir(WEB_DIR)):
            if fname.endswith(".html"):
                process_file(os.path.join(WEB_DIR, fname))
