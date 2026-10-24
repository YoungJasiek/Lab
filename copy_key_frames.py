import os
import shutil

artifact_dir = r"C:\Users\jancz\.gemini\antigravity\brain\bdbf8030-9677-49ba-84b5-5d31c9b4e61d"

sources = {
    'cs': r"insp\fl\menu\extracted\cs_source",
    'gm': r"insp\fl\menu\extracted\garrys_mod",
    'hl': r"insp\fl\menu\extracted\hl2"
}

# Copy selected representative frames to artifact dir for carousel / inspection
key_selections = [
    # CS:Source: Main menu, Server browser, Create server, Options tabs
    ('cs', 'frame_001.jpg', 'cs_main_menu.jpg'),
    ('cs', 'frame_015.jpg', 'cs_server_browser.jpg'),
    ('cs', 'frame_030.jpg', 'cs_create_server.jpg'),
    ('cs', 'frame_045.jpg', 'cs_options_dialog.jpg'),
    ('cs', 'frame_058.jpg', 'cs_video_settings.jpg'),

    # Garry's Mod: Start new game, Addons manager, Gamemodes, Settings, Loading
    ('gm', 'frame_001.jpg', 'gm_main_menu.jpg'),
    ('gm', 'frame_012.jpg', 'gm_start_new_game.jpg'),
    ('gm', 'frame_025.jpg', 'gm_addons_browser.jpg'),
    ('gm', 'frame_038.jpg', 'gm_addon_categories.jpg'),
    ('gm', 'frame_050.jpg', 'gm_server_browser.jpg'),
    ('gm', 'frame_062.jpg', 'gm_creation_menu.jpg'),

    # HL2: 3D background menu, Chapter select, Load game, Options, Audio/Video
    ('hl', 'frame_001.jpg', 'hl2_main_menu.jpg'),
    ('hl', 'frame_015.jpg', 'hl2_chapter_select.jpg'),
    ('hl', 'frame_028.jpg', 'hl2_load_game_saves.jpg'),
    ('hl', 'frame_042.jpg', 'hl2_options_audio.jpg'),
    ('hl', 'frame_055.jpg', 'hl2_options_keyboard.jpg')
]

for cat, src_name, dst_name in key_selections:
    src_path = os.path.join(sources[cat], src_name)
    dst_path = os.path.join(artifact_dir, dst_name)
    if os.path.exists(src_path):
        shutil.copy2(src_path, dst_path)
        print(f"Copied {src_name} -> {dst_name}")
    else:
        print(f"Not found: {src_path}")
