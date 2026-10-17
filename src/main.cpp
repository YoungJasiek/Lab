#include "Lab.h"
#include "LabFont.h"
#include "LabDialogs.h"
#include "LabScene.h"
#include "LabGpuPreference.h"
#include "LabDevConsole.h"
#include "../games/frozen_life/include/FrozenLifeAnim.h"
#include <iostream>
#include <algorithm>
#include <memory>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <thread>
#include <chrono>
#include <fstream>

using namespace Lab;


#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#pragma comment(lib, "psapi.lib")
static std::string getClipboardText() {
    if (!OpenClipboard(nullptr)) return "";
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (!hData) { CloseClipboard(); return ""; }
    char* pszText = static_cast<char*>(GlobalLock(hData));
    std::string text = pszText ? pszText : "";
    GlobalUnlock(hData);
    CloseClipboard();
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    text.erase(std::remove(text.begin(), text.end(), '\n'), text.end());
    return text;
}
static void setClipboardText(const std::string& text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    HGLOBAL hGlob = GlobalAlloc(GMEM_FIXED, text.size() + 1);
    if (hGlob) {
        memcpy(hGlob, text.c_str(), text.size() + 1);
        SetClipboardData(CF_TEXT, hGlob);
    }
    CloseClipboard();
}
#else
static std::string getClipboardText() { return ""; }
static void setClipboardText(const std::string&) {}
#endif

class FrozenLife : public Engine {

public:
    FrozenLife(bool showSplash = true)
        : Engine("Frozen-Life: Lab FPS", 1280, 720),
          _showSplash(showSplash),
          _splashActive(showSplash),
          _splashTimer(0.0f),
          _splashDuration(2.5f),
          _camera(75.0f, 16.0f / 9.0f, 0.01f, 1000.0f),
          _velocity{ 0, 0, 0 },
          _isGrounded(false),
          _isJumping(false),
          _bobTime(0.0f),
          _muzzleFlashTime(0.0f) {
    }

    ~FrozenLife() override {
        _characterStudio.saveConfig();
    }

    void onInit() override {
        LabLog::info("Frozen-Life Init: Modular .LABMAP and GUI System...");
        Renderer::init();

        s_instance = this;
        glfwSetCharCallback(getWindow(), [](GLFWwindow* /*w*/, unsigned int codepoint) {
            if (s_instance) {
                if (s_instance->_devConsole.isOpen) {
                    s_instance->_devConsole.onChar(codepoint);
                } else if (s_instance->_chat.isOpen) {
                    s_instance->_chat.onChar(codepoint);
                }
            }
        });

        // Setup Developer Console Callbacks
        _devConsole.onHeal = [this](int amt) {
            _hud.health = std::min(_hud.maxHealth, _hud.health + static_cast<float>(amt));
        };
        _devConsole.onArmor = [this](int amt) {
            _hud.suitArmor = std::min(100.0f, _hud.suitArmor + static_cast<float>(amt));
        };
        _devConsole.onAmmo = [this](int amt) {
            for (int i = 0; i < 9; ++i) {
                auto& w = _weaponSystem.getWeapon(static_cast<WeaponID>(i));
                w.currentReserve = std::min(w.def.maxReserve, w.currentReserve + amt);
            }
            syncHudWeapon();
        };
        _devConsole.onGetWeapon = [this](int id) {
            int idx = id;
            if (idx >= 100 && idx <= 108) idx -= 100;
            else if (idx >= 1 && idx <= 9) idx -= 1;
            if (idx >= 0 && idx < 9) {
                _weaponSystem.unlockWeapon(static_cast<WeaponID>(idx), true);
                auto& w = _weaponSystem.getWeapon(static_cast<WeaponID>(idx));
                w.currentClip = w.def.clipSize;
                w.currentReserve = w.def.maxReserve;
                syncHudWeapon();
            }
        };
        _devConsole.onGiveAll = [this]() {
            _weaponSystem.unlockAll();
            for (int i = 0; i < 9; ++i) {
                auto& w = _weaponSystem.getWeapon(static_cast<WeaponID>(i));
                w.currentClip = w.def.clipSize;
                w.currentReserve = w.def.maxReserve;
            }
            syncHudWeapon();
        };
        _devConsole.onRespawn = [this]() {
            _hud.health = _scriptEngine.getPlayerMaxHealth();
            _hud.suitArmor = 100.0f;
            _isPlayerDead = false;
            _velocity = { 0, 0, 0 };
            if (_currentMap && !_currentMap->spawnPoints.empty()) {
                _camera.setPosition(_currentMap->spawnPoints[0].position + Vec3(0, 1.8f, 0));
            }
        };
        _devConsole.onSuicide = [this]() {
            applyDamageToPlayer(999.0f, "Console Suicide");
        };
        _devConsole.onLoadMap = [this](const std::string& mapName) {
            std::string path = mapName;
            if (path.find('/') == std::string::npos && path.find('\\') == std::string::npos) {
                path = "assets/maps/" + path;
            }
            if (path.find('.') == std::string::npos) {
                path += ".labscene";
            }
            _sessionConfig.mapPath = path;
            loadSelectedMap(path);
        };
        _devConsole.onRestartMap = [this]() {
            if (!_sessionConfig.mapPath.empty()) loadSelectedMap(_sessionConfig.mapPath);
        };
        _devConsole.onSetSkybox = [this](const std::string& /*skybox*/) {
            if (_devConsole.wrd_skybox_is_color) {
                _postProcess.setClearColor(_devConsole.wrd_skybox_color);
            }
        };
        _devConsole.onSpawnModel = [this](const std::string& modelPath) {
            if (!_currentMap) return;
            Vec3 spawnPos = _camera.getPosition() + _camera.getFront() * 3.0f;
            MapProp prop;
            prop.name = "ConsoleProp_" + std::to_string(_currentMap->props.size() + 1);
            prop.modelPath = modelPath;
            prop.position = spawnPos;
            prop.rotation = Vec3(0, _camera.getYaw(), 0);
            prop.scale = Vec3(1, 1, 1);
            prop.color = Vec3(1, 1, 1);
            getMesh(modelPath);
            _currentMap->props.push_back(std::move(prop));
        };
        _devConsole.onCamShake = [this](float force) {
            _camShakeTrauma = std::min(1.0f, _camShakeTrauma + force);
        };
        _devConsole.onClearDecals = [this]() {
            _decalSystem.clear();
        };
        _devConsole.onSpawnDecal = [this](const std::string& type) {
            Vec3 origin = _camera.getPosition();
            Vec3 dir = _camera.getFront();
            DecalType dt = DecalType::BloodSplatter;
            if (type == "bullet" || type == "concrete") dt = DecalType::BulletHoleConcrete;
            else if (type == "metal") dt = DecalType::BulletHoleMetal;
            else if (type == "dirt" || type == "scorch" || type == "burn") dt = DecalType::ExplosiveScorch;

            float bestDist = 100.0f;
            Vec3 hitPos = origin + dir * 2.5f;
            Vec3 hitNormal = Vec3(0, 1, 0);
            if (_currentMap) {
                for (const auto& b : _currentMap->brushes) {
                    Vec3 bMin = b.position - b.size * 0.5f;
                    Vec3 bMax = b.position + b.size * 0.5f;
                    Vec3 invDir(1.0f / (std::abs(dir.x) > 1e-5f ? dir.x : 1e-5f),
                                1.0f / (std::abs(dir.y) > 1e-5f ? dir.y : 1e-5f),
                                1.0f / (std::abs(dir.z) > 1e-5f ? dir.z : 1e-5f));
                    Vec3 t0 = (bMin - origin) * invDir;
                    Vec3 t1 = (bMax - origin) * invDir;
                    Vec3 tMin(std::min(t0.x, t1.x), std::min(t0.y, t1.y), std::min(t0.z, t1.z));
                    Vec3 tMax(std::max(t0.x, t1.x), std::max(t0.y, t1.y), std::max(t0.z, t1.z));
                    float tEnter = std::max({tMin.x, tMin.y, tMin.z});
                    float tExit = std::min({tMax.x, tMax.y, tMax.z});
                    if (tEnter <= tExit && tEnter > 0.05f && tEnter < bestDist) {
                        bestDist = tEnter;
                        hitPos = origin + dir * tEnter;
                        Vec3 pLocal = hitPos - b.position;
                        Vec3 half = b.size * 0.5f;
                        float dx = half.x - std::abs(pLocal.x);
                        float dy = half.y - std::abs(pLocal.y);
                        float dz = half.z - std::abs(pLocal.z);
                        if (dx <= dy && dx <= dz) hitNormal = Vec3((pLocal.x > 0 ? 1.0f : -1.0f), 0, 0);
                        else if (dy <= dx && dy <= dz) hitNormal = Vec3(0, (pLocal.y > 0 ? 1.0f : -1.0f), 0);
                        else hitNormal = Vec3(0, 0, (pLocal.z > 0 ? 1.0f : -1.0f));
                    }
                }
            }
            _decalSystem.spawnDecal(dt, hitPos, hitNormal, 0.35f, 60.0f);
        };
        _devConsole.onSetVsync = [](bool vsync) {
            glfwSwapInterval(vsync ? 1 : 0);
        };
        _devConsole.onReloadShaders = []() {
            Renderer::prewarmShaders();
        };
        _devConsole.onSetBotsA = [this](int count) {
            _sessionConfig.botCount = count;
            if (_currentMap) _aiManager.spawnBotsForMap(_currentMap.get(), count, _sessionConfig.mode);
        };
        _devConsole.onSetBotsB = [this](int count) {
            _sessionConfig.botCount = count;
            if (_currentMap) _aiManager.spawnBotsForMap(_currentMap.get(), count, _sessionConfig.mode);
        };
        _devConsole.onKickBots = [this]() {
            _aiManager.bots.clear();
        };
        _devConsole.onFreezeBots = [this](bool freeze) {
            for (auto& bot : _aiManager.bots) {
                if (freeze) bot.state = AIState::Patrol;
            }
        };
        _devConsole.onSpawnAi = [this](const std::string& type) {
            Vec3 spawnPos = _camera.getPosition() + _camera.getFront() * 3.5f;
            int botId = static_cast<int>(_aiManager.bots.size()) + 1;
            std::string botName = type.empty() ? ("Combat Bot " + std::to_string(botId)) : (type + " #" + std::to_string(botId));
            Vec3 patrolEnd = spawnPos + Vec3(5.0f, 0.0f, 5.0f);
            WeaponID wep = WeaponID::M4A4S;
            if (type == "heavy" || type == "shotgun") wep = WeaponID::Shotgun;
            else if (type == "plasma") wep = WeaponID::PlasmaGun;
            else if (type == "sniper" || type == "railgun") wep = WeaponID::Railgun;
            else if (type == "rpg") wep = WeaponID::RPG;
            CombatBot newBot(botId, botName, spawnPos, patrolEnd, -1, wep);
            if (_aiManager.skeleton && !_aiManager.animations.empty()) {
                newBot.initAnimation(_aiManager.skeleton, _aiManager.animations);
            }
            _aiManager.bots.push_back(std::move(newBot));
        };
        _devConsole.onSetMasterVolume = [](float vol) {
            AudioEngine::setMasterVolume(vol);
        };
        _devConsole.onPlaySound = [](const std::string& path) {
            AudioEngine::playCustomSound(path);
        };
        _devConsole.onStopAllSounds = []() {
            AudioEngine::shutdown();
            AudioEngine::init(true);
        };
        _devConsole.onLipsyncTest = [this](const std::string& file) {
            AudioEngine::playCustomSound(file);
        };
        _devConsole.onRestartAudio = []() {
            AudioEngine::shutdown();
            AudioEngine::init(true);
        };
        _devConsole.onOpenMenu = [this](const std::string& menuName) {
            std::string m = menuName;
            std::transform(m.begin(), m.end(), m.begin(), [](unsigned char c){ return (char)std::tolower(c); });
            if (m == "main" || m == "glowna") {
                _inMenu = true;
                _menuScreen = MenuScreen::Main;
                setCursorCaptured(false);
            } else if (m == "pause" || m == "pauza" || m == "singleplayer" || m == "sp") {
                _inMenu = true;
                _menuScreen = MenuScreen::Singleplayer;
                setCursorCaptured(false);
            } else if (m == "options" || m == "opcje" || m == "character" || m == "studio") {
                _inMenu = true;
                _menuScreen = MenuScreen::CharacterStudio;
                setCursorCaptured(false);
            } else if (m == "join" || m == "server" || m == "serwer") {
                _inMenu = true;
                _menuScreen = MenuScreen::JoinGame;
                setCursorCaptured(false);
            } else if (m == "game" || m == "resume" || m == "wznow") {
                _inMenu = false;
                setCursorCaptured(true);
            }
        };
        _devConsole.onReloadUi = [this]() {
            syncHudWeapon();
        };
        _devConsole.onRestartRound = [this]() {
            _playerKills = 0;
            _playerDeaths = 0;
            _hud.frags = 0;
            _devConsole.onRespawn();
            if (_currentMap) {
                _aiManager.spawnBotsForMap(_currentMap.get(), _sessionConfig.botCount, _sessionConfig.mode);
            }
        };
        _devConsole.onNetConnect = [this](const std::string& addr) {
            std::string ip = addr;
            uint16_t port = 27015;
            size_t colon = addr.find(':');
            if (colon != std::string::npos) {
                ip = addr.substr(0, colon);
                try { port = static_cast<uint16_t>(std::stoi(addr.substr(colon + 1))); } catch (...) {}
            }
            _netClient.connect(ip, port, "Player");
            _isConnecting = true;
        };
        _devConsole.onNetDisconnect = [this]() {
            _netClient.disconnect();
            _isConnecting = false;
        };
        _devConsole.onQuit = [this]() {
            glfwSetWindowShouldClose(getWindow(), 1);
        };

        // Discover available maps in assets/maps/
        scanMapFiles();

        // Preload Core Textures (Classic engine materials + 25 new high-definition environmental textures)
        std::vector<std::string> coreTextures = {
            "concrete_wall.bmp", "floor_tiles.bmp", "cryo_ice.bmp",
            "hazard_stripes.bmp", "metal_hull.bmp", "snow_frost.bmp",
            "floor_lab.bmp", "wall_concrete.bmp", "brick_wall.bmp", "blast_door.bmp",
            "concrete_wall_normal.bmp", "floor_lab_normal.bmp", "metal_hull_normal.bmp",
            "corridor_dirty_wall.bmp", "decal_rust_stain.bmp", "cubemap_site14.bmp",
            "weapon_pipe.bmp", "weapon_pistol.bmp", "weapon_shotgun.bmp",
            "weapon_m4a4s.bmp", "weapon_sg553.bmp", "weapon_minigun.bmp",
            "weapon_plasma.bmp", "weapon_railgun.bmp", "weapon_rpg.bmp"
        };
        for (int i = 1; i <= 25; ++i) {
            char buf[32];
            snprintf(buf, sizeof(buf), "texture_%02d.png", i);
            coreTextures.push_back(buf);
        }
        for (const auto& texName : coreTextures) {
            getTexture(texName);
        }

        // Initialize Weapon System, Particles & Physics
        _weaponSystem.init();
        _particleSystem.init();
        _physicsWorld.init();
        _decalSystem.init();
        _interactiveSystem.init();

        // Initialize Lua Scripting Engine & Game Mechanics Subsystem
        if (_scriptEngine.init("assets/scripts/game_mechanics.lua")) {
            ScriptEngine::registerDoorUnlockCallback([this](int doorIdx) {
                if (_currentMap && doorIdx >= 0 && doorIdx < (int)_currentMap->doors.size()) {
                    _currentMap->doors[doorIdx].isLocked = false;
                    _currentMap->doors[doorIdx].isOpen = true;
                }
            });
            ScriptEngine::registerServerChatCallback([this](const std::string& sender, const std::string& msg) {
                _chat.addMessage(sender, msg, Vec3(0.2f, 0.9f, 1.0f));
            });

            for (const auto& wdef : _scriptEngine.getWeaponDefinitions()) {
                _weaponSystem.applyScriptOverrides(wdef.id, wdef.damage, wdef.fireRate, wdef.clipSize, wdef.maxReserve, wdef.splashDamage, wdef.splashRadius);
            }
        }

        // Initialize 3D Spatial Audio Engine (miniaudio)
        AudioEngine::init(true);

        // Initialize Dynamic Shadow Map (2048x2048 high-resolution depth FBO)
        _shadowMap.init(2048, 2048);

        // Initialize HDR Post-Processing Pipeline (GL_RGBA16F, Bloom, Cryo Frost, ACES Tonemapping)
        _postProcess.init(getWidth(), getHeight());

        // Initialize Authoritative UDP Network Subsystem
        NetworkSystem::init();
        _serverBrowser.start();
        _pnpRoomCode = PnPBroker::generateRoomCode();
        _pnpJoinCode = _pnpRoomCode;
        _sessionConfig.roomCode = _pnpRoomCode;

        // Initialize Lab Character & Weapon Studio
        _characterStudio.init();
        _characterStudio.setWindow(getWindow());
        _characterStudio.loadConfig("assets/configs/character_studio.cfg");
        _characterStudio.setOnApplyInGame([this]() {
            reloadStudioWeapons();
        });

        // Initialize Skeletal Animation Assets & Dedicated Player Animator
        _aiManager.initAssets();
        if (_aiManager.skeleton && !_aiManager.animations.empty()) {
            _playerAnimator.setSkeleton(_aiManager.skeleton);
            for (const auto& clip : _aiManager.animations) {
                _playerAnimator.addClip(clip);
            }
            _playerAnimator.playAnimation("Idle", true);
        }

        reloadStudioWeapons();
        syncHudWeapon();

        // Start in Main Menu
        _inMenu = true;
        _menuScreen = MenuScreen::Main;
        setCursorCaptured(false);
    }

    void reloadStudioWeapons() {
        if (_cachedWeaponMeshes.size() < 9) _cachedWeaponMeshes.resize(9, nullptr);
        if (_cachedWeaponTextures.size() < 9) _cachedWeaponTextures.resize(9, nullptr);

        int customSTLCount = 0;
        for (int i = 0; i < 9; ++i) {
            const auto& wDef = _weaponSystem.getWeapon((WeaponID)i).def;
            const auto& skin = _characterStudio.getWeaponSkin((WeaponID)i);

            Mesh* mesh = nullptr;
            if (!skin.modelFile.empty()) {
                if (skin.modelFile != "NONE" && skin.modelFile != "PROCEDURAL") {
                    mesh = getMesh(skin.modelFile);
                }
            } else {
                mesh = getMesh(wDef.modelFile);
            }
            _cachedWeaponMeshes[i] = mesh;

            std::string texName;
            if (!skin.textureFile.empty()) {
                texName = skin.textureFile;
            } else {
                std::string modelRef = !skin.modelFile.empty() ? skin.modelFile : wDef.modelFile;
                texName = Renderer::resolveModelTexture(modelRef, wDef.textureFile);
            }
            _cachedWeaponTextures[i] = getTexture(texName);

            if (_cachedWeaponMeshes[i]) customSTLCount++;
        }
        std::cout << "[Weapons] Synced 9 weapons with Character Studio: " << customSTLCount << " custom STL, "
                  << (9 - customSTLCount) << " built-in procedural.\n";
    }

    void syncHudWeapon() {
        const auto& w = _weaponSystem.getActiveWeapon();
        _hud.weaponName = w.def.name;
        _hud.isMeleeWeapon = w.def.isMelee;
        _hud.activeWeaponSlot = w.def.slot;
        _hud.ammoClip = w.currentClip;
        _hud.ammoReserve = w.currentReserve;
        _hud.weaponSelectorTimer = _weaponSystem.getHudSelectorTimer();
        _hud.slotWeaponNames.clear();
        _hud.slotUnlocked.clear();
        for (int i = 0; i < (int)WeaponID::Count; ++i) {
            const auto& wep = _weaponSystem.getWeapon((WeaponID)i);
            _hud.slotWeaponNames.push_back(wep.def.shortName);
            _hud.slotUnlocked.push_back(wep.unlocked);
        }
    }

    Texture* getTexture(const std::string& path) {
        if (path.empty()) return nullptr;
        auto it = _textures.find(path);
        if (it != _textures.end()) return it->second.get();
        if (_missingTextures.contains(path)) return nullptr;

        auto tex = std::make_unique<Texture>(path);
        if (tex && tex->getId() != 0) {
            Texture* ptr = tex.get();
            _textures[path] = std::move(tex);
            return ptr;
        }
        _missingTextures.insert(path);
        return nullptr;
    }

