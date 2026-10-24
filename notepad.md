General:
- [x] error textures (proceduralna fioletowo-czarna szachownica Source dla brakujacych tekstur)

players , characters:
- [x] use guard as player (guard.glb jako cialo gracza w TPP i MP)
- [x] make rigs instead of cubes (pelny rig kosci 70-bone GPU skinning zamiast klockow)
- [x] rigs MUST be compatible with 3D models (kompatybilnosc z modelami 3D glTF/FBX)
- [x] guard viewmodel (dedykowany model rak straznika w FPP trzymajacy bronie)

multiplayer:
- [x] loading map sequence (na wzor insp/fl/menu/extracted/garrys_mod/)
- [x] downloading map and .lpak if isn't on disk (FastDL pobieranie brakujacych paczek z paskiem postepu)
- [x] delete a story from arcade session (story vs. arcade - wylaczenie dialogow i cutscenek w MP/Arcade)
- [x] not approve a sp_cheats or anything what can help to player if is a official server or public server (wymuszenie sv_cheats 0)
- [x] search engine for servers (przegladarka serwerow na wzor insp/fl/menu/extracted/cs_source/ i garrys_mod/)

mods:
- [x] separate tab with mods in Hammer (zakladka Mods Explorer na wzor insp/fl/menu/extracted/garrys_mod/, zastepujaca Character Studio)
- [x] custom command in sdk what creates a mod for FL (lhmr --create-mod <nazwa>)
- [x] przykladowy mod demonstracyjny Lua (np. intro w stylu CoD jako addon wykorzystujacy Lua i skrypty startup)

ui:
- [x] rebuild ui (in FL) to better (nowe menu glowne, okna dialogowe VGUI2, osiagniecia na wzor insp/fl/menu/extracted/hl2/ i cs_source/)