    static SurfaceType getSurfaceTypeForTexture(const std::string& texturePath) {
        std::string lower = texturePath;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)::tolower(c); });
        // Wood materials
        if (lower.find("wood") != std::string::npos || lower.find("board") != std::string::npos ||
            lower.find("plank") != std::string::npos || lower.find("timber") != std::string::npos ||
            lower.find("texture_04") != std::string::npos || lower.find("texture_08") != std::string::npos ||
            lower.find("texture_13") != std::string::npos || lower.find("texture_17") != std::string::npos ||
            lower.find("texture_25") != std::string::npos) {
            return SurfaceType::Wood;
        }
        // Ice / Snow
        if (lower.find("ice") != std::string::npos || lower.find("snow") != std::string::npos ||
            lower.find("frost") != std::string::npos) {
            return SurfaceType::Ice;
        }
        // Metal materials
        if (lower.find("metal") != std::string::npos || lower.find("blast_door") != std::string::npos ||
            lower.find("hazard") != std::string::npos || lower.find("server") != std::string::npos ||
            lower.find("diamond_plate") != std::string::npos || lower.find("vent_grate") != std::string::npos ||
            lower.find("texture_02") != std::string::npos || lower.find("texture_03") != std::string::npos) {
            return SurfaceType::Metal;
        }
        // Default to Concrete / Stone / Masonry / Tile
        return SurfaceType::Concrete;
    }

    Mesh* getMesh(const std::string& path) {
        if (path.empty() || path == "NONE" || path == "PROCEDURAL") return nullptr;
        auto it = _meshes.find(path);
        if (it != _meshes.end()) return it->second.get();
        if (_missingMeshes.contains(path)) return nullptr;

        std::vector<std::string> searchPaths = {
            path,
            "assets/models/" + path,
            "assets/" + path,
            "../assets/models/" + path,
            "../../assets/models/" + path,
            "build/Release/assets/models/" + path
        };
        for (const auto& p : searchPaths) {
            if (std::filesystem::exists(p)) {
                Mesh* m = Mesh::loadSTL(p);
                if (m) {
                    Mesh* ptr = m;
                    _meshes[path] = std::unique_ptr<Mesh>(m);
                    return ptr;
                }
            }
        }
        Mesh* m = Mesh::loadSTL(path);
        if (m) {
            Mesh* ptr = m;
            _meshes[path] = std::unique_ptr<Mesh>(m);
            return ptr;
        }
        _missingMeshes.insert(path);
        return nullptr;
    }

    static std::string toSceneDisplayPath(const std::string& rawPath) {
        if (rawPath.empty()) return rawPath;
        std::filesystem::path p(rawPath);
        std::string ext = p.extension().string();
        for (char& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        if (ext == ".labmap") {
            p.replace_extension(".labscene");
        }
        std::string s = p.generic_string();
        return s;
    }

    void scanMapFiles() {
        _availableMaps.clear();
        std::vector<std::string> searchDirs = { "assets/maps", "../assets/maps", "../../assets/maps" };
        for (const auto& dir : searchDirs) {
            if (std::filesystem::exists(dir)) {
                // First auto-convert any legacy .labmap that does not yet have a .labscene companion
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    if (entry.path().extension() == ".labmap") {
                        std::filesystem::path sceneCounterpart = entry.path();
                        sceneCounterpart.replace_extension(".labscene");
                        if (!std::filesystem::exists(sceneCounterpart)) {
                            LabScene::convertLegacyMapToScene(entry.path().string());
                        }
                    }
                }
                // Collect .labscene files (and fallback to converted display path if needed)
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    if (entry.path().extension() == ".labscene") {
                        _availableMaps.push_back(entry.path().generic_string());
                    }
                }
                if (!_availableMaps.empty()) {
                    std::sort(_availableMaps.begin(), _availableMaps.end());
                    break;
                }
            }
        }
        if (_availableMaps.empty()) {
            _availableMaps.push_back("assets/maps/site14_chapter1.labscene");
            _availableMaps.push_back("assets/maps/facility_alpha.labscene");
            _availableMaps.push_back("assets/maps/cryo_outpost.labscene");
        }
        _sessionConfig.mapPath = toSceneDisplayPath(_sessionConfig.mapPath);
    }

    bool loadSelectedMap(const std::string& mapPath) {
        if (!LabDialogs::isAllowedSceneFile(mapPath)) {
            LabLog::error("Rejected invalid scene file path: " + mapPath);
            return false;
        }
        std::string displayPath = toSceneDisplayPath(mapPath);
        LabLog::info("Loading Scene: " + displayPath);
        auto loaded = LabMap::loadFromFile(mapPath);

        if (loaded) {
            _currentMap = std::move(loaded);

            // Apply map atmospheric parameters
            Renderer::setSunLight(
                _currentMap->metadata.sunDir,
                _currentMap->metadata.sunColor,
                _currentMap->metadata.ambientColor
            );

            // Set player spawn according to current mode & team
            int pTeam = (_sessionConfig.mode == GameMode::TDM) ? 1 : -1;
            MapSpawnPoint sp = _currentMap->selectBestSpawn(_sessionConfig.mode, pTeam);
            _camera.setPosition(sp.position);
            _velocity = { 0, 0, 0 };

            // Preload props and meshes
            for (const auto& prop : _currentMap->props) {
                getMesh(prop.modelPath);
                if (!prop.texturePath.empty()) {
                    getTexture(prop.texturePath);
                }
            }

            // Preload brush textures & normal maps
            for (const auto& brush : _currentMap->brushes) {
                if (!brush.texturePath.empty()) {
                    getTexture(brush.texturePath);
                }
                if (!brush.normalMapPath.empty()) {
                    getTexture(brush.normalMapPath);
                }
            }

            if (!_currentMap->metadata.cubemapPath.empty()) {
                getTexture(_currentMap->metadata.cubemapPath);
            }

            // Initialize World Weapon Spawners (Persistent 60s cooldown pads)
            _pickups.weaponPads.clear();
            for (const auto& ws : _currentMap->weaponSpawners) {
                _pickups.addWeaponPad(ws.weaponId, ws.position, ws.respawnTime, ws.yaw);
            }
            if (_pickups.weaponPads.empty()) {
                // Default tactical arena weapon spawners
                _pickups.addWeaponPad(0, Vec3(3.5f, 0.0f, -6.0f), 60.0f, 0.0f);   // Pipe
                _pickups.addWeaponPad(2, Vec3(-3.5f, 0.0f, -6.0f), 60.0f, 0.0f);  // Shotgun
                _pickups.addWeaponPad(3, Vec3(6.5f, 0.0f, -14.0f), 60.0f, 0.0f);  // M4A4-S
                _pickups.addWeaponPad(8, Vec3(-6.5f, 0.0f, -14.0f), 60.0f, 0.0f); // RPG
            }

            // Initialize Dynamic Rigid Body Physics Props (Crates & Explosive Barrels)
            _physicsWorld.clear();
            for (const auto& prop : _currentMap->props) {
                if (prop.modelPath.find("barrel") != std::string::npos) {
                    _physicsWorld.spawnExplosiveBarrel(prop.position, Vec3(0.32f, 0.48f, 0.32f) * prop.scale.x);
                } else if (prop.modelPath.find("crate") != std::string::npos) {
                    _physicsWorld.spawnCrate(prop.position, Vec3(0.45f, 0.45f, 0.45f) * prop.scale.x);
                }
            }
            if (_physicsWorld.bodies.empty()) {
                // Default Source Engine tactical arena props: wooden crates & red hazard explosive fuel barrels
                _physicsWorld.spawnCrate(Vec3(-4.0f, 0.45f, -8.0f));
                _physicsWorld.spawnCrate(Vec3(-4.0f, 1.35f, -8.0f)); // Stacked wooden crate!
                _physicsWorld.spawnCrate(Vec3(-3.1f, 0.45f, -8.0f));
                _physicsWorld.spawnExplosiveBarrel(Vec3(-4.0f, 0.48f, -6.8f)); // Hazardous fuel barrel next to crate stack!

                _physicsWorld.spawnCrate(Vec3(4.5f, 0.45f, -10.0f));
                _physicsWorld.spawnCrate(Vec3(5.4f, 0.45f, -10.0f));
                _physicsWorld.spawnExplosiveBarrel(Vec3(4.5f, 0.48f, -8.8f));

                _physicsWorld.spawnExplosiveBarrel(Vec3(0.0f, 0.48f, -16.0f)); // Central corridor explosive barrel!
            }

            // Ensure map is registered in list and selected (always formatted as .labscene)
            bool found = false;
            for (int i = 0; i < (int)_availableMaps.size(); ++i) {
                if (_availableMaps[i] == displayPath) {
                    _selectedMapIndex = i;
                    found = true;
                    break;
                }
            }
            if (!found) {
                _availableMaps.push_back(displayPath);
                _selectedMapIndex = (int)_availableMaps.size() - 1;
            }

            // Switch to gameplay mode and lock cursor
            _inMenu = false;
            setCursorCaptured(true);
            LabLog::info("Scene successfully loaded and entered: " + displayPath);
            return true;
        } else {
            LabLog::error("Failed to load scene: " + mapPath);
            return false;
        }
    }

    void applyDamageToPlayer(float rawDamage, const std::string& sourceName) {
        if (_devConsole.sp_god || _isPlayerDead || rawDamage <= 0.0f) return;

        AudioEngine::playSound(SoundID::PlayerHurt);
        _hud.triggerDamageFlash();
        _particleSystem.spawnBlood(_camera.getPosition() - Vec3(0, 0.2f, 0), Vec3(0, 1, 0), false);
        _decalSystem.spawnDecal(DecalType::BloodSplatter, Vec3(_camera.getPosition().x, 0.02f, _camera.getPosition().z), Vec3(0, 1, 0), 0.35f);

        auto dmg = _scriptEngine.calculateDamage(rawDamage, _hud.suitArmor, _hud.health);
        _hud.suitArmor = std::max(0.0f, _hud.suitArmor - dmg.absorbedByArmor);
        _hud.health = std::max(0.0f, _hud.health - dmg.finalDamage);

        if (dmg.isLethal || _hud.health <= 0.0f) {
            _hud.health = 0.0f;
            _isPlayerDead = true;
            _playerDeaths++;
            _playerRespawnTimer = _scriptEngine.getPlayerRespawnTime();
            _velocity = { 0, 0, 0 };
            _chat.addMessage("[SERVER]", "Player was eliminated by " + sourceName + "!", Vec3(1.0f, 0.3f, 0.3f));
            _hud.showCombatMessage("YOU WERE ELIMINATED! PRESS [SPACE] TO RESPAWN", 3.5f);
        }
    }

    void startSession(const GameSessionConfig& config) {
        _sessionConfig = config;
        _sessionConfig.mapPath = toSceneDisplayPath(_sessionConfig.mapPath);
        if (!loadSelectedMap(_sessionConfig.mapPath)) {
            LabLog::error("[FrozenLife] Aborting session start: failed to load scene " + _sessionConfig.mapPath);
            return;
        }

        _hud.mapName = _currentMap ? _currentMap->metadata.name : "Sector";
        _hud.gameModeName = _sessionConfig.getModeString();
        _hud.frags = 0;
        _hud.maxHealth = _scriptEngine.getPlayerMaxHealth();
        _hud.health = _hud.maxHealth;
        _hud.suitArmor = _scriptEngine.getPlayerStartArmor();
        _weaponSystem.reset();
        for (const auto& wdef : _scriptEngine.getWeaponDefinitions()) {
            _weaponSystem.applyScriptOverrides(wdef.id, wdef.damage, wdef.fireRate, wdef.clipSize, wdef.maxReserve, wdef.splashDamage, wdef.splashRadius);
        }
        syncHudWeapon();
        _particleSystem.clear();
        _decalSystem.clear();
        _interactiveSystem.clear();
        _tracers.clear();
        _pickups.clear();
        _chat.history.clear();
        _playerKills = 0;
        _playerDeaths = 0;
        _isPlayerDead = false;
        _velocity = { 0, 0, 0 };
        _playerRespawnTimer = 0.0f;

        if (_currentMap && !_currentMap->doors.empty()) {
            InteractiveEntity term;
            term.id = 1;
            term.type = InteractiveType::RetinalScanner;
            term.title = "RETINAL SCANNER";
            term.subtitle = "BIOMETRIC AIRLOCK GATE";
            term.statusText = "STAND STILL FOR RETINAL SCAN";
            term.authorizedUser = _scriptEngine.getRetinalAuthorizedUser();
            term.clearanceLevel = _scriptEngine.getRetinalClearanceLevel();
            term.scanDuration = _scriptEngine.getRetinalScanDuration();
            term.isLocked = true;
            term.isActivated = false;
            term.targetDoorIndex = 0;
            const auto& d0 = _currentMap->doors[0];
            term.position = d0.position + Vec3(-d0.size.x * 0.7f - 0.4f, 0.0f, 0.35f);
            term.normal = Vec3(0, 0, 1);
            term.themeColor = Vec3(0.2f, 0.85f, 1.0f);
            _interactiveSystem.addEntity(term);

            // Door is physically locked shut by default until biometric retinal authorization!
            _currentMap->doors[0].isLocked = true;
            _currentMap->doors[0].isOpen = false;
        }

        _chat.addMessage("[SERVER]", "Welcome to Frozen-Life :: " + (_currentMap ? _currentMap->metadata.name : "Sector"), Vec3(0.3f, 0.8f, 1.0f));
        _chat.addMessage("[SERVER]", "Mode: " + _sessionConfig.getModeString() + " | Frag Limit: " + std::to_string(_sessionConfig.fragLimit), Vec3(0.3f, 0.8f, 1.0f));
        _chat.addMessage("[SYSTEM]", "Hold [TAB] for scoreboard, press [Y] for chat, [R] to reload", Vec3(1.0f, 0.9f, 0.3f));

        if (_sessionConfig.enableBots && _sessionConfig.botCount > 0) {
            _aiManager.spawnBotsForMap(_currentMap.get(), _sessionConfig.botCount, _sessionConfig.mode);
            _hud.showCombatMessage("MATCH HOSTED: " + _sessionConfig.getModeString() + " WITH " + std::to_string(_sessionConfig.botCount) + " BOTS", 3.0f);
        } else if (_currentMap && (!_currentMap->triggers.empty() || !_currentMap->waypoints.empty())) {
            // Campaign / Story Mode: spawn scripted corridor bot for Trigger & Waypoint sequences
            _aiManager.spawnBotsForMap(_currentMap.get(), 1, _sessionConfig.mode);
            if (!_aiManager.bots.empty()) {
                std::vector<Vec3> wpNodes;
                for (const auto& wp : _currentMap->waypoints) {
                    wpNodes.push_back(wp.position);
                }
                _aiManager.bots[0].assignWaypoints(wpNodes, false);
            }
            _hud.showCombatMessage("CAMPAIGN SECTOR ENTERED :: STORY BOT ACTIVE", 3.0f);
        } else {
            _aiManager.clear();
            _hud.showCombatMessage("MATCH STARTED: BOTS DISABLED (SOLO)", 3.0f);
        }

        _inMenu = false;
        _fireLmbLast = true;
        setCursorCaptured(true);
    }

    void onFixedUpdate(float fixedDelta) override {
        if (_inMenu) return;

        // Apply World Timescale from console
        fixedDelta *= _devConsole.wrd_timescale;

        // Freeze movement if player is dead or typing in chat / console
        if (_isPlayerDead) {
            _velocity = { 0, 0, 0 };
            return;
        }
        if (_chat.isOpen || _devConsole.isOpen) {
            _velocity.x *= 0.8f;
            _velocity.z *= 0.8f;
            return;
        }

        // Physics tick rate (64 ticks per second)
        bool isSprinting = Input::isKeyPressed(340); // Left Shift
        float speed = (isSprinting ? 8.5f : 4.5f) * _devConsole.sp_speed;
        Vec3 inputDir = { 0, 0, 0 };

        Vec3 front = _camera.getFront();
        front.y = 0;
        if (front.lengthSq() > 1e-4f) front = front.normalized();

        Vec3 right = _camera.getRight();
        right.y = 0;
        if (right.lengthSq() > 1e-4f) right = right.normalized();

        if (Input::isKeyPressed('W') || Input::isKeyPressed('w')) inputDir += front;
        if (Input::isKeyPressed('S') || Input::isKeyPressed('s')) inputDir -= front;
        if (Input::isKeyPressed('A') || Input::isKeyPressed('a')) inputDir -= right;
        if (Input::isKeyPressed('D') || Input::isKeyPressed('d')) inputDir += right;

        // Developer Ghost Noclip Mode (sp_noclip) & Freecam (cam_freecam)
        if (_devConsole.sp_noclip || _devConsole.cam_freecam) {
            Vec3 camPos = _camera.getPosition();
            float flySpeed = isSprinting ? 24.0f : 12.0f;
            if (Input::isKeyPressed('W') || Input::isKeyPressed('w')) camPos += _camera.getFront() * flySpeed * fixedDelta;
            if (Input::isKeyPressed('S') || Input::isKeyPressed('s')) camPos -= _camera.getFront() * flySpeed * fixedDelta;
            if (Input::isKeyPressed('A') || Input::isKeyPressed('a')) camPos -= _camera.getRight() * flySpeed * fixedDelta;
            if (Input::isKeyPressed('D') || Input::isKeyPressed('d')) camPos += _camera.getRight() * flySpeed * fixedDelta;
            if (Input::isKeyPressed(32)) camPos.y += flySpeed * fixedDelta; // Space = Up
            if (Input::isKeyPressed(341)) camPos.y -= flySpeed * fixedDelta; // Left Ctrl = Down
            _camera.setPosition(camPos);
            _velocity = { 0, 0, 0 };
            return;
        }

        // Free-cam Noclip flying in Hammer Editor mode
        if (_hammerEditor.active) {
            Vec3 camPos = _camera.getPosition();
            float flySpeed = isSprinting ? 24.0f : 12.0f;
            if (Input::isKeyPressed('W') || Input::isKeyPressed('w')) camPos += _camera.getFront() * flySpeed * fixedDelta;
            if (Input::isKeyPressed('S') || Input::isKeyPressed('s')) camPos -= _camera.getFront() * flySpeed * fixedDelta;
            if (Input::isKeyPressed('A') || Input::isKeyPressed('a')) camPos -= _camera.getRight() * flySpeed * fixedDelta;
            if (Input::isKeyPressed('D') || Input::isKeyPressed('d')) camPos += _camera.getRight() * flySpeed * fixedDelta;
            if (Input::isKeyPressed(32)) camPos.y += flySpeed * fixedDelta; // Space = Up
            if (Input::isKeyPressed(341)) camPos.y -= flySpeed * fixedDelta; // Left Ctrl = Down
            _camera.setPosition(camPos);
            return;
        }

        bool isIceSurface = false;
        if (_currentMap && _isGrounded) {
            Vec3 feetPos = _camera.getPosition() - Vec3(0, 0.9f, 0);
            for (const auto& b : _currentMap->brushes) {
                if (b.texturePath.find("ice") != std::string::npos || b.texturePath.find("snow") != std::string::npos) {
                    Vec3 half = b.size * 0.5f + Vec3(0.25f, 0.25f, 0.25f);
                    if (feetPos.x >= b.position.x - half.x && feetPos.x <= b.position.x + half.x &&
                        feetPos.y >= b.position.y - half.y && feetPos.y <= b.position.y + half.y &&
                        feetPos.z >= b.position.z - half.z && feetPos.z <= b.position.z + half.z) {
                        isIceSurface = true;
                        break;
                    }
                }
            }
        }

        // ==================== SOURCE MOMENTUM & BUNNY HOP PHYSICS ====================
        Vec3 wishDir = inputDir;
        float wishSpeed = 0.0f;
        if (wishDir.lengthSq() > 1e-4f) {
            wishDir = wishDir.normalized();
            wishSpeed = speed;
        }

        // Bhop Grace Timer (buffer window for landing jump)
        if (_bhopGraceTimer > 0.0f) {
            _bhopGraceTimer -= fixedDelta;
            if (_bhopGraceTimer < 0.0f) _bhopGraceTimer = 0.0f;
        }

        bool jumpPressed = Input::isKeyPressed(32); // Spacebar

        if (_isGrounded) {
            // Check if jumping (immediate jump or continuous bhop jump buffer)
            if (jumpPressed) {
                _velocity.y = _devConsole.sp_jump;
                _isGrounded = false;
                _bhopGraceTimer = 0.0f;
                // Ground friction is bypassed! Full horizontal momentum preserved!
            } else {
                // Apply Ground Friction
                float curHorizSpeed = std::sqrt(_velocity.x * _velocity.x + _velocity.z * _velocity.z);
                if (curHorizSpeed > 0.001f) {
                    if (isIceSurface) {
                        _velocity.x *= 0.985f;
                        _velocity.z *= 0.985f;
                    } else {
                        float friction = _devConsole.phys_friction;
                        float drop = curHorizSpeed * friction * fixedDelta;
                        float newSpeed = std::max(0.0f, curHorizSpeed - drop);
                        _velocity.x = (_velocity.x / curHorizSpeed) * newSpeed;
                        _velocity.z = (_velocity.z / curHorizSpeed) * newSpeed;
                    }
                    if (std::abs(_velocity.x) < 0.02f) _velocity.x = 0.0f;
                    if (std::abs(_velocity.z) < 0.02f) _velocity.z = 0.0f;
                }

                // Ground Acceleration
                if (wishSpeed > 0.0f) {
                    float curSpeedInWish = _velocity.x * wishDir.x + _velocity.z * wishDir.z;
                    float addSpeed = wishSpeed - curSpeedInWish;
                    if (addSpeed > 0.0f) {
                        float groundAccel = isIceSurface ? 3.5f : 12.0f;
                        float accelSpeed = groundAccel * wishSpeed * fixedDelta;
                        if (accelSpeed > addSpeed) accelSpeed = addSpeed;
                        _velocity.x += wishDir.x * accelSpeed;
                        _velocity.z += wishDir.z * accelSpeed;
                    }
                    _bobTime += fixedDelta * (speed * 2.0f);
                }
            }
        } else {
            // IN AIR: Air strafing & air acceleration (Source Engine PM_AirAccelerate)
            if (_devConsole.sp_bhop && wishSpeed > 0.0f) {
                float maxAirWishSpeed = 30.0f; // Source Engine air wishspeed clamp
                float curSpeedInWish = _velocity.x * wishDir.x + _velocity.z * wishDir.z;
                float addSpeed = std::min(wishSpeed, maxAirWishSpeed) - curSpeedInWish;
                if (addSpeed > 0.0f) {
                    float airAccel = _devConsole.phys_air_accel;
                    float accelSpeed = airAccel * wishSpeed * fixedDelta;
                    if (accelSpeed > addSpeed) accelSpeed = addSpeed;
                    _velocity.x += wishDir.x * accelSpeed;
                    _velocity.z += wishDir.z * accelSpeed;
                }
            }
        }

        // Clamp horizontal bunnyhop / strafe momentum to prevent infinite / overpowered acceleration
        if (_devConsole.phys_max_bhop_speed > 0.0f) {
            float horizSpeed = std::sqrt(_velocity.x * _velocity.x + _velocity.z * _velocity.z);
            if (horizSpeed > _devConsole.phys_max_bhop_speed) {
                float scale = _devConsole.phys_max_bhop_speed / horizSpeed;
                _velocity.x *= scale;
                _velocity.z *= scale;
            }
        }

        // Gravity
        _velocity.y -= _devConsole.phys_gravity * fixedDelta;

        // ==================== AABB MOVE & SLIDE WALL COLLISION ====================
        bool wasGrounded = _isGrounded;
        Vec3 pos = _camera.getPosition();
        if (_currentMap) {
            auto obstacles = LabCollision::getMapSolidBoxes(*_currentMap);
            LabCollision::moveAndSlide(pos, _velocity, _isGrounded, fixedDelta, obstacles, 1.70f, 0.35f, 1.85f, _devConsole.phys_step_height);
        } else {
            pos += _velocity * fixedDelta;
            if (pos.y < 1.70f) {
                pos.y = 1.70f;
                _velocity.y = 0.0f;
                _isGrounded = true;
            }
        }
        _camera.setPosition(pos);

        // Landing transition detection: start bhop grace window
        if (!wasGrounded && _isGrounded) {
            _bhopGraceTimer = _devConsole.phys_bhop_window;
        }

        // Network command generation synchronized with fixed physics rate (64 Hz)
        if (_netClient.isConnected()) {
            uint32_t netButtons = 0;
            if (Input::isKeyPressed(32)) netButtons |= NetButton_Jump;
            if (Input::isMouseButtonPressed(0)) netButtons |= NetButton_Fire;
            if (Input::isKeyPressed('R') || Input::isKeyPressed('r')) netButtons |= NetButton_Reload;
            if (isSprinting) netButtons |= NetButton_Sprint;
            if (_flashlight.enabled) netButtons |= NetButton_Flashlight;

            float netForward = 0.0f;
            float netSide = 0.0f;
            if (!_chat.isOpen && !_isPlayerDead && !_inMenu) {
                if (Input::isKeyPressed('W') || Input::isKeyPressed('w')) netForward += 1.0f;
                if (Input::isKeyPressed('S') || Input::isKeyPressed('s')) netForward -= 1.0f;
                if (Input::isKeyPressed('A') || Input::isKeyPressed('a')) netSide -= 1.0f;
                if (Input::isKeyPressed('D') || Input::isKeyPressed('d')) netSide += 1.0f;
            }

            _netClient.update(fixedDelta, pos, _velocity, _camera.getYaw(), _camera.getPitch(), netButtons, netForward, netSide, static_cast<uint8_t>(_weaponSystem.getActiveId()));
        } else if (_netClient.getState() == ClientConnectionState::Connecting) {
            _netClient.update(fixedDelta, pos, _velocity, _camera.getYaw(), _camera.getPitch(), 0, 0.0f, 0.0f, static_cast<uint8_t>(_weaponSystem.getActiveId()));
        }

        // Procedural Door animations and distance triggers
        if (_currentMap) {
            for (auto& door : _currentMap->doors) {
                if (!door.isLocked) {
                    float distSq = (pos - door.position).lengthSq();
                    float triggerRadiusSq = door.triggerRadius * door.triggerRadius;
                    door.isOpen = (distSq < triggerRadiusSq);
                }

                if (door.isOpen && door.currentProgress < 1.0f) {
                    door.currentProgress = std::min(1.0f, door.currentProgress + fixedDelta * door.openSpeed);
                } else if (!door.isOpen && door.currentProgress > 0.0f) {
                    door.currentProgress = std::max(0.0f, door.currentProgress - fixedDelta * door.openSpeed);
                }
            }
        }
    }

    void onUpdate(const Time& time) override {
        // Startup Splash Screen Handling
        if (_splashActive) {
            _splashTimer += time.delta;
            bool skip = Input::isKeyJustPressed(GLFW_KEY_SPACE) || Input::isKeyJustPressed(GLFW_KEY_ENTER) ||
                        Input::isKeyJustPressed(GLFW_KEY_KP_ENTER) || Input::isKeyJustPressed(GLFW_KEY_ESCAPE) ||
                        Input::isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT) || Input::isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_RIGHT);
            if (skip || _splashTimer >= _splashDuration) {
                _splashActive = false;
            }
            return;
        }

        // =====================================================================
        // Developer Console Toggle & Interactive Window Input Handling
        // =====================================================================
        bool justToggled = false;
        bool gravePressed = Input::isKeyPressed(96);
        bool f10Pressed = Input::isKeyPressed(299);
        if ((gravePressed && !_gravePressedLast) || (f10Pressed && !_f10PressedLast)) {
            _devConsole.toggle();
            justToggled = true;
        }
        _gravePressedLast = gravePressed;
        _f10PressedLast = f10Pressed;

        // Synchronize mouse cursor capture when console opens or closes
        if (_devConsoleWasOpen && !_devConsole.isOpen) {
            if (!_inMenu && !_isPlayerDead) {
                setCursorCaptured(true);
            }
        } else if (!_devConsoleWasOpen && _devConsole.isOpen) {
            setCursorCaptured(false);
        }
        _devConsoleWasOpen = _devConsole.isOpen;

        // If console is active, capture all keyboard and mouse interactions
        if (_devConsole.isOpen) {
            double curX = 0.0, curY = 0.0;
            getCursorPos(curX, curY);
            if (curX == 0.0 && curY == 0.0) {
                curX = static_cast<double>(Input::getMouseX());
                curY = static_cast<double>(Input::getMouseY());
            }

            _devConsole.onMouseMove(static_cast<float>(curX), static_cast<float>(curY));

            if (_devConsole.isDragging && !Input::isMouseButtonPressed(0)) {
                _devConsole.isDragging = false;
            }

            if (Input::isMouseButtonJustPressed(0)) {
                _devConsole.onMouseButton(0, 1, static_cast<float>(curX), static_cast<float>(curY));
            } else if (Input::isMouseButtonJustReleased(0)) {
                _devConsole.onMouseButton(0, 0, static_cast<float>(curX), static_cast<float>(curY));
            }

            if (Input::isMouseButtonJustPressed(1)) {
                _devConsole.onMouseButton(1, 1, static_cast<float>(curX), static_cast<float>(curY));
            } else if (Input::isMouseButtonJustReleased(1)) {
                _devConsole.onMouseButton(1, 0, static_cast<float>(curX), static_cast<float>(curY));
            }

            if (Input::scrollDelta != 0.0f) {
                _devConsole.onMouseScroll(Input::scrollDelta);
            }

            bool shift = Input::isKeyPressed(GLFW_KEY_LEFT_SHIFT) || Input::isKeyPressed(GLFW_KEY_RIGHT_SHIFT);
            bool ctrl = Input::isKeyPressed(GLFW_KEY_LEFT_CONTROL) || Input::isKeyPressed(GLFW_KEY_RIGHT_CONTROL);

            // Forward all pressed/repeated keys directly to developer console
            for (int k = 0; k < 512; ++k) {
                if (Input::keysJustPressed[k]) {
                    if (justToggled && (k == 96 || k == 299)) {
                        continue;
                    }
                    _devConsole.onKey(k, 1, shift, ctrl);
                }
            }

            _devConsole.update(time.delta);
            return; // Lock player and menu inputs while in developer console
        }

        // Camera FOV from console
        _camera.fieldOfView = _devConsole.cam_fov;

        // Process sys_bind key mappings
        if (!_chat.isOpen && !_devConsole.getKeyBinds().empty()) {
            for (int k = 0; k < 512; ++k) {
                if (Input::keysJustPressed[k]) {
                    std::string keyName;
                    if (k >= GLFW_KEY_F1 && k <= GLFW_KEY_F12) {
                        keyName = "f" + std::to_string(k - GLFW_KEY_F1 + 1);
                    } else if (k >= 'A' && k <= 'Z') {
                        keyName = std::string(1, (char)std::tolower(k));
                    } else if (k >= '0' && k <= '9') {
                        keyName = std::string(1, (char)k);
                    } else if (k == GLFW_KEY_SPACE) {
                        keyName = "space";
                    } else if (k == GLFW_KEY_ENTER) {
                        keyName = "enter";
                    } else if (k == GLFW_KEY_TAB) {
                        keyName = "tab";
                    }
                    if (!keyName.empty()) {
                        _devConsole.executeBind(keyName);
                    }
                }
            }
        }

        // Decay camera shake trauma
        if (_camShakeTrauma > 0.0f) {
            _camShakeTrauma = std::max(0.0f, _camShakeTrauma - time.delta * 1.8f);
        }

        // Synchronize decal limits
        _decalSystem.setMaxDecals(static_cast<size_t>(_devConsole.dcl_limit));
        _decalSystem.setMaxDistance(_devConsole.dcl_distance);

        // Enforce AI aggro state
        if (_devConsole.ai_aggro) {
            for (auto& bot : _aiManager.bots) {
                if (bot.isAlive()) {
                    bot.state = AIState::Attack;
                    bot.reactionTimer = 0.0f;
                }
            }
        }

        if (_inMenu) {
            int winW = 1280, winH = 720;
            getWindowSize(winW, winH);
            int fbW = 1280, fbH = 720;
            getFramebufferSize(fbW, fbH);

            if (_menuScreen == MenuScreen::CharacterStudio) {
                float scaleX = (winW > 0 && fbW > 0) ? (static_cast<float>(fbW) / static_cast<float>(winW)) : 1.0f;
                float scaleY = (winH > 0 && fbH > 0) ? (static_cast<float>(fbH) / static_cast<float>(winH)) : 1.0f;
                float studioMx = Input::getMouseX() * scaleX;
                float studioMy = Input::getMouseY() * scaleY;
                bool rmb = Input::isMouseButtonPressed(1);

                // Keyboard shortcuts for studio
                bool ctrl = Input::isKeyPressed(GLFW_KEY_LEFT_CONTROL) || Input::isKeyPressed(GLFW_KEY_RIGHT_CONTROL);
                bool shift = Input::isKeyPressed(GLFW_KEY_LEFT_SHIFT) || Input::isKeyPressed(GLFW_KEY_RIGHT_SHIFT);
                for (int k = 0; k < 512; ++k) {
                    if (Input::keys[k] && !_lastMenuKeys[k]) {
                        _characterStudio.handleKeyDown(k, ctrl, shift);
                    }
                    _lastMenuKeys[k] = Input::keys[k];
                }

                _characterStudio.update(time.delta, studioMx, studioMy, Input::isMouseButtonPressed(0), rmb, Input::scrollDelta);
                Input::scrollDelta = 0.0f;

                if (_characterStudio.requestExit()) {
                    _characterStudio.saveConfig();
                    reloadStudioWeapons();
                    _menuScreen = MenuScreen::Main;
                    _characterStudio.clearRequestExit();
                }
                return;
            }

            double curX = 0.0, curY = 0.0;
            getCursorPos(curX, curY);
            if (curX == 0.0 && curY == 0.0) {
                curX = (double)Input::getMouseX();
                curY = (double)Input::getMouseY();
            }
            float mx = (winW > 0) ? ((float)curX * 1280.0f / (float)winW) : (float)curX;
            float my = (winH > 0) ? ((float)curY * 720.0f / (float)winH) : (float)curY;
            _menuMouseX = mx;
            _menuMouseY = my;

            bool lmbPressed = Input::isMouseButtonPressed(0);
            bool lmbJustPressed = Input::isMouseButtonJustPressed(0);
            bool lmbClick = lmbJustPressed || (lmbPressed && !_menuLmbLast);
            _menuLmbLast = lmbPressed;

            if (_menuScreen == MenuScreen::JoinGame) {
                _serverBrowser.update(time.delta);
                // F5 Refresh shortcut
                if (Input::isKeyPressed(294)) {
                    _serverBrowser.refresh();
                }

                if (_isConnecting) {
                    _connectAnimTimer += time.delta;
                    _netClient.update(time.delta, Vec3(0, 0, 0), Vec3(0, 0, 0), 0.0f, 0.0f, 0, 0.0f, 0.0f, 0);

                    if (_netClient.isConnected()) {
                        _isConnecting = false;
                        _connectStatusMsg = "CONNECTED TO SERVER! JOINING ARENA...";
                        std::string srvMap = _netClient.getConnectedMapName();
                        if (srvMap.empty()) srvMap = "facility_alpha.labmap";
                        if (srvMap.find('/') == std::string::npos && srvMap.find('\\') == std::string::npos) {
                            srvMap = "assets/maps/" + srvMap;
                        }
                        _sessionConfig.mapPath = srvMap;
                        _sessionConfig.mode = GameMode::FFA;
                        _sessionConfig.enableBots = false; // Never spawn local bots on guest
                        _sessionConfig.botCount = 0;
                        _isLocalHost = false;
                        startSession(_sessionConfig);
                        _chat.addMessage("[CLIENT]", "Connected to server " + _netClient.getServerAddressString() + "!", Vec3(0.2f, 0.9f, 0.35f));
                        _hud.showCombatMessage("CONNECTED TO MULTIPLAYER ARENA!", 3.0f);
                        return;
                    } else if (_netClient.getState() == ClientConnectionState::TimedOut) {
                        _isConnecting = false;
                        _connectStatusMsg = "FAILED TO CONNECT: TIMEOUT! Check Port 27015 / UDP Punch or connect via Radmin VPN";
                        _netClient.disconnect();
                    }
                }
            }

            handleMenuInput(mx, my, lmbClick);
            return;
        }

        // Handle Death State & Respawn countdown
        if (_isPlayerDead) {
            _playerRespawnTimer -= time.delta;
            Vec3 deathPos = _camera.getPosition();
            if (deathPos.y > 0.45f) {
                deathPos.y = std::max(0.45f, deathPos.y - time.delta * 2.5f);
                _camera.setPosition(deathPos);
            }

            bool spacePressed = Input::isKeyPressed(32);
            bool enterPressed = Input::isKeyPressed(257);
            if (_playerRespawnTimer <= 0.0f || spacePressed || enterPressed) {
                _isPlayerDead = false;
                _hud.health = _scriptEngine.getPlayerMaxHealth();
                _hud.suitArmor = _scriptEngine.getPlayerStartArmor();
                _weaponSystem.reset();
                for (const auto& wdef : _scriptEngine.getWeaponDefinitions()) {
                    _weaponSystem.applyScriptOverrides(wdef.id, wdef.damage, wdef.fireRate, wdef.clipSize, wdef.maxReserve, wdef.splashDamage, wdef.splashRadius);
                }
                syncHudWeapon();
                if (_currentMap) {
                    std::vector<Vec3> enemies;
                    for (const auto& b : _aiManager.bots) {
                        if (b.isAlive()) enemies.push_back(b.position);
                    }
                    int pTeam = (_sessionConfig.mode == GameMode::TDM) ? 1 : -1;
                    MapSpawnPoint sp = _currentMap->selectBestSpawn(_sessionConfig.mode, pTeam, enemies);
                    _camera.setPosition(sp.position);
                    _velocity = { 0, 0, 0 };
                }
                _chat.addMessage("[SERVER]", "Player respawned at designated base!", Vec3(0.3f, 0.85f, 1.0f));
                _hud.showCombatMessage("RESPAWNED - READY FOR COMBAT!", 2.5f);
            }
            return;
        }



        // Chat toggle & input handling
        bool enterPressed = Input::isKeyPressed(257);
        bool escPressed = Input::isKeyPressed(256);
        bool yPressed = Input::isKeyPressed('Y') || Input::isKeyPressed('y');
        bool tPressed = Input::isKeyPressed('T') || Input::isKeyPressed('t');
        bool backspacePressed = Input::isKeyPressed(259);

        if (_chat.isOpen) {
            if (enterPressed && !_enterPressedLast) {
                std::string sentMsg;
                _chat.onKey(257, 1, sentMsg);
                if (!sentMsg.empty() && _netClient.isConnected()) {
                    _netClient.sendChatMessage(sentMsg);
                }
            } else if (escPressed && !_escPressedLast) {
                _chat.close();
            } else if (backspacePressed && !_backspacePressedLast) {
                std::string dummy;
                _chat.onKey(259, 1, dummy);
            }
        } else {
            if ((yPressed && !_yPressedLast) || (tPressed && !_tPressedLast) || (enterPressed && !_enterPressedLast)) {
                if (!_hammerEditor.active) {
                    _chat.open();
                }
            }
        }
        _enterPressedLast = enterPressed;
        _escPressedLast = escPressed;
        _yPressedLast = yPressed;
        _tPressedLast = tPressed;
        _backspacePressedLast = backspacePressed;
        _chat.update(time.delta);

        // Poll incoming network events (chat, authoritative damage, kills, projectiles)
        if (_netClient.isConnected()) {
            for (const auto& chatMsg : _netClient.pollIncomingChat()) {
                _chat.addMessage(chatMsg.sender, chatMsg.text, Vec3(0.35f, 0.85f, 1.0f));
            }
            for (const auto& dmg : _netClient.pollIncomingDamageEvents()) {
                if (dmg.targetId == _netClient.getClientId()) {
                    applyDamageToPlayer(dmg.damage, "Player " + std::to_string(dmg.attackerId));
                    _hud.triggerDamageFlash();
                    AudioEngine::playSound(SoundID::PlayerHurt);
                }
            }
            for (const auto& kill : _netClient.pollIncomingKillEvents()) {
                std::string msg = std::string(kill.killerName) + " ELIMINATED " + kill.victimName + (kill.isHeadshot ? " (HEADSHOT!)" : "");
                _chat.addMessage("[DEATH]", msg, kill.isHeadshot ? Vec3(1.0f, 0.25f, 0.25f) : Vec3(0.9f, 0.5f, 0.2f));
                _hud.showCombatMessage(msg, 3.0f);
                if (kill.killerId == _netClient.getClientId()) {
                    _playerKills++;
                    _hud.frags = _playerKills;
                    AudioEngine::playSound(SoundID::AccessGranted);
                }
                if (kill.victimId == _netClient.getClientId()) {
                    _playerDeaths++;
                    _isPlayerDead = true;
                    _playerRespawnTimer = 3.0f;
                    _velocity = { 0, 0, 0 };
                    AudioEngine::playSound(SoundID::PlayerHurt);
                }
            }
            for (const auto& proj : _netClient.pollIncomingProjectiles()) {
                Projectile p;
                p.position = proj.position;
                p.weaponId = static_cast<WeaponID>(proj.weaponId);
                float spd = (p.weaponId == WeaponID::RPG) ? 28.0f : 35.0f;
                p.velocity = proj.direction.normalized() * spd;
                p.damage = (p.weaponId == WeaponID::RPG) ? 140.0f : 45.0f;
                p.splashRadius = (p.weaponId == WeaponID::RPG) ? 5.5f : 3.0f;
                p.splashDamage = (p.weaponId == WeaponID::RPG) ? 90.0f : 30.0f;
                p.color = (p.weaponId == WeaponID::RPG) ? Vec3(1.0f, 0.45f, 0.1f) : Vec3(0.2f, 0.85f, 1.0f);
                p.lifetime = 0.0f;
                p.maxLifetime = 6.0f;
                p.active = true;
                _weaponSystem.getProjectiles().push_back(p);
            }
        }

        // Network simulation tick and client-side prediction update
        if (_localServer.isRunning()) {
            _localServer.tick(time.delta);
        }
        if (_netClient.isConnected()) {
            if (_netClient.hasNewSnapshot()) {
                const auto& snap = _netClient.getLatestSnapshot();
                for (uint16_t i = 0; i < snap.playerCount; ++i) {
                    if (snap.players[i].clientId == _netClient.getClientId()) {
                        float errorThreshold = _isLocalHost ? 2.5f : 0.45f;
                        Vec3 correctedPos, correctedVel;
                        if (_netClient.getPrediction().reconcile(snap.lastProcessedCmd, snap.players[i].position, snap.players[i].velocity, correctedPos, correctedVel, errorThreshold)) {
                            _camera.setPosition(correctedPos);
                            _velocity = correctedVel;
                        }
                        break;
                    }
                }
                _netClient.consumeSnapshot();
            }
        }

        // Gameplay camera orientation update (only when not typing in chat)
        if (!_chat.isOpen) {
            _camera.update(Input::mouseDelta);
        }

        // Return to menu with M or ESC key (only when chat is closed)
        if (!_chat.isOpen && (Input::isKeyPressed(GLFW_KEY_M) || Input::isKeyPressed(GLFW_KEY_ESCAPE))) {
            _inMenu = true;
            _menuScreen = MenuScreen::Main;
            setCursorCaptured(false);
            return;
        }

        // ==================== WEAPON SWITCHING (SCROLL, 1..9, Q) ====================
        if (!_inMenu && !_chat.isOpen && !_isPlayerDead) {
            // Mouse Scroll Wheel Switch
            if (Input::scrollDelta != 0.0f) {
                if (Input::scrollDelta < 0.0f) {
                    _weaponSystem.nextWeapon();
                } else if (Input::scrollDelta > 0.0f) {
                    _weaponSystem.prevWeapon();
                }
                syncHudWeapon();
            }

            // Quick Switch (Q Key)
            bool qPressed = Input::isKeyPressed(81); // GLFW_KEY_Q = 81
            if (qPressed) {
                if (!_qPressedLast && !_hammerEditor.active) {
                    _weaponSystem.quickSwitch();
                    syncHudWeapon();
                    _qPressedLast = true;
                }
            } else {
                _qPressedLast = false;
            }

            // Slot Keys 1..9 (GLFW_KEY_1 = 49 to GLFW_KEY_9 = 57)
            for (int k = 0; k < 9; ++k) {
                int key = 49 + k;
                if (Input::isKeyPressed(key)) {
                    if (!_numPressedLast[k] && !_hammerEditor.active) {
                        if (_weaponSystem.equipSlot(k + 1)) {
                            syncHudWeapon();
                        }
                        _numPressedLast[k] = true;
                    }
                } else {
                    _numPressedLast[k] = false;
                }
            }
        }

        _weaponSystem.update(time.delta);

        // ==================== 3D SPATIAL AUDIO SYSTEM & LISTENER UPDATE ====================
        AudioEngine::setListener(_camera.getPosition(), _camera.getFront(), _camera.getUp());
        AudioEngine::update(time.delta);

        // Procedural footstep audio on walking/running across map terrain
        float horizSpeed = std::sqrt(_velocity.x * _velocity.x + _velocity.z * _velocity.z);
        if (_isGrounded && horizSpeed > 1.2f && !_isPlayerDead && !_inMenu) {
            _footstepTimer += time.delta * (horizSpeed / 3.8f);
            if (_footstepTimer >= 0.42f) {
                _footstepTimer = 0.0f;
                bool isCryo = (_currentMap && _currentMap->metadata.name.find("Cryo") != std::string::npos);
                if (isCryo) {
                    AudioEngine::playSound(SoundID::FootstepIce, 0.6f);
                } else {
                    AudioEngine::playSound(SoundID::FootstepConcrete, 0.6f);
                }
            }
        } else {
            _footstepTimer = 0.0f;
        }

        // ==================== 3D PARTICLE SYSTEM UPDATE & AMBIENT WEATHER ====================
        if (_currentMap) {
            _particleSystem.update(time.delta, _currentMap.get());
        } else {
            _particleSystem.update(time.delta, nullptr);
        }

        // Update Decal System lifetimes & alpha fadeouts
        _decalSystem.update(time.delta);

        // Ambient drifting weather (Snow/Frost for Cryo maps, subtle industrial dust for others)
        if (!_inMenu && !_hammerEditor.active && !_isPlayerDead) {
            bool isCryo = (_currentMap && _currentMap->metadata.name.find("Cryo") != std::string::npos);
            _particleSystem.spawnAmbientWeather(_camera.getPosition(), 2, isCryo);
        }

        // ==================== PLAYER COMBAT: 9 DISTINCT WEAPONS & FIRING MODES ====================
        bool isFireHeld = Input::isMouseButtonPressed(0);
        auto& activeWep = _weaponSystem.getActiveWeapon();
        const auto& def = activeWep.def;

        if (def.id == WeaponID::Minigun && isFireHeld && !_hammerEditor.active && !_chat.isOpen && !_isPlayerDead) {
            _weaponSystem.addMinigunSpin(time.delta * 720.0f);
        }

        bool triggerFire = false;
        if (def.isAutomatic) {
            triggerFire = isFireHeld && (_weaponSystem.getFireCooldown() <= 0.0f);
        } else {
            triggerFire = isFireHeld && (!_fireLmbLast) && (_weaponSystem.getFireCooldown() <= 0.0f);
        }
        _fireLmbLast = isFireHeld;

        if (triggerFire && !_hammerEditor.active && !_chat.isOpen && !_isPlayerDead) {
            if (def.isMelee || activeWep.currentClip > 0) {
                if (!def.isMelee) {
                    activeWep.currentClip--;
                }
                _weaponSystem.setFireCooldown(def.fireRate);
                _muzzleFlashTime = def.isMelee ? 0.0f : 0.08f;
                _weaponAnimator.onFire(def.isMelee);
                _weaponAnimator.recoilSpring.addImpulse(Vec3(0.0f, def.recoilPitch, def.recoilKick));
                _playerShootTimer = def.isMelee ? 0.35f : 0.22f;

                switch (def.id) {
                    case WeaponID::Pipe:      AudioEngine::playSound(SoundID::PipeSwing); break;
                    case WeaponID::Pistol:    AudioEngine::playSound(SoundID::PistolShot); break;
                    case WeaponID::Shotgun:   AudioEngine::playSound(SoundID::ShotgunShot); break;
                    case WeaponID::M4A4S:     AudioEngine::playSound(SoundID::M4A4SShot); break;
                    case WeaponID::SG553:     AudioEngine::playSound(SoundID::SG553Shot); break;
                    case WeaponID::Minigun:   AudioEngine::playSound(SoundID::MinigunShot); break;
                    case WeaponID::PlasmaGun: AudioEngine::playSound(SoundID::PlasmaShot); break;
                    case WeaponID::Railgun:   AudioEngine::playSound(SoundID::RailgunShot); break;
                    case WeaponID::RPG:       AudioEngine::playSound(SoundID::RPGLaunch); break;
                    default: break;
                }

                Vec3 rayOrigin = _camera.getPosition();
                Vec3 forward = _camera.getFront();
                Vec3 right = _camera.getRight();
                Vec3 up = _camera.getUp();
                Vec3 muzzlePos = rayOrigin + right * 0.22f - up * 0.18f + forward * 0.45f;

                // Spawn realistic weapon muzzle smoke & ignition sparks
                _particleSystem.spawnMuzzleEffect(muzzlePos, forward, def.id);

                if (def.isProjectile) {
                    // Projectile weapon: Plasma Gun & RPG
                    _weaponSystem.spawnProjectile(muzzlePos, forward);
                    if (_netClient.isConnected()) {
                        _netClient.sendProjectileSpawn(static_cast<uint8_t>(def.id), muzzlePos, forward);
                    }
                } else if (def.isMelee) {
                    // Melee sweep: Pipe (2.6m sweep)
                    RaycastHit hit;
                    hit.distance = def.range;
                    _aiManager.testRaycast(rayOrigin, forward, hit);
                    _physicsWorld.raycast(rayOrigin, forward, hit);

                    // Test Remote Multiplayer Players
                    if (_netClient.isConnected()) {
                        const auto& snap = _netClient.getLatestSnapshot();
                        for (uint16_t i = 0; i < snap.playerCount; ++i) {
                            const auto& p = snap.players[i];
                            if (p.clientId == _netClient.getClientId() || !p.isAlive) continue;
                            Vec3 pCenter = p.position - Vec3(0.0f, 0.775f, 0.0f);
                            Vec3 pMin = pCenter - Vec3(0.45f, 0.925f, 0.45f);
                            Vec3 pMax = pCenter + Vec3(0.45f, 0.925f, 0.45f);
                            Vec3 norm;
                            float t = 0.0f;
                            if (Raycast::rayIntersectAABB(rayOrigin, forward, pMin, pMax, t, &norm)) {
                                if (t < hit.distance) {
                                    hit.hit = true;
                                    hit.distance = t;
                                    hit.point = rayOrigin + forward * t;
                                    hit.normal = norm;
                                    hit.tag = EntityTag::Bot;
                                    hit.entityIndex = 10000 + p.clientId;
                                    hit.isHeadshot = (hit.point.y >= p.position.y - 0.25f);
                                }
                            }
                        }
                    }

                    if (hit.hit && hit.distance <= def.range) {
                        if (hit.entityIndex >= 10000) {
                            uint32_t targetId = static_cast<uint32_t>(hit.entityIndex - 10000);
                            float dmg = hit.isHeadshot ? (def.damage * def.headshotMultiplier) : def.damage;
                            _netClient.sendDamage(targetId, dmg, static_cast<uint8_t>(def.id), hit.isHeadshot, hit.point);
                            _hud.triggerHitmarker(hit.isHeadshot);
                            _particleSystem.spawnBlood(hit.point, -forward, hit.isHeadshot);
                            AudioEngine::playSound(SoundID::PipeHit);
                        } else
                        if (hit.tag == EntityTag::Bot) {
                            AudioEngine::playSound(SoundID::PipeHit);
                            for (auto& bot : _aiManager.bots) {
                                if (bot.id == hit.entityIndex && bot.isAlive()) {
                                    float dmg = hit.isHeadshot ? (def.damage * def.headshotMultiplier) : def.damage;
                                    Vec3 knockback = forward * 8.5f + Vec3(0, 3.2f, 0);
                                    bool killed = bot.takeDamage(dmg, hit.isHeadshot, knockback);
                                    _hud.triggerHitmarker(hit.isHeadshot);

                                    // Melee blood impact
                                    _particleSystem.spawnBlood(hit.point, -forward, hit.isHeadshot);
                                    if (_currentMap) {
                                        Vec3 bNorm;
                                        float bestT = 4.0f;
                                        bool foundWall = false;
                                        for (const auto& b : _currentMap->brushes) {
                                            Vec3 half = b.size * 0.5f;
                                            float t = 0.0f;
                                            if (Raycast::rayIntersectAABB(hit.point + forward * 0.05f, forward, b.position - half, b.position + half, t, &bNorm)) {
                                                if (t < bestT) {
                                                    bestT = t;
                                                    foundWall = true;
                                                }
                                            }
                                        }
                                        if (foundWall) {
                                            Vec3 bloodPos = hit.point + forward * bestT;
                                            _decalSystem.spawnDecal(DecalType::BloodSplatter, bloodPos, bNorm, 0.45f);
                                        }
                                    }
                                    _decalSystem.spawnDecal(DecalType::BloodSplatter, Vec3(hit.point.x, 0.02f, hit.point.z), Vec3(0, 1, 0), 0.35f);

                                    if (killed) {
                                        _playerKills++;
                                        _hud.frags = _playerKills;
                                        std::string killMsg = (hit.isHeadshot ? "HEADSHOT SMASH! ELIMINATED " : "ELIMINATED ") +
                                                              bot.name + " WITH " + def.name + " [" + std::to_string(_playerKills) + " FRAGS]";
                                        _hud.showCombatMessage(killMsg, 2.5f);
                                        _chat.addMessage("[SERVER]", killMsg, Vec3(1.0f, 0.4f, 0.2f));

                                        _pickups.spawnPickup(PickupType::Ammo, bot.position + Vec3(0.0f, 0.35f, 0.0f), 36);
                                        if ((rand() % 100) < 65) {
                                            _pickups.spawnPickup(PickupType::Medkit, bot.position + Vec3(0.4f, 0.35f, -0.3f), 50);
                                        }
                                        WeaponID dropWep = bot.equippedWeapon;
                                        _pickups.spawnPickup(PickupType::WeaponDrop, bot.position + Vec3(-0.35f, 0.35f, 0.2f), 30, (int)dropWep);
                                    }
                                    break;
                                }
                            }
                        } else if (hit.tag == EntityTag::RigidProp) {
                            AudioEngine::playSound(SoundID::PipeHit);
                            _physicsWorld.takeDamage(hit.entityIndex, def.damage, hit.point, forward);
                            _particleSystem.spawnImpact(hit.point, hit.normal, SurfaceType::Wood);
                            _hud.triggerHitmarker(false);
                        }
                    }
                } else {
                    // Hitscan weapons (Pistol, Shotgun, M4A4-S, SG553, Minigun, Railgun)
                    for (int p = 0; p < def.bulletsPerShot; ++p) {
                        Vec3 spreadDir = forward;
                        if (def.spread > 0.0f) {
                            float rx = ((float)(rand() % 2000) / 1000.0f - 1.0f) * def.spread;
                            float ry = ((float)(rand() % 2000) / 1000.0f - 1.0f) * def.spread;
                            spreadDir = (forward + right * rx + up * ry).normalized();
                        }

                        RaycastHit hit;
                        hit.distance = def.range;

                        // 1. Test AI Combat Bots & Dynamic Rigid Props
                        _aiManager.testRaycast(rayOrigin, spreadDir, hit);
                        _physicsWorld.raycast(rayOrigin, spreadDir, hit);

                        // Test Remote Multiplayer Players
                        if (_netClient.isConnected()) {
                            const auto& snap = _netClient.getLatestSnapshot();
                            for (uint16_t i = 0; i < snap.playerCount; ++i) {
                                const auto& rp = snap.players[i];
                                if (rp.clientId == _netClient.getClientId() || !rp.isAlive) continue;
                                Vec3 pCenter = rp.position - Vec3(0.0f, 0.775f, 0.0f);
                                Vec3 pMin = pCenter - Vec3(0.35f, 0.925f, 0.35f);
                                Vec3 pMax = pCenter + Vec3(0.35f, 0.925f, 0.35f);
                                Vec3 norm;
                                float t = 0.0f;
                                if (Raycast::rayIntersectAABB(rayOrigin, spreadDir, pMin, pMax, t, &norm)) {
                                    if (t < hit.distance) {
                                        hit.hit = true;
                                        hit.distance = t;
                                        hit.point = rayOrigin + spreadDir * t;
                                        hit.normal = norm;
                                        hit.tag = EntityTag::Bot;
                                        hit.entityIndex = 10000 + rp.clientId;
                                        hit.isHeadshot = (hit.point.y >= rp.position.y - 0.25f);
                                    }
                                }
                            }
                        }

                        // 2. Test Solid Map Geometry (Brushes & Doors)
                        if (_currentMap) {
                            Vec3 norm;
                            for (size_t i = 0; i < _currentMap->brushes.size(); ++i) {
                                const auto& b = _currentMap->brushes[i];
                                Vec3 half = b.size * 0.5f;
                                float t = 0.0f;
                                if (Raycast::rayIntersectAABB(rayOrigin, spreadDir, b.position - half, b.position + half, t, &norm)) {
                                    if (t < hit.distance) {
                                        hit.hit = true;
                                        hit.distance = t;
                                        hit.point = rayOrigin + spreadDir * t;
                                        hit.normal = norm;
                                        hit.tag = EntityTag::World;
                                        hit.entityIndex = (int)i;
                                        hit.isHeadshot = false;
                                    }
                                }
                            }

                            for (size_t i = 0; i < _currentMap->doors.size(); ++i) {
                                const auto& d = _currentMap->doors[i];
                                Vec3 animPos = d.position + d.openOffset * d.currentProgress;
                                Vec3 half = d.size * 0.5f;
                                float t = 0.0f;
                                if (Raycast::rayIntersectAABB(rayOrigin, spreadDir, animPos - half, animPos + half, t, &norm)) {
                                    if (t < hit.distance) {
                                        hit.hit = true;
                                        hit.distance = t;
                                        hit.point = rayOrigin + spreadDir * t;
                                        hit.normal = norm;
                                        hit.tag = EntityTag::Door;
                                        hit.entityIndex = (int)i;
                                        hit.isHeadshot = false;
                                    }
                                }
                            }
                        }

                        // 3. Bullet Tracer from Gun Muzzle
                        BulletTracer tr;
                        tr.start = rayOrigin + right * 0.22f - up * 0.18f + forward * 0.45f;
                        tr.end = hit.hit ? hit.point : (rayOrigin + spreadDir * def.range);
                        tr.color = def.tracerColor;
                        tr.lifetime = 0.0f;
                        tr.maxLifetime = def.tracerLifetime;
                        tr.thickness = def.tracerThickness;
                        _tracers.push_back(tr);

                        // Railgun ionized spark trail
                        if (def.id == WeaponID::Railgun) {
                            _particleSystem.spawnBeamSparks(tr.start, tr.end, def.tracerColor, 18);
                        }

                        // 4. Hit Processing & Particle Effects
                        if (hit.hit) {
                            if (hit.entityIndex >= 10000) {
                                uint32_t targetId = static_cast<uint32_t>(hit.entityIndex - 10000);
                                float dmg = hit.isHeadshot ? (def.damage * def.headshotMultiplier) : def.damage;
                                _netClient.sendDamage(targetId, dmg, static_cast<uint8_t>(def.id), hit.isHeadshot, hit.point);
                                _hud.triggerHitmarker(hit.isHeadshot);
                                _particleSystem.spawnBlood(hit.point, -spreadDir, hit.isHeadshot);
                                AudioEngine::playSound(SoundID::PlayerHurt);
                            } else if (hit.tag == EntityTag::Bot) {
                                // Crimson blood splatter spray & mist
                                _particleSystem.spawnBlood(hit.point, -spreadDir, hit.isHeadshot);
                                if (_currentMap) {
                                    Vec3 bNorm;
                                    float bestT = 5.0f;
                                    bool foundWall = false;
                                    for (const auto& b : _currentMap->brushes) {
                                        Vec3 half = b.size * 0.5f;
                                        float t = 0.0f;
                                        if (Raycast::rayIntersectAABB(hit.point + spreadDir * 0.05f, spreadDir, b.position - half, b.position + half, t, &bNorm)) {
                                            if (t < bestT) {
                                                bestT = t;
                                                foundWall = true;
                                            }
                                        }
                                    }
                                    if (foundWall) {
                                        Vec3 bloodPos = hit.point + spreadDir * bestT;
                                        _decalSystem.spawnDecal(DecalType::BloodSplatter, bloodPos, bNorm, 0.40f + (float)(rand() % 20) * 0.01f);
                                    }
                                }
                                _decalSystem.spawnDecal(DecalType::BloodSplatter, Vec3(hit.point.x, 0.02f, hit.point.z), Vec3(0, 1, 0), 0.32f);

                                for (auto& bot : _aiManager.bots) {
                                    if (bot.id == hit.entityIndex && bot.isAlive()) {
                                        float dmg = hit.isHeadshot ? (def.damage * def.headshotMultiplier) : def.damage;
                                        Vec3 knockback = spreadDir * (def.damage * 0.12f) + Vec3(0.0f, 1.2f, 0.0f);
                                        bool killed = bot.takeDamage(dmg, hit.isHeadshot, knockback);
                                        _hud.triggerHitmarker(hit.isHeadshot);

                                        if (killed) {
                                            _playerKills++;
                                            _hud.frags = _playerKills;
                                            std::string killMsg = (hit.isHeadshot ? "HEADSHOT! ELIMINATED " : "ELIMINATED ") +
                                                                  bot.name + " WITH " + def.name + " [" + std::to_string(_playerKills) + " FRAGS]";
                                            _hud.showCombatMessage(killMsg, 2.5f);
                                            _chat.addMessage("[SERVER]", killMsg, hit.isHeadshot ? Vec3(1.0f, 0.25f, 0.25f) : Vec3(0.3f, 0.9f, 0.4f));

                                            _pickups.spawnPickup(PickupType::Ammo, bot.position + Vec3(0.0f, 0.35f, 0.0f), 36);
                                            if ((rand() % 100) < 65) {
                                                _pickups.spawnPickup(PickupType::Medkit, bot.position + Vec3(0.4f, 0.35f, -0.3f), 50);
                                            }
                                            WeaponID dropWep = bot.equippedWeapon;
                                            _pickups.spawnPickup(PickupType::WeaponDrop, bot.position + Vec3(-0.35f, 0.35f, 0.2f), 30, (int)dropWep);
                                            _chat.addMessage(bot.name, "Critical damage! Unit offline...", Vec3(0.9f, 0.45f, 0.45f));
                                        }
                                        break;
                                    }
                                }
                            } else if (hit.tag == EntityTag::RigidProp) {
                                _physicsWorld.takeDamage(hit.entityIndex, def.damage, hit.point, spreadDir);
                                _particleSystem.spawnImpact(hit.point, hit.normal, SurfaceType::Wood);
                                _decalSystem.spawnDecal(DecalType::BulletHoleMetal, hit.point, hit.normal, 0.15f);
                                _hud.triggerHitmarker(false);
                            } else if (hit.tag == EntityTag::World) {
                                std::string brushTex = "";
                                if (_currentMap && hit.entityIndex >= 0 && hit.entityIndex < (int)_currentMap->brushes.size()) {
                                    brushTex = _currentMap->brushes[hit.entityIndex].texturePath;
                                }
                                SurfaceType st = getSurfaceTypeForTexture(brushTex);
                                _particleSystem.spawnImpact(hit.point, hit.normal, st);
                                if (st == SurfaceType::Metal) {
                                    _decalSystem.spawnDecal(DecalType::BulletHoleMetal, hit.point, hit.normal, 0.16f + (float)(rand() % 6) * 0.01f);
                                } else {
                                    _decalSystem.spawnDecal(DecalType::BulletHoleConcrete, hit.point, hit.normal, 0.18f + (float)(rand() % 8) * 0.01f);
                                }
                            } else if (hit.tag == EntityTag::Door) {
                                // Metal ricochet spark spray + metallic shrapnel
                                _particleSystem.spawnImpact(hit.point, hit.normal, SurfaceType::Metal);
                                _decalSystem.spawnDecal(DecalType::BulletHoleMetal, hit.point, hit.normal, 0.16f + (float)(rand() % 6) * 0.01f);
                            }
                        }
                    }
                }
                syncHudWeapon();
            } else {
                AudioEngine::playSound(SoundID::DryFire);
                _weaponSystem.setFireCooldown(0.25f);
            }
        }

        // ==================== PROJECTILE SIMULATION & AOE EXPLOSIONS ====================
        for (auto& proj : _weaponSystem.getProjectiles()) {
            if (!proj.active) continue;

            // Spawn projectile flight trail (RPG smoke/flame, Plasma cyan energy)
            _particleSystem.spawnProjectileTrail(proj.position, proj.weaponId);

            bool hitAnything = false;
            Vec3 hitPos = proj.position;

            // Collision with Bots
            for (auto& bot : _aiManager.bots) {
                if (!bot.isAlive()) continue;
                if ((proj.position - (bot.position + Vec3(0.0f, 1.0f, 0.0f))).lengthSq() < 1.0f) {
                    hitAnything = true;
                    hitPos = proj.position;
                    break;
                }
            }

            // Collision with Map brushes
            if (!hitAnything && _currentMap) {
                for (const auto& b : _currentMap->brushes) {
                    Vec3 half = b.size * 0.5f;
                    if (proj.position.x >= b.position.x - half.x && proj.position.x <= b.position.x + half.x &&
                        proj.position.y >= b.position.y - half.y && proj.position.y <= b.position.y + half.y &&
                        proj.position.z >= b.position.z - half.z && proj.position.z <= b.position.z + half.z) {
                        hitAnything = true;
                        hitPos = proj.position;
                        break;
                    }
                }
            }

            // Ground impact
            if (!hitAnything && proj.position.y <= 0.15f) {
                hitAnything = true;
                hitPos = proj.position;
            }

            if (hitAnything) {
                proj.active = false;
                _particleSystem.spawnExplosion(hitPos, proj.splashRadius, proj.color);
                _decalSystem.spawnExplosionScorch(hitPos, proj.splashRadius * 0.75f, _currentMap ? _currentMap->brushes : std::vector<MapBrush>{});
                AudioEngine::playSound3D(SoundID::RPGExplosion, hitPos);
                _physicsWorld.applyExplosionImpulse(hitPos, proj.splashRadius, 420.0f, proj.damage + proj.splashDamage);

                // Affect player if in splash radius
                float pDist = (_camera.getPosition() - hitPos).length();
                if (pDist <= proj.splashRadius) {
                    float factor = 1.0f - (pDist / proj.splashRadius);
                    float pDmg = (proj.damage * 0.5f + proj.splashDamage) * factor;
                    applyDamageToPlayer(pDmg, (proj.weaponId == WeaponID::RPG) ? "RPG Splash" : "Plasma Blast");
                }

                // Affect remote multiplayer players
                if (_netClient.isConnected()) {
                    const auto& snap = _netClient.getLatestSnapshot();
                    for (uint16_t i = 0; i < snap.playerCount; ++i) {
                        const auto& p = snap.players[i];
                        if (p.clientId == _netClient.getClientId() || !p.isAlive) continue;
                        float pDistRemote = (p.position - hitPos).length();
                        if (pDistRemote <= proj.splashRadius) {
                            float factor = 1.0f - (pDistRemote / proj.splashRadius);
                            float pDmg = (proj.damage * 0.5f + proj.splashDamage) * factor;
                            _netClient.sendDamage(p.clientId, pDmg, static_cast<uint8_t>(proj.weaponId), false, hitPos);
                            _hud.triggerHitmarker(false);
                        }
                    }
                }

                float radiusSq = proj.splashRadius * proj.splashRadius;

                for (auto& bot : _aiManager.bots) {
                    if (!bot.isAlive()) continue;
                    float dSq = (bot.position - hitPos).lengthSq();
                    if (dSq <= radiusSq) {
                        float dist = std::sqrt(dSq);
                        float factor = 1.0f - (dist / proj.splashRadius);
                        float dmg = proj.damage + proj.splashDamage * factor;
                        Vec3 bDir = (bot.position - hitPos).normalized();
                        if (bDir.lengthSq() < 0.01f) bDir = Vec3(0, 1, 0);
                        Vec3 knockback = bDir * (13.5f * factor) + Vec3(0.0f, 6.5f * factor, 0.0f);
                        bool killed = bot.takeDamage(dmg, false, knockback);
                        _hud.triggerHitmarker(false);

                        if (killed) {
                            _playerKills++;
                            _hud.frags = _playerKills;
                            std::string wName = (proj.weaponId == WeaponID::RPG) ? "RPG" : "PLASMA GUN";
                            std::string killMsg = "SPLASH KILL! " + bot.name + " WITH " + wName + " [" + std::to_string(_playerKills) + " FRAGS]";
                            _hud.showCombatMessage(killMsg, 2.5f);
                            _chat.addMessage("[SERVER]", killMsg, Vec3(1.0f, 0.45f, 0.15f));

                            _pickups.spawnPickup(PickupType::Ammo, bot.position + Vec3(0.0f, 0.35f, 0.0f), 36);
                            if ((rand() % 100) < 65) {
                                _pickups.spawnPickup(PickupType::Medkit, bot.position + Vec3(0.4f, 0.35f, -0.3f), 50);
                            }
                            WeaponID dropWep = bot.equippedWeapon;
                            _pickups.spawnPickup(PickupType::WeaponDrop, bot.position + Vec3(-0.35f, 0.35f, 0.2f), 30, (int)dropWep);
                        }
                    }
                }

                // Visual explosion shockwave
                BulletTracer tr;
                tr.start = hitPos - Vec3(0.0f, 0.2f, 0.0f);
                tr.end = hitPos + Vec3(0.0f, 0.9f, 0.0f);
                tr.color = proj.color;
                tr.lifetime = 0.0f;
                tr.maxLifetime = 0.35f;
                tr.thickness = 0.30f;
                _tracers.push_back(tr);
            }
        }

        if (_muzzleFlashTime > 0.0f) {
            _muzzleFlashTime -= time.delta;
        }

        // ==================== RIGID BODY PHYSICS & PROP DESTRUCTION ====================
        if (_currentMap) {
            auto solidBoxes = LabCollision::getMapSolidBoxes(*_currentMap);
            _physicsWorld.update(time.delta, solidBoxes);

            // Process barrel detonations & shockwaves
            for (const auto& exp : _physicsWorld.pendingExplosions) {
                _particleSystem.spawnExplosion(exp.position, exp.radius, Vec3(1.0f, 0.45f, 0.1f));
                _decalSystem.spawnExplosionScorch(exp.position, exp.radius * 0.75f, _currentMap ? _currentMap->brushes : std::vector<MapBrush>{});
                AudioEngine::playSound3D(SoundID::RPGExplosion, exp.position, 1.0f, 1.0f);

                // Affect player if in blast radius
                Vec3 playerPos = _camera.getPosition();
                float playerDist = (playerPos - exp.position).length();
                if (playerDist < exp.radius) {
                    float factor = 1.0f - (playerDist / exp.radius);
                    Vec3 pDir = (playerPos - exp.position).normalized();
                    if (pDir.lengthSq() < 0.01f) pDir = Vec3(0, 1, 0);
                    _velocity += pDir * (14.0f * factor) + Vec3(0.0f, 6.0f * factor, 0.0f);
                    float pDmg = exp.maxDamage * factor * _scriptEngine.getBarrelDamageMultiplier();
                    applyDamageToPlayer(pDmg, "Explosive Barrel");
                }

                // Affect bots
                for (auto& bot : _aiManager.bots) {
                    if (!bot.isAlive()) continue;
                    float bDist = (bot.position - exp.position).length();
                    if (bDist < exp.radius) {
                        float factor = 1.0f - (bDist / exp.radius);
                        Vec3 bDir = (bot.position - exp.position).normalized();
                        if (bDir.lengthSq() < 0.01f) bDir = Vec3(0, 1, 0);
                        Vec3 knockback = bDir * (exp.maxImpulse * 0.035f * factor) + Vec3(0.0f, 6.0f * factor, 0.0f);
                        bool killed = bot.takeDamage(exp.maxDamage * factor, false, knockback);
                        if (killed) {
                            _playerKills++;
                            _hud.frags = _playerKills;
                            std::string killMsg = "BARREL EXPLOSION! " + bot.name + " [" + std::to_string(_playerKills) + " FRAGS]";
                            _hud.showCombatMessage(killMsg, 2.5f);
                            _chat.addMessage("[SERVER]", killMsg, Vec3(1.0f, 0.45f, 0.15f));
                        }
                    }
                }
            }
            _physicsWorld.pendingExplosions.clear();
        }

        // ==================== COMBAT AI BOTS UPDATE & RETALIATION ====================
        float botDamageToPlayer = 0.0f;
        if (_currentMap && !_devConsole.ai_disable) {
            // Check Story / Gameplay Trigger Volumes against player position
            auto firedTriggers = LabCollision::checkTriggerVolumes(_camera.getPosition(), *_currentMap);
            for (int trigIdx : firedTriggers) {
                if (trigIdx >= 0 && trigIdx < (int)_currentMap->triggers.size()) {
                    const auto& trig = _currentMap->triggers[trigIdx];
                    _aiManager.triggerStoryEventOnBot(trig.targetBot, trig.dialogText, trig.audioPath, *_currentMap);
                    if (!trig.dialogText.empty()) {
                        _hud.showCombatMessage(trig.dialogText, 4.5f);
                        _chat.addMessage("[" + (trig.targetBot.empty() ? "CorridorBot" : trig.targetBot) + "]", trig.dialogText, Vec3(0.3f, 0.9f, 1.0f));
                    }
                }
            }

            int pTeam = (_sessionConfig.mode == GameMode::TDM) ? 1 : -1;
            bool targetable = !_isPlayerDead && !_devConsole.ai_ignore_player;
            _aiManager.update(time.delta, _camera.getPosition(), targetable, pTeam, *_currentMap, _tracers, botDamageToPlayer, &_pickups, &_chat);
        }

        if (botDamageToPlayer > 0.0f) {
            applyDamageToPlayer(botDamageToPlayer, "Combat Synth");
        }

        // ==================== LOCAL PLAYER SKELETAL ANIMATOR UPDATE ====================
        if (!_playerAnimator.getSkeleton() && _aiManager.skeleton && !_aiManager.animations.empty()) {
            _playerAnimator.setSkeleton(_aiManager.skeleton);
            for (const auto& clip : _aiManager.animations) {
                _playerAnimator.addClip(clip);
            }
            _playerAnimator.playAnimation("Idle", true);
        }

        if (_playerAnimator.getSkeleton()) {
            float hSpeed = std::sqrt(_velocity.x * _velocity.x + _velocity.z * _velocity.z);
            if (_isPlayerDead) {
                _playerAnimator.playAnimation("Death", false, 0.1f);
            } else if (_playerShootTimer > 0.0f) {
                _playerShootTimer -= time.delta;
                const auto& curWep = _weaponSystem.getActiveWeapon();
                if (curWep.def.isMelee) {
                    _playerAnimator.playAnimation("Heavy Weapon Swing", false, 0.05f);
                } else {
                    _playerAnimator.playAnimation("Firing Rifle", false, 0.05f);
                }
            } else if (!_isGrounded) {
                _playerAnimator.playAnimation("Walk", true, 0.2f);
            } else if (hSpeed > 5.5f) {
                _playerAnimator.playAnimation("Run Forward", true, 0.15f);
            } else if (hSpeed > 0.3f) {
                _playerAnimator.playAnimation("Walking", true, 0.15f);
            } else {
                _playerAnimator.playAnimation("Idle", true, 0.2f);
            }
            _playerAnimator.update(time.delta);
        }

        // Update Pickups and Proximity Collection
        int ammoAdded = 0;
        float healthAdded = 0.0f;
        int weaponUnlocked = -1;
        std::string pickupNotice;
        bool weaponRespawned = false;
        _pickups.update(time.delta, _camera.getPosition(), ammoAdded, healthAdded, weaponUnlocked, pickupNotice, weaponRespawned);
        if (weaponUnlocked >= 0 && weaponUnlocked < 9) {
            _weaponSystem.unlockWeapon((WeaponID)weaponUnlocked, true);
            _weaponSystem.switchWeapon((WeaponID)weaponUnlocked);
            syncHudWeapon();
            _hud.showCombatMessage(pickupNotice, 2.5f);
            _chat.addMessage("[ARSENAL]", pickupNotice, Vec3(0.3f, 0.85f, 1.0f));
            _particleSystem.spawnBeamSparks(_camera.getPosition(), _camera.getPosition() + Vec3(0, 1.2f, 0), Vec3(0.2f, 0.85f, 1.0f), 16);
            AudioEngine::playSound(SoundID::WeaponSpawn);
        }
        if (weaponRespawned) {
            _chat.addMessage("[ARSENAL]", "A weapon has respawned on the arena pad!", Vec3(0.3f, 0.7f, 0.9f));
            AudioEngine::playSound(SoundID::WeaponSpawn);
        }
        if (ammoAdded > 0) {
            _weaponSystem.getActiveWeapon().addAmmo(ammoAdded);
            syncHudWeapon();
            if (weaponUnlocked < 0) {
                _hud.showCombatMessage(pickupNotice, 2.0f);
                _chat.addMessage("[ITEM]", pickupNotice, Vec3(0.95f, 0.82f, 0.15f));
            }
            AudioEngine::playSound(SoundID::PickupAmmo);
        }
        if (healthAdded > 0.0f) {
            _hud.health = std::min(_hud.maxHealth, _hud.health + healthAdded);
            _hud.showCombatMessage(pickupNotice, 2.0f);
            _chat.addMessage("[ITEM]", pickupNotice, Vec3(0.2f, 0.95f, 0.4f));
            AudioEngine::playSound(SoundID::PickupMedkit);
        }

        // Update active Bullet Tracers
        for (auto it = _tracers.begin(); it != _tracers.end(); ) {
            it->lifetime += time.delta;
            if (it->isExpired()) it = _tracers.erase(it);
            else ++it;
        }

        // Weapon Reload (R key when not in Hammer Editor)
        if (Input::isKeyPressed('R') || Input::isKeyPressed('r')) {
            if (!_hammerEditor.active && !_chat.isOpen && !_isPlayerDead) {
                auto& curWep = _weaponSystem.getActiveWeapon();
                if (curWep.canRefill() && !_weaponAnimator.isReloading()) {
                    curWep.refillClip();
                    syncHudWeapon();
                    _weaponAnimator.onReload(1.8f);
                    AudioEngine::playSound(SoundID::Refill);
                }
            }
        }

        // Tactical Flashlight (F key - Half-Life 2 style toggleable spotlight)
        if (Input::isKeyPressed('F') || Input::isKeyPressed('f')) {
            if (!_hammerEditor.active && !_chat.isOpen && !_isPlayerDead && !_fPressedLast) {
                _flashlight.toggle();
                AudioEngine::playSound(SoundID::FlashlightToggle);
                _hud.showCombatMessage(_flashlight.enabled ? "FLASHLIGHT: ACTIVATED" : "FLASHLIGHT: DEACTIVATED", 1.5f);
                _fPressedLast = true;
            }
        } else {
            _fPressedLast = false;
        }

        // Weapon Inspect (V key when not in Hammer Editor)
        if (Input::isKeyPressed('V') || Input::isKeyPressed('v')) {
            if (!_hammerEditor.active && !_chat.isOpen && !_isPlayerDead && !_vPressedLast) {
                if (!_weaponAnimator.isReloading() && !_weaponAnimator.isInspecting()) {
                    _weaponAnimator.onInspect(2.4f);
                }
                _vPressedLast = true;
            }
        } else {
            _vPressedLast = false;
        }

        // Update tactical flashlight beam kinematics
        _flashlight.update(_camera.getPosition(), _camera.getFront(), _camera.getRight(), _camera.getUp());

        // Procedural Weapon Sway and Bob update
        float horizontalSpeed = std::sqrt(_velocity.x * _velocity.x + _velocity.z * _velocity.z);
        _weaponAnimator.update(time.delta, Input::mouseDelta, _devConsole.cam_bob ? horizontalSpeed : 0.0f);

        // Feed real dynamic player values to HUD
        if (_currentMap) _hud.mapName = _currentMap->metadata.name;
        _hud.update(time.delta);

        // F2 Hammer Editor Toggle
        if (Input::isKeyPressed(291)) { // GLFW_KEY_F2
            if (!_f2PressedLast) {
                _hammerEditor.toggle(_camera);
                LabLog::info("Lab Hammer Editor: " + std::string(_hammerEditor.active ? "OPENED" : "CLOSED"));
                _f2PressedLast = true;
            }
        } else {
            _f2PressedLast = false;
        }

        // Hammer Editor Controls when active
        if (_hammerEditor.active && _currentMap) {
            _hammerEditor.update(time.delta, _camera, *_currentMap);

            // E key: Place object (Brush, Light, or Door)
            if (Input::isKeyPressed('E') || Input::isKeyPressed('e')) {
                if (!_ePressedLast) {
                    if (_hammerEditor.currentTool == EditorTool::CreateBrush) {
                        _hammerEditor.placeBrush(*_currentMap);
                    } else if (_hammerEditor.currentTool == EditorTool::CreateLight) {
                        _hammerEditor.placeLight(*_currentMap);
                    } else if (_hammerEditor.currentTool == EditorTool::CreateDoor) {
                        _hammerEditor.placeDoor(*_currentMap);
                    }
                    _ePressedLast = true;
                }
            } else {
                _ePressedLast = false;
            }

            // R key: Toggle Tool (Brush -> Light -> Door -> Brush)
            if (Input::isKeyPressed('R') || Input::isKeyPressed('r')) {
                if (!_rPressedLast) {
                    if (_hammerEditor.currentTool == EditorTool::CreateBrush) {
                        _hammerEditor.currentTool = EditorTool::CreateLight;
                        _hammerEditor.statusMessage = "Hammer Tool: Point Light [E: Place, R: Cycle]";
                    } else if (_hammerEditor.currentTool == EditorTool::CreateLight) {
                        _hammerEditor.currentTool = EditorTool::CreateDoor;
                        _hammerEditor.statusMessage = "Hammer Tool: Dynamic Door [E: Place, R: Cycle]";
                    } else {
                        _hammerEditor.currentTool = EditorTool::CreateBrush;
                        _hammerEditor.statusMessage = "Hammer Tool: Geometry Brush [E: Place, R: Cycle]";
                    }
                    _rPressedLast = true;
                }
            } else {
                _rPressedLast = false;
            }

            // L key: Toggle Darkness Mode (Pitch black ambient/sun)
            if (Input::isKeyPressed('L') || Input::isKeyPressed('l')) {
                if (!_lPressedLast) {
                    _hammerEditor.toggleDarkness();
                    _lPressedLast = true;
                }
            } else {
                _lPressedLast = false;
            }

            // K key: Quick Save Map to assets/maps/hammer_export.labmap
            if (Input::isKeyPressed('K') || Input::isKeyPressed('k')) {
                if (!_kPressedLast) {
                    _hammerEditor.saveMap(*_currentMap, "assets/maps/hammer_export.labmap");
                    _kPressedLast = true;
                }
            } else {
                _kPressedLast = false;
            }

            // Backspace / Delete: Delete last placed brush or point light
            if (Input::isKeyPressed(259)) { // GLFW_KEY_BACKSPACE
                if (!_backspacePressedLast) {
                    _hammerEditor.deleteLast(*_currentMap);
                    _backspacePressedLast = true;
                }
            } else {
                _backspacePressedLast = false;
            }
        }

        // F3 Debug Mode Toggle
        if (Input::isKeyPressed(292)) { // GLFW_KEY_F3
            if (!_f3PressedLast) {
                _debugMode = !_debugMode;
                _weaponSystem.unlockAll();
                syncHudWeapon();
                LabLog::info("Debug mode: " + std::string(_debugMode ? "ON" : "OFF") + " (All weapons unlocked)");
                _chat.addMessage("[SYSTEM]", "Debug mode ON: All 9 weapons unlocked!", Vec3(0.3f, 0.95f, 0.5f));
                _f3PressedLast = true;
            }
        } else {
            _f3PressedLast = false;
        }

        // Interactive In-World Systems & Usable Entities ([E] Key)
        if (!_hammerEditor.active && !_inMenu && !_isPlayerDead) {
            bool useKey = (Input::isKeyPressed('E') || Input::isKeyPressed('e'));
            bool useJustPressed = false;
            if (useKey) {
                if (!_ePressedLast) {
                    useJustPressed = true;
                    _ePressedLast = true;
                }
            } else {
                _ePressedLast = false;
            }
            _interactiveSystem.update(time.delta, _camera.getPosition(), _camera.getFront(), useJustPressed, _currentMap.get());
        }

        // F1 Wireframe Toggle
        if (Input::isKeyPressed(290)) { // GLFW_KEY_F1
            if (!_f1PressedLast) {
                _wireframeMode = !_wireframeMode;
                glPolygonMode(GL_FRONT_AND_BACK, _wireframeMode ? GL_LINE : GL_FILL);
                LabLog::info("Wireframe mode: " + std::string(_wireframeMode ? "ON" : "OFF"));
                _f1PressedLast = true;
            }
        } else {
            _f1PressedLast = false;
        }
    }

    void handleMenuInput(float mx, float my, bool lmbClick) {
        // Keyboard shortcuts in menu
        if (Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
            if (!_escPressedLast) {
                if (_menuScreen == MenuScreen::HostGame || _menuScreen == MenuScreen::JoinGame) {
                    _menuScreen = MenuScreen::MultiSelect;
                } else if (_menuScreen == MenuScreen::MultiSelect || _menuScreen == MenuScreen::Singleplayer || _menuScreen == MenuScreen::CharacterStudio) {
                    _menuScreen = MenuScreen::Main;
                } else if (_menuScreen == MenuScreen::Main && _currentMap) {
                    _inMenu = false;
                    _fireLmbLast = true;
                    setCursorCaptured(true);
                }
                _escPressedLast = true;
            }
        } else {
            _escPressedLast = false;
        }

        if (_menuScreen == MenuScreen::CharacterStudio) {
            return;
        }

        if (_menuScreen == MenuScreen::Main) {
            if (lmbClick) {
                // Button 0: Campaign / Singleplayer (x: 120..500, y: 185..233)
                if (mx >= 120.0f && mx <= 500.0f && my >= 185.0f && my <= 233.0f) {
                    LabLog::info("[FrozenLife] Menu: Campaign selected");
                    _menuScreen = MenuScreen::Singleplayer;
                    return;
                }
                // Button 1: Multiplayer (x: 120..500, y: 245..293)
                if (mx >= 120.0f && mx <= 500.0f && my >= 245.0f && my <= 293.0f) {
                    LabLog::info("[FrozenLife] Menu: Multiplayer selected");
                    _menuScreen = MenuScreen::MultiSelect;
                    return;
                }
                // Button 2: Character & Weapon Studio (x: 120..500, y: 305..353)
                if (mx >= 120.0f && mx <= 500.0f && my >= 305.0f && my <= 353.0f) {
                    LabLog::info("[FrozenLife] Menu: Character Studio selected");
                    _menuScreen = MenuScreen::CharacterStudio;
                    return;
                }
                // Button 3: Resume Mission (x: 120..500, y: 365..413)
                if (_currentMap && mx >= 120.0f && mx <= 500.0f && my >= 365.0f && my <= 413.0f) {
                    LabLog::info("[FrozenLife] Menu: Resume Mission selected");
                    _inMenu = false;
                    _fireLmbLast = true;
                    setCursorCaptured(true);
                    return;
                }
                // Button 4: Quit Game (x: 120..500, y: 425..473)
                if (mx >= 120.0f && mx <= 500.0f && my >= 425.0f && my <= 473.0f) {
                    LabLog::info("[FrozenLife] Menu: Quit Game selected");
                    stop();
                    return;
                }
            }
        }
        else if (_menuScreen == MenuScreen::MultiSelect) {
            if (lmbClick) {
                // Card 1: Host Game / Create Server (x: 140..600, y: 180..420)
                if (mx >= 140.0f && mx <= 600.0f && my >= 180.0f && my <= 420.0f) {
                    LabLog::info("[FrozenLife] Menu: Host Game selected");
                    _menuScreen = MenuScreen::HostGame;
                    return;
                }
                // Card 2: Find Servers / Join Game (x: 680..1140, y: 180..420)
                if (mx >= 680.0f && mx <= 1140.0f && my >= 180.0f && my <= 420.0f) {
                    LabLog::info("[FrozenLife] Menu: Join Game selected");
                    _menuScreen = MenuScreen::JoinGame;
                    return;
                }
                // Back Button (x: 140..380, y: 580..628)
                if (mx >= 140.0f && mx <= 380.0f && my >= 580.0f && my <= 628.0f) {
                    LabLog::info("[FrozenLife] Menu: Back to Main Menu selected");
                    _menuScreen = MenuScreen::Main;
                    return;
                }
            }
        }
        else if (_menuScreen == MenuScreen::HostGame) {
            // Text typing for friend punch target IP
            if (_punchTargetActive) {
                for (int k = 0; k < 512; ++k) {
                    if (Input::keys[k] && !_lastMenuKeys[k]) {
                        if (k == 259) { // Backspace
                            if (!_punchTargetIP.empty()) _punchTargetIP.pop_back();
                        } else if (k == 'V' && (Input::isKeyPressed(GLFW_KEY_LEFT_CONTROL) || Input::isKeyPressed(GLFW_KEY_RIGHT_CONTROL))) {
                            std::string clip = getClipboardText();
                            if (!clip.empty()) _punchTargetIP = clip;
                        } else if (k >= 32 && k <= 126) {
                            if (_punchTargetIP.size() < 45) {
                                _punchTargetIP.push_back(static_cast<char>(k));
                            }
                        }
                    }
                    _lastMenuKeys[k] = Input::keys[k];
                }
            }

            // O key: Open native Windows File Dialog for scene
            if (Input::isKeyPressed('O') || Input::isKeyPressed('o')) {
                if (!_oPressedLast) {
                    std::string picked = LabDialogs::openFileDialog(getWindow(), "Lab Scene Files (*.labscene)\0*.labscene\0Lab Map Files (*.labmap)\0*.labmap\0", "assets\\maps");
                    if (!picked.empty() && LabDialogs::isAllowedSceneFile(picked)) {
                        _sessionConfig.mapPath = toSceneDisplayPath(picked);
                    }
                    _oPressedLast = true;
                }
            } else {
                _oPressedLast = false;
            }

            if (lmbClick) {
                // Left Panel: Map items (x: 115..525, y: 170 + i * 50)
                for (int i = 0; i < (int)_availableMaps.size(); ++i) {
                    float iy = 170.0f + i * 50.0f;
                    if (mx >= 115.0f && mx <= 525.0f && my >= iy && my <= iy + 42.0f) {
                        _selectedMapIndex = i;
                        _sessionConfig.mapPath = _availableMaps[i];
                        LabLog::info("[FrozenLife] HostGame: Selected map " + _availableMaps[i]);
                        return;
                    }
                }

                // Open from disk button (x: 115..525, y: 485..527)
                if (mx >= 115.0f && mx <= 525.0f && my >= 485.0f && my <= 527.0f) {
                    std::string picked = LabDialogs::openFileDialog(getWindow(), "Lab Scene Files (*.labscene)\0*.labscene\0Lab Map Files (*.labmap)\0*.labmap\0", "assets\\maps");
                    if (!picked.empty() && LabDialogs::isAllowedSceneFile(picked)) {
                        _sessionConfig.mapPath = toSceneDisplayPath(picked);
                        LabLog::info("[FrozenLife] HostGame: Picked disk scene " + _sessionConfig.mapPath);
                    }
                    return;
                }

                // 1. Network Mode toggle: LAN (580..855) vs P2P (870..1160) at y: 182..220
                if (my >= 182.0f && my <= 220.0f) {
                    if (mx >= 580.0f && mx <= 855.0f) { _sessionConfig.networkMode = NetworkMode::LAN; LabLog::info("[FrozenLife] HostGame: Network LAN"); return; }
                    if (mx >= 870.0f && mx <= 1160.0f) { _sessionConfig.networkMode = NetworkMode::P2P; LabLog::info("[FrozenLife] HostGame: Network P2P"); return; }
                }

                // 2. Game Mode buttons: FFA (580..760), DM (770..950), TDM (960..1160) at y: 248..284
                if (my >= 248.0f && my <= 284.0f) {
                    if (mx >= 580.0f && mx <= 760.0f) { _sessionConfig.mode = GameMode::FFA; LabLog::info("[FrozenLife] HostGame: Mode FFA"); return; }
                    if (mx >= 770.0f && mx <= 950.0f) { _sessionConfig.mode = GameMode::DM; LabLog::info("[FrozenLife] HostGame: Mode DM"); return; }
                    if (mx >= 960.0f && mx <= 1160.0f) { _sessionConfig.mode = GameMode::TDM; LabLog::info("[FrozenLife] HostGame: Mode TDM"); return; }
                }

                // 3. Combat AI Bots toggle (x: 580..855, y: 314..350)
                if (mx >= 580.0f && mx <= 855.0f && my >= 314.0f && my <= 350.0f) {
                    _sessionConfig.enableBots = !_sessionConfig.enableBots;
                    LabLog::info(std::string("[FrozenLife] HostGame: Bots ") + (_sessionConfig.enableBots ? "Enabled" : "Disabled"));
                    return;
                }

                // Bot count [-] (875..920) and [+] (1066..1111) at y: 314..350
                if (my >= 314.0f && my <= 350.0f) {
                    if (mx >= 875.0f && mx <= 920.0f) { _sessionConfig.botCount = std::max(0, _sessionConfig.botCount - 1); LabLog::info("[FrozenLife] HostGame: Bot count " + std::to_string(_sessionConfig.botCount)); return; }
                    if (mx >= 1066.0f && mx <= 1111.0f) { _sessionConfig.botCount = std::min(8, _sessionConfig.botCount + 1); LabLog::info("[FrozenLife] HostGame: Bot count " + std::to_string(_sessionConfig.botCount)); return; }
                }

                // 4. Frag Limit [-] (580..625) and [+] (771..816) at y: 380..416
                if (my >= 380.0f && my <= 416.0f) {
                    if (mx >= 580.0f && mx <= 625.0f) { _sessionConfig.fragLimit = std::max(5, _sessionConfig.fragLimit - 5); LabLog::info("[FrozenLife] HostGame: Frag limit " + std::to_string(_sessionConfig.fragLimit)); return; }
                    if (mx >= 771.0f && mx <= 816.0f) { _sessionConfig.fragLimit = std::min(100, _sessionConfig.fragLimit + 5); LabLog::info("[FrozenLife] HostGame: Frag limit " + std::to_string(_sessionConfig.fragLimit)); return; }
                }

                // Copy Invite / IP button (x: 580..1160, y: 476..514)
                if (mx >= 580.0f && mx <= 1160.0f && my >= 476.0f && my <= 514.0f) {
                    std::string inviteIP = _hostNATStatus.publicIP.empty() ? _hostNATStatus.localIP : _hostNATStatus.publicIP;
                    std::string inviteStr = inviteIP + ":" + std::to_string(_sessionConfig.port);
                    setClipboardText(inviteStr);
                    _chat.addMessage("[INVITE]", "Copied host address to clipboard: " + inviteStr, Vec3(0.3f, 0.9f, 0.4f));
                    _punchStatusMsg = "Copied host address (" + inviteStr + ") to clipboard!";
                    return;
                }

                // Plug & Play: Copy Room Code button (x: 820..980, y: 518..556)
                if (mx >= 820.0f && mx <= 980.0f && my >= 518.0f && my <= 556.0f) {
                    setClipboardText(_pnpRoomCode);
                    _punchStatusMsg = "Copied Room Code [" + _pnpRoomCode + "] to clipboard! Share with friend.";
                    return;
                }

                // Plug & Play: Generate New Room Code button (x: 990..1160, y: 518..556)
                if (mx >= 990.0f && mx <= 1160.0f && my >= 518.0f && my <= 556.0f) {
                    _pnpRoomCode = PnPBroker::generateRoomCode();
                    _sessionConfig.roomCode = _pnpRoomCode;
                    _punchStatusMsg = "Generated new Room Code: [" + _pnpRoomCode + "]!";
                    return;
                }

                // 5. Bottom Back button (x: 100..300, y: 570..618)
                if (mx >= 100.0f && mx <= 300.0f && my >= 570.0f && my <= 618.0f) {
                    LabLog::info("[FrozenLife] HostGame: Back selected");
                    _menuScreen = MenuScreen::MultiSelect;
                    return;
                }

                // 6. Bottom Start Server button (x: 740..1180, y: 570..618)
                if (mx >= 740.0f && mx <= 1180.0f && my >= 570.0f && my <= 618.0f) {
                    LabLog::info("[FrozenLife] HostGame: Starting Server and Session");
                    ServerConfig srvCfg;
                    srvCfg.port = _sessionConfig.port;
                    srvCfg.roomCode = _pnpRoomCode;
                    srvCfg.serverName = (_sessionConfig.networkMode == NetworkMode::LAN)
                        ? "Lab LAN Dedicated Arena"
                        : "Lab P2P Dedicated Arena";
                    srvCfg.mapName = _sessionConfig.mapPath;
                    srvCfg.gameMode = (_sessionConfig.mode == GameMode::FFA ? "FFA" : (_sessionConfig.mode == GameMode::DM ? "DM" : "TDM"));
                    srvCfg.enableBots = _sessionConfig.enableBots;
                    srvCfg.botCount = _sessionConfig.botCount;
                    srvCfg.fragLimit = _sessionConfig.fragLimit;
                    srvCfg.maxPlayers = 16;
                    srvCfg.tickrate = 64;
                    srvCfg.networkMode = (_sessionConfig.networkMode == NetworkMode::LAN) ? NetMatchMode::LAN : NetMatchMode::P2P;
                    srvCfg.lanMode = (_sessionConfig.networkMode == NetworkMode::LAN);

                    // Strictly save to assets/configs/server.cfg
                    srvCfg.saveToFile("assets/configs/server.cfg");

                    // Publish to Cloud Broker for zero-config Plug & Play
                    std::string pubIP = NATDiscovery::discoverPublicIP(srvCfg.port);
                    std::string lanIP = NATDiscovery::getLocalIP();
                    PnPBroker::publishRoom(_pnpRoomCode, pubIP, srvCfg.port, lanIP, srvCfg.port, srvCfg.mapName, srvCfg.gameMode);

                    _localServer.stop();
                    _isLocalHost = true;

                    std::string spawnErr;
                    bool ok = launchDedicatedServer(srvCfg, spawnErr);
                    if (ok) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));
                        _netClient.connect("127.0.0.1", srvCfg.port, "HostPlayer");
                        startSession(_sessionConfig);
                        _chat.addMessage("[SERVER]", "Started LabServer.exe using assets/configs/server.cfg (Port " + std::to_string(srvCfg.port) + ")", Vec3(0.2f, 0.9f, 0.35f));
                        _chat.addMessage("[CONFIG]", std::string("Network Mode: ") + (_sessionConfig.networkMode == NetworkMode::LAN ? "LAN (Broadcast)" : "P2P (Direct/Relay)"), Vec3(0.3f, 0.85f, 1.0f));
                    } else {
                        _chat.addMessage("[SERVER]", "LabServer spawn failed (" + spawnErr + "). Running internal server.", Vec3(0.95f, 0.75f, 0.2f));
                        _localServer.start(srvCfg);
                        _netClient.connect("127.0.0.1", srvCfg.port, "HostPlayer");
                        startSession(_sessionConfig);
                    }
                    return;
                }
            }
        }
        else if (_menuScreen == MenuScreen::JoinGame) {
            // Text typing for direct connect target address or plug & play room code
            for (int k = 0; k < 512; ++k) {
                if (Input::keys[k] && !_lastMenuKeys[k]) {
                    if (_pnpJoinCodeActive) {
                        if (k == 259) { // Backspace
                            if (!_pnpJoinCode.empty()) _pnpJoinCode.pop_back();
                        } else if (k == 'V' && (Input::isKeyPressed(GLFW_KEY_LEFT_CONTROL) || Input::isKeyPressed(GLFW_KEY_RIGHT_CONTROL))) {
                            std::string clip = getClipboardText();
                            if (!clip.empty()) _pnpJoinCode = clip;
                        } else if (k >= 32 && k <= 126) {
                            if (_pnpJoinCode.size() < 12) {
                                _pnpJoinCode.push_back(static_cast<char>(k));
                            }
                        }
                    } else if (_directConnectActive) {
                        if (k == 259) { // Backspace
                            if (!_directConnectIP.empty()) _directConnectIP.pop_back();
                        } else if (k == 'V' && (Input::isKeyPressed(GLFW_KEY_LEFT_CONTROL) || Input::isKeyPressed(GLFW_KEY_RIGHT_CONTROL))) {
                            std::string clip = getClipboardText();
                            if (!clip.empty()) _directConnectIP = clip;
                        } else if (k >= 32 && k <= 126) {
                            if (_directConnectIP.size() < 45) {
                                _directConnectIP.push_back(static_cast<char>(k));
                            }
                        }
                    }
                }
                _lastMenuKeys[k] = Input::keys[k];
            }

            if (lmbClick) {
                // 0A. Click Room Code input field (x: 110..370, y: 116..154)
                if (mx >= 110.0f && mx <= 370.0f && my >= 116.0f && my <= 154.0f) {
                    _pnpJoinCodeActive = true;
                    _directConnectActive = false;
                    return;
                }

                // 0B. Click PASTE Room Code button (x: 380..470, y: 116..154)
                if (mx >= 380.0f && mx <= 470.0f && my >= 116.0f && my <= 154.0f) {
                    std::string clip = getClipboardText();
                    if (!clip.empty()) _pnpJoinCode = clip;
                    return;
                }

                // 0C. Click JOIN VIA ROOM CODE (PLUG & PLAY) button (x: 485..865, y: 116..154)
                if (mx >= 485.0f && mx <= 865.0f && my >= 116.0f && my <= 154.0f) {
                    if (_pnpJoinCode.empty()) {
                        _connectStatusMsg = "Please enter 4-digit Room Code first!";
                        return;
                    }
                    _isConnecting = true;
                    _connectAnimTimer = 0.0f;
                    _connectStatusMsg = "LOOKING UP ROOM [" + _pnpJoinCode + "] ON CLOUD BROKER...";

                    PnPRoomInfo room;
                    if (PnPBroker::fetchRoom(_pnpJoinCode, room) && room.valid) {
                        std::string myPub = NATDiscovery::discoverPublicIP(27016);
                        std::string targetIP = room.publicIP;
                        uint16_t targetPort = room.publicPort;

                        // Automatic LAN detection: if my public IP equals host's public IP, we are in the same home!
                        if (!room.lanIP.empty() && (myPub == room.publicIP || room.publicIP == "127.0.0.1" || room.publicIP.empty())) {
                            targetIP = room.lanIP;
                            targetPort = room.lanPort;
                            _connectStatusMsg = "SAME NETWORK DETECTED! JOINING VIA LAN: " + targetIP;
                        } else {
                            PnPBroker::registerGuestPunch(_pnpJoinCode, myPub, 27016);
                            NATDiscovery::punchHole(room.publicIP, room.publicPort, 27016);
                            _connectStatusMsg = "FOUND HOST AT " + targetIP + ":" + std::to_string(targetPort) + " (PLUG & PLAY)!";
                        }
                        _directConnectIP = targetIP + ":" + std::to_string(targetPort);
                        _netClient.connect(targetIP, targetPort, "GuestPlayer");
                    } else {
                        _isConnecting = false;
                        _connectStatusMsg = "ROOM [" + _pnpJoinCode + "] NOT FOUND! Ensure host clicked START GAME.";
                    }
                    return;
                }

                // COPY MY IP button (x: 1050..1160, y: 72..100)
                if (mx >= 1050.0f && mx <= 1160.0f && my >= 72.0f && my <= 100.0f) {
                    std::string myIP = (_guestNATStatus.publicIP.empty() ? "127.0.0.1" : _guestNATStatus.publicIP) + ":27016";
                    setClipboardText(myIP);
                    _connectStatusMsg = "Copied your IP (" + myIP + ") to clipboard! Send to Host to punch firewall.";
                    return;
                }

                const auto& servers = _serverBrowser.getServers();

                // 1. Select server row
                for (size_t i = 0; i < servers.size() && i < 6; ++i) {
                    float rowY = 168.0f + static_cast<float>(i) * 52.0f;
                    if (mx >= 110.0f && mx <= 1170.0f && my >= rowY && my <= rowY + 46.0f) {
                        _selectedServerIndex = static_cast<int>(i);
                        _directConnectIP = servers[i].ip + ":" + std::to_string(servers[i].port);
                        return;
                    }
                }

                // 2. Direct Connect input box (x: 110..800, y: 480..524)
                if (mx >= 110.0f && mx <= 800.0f && my >= 480.0f && my <= 524.0f) {
                    _directConnectActive = true;
                    _pnpJoinCodeActive = false;
                    return;
                }

                // 3. PASTE button (x: 815..940, y: 480..524)
                if (mx >= 815.0f && mx <= 940.0f && my >= 480.0f && my <= 524.0f) {
                    std::string clip = getClipboardText();
                    if (!clip.empty()) {
                        _directConnectIP = clip;
                    }
                    return;
                }

                // 4. CLEAR button (x: 955..1060, y: 480..524)
                if (mx >= 955.0f && mx <= 1060.0f && my >= 480.0f && my <= 524.0f) {
                    _directConnectIP.clear();
                    return;
                }

                // 5. Refresh button (x: 620..800, y: 560..608)
                if (mx >= 620.0f && mx <= 800.0f && my >= 560.0f && my <= 608.0f) {
                    _serverBrowser.refresh();
                    return;
                }

                // 6. Connect to Server button (x: 830..1170, y: 560..608)
                if (mx >= 830.0f && mx <= 1170.0f && my >= 560.0f && my <= 608.0f) {
                    if (_isConnecting) {
                        _isConnecting = false;
                        _netClient.disconnect();
                        _connectStatusMsg = "Connection cancelled by user.";
                        return;
                    }

                    _localServer.stop();
#ifdef _WIN32
                    if (_hasSpawnedServer && _dedicatedServerPI.hProcess) {
                        TerminateProcess(_dedicatedServerPI.hProcess, 0);
                        CloseHandle(_dedicatedServerPI.hProcess);
                        CloseHandle(_dedicatedServerPI.hThread);
                        _hasSpawnedServer = false;
                    }
#endif
                    _isLocalHost = false;
                    std::string targetIP = "127.0.0.1";
                    uint16_t targetPort = DEFAULT_SERVER_PORT;

                    if (!_directConnectIP.empty()) {
                        size_t colon = _directConnectIP.find(':');
                        if (colon != std::string::npos) {
                            targetIP = _directConnectIP.substr(0, colon);
                            try { targetPort = static_cast<uint16_t>(std::stoi(_directConnectIP.substr(colon + 1))); } catch (...) {}
                        } else {
                            targetIP = _directConnectIP;
                        }
                    } else if (!servers.empty() && _selectedServerIndex >= 0 && _selectedServerIndex < (int)servers.size()) {
                        targetIP = servers[_selectedServerIndex].ip;
                        targetPort = servers[_selectedServerIndex].port;
                    }

                    _isConnecting = true;
                    _connectAnimTimer = 0.0f;
                    _connectStatusMsg = "CONNECTING TO " + targetIP + ":" + std::to_string(targetPort) + "...";
                    _netClient.connect(targetIP, targetPort, "GuestPlayer");
                    return;
                }

                // 7. Back button (x: 110..290, y: 560..608)
                if (mx >= 110.0f && mx <= 290.0f && my >= 560.0f && my <= 608.0f) {
                    if (_isConnecting) {
                        _isConnecting = false;
                        _netClient.disconnect();
                        _connectStatusMsg = "";
                    }
                    _menuScreen = MenuScreen::MultiSelect;
                    return;
                }
            }
        }
        else if (_menuScreen == MenuScreen::Singleplayer) {
            if (lmbClick) {
                // Map list click (x: 140..940, y: 130 + i * 55)
                for (int i = 0; i < (int)_availableMaps.size(); ++i) {
                    float iy = 130.0f + i * 55.0f;
                    if (mx >= 140.0f && mx <= 940.0f && my >= iy && my <= iy + 48.0f) {
                        _selectedMapIndex = i;
                        _sessionConfig.mapPath = _availableMaps[i];
                        _sessionConfig.enableBots = false; // Solo singleplayer exploration
                        return;
                    }
                }

                // Launch Map (x: 140..370, y: 560..608)
                if (mx >= 140.0f && mx <= 370.0f && my >= 560.0f && my <= 608.0f) {
                    if (!_availableMaps.empty() && _selectedMapIndex < (int)_availableMaps.size()) {
                        _sessionConfig.mapPath = _availableMaps[_selectedMapIndex];
                        _sessionConfig.enableBots = false;
                        startSession(_sessionConfig);
                    }
                    return;
                }
                // Open from disk (x: 390..660, y: 560..608)
                if (mx >= 390.0f && mx <= 660.0f && my >= 560.0f && my <= 608.0f) {
                    std::string picked = LabDialogs::openFileDialog(getWindow(), "Lab Scene Files (*.labscene)\0*.labscene\0Lab Map Files (*.labmap)\0*.labmap\0", "assets\\maps");
                    if (!picked.empty() && LabDialogs::isAllowedSceneFile(picked)) {
                        _sessionConfig.mapPath = toSceneDisplayPath(picked);
                        _sessionConfig.enableBots = false;
                        startSession(_sessionConfig);
                    }
                    return;
                }
                // Resume (x: 680..920, y: 560..608)
                if (_currentMap && mx >= 680.0f && mx <= 920.0f && my >= 560.0f && my <= 608.0f) {
                    _inMenu = false;
                    setCursorCaptured(true);
                    return;
                }
                // Back (x: 940..1120, y: 560..608)
                if (mx >= 940.0f && mx <= 1120.0f && my >= 560.0f && my <= 608.0f) {
                    _menuScreen = MenuScreen::Main;
                    return;
                }
            }
        }
    }

    void drawWeapon() {
        int curIdx = (int)_weaponSystem.getActiveId();
        const auto& grip = _characterStudio.getWeaponGrip((WeaponID)curIdx);
        const auto& skin = _characterStudio.getWeaponSkin((WeaponID)curIdx);

        // Texture resolution
        Texture* tex = nullptr;
        if (!skin.textureFile.empty()) {
            tex = getTexture(skin.textureFile);
        }
        if (!tex && curIdx >= 0 && curIdx < (int)_cachedWeaponTextures.size()) {
            tex = _cachedWeaponTextures[curIdx];
        }

        // 3D Model resolution (Studio skin override -> cached weapon mesh)
        Mesh* stlMesh = nullptr;
        if (!skin.modelFile.empty()) {
            if (skin.modelFile != "NONE" && skin.modelFile != "PROCEDURAL") {
                stlMesh = getMesh(skin.modelFile);
            }
        } else if (curIdx >= 0 && curIdx < (int)_cachedWeaponMeshes.size()) {
            stlMesh = _cachedWeaponMeshes[curIdx];
        }

        Vec3 animPosOffset = _weaponAnimator.calculatePositionOffset(Vec3(0, 0, 0)) + _devConsole.cam_viewmodel_offset;
        Vec3 animRotOffset = _weaponAnimator.calculateRotationOffset(Vec3(0, 0, 0));
        float savedFov = _camera.fieldOfView;
        _camera.fieldOfView = _devConsole.cam_viewmodel_fov;
        _weaponSystem.renderViewModel(_camera, tex, stlMesh, _muzzleFlashTime,
                                      animPosOffset, animRotOffset,
                                      &grip.rightSocketPos, &grip.rightSocketRot,
                                      &grip.leftSocketPos, &grip.leftSocketRot,
                                      &skin.tintColor,
                                      &grip.weaponOffset, &grip.weaponRotation, &grip.weaponScale,
                                      skin.uvScale,
                                      grip.lockHands);
        _camera.fieldOfView = savedFov;
    }

    void drawMenu() {
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        int curW = (fbW > 0) ? fbW : getWidth();
        int curH = (fbH > 0) ? fbH : getHeight();

        if (_menuScreen == MenuScreen::CharacterStudio) {
            _characterStudio.render(curW, curH);
            return;
        }

        int w = 1280, h = 720;
        Renderer::beginUI(w, h);

        // Dark background overlay (Half-Life 2 style backdrop)
        Renderer::drawRect(0, 0, (float)w, (float)h, { 0.06f, 0.08f, 0.11f });

        if (_menuScreen == MenuScreen::Main) {
            // Main Frame
            Renderer::drawRect(80.0f, 60.0f, 1120.0f, 600.0f, { 0.1f, 0.13f, 0.18f });
            Renderer::drawRect(82.0f, 62.0f, 1116.0f, 44.0f, { 0.15f, 0.22f, 0.32f });
            Renderer::drawRect(82.0f, 106.0f, 1116.0f, 4.0f, { 0.2f, 0.75f, 0.95f });

            LabFont::drawText(100.0f, 74.0f, "FROZEN-LIFE : LAB FPS ENGINE", 2.4f, Vec3(0.95f, 0.98f, 1.0f), LabFontType::GeoSans);
            LabFont::drawText(720.0f, 76.0f, "v0.4 Tactical Combat & Bot Arena", 1.7f, Vec3(0.4f, 0.7f, 0.95f), LabFontType::GeoSans);

            // Left Navigation Menu
            struct MenuItem {
                std::string title;
                std::string desc;
                float y;
                bool active;
            };

            std::vector<MenuItem> items = {
                { "CAMPAIGN / MISSIONS", "Explore maps solo without hostile combat bot squads", 185.0f, true },
                { "MULTIPLAYER (HOST / JOIN)", "Host a custom LAN match with bots, game modes (FFA/DM/TDM) & rules", 245.0f, true },
                { "CHARACTER & WEAPON STUDIO", "Lab Studio: 3D grip poser, reload timeline, skins & face lip-sync", 305.0f, true },
                { "RESUME MISSION [ESC]", "Return to current active gameplay session", 365.0f, (_currentMap != nullptr) },
                { "QUIT GAME", "Exit to desktop", 425.0f, true }
            };

            for (const auto& it : items) {
                bool isHovered = it.active && (_menuMouseX >= 120.0f && _menuMouseX <= 500.0f && _menuMouseY >= it.y && _menuMouseY <= it.y + 48.0f);
                Vec3 bgCol = it.active ? (isHovered ? Vec3(0.24f, 0.40f, 0.62f) : Vec3(0.18f, 0.28f, 0.40f)) : Vec3(0.12f, 0.14f, 0.18f);
                Vec3 txtCol = it.active ? (isHovered ? Vec3(1.0f, 1.0f, 0.82f) : Vec3(1, 1, 1)) : Vec3(0.4f, 0.4f, 0.45f);

                Renderer::drawRect(120.0f, it.y, 380.0f, 48.0f, bgCol);
                Renderer::drawRect(120.0f, it.y, 4.0f, 48.0f, isHovered ? Vec3(1.0f, 0.85f, 0.2f) : Vec3(0.2f, 0.75f, 0.95f));
                if (isHovered) {
                    Renderer::drawRect(120.0f, it.y, 380.0f, 1.5f, Vec3(0.35f, 0.85f, 1.0f));
                    Renderer::drawRect(120.0f, it.y + 46.5f, 380.0f, 1.5f, Vec3(0.35f, 0.85f, 1.0f));
                    Renderer::drawRect(498.5f, it.y, 1.5f, 48.0f, Vec3(0.35f, 0.85f, 1.0f));
                }
                LabFont::drawText(140.0f, it.y + 14.0f, it.title, 2.0f, txtCol, LabFontType::GeoSans);
            }

            // Right side operational briefing card
            Renderer::drawRoundedRect(540.0f, 185.0f, 620.0f, 288.0f, Vec3(0.05f, 0.07f, 0.10f), 0.78f, 8.0f, 1.2f, Vec3(1.0f, 0.74f, 0.14f), 0.35f);
            LabFont::drawText(564.0f, 206.0f, "SECTOR BRIEFING // CRYO-WASTELAND", 2.0f, Vec3(1.0f, 0.74f, 0.14f), LabFontType::GeoSans);
            Renderer::drawRect(564.0f, 236.0f, 572.0f, 1.0f, Vec3(1.0f, 0.74f, 0.14f), 0.28f);
            LabFont::drawText(564.0f, 254.0f, "ACTIVE MAP:     " + (_currentMap ? _currentMap->metadata.name : std::string("Cryo Wasteland Outpost")), 1.65f, Vec3(0.86f, 0.90f, 0.95f), LabFontType::GeoSans);
            LabFont::drawText(564.0f, 284.0f, "COMBAT MODE:    " + (_sessionConfig.mode == GameMode::TDM ? std::string("Team Deathmatch") : std::string("Free-For-All Combat")), 1.65f, Vec3(0.86f, 0.90f, 0.95f), LabFontType::GeoSans);
            LabFont::drawText(564.0f, 314.0f, "HOSTILE SQUAD:  " + std::to_string(_aiManager.bots.size()) + " Autonomous Synth Units", 1.65f, Vec3(0.86f, 0.90f, 0.95f), LabFontType::GeoSans);
            LabFont::drawText(564.0f, 344.0f, "ARSENAL:        Slots [1-9] Tactical Weapons & Physics Props", 1.65f, Vec3(0.86f, 0.90f, 0.95f), LabFontType::GeoSans);
            LabFont::drawText(564.0f, 374.0f, "CONTROLS:       WASD Move | LMB Fire | R Reload | F Flashlight | ~ Console", 1.55f, Vec3(0.65f, 0.75f, 0.85f), LabFontType::GeoSans);

            LabFont::drawText(120.0f, 625.0f, "[ESC] RESUME GAME   |   [F2] LEVEL EDITOR   |   [TAB] SCOREBOARD", 1.5f, Vec3(0.65f, 0.72f, 0.80f), LabFontType::GeoSans);
        }
        else if (_menuScreen == MenuScreen::MultiSelect) {
            // Multiplayer Mode Selection Frame
            Renderer::drawRect(80.0f, 60.0f, 1120.0f, 600.0f, { 0.1f, 0.13f, 0.18f });
            Renderer::drawRect(82.0f, 62.0f, 1116.0f, 44.0f, { 0.15f, 0.22f, 0.32f });
            Renderer::drawRect(82.0f, 106.0f, 1116.0f, 4.0f, { 0.2f, 0.75f, 0.95f });

            LabFont::drawText(100.0f, 74.0f, "MULTIPLAYER : SELECT MODE", 2.4f, Vec3(0.95f, 0.98f, 1.0f), LabFontType::GeoSans);

            // Card 1: Host Game / Create Server
            Renderer::drawRect(140.0f, 180.0f, 460.0f, 240.0f, Vec3(0.14f, 0.18f, 0.25f));
            Renderer::drawRect(140.0f, 180.0f, 460.0f, 36.0f, Vec3(0.18f, 0.45f, 0.75f));
            LabFont::drawText(160.0f, 192.0f, "CREATE SERVER / HOST GAME", 2.0f, Vec3(1, 1, 1), LabFontType::GeoSans);
            LabFont::drawText(160.0f, 240.0f, "Host a custom LAN match with bots.", 1.7f, Vec3(0.8f, 0.85f, 0.9f), LabFontType::GeoSans);
            LabFont::drawText(160.0f, 270.0f, "Select map, choose FFA / DM / TDM mode,", 1.6f, Vec3(0.7f, 0.75f, 0.8f), LabFontType::GeoSans);
            LabFont::drawText(160.0f, 300.0f, "and configure exact bot squad count (0-8).", 1.6f, Vec3(0.7f, 0.75f, 0.8f), LabFontType::GeoSans);
            Renderer::drawRect(160.0f, 350.0f, 420.0f, 45.0f, Vec3(0.20f, 0.65f, 0.42f));
            LabFont::drawText(220.0f, 364.0f, "CONFIGURE & HOST SERVER", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // Card 2: Find Servers / Join Game
            Renderer::drawRect(680.0f, 180.0f, 460.0f, 240.0f, Vec3(0.14f, 0.18f, 0.25f));
            Renderer::drawRect(680.0f, 180.0f, 460.0f, 36.0f, Vec3(0.25f, 0.35f, 0.55f));
            LabFont::drawText(700.0f, 192.0f, "FIND SERVERS / JOIN GAME", 2.0f, Vec3(1, 1, 1), LabFontType::GeoSans);
            LabFont::drawText(700.0f, 240.0f, "Browse active servers on local LAN.", 1.7f, Vec3(0.8f, 0.85f, 0.9f), LabFontType::GeoSans);
            LabFont::drawText(700.0f, 270.0f, "View ping, active map, and player counts.", 1.6f, Vec3(0.7f, 0.75f, 0.8f), LabFontType::GeoSans);
            LabFont::drawText(700.0f, 300.0f, "Direct connect to servers via IP address.", 1.6f, Vec3(0.7f, 0.75f, 0.8f), LabFontType::GeoSans);
            Renderer::drawRect(700.0f, 350.0f, 420.0f, 45.0f, Vec3(0.22f, 0.48f, 0.78f));
            LabFont::drawText(770.0f, 364.0f, "OPEN SERVER BROWSER", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // Back button
            Renderer::drawRect(140.0f, 580.0f, 240.0f, 48.0f, Vec3(0.20f, 0.25f, 0.35f));
            LabFont::drawText(170.0f, 596.0f, "< BACK TO MAIN MENU", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);
        }
        else if (_menuScreen == MenuScreen::HostGame) {
            // Host Game Configuration Screen
            Renderer::drawRect(80.0f, 60.0f, 1120.0f, 600.0f, { 0.1f, 0.13f, 0.18f });
            Renderer::drawRect(82.0f, 62.0f, 1116.0f, 44.0f, { 0.15f, 0.22f, 0.32f });
            Renderer::drawRect(82.0f, 106.0f, 1116.0f, 4.0f, { 0.2f, 0.75f, 0.95f });

            LabFont::drawText(100.0f, 74.0f, "MULTIPLAYER : HOST SERVER & MATCH SETUP", 2.4f, Vec3(0.95f, 0.98f, 1.0f), LabFontType::GeoSans);

            // Left Column: Map Selection Box
            Renderer::drawRect(100.0f, 120.0f, 440.0f, 425.0f, Vec3(0.08f, 0.10f, 0.14f));
            Renderer::drawRect(100.0f, 120.0f, 440.0f, 32.0f, Vec3(0.18f, 0.35f, 0.55f));
            LabFont::drawText(120.0f, 128.0f, "1. SELECT MAP", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);

            for (int i = 0; i < (int)_availableMaps.size(); ++i) {
                float iy = 170.0f + i * 50.0f;
                bool isSel = (_sessionConfig.mapPath == _availableMaps[i]);
                Renderer::drawRect(115.0f, iy, 410.0f, 42.0f, isSel ? Vec3(0.18f, 0.45f, 0.75f) : Vec3(0.12f, 0.15f, 0.20f));
                if (isSel) Renderer::drawRect(115.0f, iy, 5.0f, 42.0f, Vec3(0.98f, 0.78f, 0.08f));
                LabFont::drawText(130.0f, iy + 12.0f, toSceneDisplayPath(_availableMaps[i]), 1.7f, isSel ? Vec3(1, 1, 1) : Vec3(0.7f, 0.75f, 0.8f), LabFontType::GeoSans);
            }

            Renderer::drawRect(115.0f, 485.0f, 410.0f, 42.0f, Vec3(0.20f, 0.32f, 0.48f));
            LabFont::drawText(170.0f, 498.0f, "OPEN SCENE FROM DISK... [O]", 1.6f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // Right Column: Rules & Bot Settings Box
            Renderer::drawRect(560.0f, 120.0f, 620.0f, 425.0f, Vec3(0.08f, 0.10f, 0.14f));
            Renderer::drawRect(560.0f, 120.0f, 620.0f, 32.0f, Vec3(0.18f, 0.35f, 0.55f));
            LabFont::drawText(580.0f, 128.0f, "2. SERVER & MATCH RULES", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // 1. Network Mode: LAN vs P2P
            LabFont::drawText(580.0f, 160.0f, "NETWORK MODE:", 1.7f, Vec3(0.85f, 0.88f, 0.95f), LabFontType::GeoSans);
            bool isLAN = (_sessionConfig.networkMode == NetworkMode::LAN);
            bool isP2P = (_sessionConfig.networkMode == NetworkMode::P2P);

            Renderer::drawRect(580.0f, 182.0f, 275.0f, 38.0f, isLAN ? Vec3(0.18f, 0.65f, 0.45f) : Vec3(0.14f, 0.18f, 0.24f));
            if (isLAN) Renderer::drawRect(580.0f, 182.0f, 275.0f, 2.0f, Vec3(0.98f, 0.78f, 0.08f));
            LabFont::drawText(660.0f, 194.0f, "[ LAN ]", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(870.0f, 182.0f, 290.0f, 38.0f, isP2P ? Vec3(0.20f, 0.55f, 0.75f) : Vec3(0.14f, 0.18f, 0.24f));
            if (isP2P) Renderer::drawRect(870.0f, 182.0f, 290.0f, 2.0f, Vec3(0.2f, 0.85f, 1.0f));
            LabFont::drawText(960.0f, 194.0f, "[ P2P ]", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // 2. Game Mode Buttons
            LabFont::drawText(580.0f, 228.0f, "GAME MODE:", 1.7f, Vec3(0.85f, 0.88f, 0.95f), LabFontType::GeoSans);
            bool isFFA = (_sessionConfig.mode == GameMode::FFA);
            bool isDM  = (_sessionConfig.mode == GameMode::DM);
            bool isTDM = (_sessionConfig.mode == GameMode::TDM);

            Renderer::drawRect(580.0f, 248.0f, 180.0f, 36.0f, isFFA ? Vec3(0.18f, 0.65f, 0.45f) : Vec3(0.14f, 0.18f, 0.24f));
            if (isFFA) Renderer::drawRect(580.0f, 248.0f, 180.0f, 2.0f, Vec3(0.98f, 0.78f, 0.08f));
            LabFont::drawText(620.0f, 258.0f, "FFA (All)", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(770.0f, 248.0f, 180.0f, 36.0f, isDM ? Vec3(0.18f, 0.65f, 0.45f) : Vec3(0.14f, 0.18f, 0.24f));
            if (isDM) Renderer::drawRect(770.0f, 248.0f, 180.0f, 2.0f, Vec3(0.98f, 0.78f, 0.08f));
            LabFont::drawText(800.0f, 258.0f, "Deathmatch", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(960.0f, 248.0f, 200.0f, 36.0f, isTDM ? Vec3(0.18f, 0.65f, 0.45f) : Vec3(0.14f, 0.18f, 0.24f));
            if (isTDM) Renderer::drawRect(960.0f, 248.0f, 200.0f, 2.0f, Vec3(0.98f, 0.78f, 0.08f));
            LabFont::drawText(1010.0f, 258.0f, "Team DM", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // 3. Bots Toggle & Count
            LabFont::drawText(580.0f, 294.0f, "COMBAT AI BOTS:", 1.7f, Vec3(0.85f, 0.88f, 0.95f), LabFontType::GeoSans);
            Vec3 botBtnBg = _sessionConfig.enableBots ? Vec3(0.18f, 0.65f, 0.35f) : Vec3(0.24f, 0.26f, 0.30f);
            std::string botBtnText = _sessionConfig.enableBots ? "BOTS: ENABLED (ON)" : "BOTS: DISABLED (OFF)";
            Renderer::drawRect(580.0f, 314.0f, 275.0f, 36.0f, botBtnBg);
            LabFont::drawText(610.0f, 324.0f, botBtnText, 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(875.0f, 314.0f, 45.0f, 36.0f, Vec3(0.22f, 0.28f, 0.38f));
            LabFont::drawText(892.0f, 323.0f, "-", 2.2f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(928.0f, 314.0f, 130.0f, 36.0f, Vec3(0.12f, 0.15f, 0.20f));
            std::string botCountStr = std::to_string(_sessionConfig.botCount) + " BOTS";
            LabFont::drawText(950.0f, 324.0f, botCountStr, 1.8f, Vec3(0.95f, 0.85f, 0.2f), LabFontType::GeoSans);

            Renderer::drawRect(1066.0f, 314.0f, 45.0f, 36.0f, Vec3(0.22f, 0.28f, 0.38f));
            LabFont::drawText(1082.0f, 323.0f, "+", 2.2f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // 4. Frag Limit & Port Badge
            LabFont::drawText(580.0f, 360.0f, "FRAG LIMIT & SERVER CONFIG:", 1.7f, Vec3(0.85f, 0.88f, 0.95f), LabFontType::GeoSans);
            Renderer::drawRect(580.0f, 380.0f, 45.0f, 36.0f, Vec3(0.22f, 0.28f, 0.38f));
            LabFont::drawText(598.0f, 389.0f, "-", 2.2f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(633.0f, 380.0f, 130.0f, 36.0f, Vec3(0.12f, 0.15f, 0.20f));
            std::string fragLimitStr = std::to_string(_sessionConfig.fragLimit) + " KILLS";
            LabFont::drawText(652.0f, 390.0f, fragLimitStr, 1.8f, Vec3(0.3f, 0.85f, 1.0f), LabFontType::GeoSans);

            Renderer::drawRect(771.0f, 380.0f, 45.0f, 36.0f, Vec3(0.22f, 0.28f, 0.38f));
            LabFont::drawText(787.0f, 389.0f, "+", 2.2f, Vec3(1, 1, 1), LabFontType::GeoSans);

            if (!_hasQueriedNAT) {
                _hostNATStatus = NATDiscovery::queryFullStatus(_sessionConfig.port);
                _hasQueriedNAT = true;
            }

            Renderer::drawRect(830.0f, 380.0f, 330.0f, 36.0f, Vec3(0.10f, 0.14f, 0.20f));
            Renderer::drawRect(830.0f, 380.0f, 330.0f, 1.0f, Vec3(0.2f, 0.75f, 0.95f));
            std::string ipDisplay = "LAN: " + (_hostNATStatus.localIP.empty() ? "127.0.0.1" : _hostNATStatus.localIP) + ":" + std::to_string(_sessionConfig.port);
            LabFont::drawText(845.0f, 391.0f, ipDisplay, 1.4f, Vec3(0.3f, 0.85f, 1.0f), LabFontType::GeoSans);

            // 5. Descriptive Info Box
            Renderer::drawRect(580.0f, 424.0f, 580.0f, 46.0f, Vec3(0.06f, 0.08f, 0.12f));
            Renderer::drawRect(580.0f, 424.0f, 4.0f, 46.0f, isLAN ? Vec3(0.2f, 0.85f, 0.4f) : (_hostNATStatus.upnpActive ? Vec3(0.2f, 0.85f, 0.4f) : Vec3(0.95f, 0.65f, 0.15f)));
            if (isLAN) {
                LabFont::drawText(595.0f, 432.0f, "LAN Mode: Broadcasts UDP discovery on local network so nearby players find server.", 1.35f, Vec3(0.85f, 0.9f, 0.95f), LabFontType::System);
                LabFont::drawText(595.0f, 450.0f, "Direct connect available on " + (_hostNATStatus.localIP.empty() ? "127.0.0.1" : _hostNATStatus.localIP) + ":" + std::to_string(_sessionConfig.port), 1.35f, Vec3(0.7f, 0.75f, 0.8f), LabFontType::System);
            } else {
                std::string pubStr = "Public Internet IP: " + (_hostNATStatus.publicIP.empty() ? _hostNATStatus.localIP : _hostNATStatus.publicIP) + ":" + std::to_string(_sessionConfig.port);
                LabFont::drawText(595.0f, 432.0f, pubStr, 1.35f, Vec3(0.2f, 0.9f, 0.45f), LabFontType::System);
                std::string notice = _hostNATStatus.upnpActive ? "UPnP IGD Active: Port mapped on router! Share Public IP with friend." : "Router Firewall: Forward UDP 27015, or use Radmin VPN, or Punch below";
                LabFont::drawText(595.0f, 450.0f, notice, 1.35f, _hostNATStatus.upnpActive ? Vec3(0.4f, 0.95f, 0.5f) : Vec3(0.98f, 0.75f, 0.25f), LabFontType::System);
            }

            // Copy Invite / IP button (x: 580..1160, y: 476..512)
            Renderer::drawRect(580.0f, 476.0f, 580.0f, 36.0f, Vec3(0.16f, 0.32f, 0.48f));
            Renderer::drawRect(580.0f, 476.0f, 580.0f, 2.0f, Vec3(0.3f, 0.85f, 1.0f));
            std::string copyInviteBtnText = "[COPY INVITE ADDRESS TO CLIPBOARD]";
            LabFont::drawText(660.0f, 487.0f, copyInviteBtnText, 1.6f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // Plug & Play Room Code Box (x: 580..1160, y: 518..554)
            Renderer::drawRect(580.0f, 518.0f, 230.0f, 36.0f, Vec3(0.10f, 0.18f, 0.28f));
            Renderer::drawRect(580.0f, 518.0f, 230.0f, 1.0f, Vec3(0.3f, 0.85f, 1.0f));
            LabFont::drawText(592.0f, 529.0f, "ROOM: [" + _pnpRoomCode + "]", 1.6f, Vec3(0.3f, 0.95f, 1.0f), LabFontType::GeoSans);

            Renderer::drawRect(820.0f, 518.0f, 160.0f, 36.0f, Vec3(0.18f, 0.34f, 0.50f));
            Renderer::drawRect(820.0f, 518.0f, 160.0f, 1.0f, Vec3(0.3f, 0.75f, 1.0f));
            LabFont::drawText(836.0f, 529.0f, "COPY ROOM CODE", 1.4f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(990.0f, 518.0f, 170.0f, 36.0f, Vec3(0.20f, 0.55f, 0.35f));
            Renderer::drawRect(990.0f, 518.0f, 170.0f, 1.0f, Vec3(0.4f, 0.95f, 0.55f));
            LabFont::drawText(1015.0f, 529.0f, "NEW ROOM CODE", 1.4f, Vec3(1, 1, 1), LabFontType::GeoSans);

            if (!_punchStatusMsg.empty()) {
                LabFont::drawText(580.0f, 558.0f, _punchStatusMsg, 1.25f, Vec3(0.35f, 0.95f, 0.5f), LabFontType::System);
            }

            // Bottom Buttons: Back and Launch
            Renderer::drawRect(100.0f, 570.0f, 200.0f, 48.0f, Vec3(0.20f, 0.25f, 0.35f));
            LabFont::drawText(150.0f, 586.0f, "< BACK", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);

            std::string launchBtnText = "START SERVER & LAUNCH MATCH";
            Vec3 launchBtnBg = isLAN ? Vec3(0.18f, 0.65f, 0.35f) : Vec3(0.20f, 0.55f, 0.75f);
            Renderer::drawRect(740.0f, 570.0f, 440.0f, 48.0f, launchBtnBg);
            Renderer::drawRect(740.0f, 570.0f, 440.0f, 2.0f, Vec3(0.98f, 0.78f, 0.08f));
            LabFont::drawText(770.0f, 586.0f, launchBtnText, 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);
        }
        else if (_menuScreen == MenuScreen::JoinGame) {
            // Server Browser Frame
            Renderer::drawRect(80.0f, 60.0f, 1120.0f, 600.0f, { 0.08f, 0.10f, 0.14f });
            Renderer::drawRect(82.0f, 62.0f, 1116.0f, 44.0f, { 0.12f, 0.16f, 0.22f });
            Renderer::drawRect(82.0f, 106.0f, 1116.0f, 4.0f, { 0.2f, 0.75f, 0.95f });

            LabFont::drawText(100.0f, 74.0f, "MULTIPLAYER : PLUG & PLAY QUICK JOIN & BROWSER", 2.2f, Vec3(0.95f, 0.98f, 1.0f), LabFontType::GeoSans);

            if (!_hasQueriedGuestNAT) {
                _guestNATStatus = NATDiscovery::queryFullStatus(27016);
                _hasQueriedGuestNAT = true;
            }

            std::string myPubDisplay = "MY IP: " + (_guestNATStatus.publicIP.empty() ? "127.0.0.1" : _guestNATStatus.publicIP) + ":27016";
            LabFont::drawText(720.0f, 76.0f, myPubDisplay, 1.35f, Vec3(0.95f, 0.85f, 0.2f), LabFontType::System);

            Renderer::drawRect(1050.0f, 70.0f, 110.0f, 26.0f, Vec3(0.18f, 0.38f, 0.58f));
            Renderer::drawRect(1050.0f, 70.0f, 110.0f, 1.0f, Vec3(0.4f, 0.85f, 1.0f));
            LabFont::drawText(1062.0f, 77.0f, "COPY MY IP", 1.25f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // Plug & Play Quick Join Bar (x: 110..1170, y: 116..154)
            Renderer::drawRect(110.0f, 116.0f, 250.0f, 38.0f, _pnpJoinCodeActive ? Vec3(0.12f, 0.22f, 0.35f) : Vec3(0.08f, 0.12f, 0.18f));
            Renderer::drawRect(110.0f, 116.0f, 250.0f, 1.0f, _pnpJoinCodeActive ? Vec3(0.3f, 0.85f, 1.0f) : Vec3(0.2f, 0.3f, 0.45f));
            std::string codeDisplay = "ROOM CODE: " + _pnpJoinCode + (_pnpJoinCodeActive ? "_" : "");
            LabFont::drawText(125.0f, 126.0f, codeDisplay, 1.6f, Vec3(0.3f, 0.95f, 1.0f), LabFontType::GeoSans);

            Renderer::drawRect(370.0f, 116.0f, 85.0f, 38.0f, Vec3(0.18f, 0.34f, 0.50f));
            LabFont::drawText(388.0f, 126.0f, "PASTE", 1.5f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(465.0f, 116.0f, 400.0f, 38.0f, Vec3(0.18f, 0.65f, 0.38f));
            Renderer::drawRect(465.0f, 116.0f, 400.0f, 2.0f, Vec3(0.4f, 0.95f, 0.65f));
            LabFont::drawText(485.0f, 126.0f, "JOIN VIA ROOM CODE (PLUG & PLAY)", 1.5f, Vec3(1, 1, 1), LabFontType::GeoSans);

            LabFont::drawText(880.0f, 128.0f, "Zero config / Auto-NAT punch", 1.25f, Vec3(0.5f, 0.85f, 0.65f), LabFontType::System);

            // Table Header
            Renderer::drawRect(110.0f, 164.0f, 1060.0f, 30.0f, Vec3(0.14f, 0.20f, 0.28f));
            LabFont::drawText(130.0f, 172.0f, "SERVER NAME", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);
            LabFont::drawText(500.0f, 172.0f, "MAP", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);
            LabFont::drawText(720.0f, 172.0f, "MODE", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);
            LabFont::drawText(880.0f, 172.0f, "PLAYERS", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);
            LabFont::drawText(1030.0f, 172.0f, "PING", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);

            const auto& servers = _serverBrowser.getServers();
            if (servers.empty()) {
                Renderer::drawRect(110.0f, 202.0f, 1060.0f, 80.0f, Vec3(0.06f, 0.08f, 0.11f));
                Renderer::drawRect(110.0f, 202.0f, 4.0f, 80.0f, Vec3(0.85f, 0.35f, 0.25f));
                LabFont::drawText(130.0f, 216.0f, "NO RUNNING SERVERS FOUND ON LAN / LOCALHOST", 1.8f, Vec3(0.95f, 0.45f, 0.45f), LabFontType::GeoSans);
                LabFont::drawText(130.0f, 244.0f, "To connect over Internet: Enter 4-digit Room Code above and click JOIN (Plug & Play)!", 1.4f, Vec3(0.4f, 0.85f, 0.95f), LabFontType::System);
            } else {
                for (size_t i = 0; i < servers.size() && i < 5; ++i) {
                    const auto& s = servers[i];
                    float rowY = 202.0f + static_cast<float>(i) * 50.0f;
                    bool isSelected = (_selectedServerIndex == static_cast<int>(i));

                    Vec3 rowBg = isSelected ? Vec3(0.18f, 0.40f, 0.65f) : ((i % 2 == 0) ? Vec3(0.10f, 0.14f, 0.19f) : Vec3(0.08f, 0.11f, 0.15f));
                    Renderer::drawRect(110.0f, rowY, 1060.0f, 44.0f, rowBg);
                    if (isSelected) {
                        Renderer::drawRect(110.0f, rowY, 5.0f, 44.0f, Vec3(0.2f, 0.85f, 1.0f));
                    }

                    LabFont::drawText(130.0f, rowY + 12.0f, s.name, 1.8f, isSelected ? Vec3(1, 1, 1) : Vec3(0.9f, 0.95f, 1.0f), LabFontType::GeoSans);
                    LabFont::drawText(500.0f, rowY + 12.0f, toSceneDisplayPath(s.map), 1.8f, Vec3(0.75f, 0.85f, 0.95f), LabFontType::GeoSans);
                    LabFont::drawText(720.0f, rowY + 12.0f, s.mode, 1.8f, Vec3(0.95f, 0.85f, 0.2f), LabFontType::GeoSans);
                    std::string plStr = std::to_string(s.playerCount) + " / " + std::to_string(s.maxPlayers);
                    LabFont::drawText(880.0f, rowY + 12.0f, plStr, 1.8f, Vec3(0.9f, 0.95f, 1.0f), LabFontType::GeoSans);
                    std::string pingStr = std::to_string(s.pingMs) + " ms";
                    LabFont::drawText(1030.0f, rowY + 12.0f, pingStr, 1.8f, Vec3(0.2f, 0.9f, 0.35f), LabFontType::GeoSans);
                }
            }

            // Direct Connect field
            Renderer::drawRect(110.0f, 480.0f, 690.0f, 44.0f, _directConnectActive ? Vec3(0.12f, 0.18f, 0.28f) : Vec3(0.06f, 0.08f, 0.12f));
            Renderer::drawRect(110.0f, 480.0f, 690.0f, 2.0f, _directConnectActive ? Vec3(0.3f, 0.85f, 1.0f) : Vec3(0.18f, 0.24f, 0.32f));
            std::string dcDisplay = "DIRECT IP TARGET:  " + _directConnectIP + (_directConnectActive ? "_" : "");
            LabFont::drawText(130.0f, 494.0f, dcDisplay, 1.8f, Vec3(0.3f, 0.85f, 1.0f), LabFontType::GeoSans);

            // [PASTE] button (x: 815..940, y: 480..524)
            Renderer::drawRect(815.0f, 480.0f, 125.0f, 44.0f, Vec3(0.18f, 0.38f, 0.58f));
            Renderer::drawRect(815.0f, 480.0f, 125.0f, 2.0f, Vec3(0.4f, 0.85f, 1.0f));
            LabFont::drawText(850.0f, 494.0f, "PASTE", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // [CLEAR] button (x: 955..1060, y: 480..524)
            Renderer::drawRect(955.0f, 480.0f, 105.0f, 44.0f, Vec3(0.28f, 0.18f, 0.22f));
            LabFont::drawText(980.0f, 494.0f, "CLEAR", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // Connection Status Feedback line
            if (_isConnecting) {
                Renderer::drawRect(110.0f, 530.0f, 1060.0f, 24.0f, Vec3(0.08f, 0.12f, 0.18f));
                std::string connStatus = ">> CONNECTING TO " + _netClient.getServerAddressString() + " (ATTEMPT " + std::to_string(_netClient.getConnectAttempts()) + "/20) ...";
                LabFont::drawText(130.0f, 534.0f, connStatus, 1.4f, Vec3(0.3f, 0.85f, 1.0f), LabFontType::System);
            } else if (!_connectStatusMsg.empty()) {
                bool isErr = (_connectStatusMsg.find("FAILED") != std::string::npos || _connectStatusMsg.find("TIMED OUT") != std::string::npos);
                Renderer::drawRect(110.0f, 530.0f, 1060.0f, 24.0f, Vec3(0.08f, 0.12f, 0.18f));
                LabFont::drawText(130.0f, 534.0f, _connectStatusMsg, 1.4f, isErr ? Vec3(1.0f, 0.35f, 0.35f) : Vec3(0.35f, 0.9f, 0.45f), LabFontType::System);
            }

            // Bottom Buttons: BACK, REFRESH, CONNECT TO SERVER / CANCEL
            Renderer::drawRect(110.0f, 560.0f, 180.0f, 48.0f, Vec3(0.20f, 0.25f, 0.35f));
            LabFont::drawText(150.0f, 576.0f, "< BACK", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(620.0f, 560.0f, 180.0f, 48.0f, Vec3(0.18f, 0.32f, 0.48f));
            LabFont::drawText(655.0f, 576.0f, "REFRESH", 1.8f, Vec3(0.9f, 0.95f, 1.0f), LabFontType::GeoSans);

            if (_isConnecting) {
                Renderer::drawRect(830.0f, 560.0f, 340.0f, 48.0f, Vec3(0.75f, 0.25f, 0.15f));
                Renderer::drawRect(830.0f, 560.0f, 340.0f, 2.0f, Vec3(1.0f, 0.5f, 0.3f));
                std::string cancelText = "CANCEL CONNECTING [" + std::to_string(_netClient.getConnectAttempts()) + "/20]";
                LabFont::drawText(845.0f, 576.0f, cancelText, 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);
            } else {
                Renderer::drawRect(830.0f, 560.0f, 340.0f, 48.0f, Vec3(0.20f, 0.65f, 0.42f));
                Renderer::drawRect(830.0f, 560.0f, 340.0f, 2.0f, Vec3(0.4f, 0.95f, 0.65f));
                LabFont::drawText(860.0f, 576.0f, "CONNECT TO SERVER", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);
            }
        }
        else if (_menuScreen == MenuScreen::Singleplayer) {
            // Existing Singleplayer Map Selection
            Renderer::drawRect(100.0f, 60.0f, 1080.0f, 600.0f, { 0.1f, 0.13f, 0.18f });
            Renderer::drawRect(102.0f, 62.0f, 1076.0f, 40.0f, { 0.15f, 0.22f, 0.32f });
            Renderer::drawRect(102.0f, 100.0f, 1076.0f, 4.0f, { 0.2f, 0.75f, 0.95f });

            LabFont::drawText(120.0f, 72.0f, "CAMPAIGN & SOLO SECTOR EXPLORATION", 2.2f, Vec3(0.9f, 0.95f, 1.0f), LabFontType::GeoSans);

            float startY = 130.0f;
            for (int i = 0; i < (int)_availableMaps.size(); ++i) {
                bool isSelected = (i == _selectedMapIndex);
                float itemY = startY + i * 55.0f;

                Vec3 barColor = isSelected ? Vec3(0.18f, 0.45f, 0.75f) : Vec3(0.12f, 0.16f, 0.22f);
                Renderer::drawRect(140.0f, itemY, 800.0f, 45.0f, barColor);
                if (isSelected) Renderer::drawRect(140.0f, itemY, 6.0f, 45.0f, Vec3(0.98f, 0.78f, 0.08f));

                std::string mapDisplay = toSceneDisplayPath(_availableMaps[i]);
                LabFont::drawText(160.0f, itemY + 14.0f, mapDisplay, 2.0f, isSelected ? Vec3(1, 1, 1) : Vec3(0.7f, 0.75f, 0.8f), LabFontType::GeoSans);
            }

            // Launch button
            Renderer::drawRect(140.0f, 560.0f, 230.0f, 48.0f, Vec3(0.18f, 0.65f, 0.45f));
            LabFont::drawText(160.0f, 576.0f, "LAUNCH MAP [ENTER]", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // Open from disk
            Renderer::drawRect(390.0f, 560.0f, 270.0f, 48.0f, Vec3(0.22f, 0.45f, 0.75f));
            LabFont::drawText(405.0f, 576.0f, "OPEN FROM DISK... [O]", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // Resume
            if (_currentMap) {
                Renderer::drawRect(680.0f, 560.0f, 240.0f, 48.0f, Vec3(0.75f, 0.45f, 0.15f));
                LabFont::drawText(695.0f, 576.0f, "RESUME [ESC]", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);
            }

            // Back
            Renderer::drawRect(940.0f, 560.0f, 180.0f, 48.0f, Vec3(0.20f, 0.25f, 0.35f));
            LabFont::drawText(980.0f, 576.0f, "< BACK", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);
        }

        Renderer::endUI();
    }

    void drawUI() {
        int w = 1280, h = 720;
        if (_devConsole.ui_scale > 0.1f && std::abs(_devConsole.ui_scale - 1.0f) > 0.01f) {
            w = static_cast<int>(1280.0f / _devConsole.ui_scale);
            h = static_cast<int>(720.0f / _devConsole.ui_scale);
        }

        _hud.showCrosshair = _devConsole.ui_crosshair;
        _hud.crosshairColor = _devConsole.ui_crosshair_color_rgb;

        if (_devConsole.ui_hide) {
            int winW = w, winH = h;
            getWindowSize(winW, winH);
            if (winW <= 0) winW = w;
            if (winH <= 0) winH = h;
            if (_devConsole.isOpen) {
                _devConsole.render(winW, winH);
            }
            return;
        }

        if (!_hammerEditor.active) {
            if (_isPlayerDead) {
                _hud.renderDeathScreen(w, h, _playerRespawnTimer);
            } else {
                _hud.render(w, h);
                _interactiveSystem.renderHUD(w, h);
            }
            _chat.render(w, h);

            // Scoreboard (Hold or toggle TAB)
            if (Input::isKeyPressed(258)) { // GLFW_KEY_TAB
                std::vector<ScoreboardEntry> entries;

                ScoreboardEntry pe;
                pe.name = "[YOU] Player";
                pe.kills = _playerKills;
                pe.deaths = _playerDeaths;
                pe.ping = "5ms";
                pe.isBot = false;
                pe.isLocalPlayer = true;
                pe.isAlive = !_isPlayerDead;
                pe.status = _isPlayerDead ? "DEAD (" + std::to_string(std::max(0, (int)std::ceil(_playerRespawnTimer))) + "s)" : "ALIVE";
                pe.team = (_sessionConfig.mode == GameMode::TDM) ? "Blue (Alpha)" : "Free-For-All";
                entries.push_back(pe);

                for (const auto& bot : _aiManager.bots) {
                    ScoreboardEntry be;
                    be.name = bot.name;
                    be.kills = bot.kills;
                    be.deaths = bot.deaths;
                    be.ping = "BOT";
                    be.isBot = true;
                    be.isLocalPlayer = false;
                    be.isAlive = bot.isAlive();
                    be.status = bot.isAlive() ? "ALIVE" : ("DEAD (" + std::to_string(std::max(0, (int)std::ceil(bot.respawnTimer))) + "s)");
                    be.team = (_sessionConfig.mode == GameMode::TDM) ? ((bot.team == 0) ? "Red" : "Blue") : "Free-For-All";
                    entries.push_back(be);
                }

                std::sort(entries.begin(), entries.end(), [](const ScoreboardEntry& a, const ScoreboardEntry& b) {
                    return a.kills > b.kills;
                });

                std::string mapTitle = _currentMap ? _currentMap->metadata.name : "Sector";
                _hud.renderScoreboard(w, h, entries, mapTitle, _sessionConfig.getModeString(), _sessionConfig.fragLimit);
            }
        } else {
            _hammerEditor.drawUI(w, h);
        }

        // Debug mode overlay (F3)
        if (_debugMode) {
            Renderer::beginUI(w, h);
            Renderer::drawRect(10.0f, 10.0f, 280.0f, 45.0f, { 0.1f, 0.1f, 0.15f });
            Renderer::drawRect(12.0f, 12.0f, 276.0f, 41.0f, { 0.18f, 0.35f, 0.55f });

            std::string dbgBots = "Alive Bots: " + std::to_string(std::count_if(_aiManager.bots.begin(), _aiManager.bots.end(), [](const auto& b){ return b.isAlive(); })) +
                                  " | Pickups: " + std::to_string(_pickups.items.size());
            LabFont::drawText(20.0f, 25.0f, dbgBots, 1.5f, Vec3(1, 1, 1), LabFontType::System);
            Renderer::endUI();
        }

        // 2D Telemetry overlays & Developer Console (real measured frame timing)
        static auto s_prevTelemetryTime = std::chrono::steady_clock::now();
        static float s_smoothedFrameMs = 16.67f;
        auto nowTelemetryTime = std::chrono::steady_clock::now();
        float rawFrameMs = std::chrono::duration<float, std::milli>(nowTelemetryTime - s_prevTelemetryTime).count();
        s_prevTelemetryTime = nowTelemetryTime;
        if (rawFrameMs > 0.1f && rawFrameMs < 500.0f) {
            s_smoothedFrameMs = s_smoothedFrameMs * 0.90f + rawFrameMs * 0.10f;
        }

        DevTelemetryData tData;
        tData.currentSpeed = _velocity.length();
        tData.horizontalSpeed = std::sqrt(_velocity.x * _velocity.x + _velocity.z * _velocity.z);
        tData.velocity = _velocity;
        tData.playerPos = _camera.getPosition();
        tData.pitch = _camera.getPitch();
        tData.yaw = _camera.getYaw();
        tData.frametimeMs = s_smoothedFrameMs;
        tData.fps = (s_smoothedFrameMs > 0.01f) ? (1000.0f / s_smoothedFrameMs) : 60.0f;
        tData.pingMs = _netClient.isConnected() ? 25 : 0;
        tData.activeBots = (int)_aiManager.bots.size();
        tData.isGrounded = _isGrounded;
        tData.isBhopActive = (!_isGrounded && tData.horizontalSpeed > 5.2f);
        tData.drawCalls = Renderer::getDrawCallCount();
        tData.vertexCount = Renderer::getVertexCount();
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
            tData.ramWorkingSetMB = pmc.WorkingSetSize / (1024 * 1024);
        }
#endif
        // Closest AI Bot telemetry
        float closestDist = 999.0f;
        const CombatBot* closestBot = nullptr;
        for (const auto& b : _aiManager.bots) {
            if (b.isAlive()) {
                float dist = (b.position - _camera.getPosition()).length();
                if (dist < closestDist) {
                    closestDist = dist;
                    closestBot = &b;
                }
            }
        }
        if (closestBot) {
            std::string stStr = "PATROL";
            switch (closestBot->state) {
                case AIState::Patrol: stStr = "PATROL"; break;
                case AIState::Talk:   stStr = "TALK"; break;
                case AIState::Alert:  stStr = "ALERT"; break;
                case AIState::Chase:  stStr = "CHASE"; break;
                case AIState::Attack: stStr = "ATTACK"; break;
                case AIState::Hurt:   stStr = "HURT"; break;
                case AIState::Dead:   stStr = "DEAD"; break;
            }
            char bBuf[128];
            snprintf(bBuf, sizeof(bBuf), "%s | State: %s | Dist: %.1fm", closestBot->name.c_str(), stStr.c_str(), closestDist);
            tData.closestBotInfo = bBuf;

            if (closestBot->state == AIState::Talk && !closestBot->activeSubtitle.empty()) {
                Renderer::beginUI(w, h);
                float subW = 680.0f;
                float subH = 44.0f;
                float subX = (w - subW) * 0.5f;
                float subY = h - 135.0f;
                Renderer::drawRect(subX, subY, subW, subH, Vec3(0.06f, 0.09f, 0.14f));
                Renderer::drawRect(subX, subY, 4.0f, subH, Vec3(0.2f, 0.85f, 1.0f));
                std::string line = "[" + closestBot->name + "]: " + closestBot->activeSubtitle;
                LabFont::drawText(subX + 14.0f, subY + 14.0f, line, 1.55f, Vec3(0.85f, 0.96f, 1.0f), LabFontType::GeoSans);
                Renderer::endUI();
            }
        }

        int winW = w, winH = h;
        getWindowSize(winW, winH);
        if (winW <= 0) winW = w;
        if (winH <= 0) winH = h;

        _devConsole.renderOverlay(winW, winH, tData);
        if (_devConsole.isOpen) {
            _devConsole.render(winW, winH);
        }
    }

    void renderSplashScreen() {
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        float w = (fbW > 0) ? (float)fbW : (float)getWidth();
        float h = (fbH > 0) ? (float)fbH : (float)getHeight();
        if (w <= 0.0f) w = 1280.0f;
        if (h <= 0.0f) h = 720.0f;

        glClearColor(0.025f, 0.035f, 0.048f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Renderer::beginUI((int)w, (int)h);

        // Smooth fade-in (0.0..0.4s) and fade-out (2.1..2.5s)
        float alpha = 1.0f;
        if (_splashTimer < 0.4f) {
            alpha = _splashTimer / 0.4f;
        } else if (_splashTimer > 2.1f) {
            alpha = std::clamp((_splashDuration - _splashTimer) / 0.4f, 0.0f, 1.0f);
        }
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        Vec3 bgDark = Vec3(0.025f, 0.035f, 0.048f) * alpha;
        Renderer::drawRect(0.0f, 0.0f, w, h, bgDark);

        float centerX = w * 0.5f;
        float centerY = h * 0.44f;

        // "p o w e r e d   b y" subtitle in authentic Source Engine styling
        Vec3 subCol = Vec3(0.60f, 0.65f, 0.72f) * alpha;
        std::string poweredBy = "p o w e r e d   b y";
        float subW = LabFont::getTextWidth(poweredBy, 1.45f, LabFontType::GeoSans);
        LabFont::drawText(centerX - subW * 0.5f, centerY - 95.0f, poweredBy, 1.45f, subCol, LabFontType::GeoSans);

        // Official LaB logo (Try LabLogoText.png, LabLogoText_White.png, LabLogo.png)
        Texture* logoTex = getTexture("LabLogoText.png");
        if (!logoTex) logoTex = getTexture("LabLogoText_White.png");
        if (!logoTex) logoTex = getTexture("LabLogo.png");

        if (logoTex) {
            float logoW = 340.0f;
            float logoH = 175.0f;
            Vec3 tint = Vec3(1.0f, 1.0f, 1.0f) * alpha;
            Renderer::drawTextureRect(centerX - logoW * 0.5f, centerY - 65.0f, logoW, logoH, *logoTex, tint);
        } else {
            float emblemW = 280.0f;
            float emblemH = 68.0f;
            float eX = centerX - emblemW * 0.5f;
            float eY = centerY - 34.0f;
            Renderer::drawRect(eX, eY, emblemW, emblemH, Vec3(0.08f, 0.11f, 0.16f) * alpha);
            Renderer::drawRect(eX, eY + emblemH - 3.0f, emblemW, 3.0f, Vec3(0.18f, 0.78f, 0.95f) * alpha);
            std::string logoTitle = "L a B   E N G I N E";
            float tw = LabFont::getTextWidth(logoTitle, 2.2f, LabFontType::GeoSans);
            LabFont::drawText(centerX - tw * 0.5f, eY + 20.0f, logoTitle, 2.2f, Vec3(0.95f, 0.98f, 1.0f) * alpha, LabFontType::GeoSans);
        }

        // Technology partner logos underneath: OpenGL, CMake, miniaudio, Lua 5.4
        float badgesY = centerY + 130.0f;
        struct TechPartner {
            std::string texName;
            std::string name;
            float logoW;
            float logoH;
            Vec3 accentCol;
        };
        std::vector<TechPartner> partners = {
            { "opengl_logo.png", "OpenGL", 108.0f, 45.0f, Vec3(0.33f, 0.55f, 0.72f) },
            { "cmake_logo.png", "CMake", 44.0f, 44.0f, Vec3(0.85f, 0.32f, 0.28f) },
            { "miniaudio_logo.png", "miniaudio", 120.0f, 18.0f, Vec3(0.95f, 0.78f, 0.25f) },
            { "lua_logo.png", "Lua 5.4", 42.0f, 42.0f, Vec3(0.25f, 0.65f, 0.95f) }
        };

        float cardW = 160.0f;
        float cardH = 72.0f;
        float gap = 20.0f;
        float totalBadgesW = partners.size() * cardW + (partners.size() - 1) * gap;
        float startBX = centerX - totalBadgesW * 0.5f;

        for (size_t i = 0; i < partners.size(); ++i) {
            float bx = startBX + i * (cardW + gap);
            Renderer::drawRect(bx, badgesY, cardW, cardH, Vec3(0.06f, 0.08f, 0.11f) * alpha);
            Renderer::drawRect(bx, badgesY + cardH - 2.0f, cardW, 2.0f, partners[i].accentCol * alpha);

            Texture* pTex = getTexture(partners[i].texName);
            if (pTex) {
                float imgX = bx + (cardW - partners[i].logoW) * 0.5f;
                float imgY = badgesY + 6.0f + (cardH - 22.0f - partners[i].logoH) * 0.5f;
                Renderer::drawTextureRect(imgX, imgY, partners[i].logoW, partners[i].logoH, *pTex, Vec3(1.0f, 1.0f, 1.0f) * alpha);
                float nameW = LabFont::getTextWidth(partners[i].name, 1.15f, LabFontType::System);
                LabFont::drawText(bx + (cardW - nameW) * 0.5f, badgesY + cardH - 18.0f, partners[i].name, 1.15f, Vec3(0.65f, 0.70f, 0.78f) * alpha, LabFontType::System);
            } else {
                float nameW = LabFont::getTextWidth(partners[i].name, 1.35f, LabFontType::System);
                LabFont::drawText(bx + (cardW - nameW) * 0.5f, badgesY + (cardH - 16.0f) * 0.5f, partners[i].name, 1.35f, partners[i].accentCol * alpha, LabFontType::System);
            }
        }

        std::string skipHint = "Press [ SPACE ], [ ENTER ] or [ ESC ] to skip";
        float hintW = LabFont::getTextWidth(skipHint, 1.15f, LabFontType::System);
        LabFont::drawText(centerX - hintW * 0.5f, h - 38.0f, skipHint, 1.15f, Vec3(0.35f, 0.40f, 0.48f) * alpha, LabFontType::System);

        Renderer::endUI();
    }

    void onRender() override {
        if (_splashActive) {
            renderSplashScreen();
            return;
        }

        if (_inMenu) {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            drawMenu();
            if (_devConsole.isOpen) {
                int w = 1280, h = 720;
                getWindowSize(w, h);
                _devConsole.render(w, h);
            }
            return;
        }

        // ==================== PASS 1: DYNAMIC SHADOW DEPTH PASS ====================
        Mat4 lightSpaceMatrix;
        if (_devConsole.r_shadows && _currentMap && _shadowMap.isInitialized()) {
            if (_shadowMap.getWidth() != _devConsole.r_shadow_res && _devConsole.r_shadow_res >= 512) {
                _shadowMap.init(_devConsole.r_shadow_res, _devConsole.r_shadow_res);
            }
            Vec3 sunDir = (_currentMap && _currentMap->metadata.sunDir.lengthSq() > 0.01f)
                ? _currentMap->metadata.sunDir
                : Vec3(-0.35f, -1.0f, -0.45f);
            lightSpaceMatrix = ShadowMap::computeSunLightSpaceMatrix(sunDir, _camera.getPosition(), 28.0f);
            _shadowMap.beginShadowPass(lightSpaceMatrix);
            Renderer::beginShadowDepthPass(lightSpaceMatrix);

            // Render map brushes into shadow map
            for (const auto& b : _currentMap->brushes) {
                if (b.type == "poly" && !b.customVertices.empty()) {
                    if (!b.runtimeMesh) b.runtimeMesh = std::make_shared<Mesh>(b.customVertices, b.customIndices);
                    Renderer::drawShadowMesh(*b.runtimeMesh, Vec3(0, 0, 0), Vec3(0, 0, 0), Vec3(1, 1, 1));
                } else {
                    Renderer::drawShadowCube(b.position, b.size);
                }
            }
            // Render props into shadow map
            for (const auto& p : _currentMap->props) {
                if (_meshes.contains(p.modelPath)) {
                    Renderer::drawShadowMesh(*_meshes[p.modelPath], p.position, p.rotation, p.scale);
                }
            }
            // Render doors into shadow map
            for (const auto& d : _currentMap->doors) {
                Vec3 animatedPos = d.position + d.openOffset * d.currentProgress;
                Renderer::drawShadowCube(animatedPos, d.size);
            }

            // Render Combat AI bots into shadow map
            _aiManager.renderShadowPass();

            // Render Local Player shadow in third person
            if (_devConsole.cam_thirdperson && _aiManager.skinnedMesh && _playerAnimator.getSkeleton()) {
                float pScale = _aiManager.skinnedMesh->getBaseScale(1.85f);
                float yOff = -_aiManager.skinnedMesh->getMinBounds().y * pScale;
                Vec3 pFeet = _camera.getPosition() - Vec3(0.0f, 1.70f, 0.0f);
                float modelYaw = std::atan2(_camera.getFront().x, _camera.getFront().z) * 180.0f / 3.14159265f;
                Mat4 pModel = Mat4::translate(pFeet + Vec3(0.0f, yOff, 0.0f)) *
                              Mat4::rotate(modelYaw * 3.14159265f / 180.0f, Vec3(0, 1, 0)) *
                              Mat4::scale(Vec3(pScale, pScale, pScale));
                Renderer::drawShadowSkinnedMesh(*_aiManager.skinnedMesh, pModel, _playerAnimator.getSkinMatrices());
            }

            // Render Dynamic Rigid Body Physics Props into shadow map
            _physicsWorld.renderShadow();

            Renderer::endShadowDepthPass();
            _shadowMap.endShadowPass(getWidth(), getHeight());
        }

        // Ensure post-process pipeline and camera aspect match physical framebuffer dimensions (prevents High-DPI blur)
        int curW = 1280, curH = 720;
        getFramebufferSize(curW, curH);
        if (curW > 0 && curH > 0) {
            _camera.setAspect((float)curW / (float)curH);
            if (curW != _postProcess.getWidth() || curH != _postProcess.getHeight()) {
                _postProcess.resize(curW, curH);
            }
        }

        // ==================== PASS 2: COLOR & LIGHTING SCENE PASS (HDR FBO) ====================
        _postProcess.setClearColor(_devConsole.wrd_skybox_color);
        _postProcess.beginScene();

        // Update Sun & Ambient Lighting (Handling Hammer Darkness Mode & wrd_light_ambient)
        if (_hammerEditor.active && _hammerEditor.darknessMode) {
            Renderer::setSunLight(
                Vec3(0.0f, -1.0f, 0.0f),
                Vec3(0.0f, 0.0f, 0.0f),
                Vec3(0.012f, 0.015f, 0.022f)
            );
        } else if (_currentMap) {
            Renderer::setSunLight(
                _currentMap->metadata.sunDir,
                _currentMap->metadata.sunColor,
                _devConsole.wrd_light_ambient
            );
        }

        // Atmospheric Volumetric Fog (wrd_fog & wrd_fog_density)
        if (_devConsole.wrd_fog) {
            float fogEnd = 1.0f / std::max(0.001f, _devConsole.wrd_fog_density);
            Renderer::setFog(true, _devConsole.wrd_skybox_color, 2.0f, fogEnd);
        } else {
            Renderer::setFog(false);
        }

        // Upload Dynamic Point Lights to Renderer
        if (_currentMap) {
            std::vector<PointLight> dynamicLights;
            dynamicLights.reserve(_currentMap->lights.size());
            for (const auto& lgt : _currentMap->lights) {
                dynamicLights.push_back(PointLight{
                    .position = lgt.position,
                    .color = lgt.color,
                    .intensity = lgt.intensity,
                    .radius = lgt.radius
                });
            }
            Renderer::setPointLights(dynamicLights);
        } else {
            Renderer::clearPointLights();
        }

        // Camera Thirdperson & Camera Shake Setup
        Vec3 origCamPos = _camera.getPosition();
        float origYaw = _camera.getYaw();
        float origPitch = _camera.getPitch();

        if (_camShakeTrauma > 0.0f) {
            float trauma2 = _camShakeTrauma * _camShakeTrauma;
            float t = static_cast<float>(glfwGetTime());
            _camera.setYaw(origYaw + std::sin(t * 45.0f) * 2.5f * trauma2);
            _camera.setPitch(origPitch + std::cos(t * 37.0f) * 1.8f * trauma2);
        }

        if (_devConsole.cam_thirdperson) {
            Vec3 tppOffset = _camera.getRight() * 0.45f + Vec3(0.0f, 0.35f, 0.0f) - _camera.getFront() * 2.5f;
            float targetDist = tppOffset.length();
            if (targetDist > 0.001f) {
                Vec3 rayDir = tppOffset / targetDist;
                float allowedDist = targetDist;
                if (_currentMap) {
                    auto solidBoxes = LabCollision::getMapSolidBoxes(*_currentMap);
                    for (const auto& box : solidBoxes) {
                        float tHit = 0.0f;
                        if (Raycast::rayIntersectAABB(origCamPos, rayDir, box.min, box.max, tHit)) {
                            if (tHit > 0.1f && tHit < allowedDist) {
                                allowedDist = std::max(0.2f, tHit - 0.25f);
                            }
                        }
                    }
                }
                _camera.setPosition(origCamPos + rayDir * allowedDist);
            }
        }

        Renderer::beginFrame(_camera);

        // Dynamic gunfire & projectile point lights (Source Engine style dynamic illumination)
        if (_muzzleFlashTime > 0.0f) {
            Vec3 flashWorldPos = _camera.getPosition() + _camera.getFront() * 0.75f;
            Renderer::addPointLight(flashWorldPos, Vec3(1.0f, 0.78f, 0.25f), 2.8f, 7.0f);
        }
        for (const auto& bot : _aiManager.bots) {
            if (bot.isAlive() && bot.muzzleFlashTimer > 0.0f) {
                Renderer::addPointLight(bot.position + Vec3(0.0f, 1.25f, 0.0f), Vec3(1.0f, 0.75f, 0.22f), 2.2f, 5.5f);
            }
        }
        for (const auto& proj : _weaponSystem.getProjectiles()) {
            if (proj.active) {
                Renderer::addPointLight(proj.position, proj.color, 2.4f, 6.0f);
            }
        }

        if (_devConsole.wrd_wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }

        if (_devConsole.r_shadows && _shadowMap.isInitialized()) {
            Renderer::setShadowMap(lightSpaceMatrix, _shadowMap.getDepthTexture());
        } else {
            Renderer::disableShadowMap();
        }

        if (_flashlight.enabled) {
            Renderer::setFlashlight(_flashlight.position, _flashlight.direction, _flashlight.color,
                                   _flashlight.innerCone, _flashlight.outerCone, _flashlight.range, _flashlight.intensity);
        } else {
            Renderer::disableFlashlight();
        }

        if (_currentMap) {
            // Render map brushes with Frustum Culling & Source Tri-Planar UV scaling
            for (const auto& b : _currentMap->brushes) {
                Vec3 halfSize = b.size * 0.5f;
                Vec3 bMin = b.position - halfSize;
                Vec3 bMax = b.position + halfSize;
                if (!_camera.isInFrustum(bMin, bMax)) continue;

                Texture* tex = b.texturePath.empty() ? nullptr : getTexture(b.texturePath);
                if (b.type == "poly" && !b.customVertices.empty()) {
                    if (!b.runtimeMesh) b.runtimeMesh = std::make_shared<Mesh>(b.customVertices, b.customIndices);
                    Renderer::drawMesh(*b.runtimeMesh, Vec3(0, 0, 0), Vec3(0, 0, 0), Vec3(1, 1, 1), b.color, tex);
                } else {
                    Renderer::drawCube(b.position, b.size, b.color, tex, true, b.uvScale, b.uvMode);
                }
            }

            // Render map props (STL models) with Frustum Culling
            for (const auto& p : _currentMap->props) {
                if (_meshes.contains(p.modelPath)) {
                    Vec3 halfScale = p.scale * 0.5f;
                    Vec3 pMin = p.position - halfScale;
                    Vec3 pMax = p.position + halfScale;
                    if (!_camera.isInFrustum(pMin, pMax)) continue;

                    Texture* tex = p.texturePath.empty() ? nullptr : getTexture(p.texturePath);
                    Renderer::drawMesh(*_meshes[p.modelPath], p.position, p.rotation, p.scale, p.color, tex);
                }
            }

            // Render procedural animated doors with Frustum Culling
            for (const auto& d : _currentMap->doors) {
                Vec3 animatedPos = d.position + d.openOffset * d.currentProgress;
                Vec3 halfSize = d.size * 0.5f;
                Vec3 dMin = animatedPos - halfSize;
                Vec3 dMax = animatedPos + halfSize;
                if (!_camera.isInFrustum(dMin, dMax)) continue;

                Renderer::drawCube(animatedPos, d.size, d.color);
            }
        }

        // Render Combat AI Bots
        _aiManager.render();

        // Render other connected multiplayer players from network snapshot
        if (_netClient.isConnected()) {
            const auto& snap = _netClient.getLatestSnapshot();
            for (uint16_t i = 0; i < snap.playerCount; ++i) {
                const auto& p = snap.players[i];
                if (p.clientId == _netClient.getClientId() || !p.isAlive) continue;

                Vec3 pFeet = p.position - Vec3(0.0f, 1.70f, 0.0f);
                if (_aiManager.skinnedMesh && _aiManager.skeleton) {
                    float pScale = _aiManager.skinnedMesh->getBaseScale(1.85f);
                    float yOff = -_aiManager.skinnedMesh->getMinBounds().y * pScale;
                    Mat4 pModel = Mat4::translate(pFeet + Vec3(0.0f, yOff, 0.0f)) *
                                  Mat4::rotate(p.yaw * 3.14159265f / 180.0f, Vec3(0, 1, 0)) *
                                  Mat4::scale(Vec3(pScale, pScale, pScale));
                    const auto& matrices = !_aiManager.bots.empty() ? _aiManager.bots[0].animator.getSkinMatrices()
                                                                    : (_playerAnimator.getSkeleton() ? _playerAnimator.getSkinMatrices() : std::vector<Mat4>());
                    Renderer::drawSkinnedMesh(*_aiManager.skinnedMesh, pModel,
                                              matrices,
                                              Vec3(0.35f, 0.65f, 0.95f), nullptr, true);
                } else {
                    Renderer::drawCylinder(pFeet + Vec3(0.0f, 0.92f, 0.0f), Vec3(0.0f, p.yaw, 0.0f), Vec3(0.55f, 1.85f, 0.35f), Vec3(0.2f, 0.5f, 0.85f));
                }
            }
        }

        // Render Local Player in Thirdperson mode (cam_thirdperson)
        if (_devConsole.cam_thirdperson) {
            Vec3 pFeet = origCamPos - Vec3(0.0f, 1.70f, 0.0f);
            float modelYaw = std::atan2(_camera.getFront().x, _camera.getFront().z) * 180.0f / 3.14159265f;
            if (_aiManager.skinnedMesh && _playerAnimator.getSkeleton()) {
                float pScale = _aiManager.skinnedMesh->getBaseScale(1.85f);
                float yOff = -_aiManager.skinnedMesh->getMinBounds().y * pScale;
                Mat4 pModel = Mat4::translate(pFeet + Vec3(0.0f, yOff, 0.0f)) *
                              Mat4::rotate(modelYaw * 3.14159265f / 180.0f, Vec3(0, 1, 0)) *
                              Mat4::scale(Vec3(pScale, pScale, pScale));
                Vec3 pColor = (_sessionConfig.mode == GameMode::TDM) ? Vec3(0.2f, 0.6f, 1.0f) : Vec3(0.2f, 0.85f, 0.45f);
                Renderer::drawSkinnedMesh(*_aiManager.skinnedMesh, pModel,
                                          _playerAnimator.getSkinMatrices(),
                                          pColor, nullptr, true);

                // Render equipped weapon in player's hands via hand socket
                int wIdx = static_cast<int>(_weaponSystem.getActiveWeapon().def.id);
                if (wIdx >= 0 && wIdx < 9) {
                    const Mesh* wMesh = _aiManager.weaponMeshes[wIdx] ? _aiManager.weaponMeshes[wIdx].get() : _aiManager.weaponMesh.get();
                    if (wMesh) {
                        const Texture* wTex = _aiManager.weaponTextures[wIdx].get();
                        const BotWeaponConfig* bCfg = &_aiManager.botWeaponConfigs[wIdx];
                        float invScale = (pScale > 0.00001f) ? (1.0f / pScale) : 1.0f;
                        Vec3 offset = bCfg ? bCfg->offset : Vec3(0.0f, -0.05f, 0.02f);
                        Vec3 rot = bCfg ? bCfg->rotation : Vec3(5.73f, -11.46f, 0.0f);
                        Vec3 scale = bCfg ? bCfg->scale : Vec3(1.0f, 1.0f, 1.0f);
                        Vec3 socketPos = offset * invScale;
                        Quat socketRot = Quat::fromEuler(rot.x * 3.14159265f / 180.0f,
                                                         rot.y * 3.14159265f / 180.0f,
                                                         rot.z * 3.14159265f / 180.0f);
                        Vec3 weaponScale = Vec3(0.016f * scale.x * invScale,
                                                0.016f * scale.y * invScale,
                                                0.016f * scale.z * invScale);
                        Mat4 weaponSocket = _playerAnimator.getSocketTransform("Socket_Weapon", pModel,
                            makeTransform(socketPos, socketRot, weaponScale));
                        Renderer::drawMesh(*wMesh, weaponSocket, Vec3(0.92f, 0.92f, 0.95f), wTex, true);
                    }
                }
            } else {
                Renderer::drawCylinder(pFeet + Vec3(0.0f, 0.92f, 0.0f), Vec3(0.0f, modelYaw, 0.0f), Vec3(0.55f, 1.85f, 0.35f), Vec3(0.2f, 0.8f, 0.4f));
            }
        }

        // Render Dynamic Rigid Body Physics Props (Crates, Cylindrical Barrels, Shattered Debris)
        _physicsWorld.render();

        // Render 3D World Pickups (Ammo crates, Medkits, and 1-min Weapon Spawn Pads)
        _pickups.render(_cachedWeaponMeshes, _cachedWeaponTextures);

        // Render 3D Bullet Tracers (Source / Half-Life 2 style additive luminous beams)
        for (const auto& tr : _tracers) {
            float alpha = std::clamp(1.0f - (tr.lifetime / (std::max)(0.01f, tr.maxLifetime)), 0.1f, 1.0f);
            Renderer::drawAdditiveTracer(tr.start, tr.end, tr.color * alpha, tr.thickness);
        }

        // Render Active Projectiles (RPG Rockets & Plasma Orbs)
        for (const auto& proj : _weaponSystem.getProjectiles()) {
            if (!proj.active) continue;
            float pSize = (proj.weaponId == WeaponID::RPG) ? 0.24f : 0.18f;
            Vec3 pDir = (proj.velocity.lengthSq() > 0.001f) ? proj.velocity.normalized() : Vec3(0, 0, -1);
            Renderer::drawMuzzleFlash(proj.position, pDir, pSize, proj.color);
        }

        // Render Dynamic Projective Decals (Bullet holes, blood splatters, scorch marks)
        _decalSystem.render(_camera);

        // Render In-World Interactive Terminals & Consoles (Sprint 8)
        _interactiveSystem.render(_camera);

        // Render 3D Particle System (Sparks, blood, smoke, fire, frost)
        _particleSystem.render(_camera);

        // Lab Hammer Editor 3D Ghost/Grid Overlay
        _hammerEditor.draw3DOverlay(_currentMap.get());

        // ==================== 3D DEBUG OVERLAYS (COLLIDERS, TRIGGERS, AI, PHYSICS) ====================
        if (_devConsole.log_colliders || _devConsole.log_all) {
            if (_currentMap) {
                for (const auto& b : _currentMap->brushes) {
                    Renderer::drawWireCube(b.position, b.size, Vec3(0.1f, 1.0f, 0.3f));
                }
            }
            for (const auto& bot : _aiManager.bots) {
                if (bot.isAlive()) {
                    Renderer::drawWireCube(bot.position + Vec3(0.0f, 0.92f, 0.0f), Vec3(0.6f, 1.85f, 0.6f), Vec3(1.0f, 0.2f, 0.2f));
                }
            }
            for (const auto& rb : _physicsWorld.bodies) {
                if (!rb->isDestroyed) {
                    Renderer::drawWireCube(rb->position, rb->halfExtents * 2.0f, Vec3(0.1f, 0.8f, 1.0f));
                }
            }
            Renderer::drawWireCube(origCamPos - Vec3(0.0f, 0.78f, 0.0f), Vec3(0.65f, 1.85f, 0.65f), Vec3(1.0f, 0.9f, 0.1f));
        }

        if (_devConsole.log_triggers || _devConsole.log_all) {
            if (_currentMap) {
                for (const auto& d : _currentMap->doors) {
                    Renderer::drawWireCube(d.position, d.size * 1.15f, Vec3(1.0f, 0.6f, 0.1f));
                }
                for (const auto& tr : _currentMap->triggers) {
                    Vec3 trCol = tr.triggered ? Vec3(0.4f, 0.5f, 0.55f) : Vec3(0.15f, 0.9f, 1.0f);
                    Renderer::drawWireCube(tr.position, tr.size, trCol);
                }
            }
            for (const auto& item : _pickups.items) {
                if (!item.collected) {
                    Renderer::drawWireCube(item.position, Vec3(0.8f, 0.8f, 0.8f), Vec3(0.9f, 0.2f, 0.9f));
                }
            }
            for (const auto& pad : _pickups.weaponPads) {
                Renderer::drawWireCube(pad.position + Vec3(0.0f, 0.4f, 0.0f), Vec3(1.2f, 0.8f, 1.2f), Vec3(0.4f, 0.7f, 1.0f));
            }
        }

        if (_devConsole.ai_sight || _devConsole.log_all) {
            for (const auto& bot : _aiManager.bots) {
                if (!bot.isAlive()) continue;
                float radYaw = bot.rotation.y * 3.14159265f / 180.0f;
                Vec3 fwd(std::sin(radYaw), 0.0f, std::cos(radYaw));
                Vec3 sightCenter = bot.position + Vec3(0.0f, 1.2f, 0.0f) + fwd * (bot.sightRange * 0.5f);
                Vec3 coneCol = (bot.state == AIState::Attack) ? Vec3(1.0f, 0.2f, 0.2f) :
                               (bot.state == AIState::Chase || bot.state == AIState::Alert) ? Vec3(1.0f, 0.8f, 0.1f) :
                               Vec3(0.2f, 0.9f, 0.3f);
                Renderer::drawWireCube(sightCenter, Vec3(bot.sightRange * 0.5f, 1.5f, bot.sightRange), coneCol);
            }
        }

        if (_devConsole.ai_paths || _devConsole.log_all) {
            for (const auto& bot : _aiManager.bots) {
                if (!bot.isAlive()) continue;
                Renderer::drawWireCube(bot.patrolStart, Vec3(0.4f, 0.4f, 0.4f), Vec3(0.2f, 0.5f, 1.0f));
                Renderer::drawWireCube(bot.patrolEnd, Vec3(0.4f, 0.4f, 0.4f), Vec3(1.0f, 0.3f, 0.6f));
                Vec3 pathMid = (bot.patrolStart + bot.patrolEnd) * 0.5f;
                Vec3 pathDiff = bot.patrolEnd - bot.patrolStart;
                float pLen = pathDiff.length();
                if (pLen > 0.1f) {
                    float pYaw = std::atan2(pathDiff.x, pathDiff.z) * 180.0f / 3.14159265f;
                    Renderer::drawCube(pathMid + Vec3(0.0f, 0.05f, 0.0f), Vec3(0.0f, pYaw, 0.0f), Vec3(0.06f, 0.06f, pLen), Vec3(0.3f, 0.8f, 1.0f), nullptr, false);
                }
            }
            if (_currentMap) {
                for (size_t i = 0; i < _currentMap->waypoints.size(); ++i) {
                    const auto& wp = _currentMap->waypoints[i];
                    Renderer::drawWireCube(wp.position, Vec3(0.5f, 0.5f, 0.5f), Vec3(1.0f, 0.85f, 0.2f));
                    if (i + 1 < _currentMap->waypoints.size()) {
                        Vec3 diff = _currentMap->waypoints[i + 1].position - wp.position;
                        float len = diff.length();
                        if (len > 0.05f) {
                            Vec3 mid = wp.position + diff * 0.5f;
                            float yaw = std::atan2(diff.x, diff.z) * 180.0f / 3.14159265f;
                            Renderer::drawCube(mid + Vec3(0, 0.1f, 0), Vec3(0, yaw, 0), Vec3(0.06f, 0.06f, len), Vec3(1.0f, 0.85f, 0.2f), nullptr, false);
                        }
                    }
                }
            }
        }

        if (_devConsole.phys_debug) {
            float vLen = _velocity.length();
            if (vLen > 0.1f) {
                Vec3 vMid = origCamPos + _velocity * 0.25f;
                float vYaw = std::atan2(_velocity.x, _velocity.z) * 180.0f / 3.14159265f;
                Renderer::drawCube(vMid, Vec3(0.0f, vYaw, 0.0f), Vec3(0.08f, 0.08f, vLen * 0.5f), Vec3(1.0f, 0.95f, 0.1f), nullptr, false);
            }
            Renderer::drawWireCube(origCamPos - Vec3(0.0f, 1.65f, 0.0f), Vec3(0.6f, 0.1f, 0.6f), _isGrounded ? Vec3(0.2f, 1.0f, 0.3f) : Vec3(1.0f, 0.2f, 0.2f));

            for (const auto& rb : _physicsWorld.bodies) {
                if (rb->isDestroyed) continue;
                float rLen = rb->linearVelocity.length();
                if (rLen > 0.1f) {
                    Vec3 rMid = rb->position + rb->linearVelocity * 0.25f;
                    float rYaw = std::atan2(rb->linearVelocity.x, rb->linearVelocity.z) * 180.0f / 3.14159265f;
                    Renderer::drawCube(rMid, Vec3(0.0f, rYaw, 0.0f), Vec3(0.06f, 0.06f, rLen * 0.5f), Vec3(1.0f, 0.5f, 0.1f), nullptr, false);
                }
            }
        }

        if (_devConsole.wrd_wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        // Viewmodel (only drawn when not in Hammer Editor mode and not in thirdperson)
        if (!_hammerEditor.active && !_devConsole.cam_thirdperson) {
            drawWeapon();
        }

        Renderer::disableShadowMap();
        Renderer::disableFlashlight();
        Renderer::endFrame();
        _postProcess.endScene();

        // Restore camera orientation and position if altered for thirdperson or camera shake
        if (_devConsole.cam_thirdperson || _camShakeTrauma > 0.0f) {
            _camera.setPosition(origCamPos);
            _camera.setYaw(origYaw);
            _camera.setPitch(origPitch);
        }

        // ==================== PASS 3: HDR TONEMAPPING, BLOOM & CRYO FROST ====================
        _postProcess.setBloomEnabled(_devConsole.r_bloom);
        _postProcess.setFXAAEnabled(_devConsole.r_fxaa);
        _postProcess.setPostProcessEnabled(_devConsole.r_postprocess);

        bool isCryo = (_currentMap && _currentMap->metadata.name.find("Cryo") != std::string::npos);
        float frost = PostProcessPipeline::calculateFrostVignette(_hud.health, _hud.maxHealth, isCryo);
        _postProcess.render(1.0f, frost, (float)glfwGetTime(), TonemapperType::ACESFilmic);

        // ==================== PASS 4: 2D HUD, CHAT & SCOREBOARD ====================
        drawUI();
    }

    void onShutdown() override {
        _netClient.disconnect();
        _localServer.stop();
#ifdef _WIN32
        if (_hasSpawnedServer && _dedicatedServerPI.hProcess) {
            TerminateProcess(_dedicatedServerPI.hProcess, 0);
            CloseHandle(_dedicatedServerPI.hProcess);
            CloseHandle(_dedicatedServerPI.hThread);
            _hasSpawnedServer = false;
        }
#endif
        NetworkSystem::shutdown();
        _shadowMap.shutdown();
        AudioEngine::shutdown();
        _particleSystem.shutdown();
        _decalSystem.shutdown();
        _interactiveSystem.shutdown();
        _scriptEngine.shutdown();
        _postProcess.shutdown();
        Renderer::shutdown();
    }

    bool launchDedicatedServer(const ServerConfig& cfg, std::string& outError) {
#ifdef _WIN32
        if (_hasSpawnedServer && _dedicatedServerPI.hProcess) {
            DWORD exitCode = 0;
            if (GetExitCodeProcess(_dedicatedServerPI.hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
                return true;
            }
            CloseHandle(_dedicatedServerPI.hProcess);
            CloseHandle(_dedicatedServerPI.hThread);
            _hasSpawnedServer = false;
        }

        const std::vector<std::string> searchPaths = {
            "LabServer.exe",
            "build\\Release\\LabServer.exe",
            "Release\\LabServer.exe",
            "bin\\LabServer.exe",
            "..\\build\\Release\\LabServer.exe"
        };
        std::string foundExe;
        for (const auto& p : searchPaths) {
            std::ifstream test(p);
            if (test.good()) {
                foundExe = p;
                break;
            }
        }
        if (foundExe.empty()) {
            outError = "LabServer.exe not found in working directory or build/Release";
            return false;
        }

        std::string modeFlag = (cfg.networkMode == NetMatchMode::P2P) ? " -p2p" : " -lan";
        std::string cmd = foundExe + " -config assets/configs/server.cfg -port " + std::to_string(cfg.port) + modeFlag;
        STARTUPINFOA si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&_dedicatedServerPI, sizeof(_dedicatedServerPI));

        char cmdBuffer[512];
        strncpy_s(cmdBuffer, cmd.c_str(), sizeof(cmdBuffer));

        if (CreateProcessA(NULL, cmdBuffer, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &_dedicatedServerPI)) {
            _hasSpawnedServer = true;
            return true;
        } else {
            outError = "Failed to launch process: " + std::to_string(GetLastError());
            return false;
        }
#else
        std::string modeFlag = (cfg.networkMode == NetMatchMode::P2P) ? " -p2p" : " -lan";
        std::string cmd = "./LabServer -config assets/configs/server.cfg -port " + std::to_string(cfg.port) + modeFlag + " &";
        int res = system(cmd.c_str());
        if (res == 0) {
            _hasSpawnedServer = true;
            return true;
        }
        outError = "POSIX system() failed";
        return false;
#endif
    }

private:
    Camera _camera;
    ParticleSystem _particleSystem;
    DecalSystem _decalSystem;
    InteractiveSystem _interactiveSystem;
    ScriptEngine _scriptEngine;
    PostProcessPipeline _postProcess;
    DedicatedServer _localServer;
    NetworkClient _netClient;
    ServerBrowser _serverBrowser;
    int _selectedServerIndex = 0;
    std::string _directConnectIP = "127.0.0.1:27015";
    bool _directConnectActive = false;
    bool _isConnecting = false;
    std::string _connectStatusMsg = "";
    float _connectAnimTimer = 0.0f;
    NATStatus _hostNATStatus;
    bool _hasQueriedNAT = false;
    NATStatus _guestNATStatus;
    bool _hasQueriedGuestNAT = false;
    std::string _punchTargetIP = "";
    bool _punchTargetActive = false;
    std::string _punchStatusMsg = "";
    std::string _pnpRoomCode = "7788";
    std::string _pnpJoinCode = "7788";
    bool _pnpJoinCodeActive = false;
#ifdef _WIN32
    PROCESS_INFORMATION _dedicatedServerPI{ 0 };
#endif
    bool _hasSpawnedServer = false;
    bool _isLocalHost = false;
    std::unique_ptr<LabMap> _currentMap;
    std::unordered_map<std::string, std::unique_ptr<Texture>> _textures;
    std::unordered_map<std::string, std::unique_ptr<Mesh>> _meshes;
    std::unordered_set<std::string> _missingTextures;
    std::unordered_set<std::string> _missingMeshes;
    std::vector<Mesh*> _cachedWeaponMeshes;
    std::vector<Texture*> _cachedWeaponTextures;

    // Session & Menu state
    GameSessionConfig _sessionConfig;
    MenuScreen _menuScreen = MenuScreen::Main;
    CharacterStudio _characterStudio;
    bool _inMenu = true;
    std::vector<std::string> _availableMaps;
    int _selectedMapIndex = 0;
    bool _escPressedLast = false;
    bool _oPressedLast = false;
    bool _menuLmbLast = false;
    bool _lastMenuKeys[512] = { false };

    // Movement state
    Vec3 _velocity;
    bool _isGrounded;
    bool _isJumping;
    float _bobTime;
    float _bhopGraceTimer = 0.0f;

    // Developer Console & Telemetry
    LabDevConsole _devConsole;
    float _camShakeTrauma = 0.0f;
    bool _gravePressedLast = false;
    bool _f10PressedLast = false;
    bool _tabPressedLast = false;
    bool _devConsoleWasOpen = false;
    bool _delPressedLast = false;
    bool _upPressedLast = false;
    bool _downPressedLast = false;
    bool _leftPressedLast = false;
    bool _rightPressedLast = false;
    bool _pgUpPressedLast = false;
    bool _pgDnPressedLast = false;

    // Combat & Respawn stats
    int _playerKills = 0;
    int _playerDeaths = 0;
    bool _isPlayerDead = false;
    float _playerRespawnTimer = 0.0f;

    // Chat & Pickups
    LabChat _chat;
    PickupManager _pickups;
    bool _yPressedLast = false;
    bool _tPressedLast = false;
    bool _enterPressedLast = false;

    // Weapon System & Switching State
    WeaponSystem _weaponSystem;
    bool _numPressedLast[9] = { false };
    bool _qPressedLast = false;
    bool _fireLmbLast = false;

    // Animation & Combat state
    FrozenLifeGame::WeaponAnimator _weaponAnimator;
    AIManager _aiManager;
    Animator _playerAnimator;
    float _playerShootTimer = 0.0f;
    PhysicsWorld _physicsWorld;
    std::vector<BulletTracer> _tracers;
    float _muzzleFlashTime;
    float _footstepTimer = 0.0f;

    // Hammer Editor & HUD
    LabHammerEditor _hammerEditor;
    LabHUD _hud;
    bool _f2PressedLast = false;
    bool _ePressedLast = false;
    bool _rPressedLast = false;
    bool _kPressedLast = false;
    bool _backspacePressedLast = false;
    bool _fPressedLast = false;
    bool _vPressedLast = false;
    bool _lPressedLast = false;

    // Real-Time Lighting & Shadow Mapping (Sprint 3)
    Flashlight _flashlight;
    ShadowMap _shadowMap;

    // Debug mode
    bool _debugMode = false;
    bool _wireframeMode = false;
    bool _f3PressedLast = false;
    bool _f1PressedLast = false;
    float _menuMouseX = 640.0f;
    float _menuMouseY = 360.0f;

    // Startup Splash Screen
    bool _showSplash = true;
    bool _splashActive = true;
    float _splashTimer = 0.0f;
    float _splashDuration = 2.5f;

    static FrozenLife* s_instance;
};

FrozenLife* FrozenLife::s_instance = nullptr;

int main(int argc, char* argv[]) {
    bool showSplash = true;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--nosplash" || arg == "-nosplash") {
            showSplash = false;
        }
    }
    FrozenLife game(showSplash);
    game.run();
    return 0;
}
