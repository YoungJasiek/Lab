#include "Lab.h"
#include "LabFont.h"
#include "LabDialogs.h"
#include "LabScene.h"
#include "LabGpuPreference.h"
#include "LabDevConsole.h"
#include "LabVFS.h"
#include "LabProfile.h"
#include "LabProject.h"
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
    FrozenLife(bool showSplash = true, const std::string& directMap = "")
        : Engine("Frozen-Life", 1280, 720),
          _showSplash(directMap.empty() ? showSplash : false),
          _splashActive(directMap.empty() ? showSplash : false),
          _splashTimer(0.0f),
          _splashDuration(2.5f),
          _initialDirectMap(directMap),
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
        LabLog::info("Frozen-Life Init: Modular .LMAP and GUI System...");
        LabMap::bakeUnbakedMapsInDirectory("assets/maps");
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
                path += ".lmap";
            }
            GameSessionConfig cfg = _sessionConfig;
            cfg.mapPath = path;
            startSession(cfg);
        };
        _devConsole.onRestartMap = [this]() {
            if (!_sessionConfig.mapPath.empty()) startSession(_sessionConfig);
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
            std::string text = file.empty() ? "Facility breach detected. Hold your positions!" : file;
            if (_currentMap) {
                _aiManager.triggerStoryEventOnBot("", text, "", *_currentMap);
            }
            _hud.showCombatMessage(text, 4.5f);
            _chat.addMessage("[Guard]", text, Vec3(0.3f, 0.9f, 1.0f));
            if (!file.empty() && std::filesystem::exists(file)) {
                AudioEngine::playCustomSound(file);
            }
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
                _menuScreen = MenuScreen::Options;
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
            _netClient.connect(ip, port, ProfileManager::get().getProfile().name);
            _isConnecting = true;
        };
        _devConsole.onNetDisconnect = [this]() {
            disconnectActiveSession();
        };
        _devConsole.onQuit = [this]() {
            glfwSetWindowShouldClose(getWindow(), 1);
        };

        // Discover available maps in assets/maps/
        // Initialize Career Achievements and Statistics
        initAchievements();

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

        // Start in Main Menu (or immediately load map if specified via +map command line)
        if (!_initialDirectMap.empty()) {
            std::string mapPath = _initialDirectMap;
            if (mapPath.find(".labscene") == std::string::npos && mapPath.find(".labmap") == std::string::npos && mapPath.find(".lmap") == std::string::npos) {
                if (mapPath.find('/') == std::string::npos && mapPath.find('\\') == std::string::npos) {
                    mapPath = "assets/maps/" + mapPath + ".lmap";
                } else {
                    mapPath += ".lmap";
                }
            }
            GameSessionConfig cfg;
            cfg.mapPath = mapPath;
            cfg.mode = GameMode::Campaign;
            cfg.enableBots = true;
            cfg.botCount = 1;
            startSession(cfg);
            _hasActiveGame = true;
            _inMenu = false;
            setCursorCaptured(true);
            LabLog::info("[FrozenLife] Direct map launch active: " + mapPath);
        } else {
            _hasActiveGame = false;
            _inMenu = true;
            _menuScreen = MenuScreen::Main;
            setCursorCaptured(false);
            loadSelectedMap("assets/maps/menu_background.lmap");
            _hasActiveGame = false;
            _inMenu = true;
            setCursorCaptured(false);
        }
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

        bool isGlb = false;
        {
            std::filesystem::path fp(path);
            std::string ext = fp.extension().string();
            for (char& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
            isGlb = (ext == ".glb" || ext == ".gltf");
        }
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
                Mesh* m = isGlb ? Mesh::loadGLB(p) : Mesh::loadSTL(p);
                if (m) {
                    Mesh* ptr = m;
                    _meshes[path] = std::unique_ptr<Mesh>(m);
                    return ptr;
                }
            }
        }
        Mesh* m = isGlb ? Mesh::loadGLB(path) : Mesh::loadSTL(path);
        if (m) {
            Mesh* ptr = m;
            _meshes[path] = std::unique_ptr<Mesh>(m);
            return ptr;
        }
        _missingMeshes.insert(path);
        return nullptr;
    }

    static std::string getMapTitle(const std::string& rawPath) {
        if (rawPath.empty()) return "Untitled Sector";
        std::filesystem::path p(rawPath);
        std::string stem = p.stem().string();
        if (stem == "site14_chapter1") return "Site 14 :: Chapter 1";
        if (stem == "site14_dirty_corridor") return "Site 14 :: Dirty Corridor";
        if (stem == "cryo_complex") return "Cryo Complex";
        if (stem == "cryo_outpost") return "Cryo Outpost";
        if (stem == "facility_alpha") return "Facility Alpha";
        if (stem == "darkness") return "Sector 07 :: Darkness";
        if (stem == "qa_labrunner_escape") return "LabRunner Escape";
        if (stem == "qa_magic_rpg") return "Magic RPG Arena";
        std::string title = stem;
        for (size_t i = 0; i < title.size(); ++i) {
            if (title[i] == '_') title[i] = ' ';
            else if (i == 0 || title[i - 1] == ' ') title[i] = static_cast<char>(::toupper(static_cast<unsigned char>(title[i])));
        }
        return title;
    }

    static std::string toSceneDisplayPath(const std::string& rawPath) {
        if (rawPath.empty()) return rawPath;
        std::filesystem::path p(rawPath);
        std::string ext = p.extension().string();
        for (char& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        if (ext == ".labmap" || ext == ".labscene") {
            p.replace_extension(".lmap");
        }
        std::string s = p.generic_string();
        return s;
    }

    void scanMapFiles() {
        _availableMaps.clear();
        std::vector<std::string> searchDirs = { "assets/maps", "../assets/maps", "../../assets/maps" };
        std::unordered_map<std::string, std::string> mapByStem;

        for (const auto& dir : searchDirs) {
            if (std::filesystem::exists(dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    std::string ext = entry.path().extension().string();
                    for (char& ch : ext) ch = static_cast<char>(::tolower(static_cast<unsigned char>(ch)));
                    std::string stem = entry.path().stem().string();
                    if (stem == "menu_background") continue; // Exclude internal menu scene

                    if (ext == ".lmap") {
                        mapByStem[stem] = entry.path().generic_string();
                    } else if (ext == ".labscene" || ext == ".labmap") {
                        if (!mapByStem.contains(stem)) {
                            mapByStem[stem] = entry.path().generic_string();
                        }
                    }
                }
                if (!mapByStem.empty()) break;
            }
        }

        for (const auto& [stem, p] : mapByStem) {
            _availableMaps.push_back(p);
        }
        std::sort(_availableMaps.begin(), _availableMaps.end());

        if (_availableMaps.empty()) {
            _availableMaps.push_back("assets/maps/site14_chapter1.lmap");
            _availableMaps.push_back("assets/maps/facility_alpha.lmap");
            _availableMaps.push_back("assets/maps/cryo_outpost.lmap");
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
                _pickups.addWeaponPad(ws.weaponId, ws.position, ws.respawnTime, ws.yaw, ws.prefabRef);
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

            // Switch to gameplay mode and lock cursor if active game
            if (_hasActiveGame) {
                _inMenu = false;
                setCursorCaptured(true);
            }
            LabLog::info("Scene successfully loaded: " + displayPath);
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

    void beginLoadingSequence(const GameSessionConfig& config, bool isMultiplayer = false) {
        _loadingState.begin(config, isMultiplayer);
        _inMenu = true;
        _menuScreen = MenuScreen::LoadingMap;
        setCursorCaptured(false);
    }

    void startSession(const GameSessionConfig& config) {
        _sessionConfig = config;
        _sessionConfig.mapPath = toSceneDisplayPath(_sessionConfig.mapPath);
        _hasActiveGame = true;
        if (!loadSelectedMap(_sessionConfig.mapPath)) {
            LabLog::error("[FrozenLife] Aborting session start: failed to load scene " + _sessionConfig.mapPath);
            _hasActiveGame = false;
            return;
        }

        bool isCampaignMode = (_sessionConfig.mode == GameMode::Campaign) && !_netClient.isConnected() && !_localServer.isRunning();
        _devConsole.isPublicMultiplayerServer = (_netClient.isConnected() && !_isLocalHost);
        if (_devConsole.isPublicMultiplayerServer) {
            _devConsole.sv_cheats = false;
            _devConsole.sp_cheats = false;
            _devConsole.sp_god = false;
            _devConsole.sp_noclip = false;
            _devConsole.sp_speed = 1.0f;
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

        if (_netClient.isConnected()) {
            Vec3 authSpawn = _netClient.getAssignedSpawnPos();
            if (authSpawn.lengthSq() > 0.001f) {
                _camera.setPosition(authSpawn);
            }
            _netClient.getPrediction().reset();
        }

        if (_currentMap && !_currentMap->doors.empty()) {
            if (isCampaignMode) {
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

                // Door is physically locked shut in Campaign mode until biometric retinal authorization
                _currentMap->doors[0].isLocked = true;
                _currentMap->doors[0].isOpen = false;
            } else {
                // Arcade / Multiplayer mode: bypass story locks so arena flow is unrestricted
                for (auto& d : _currentMap->doors) {
                    d.isLocked = false;
                }
            }
        }

        _chat.addMessage("[SERVER]", "Welcome to Frozen-Life :: " + (_currentMap ? _currentMap->metadata.name : "Sector"), Vec3(0.3f, 0.8f, 1.0f));
        _chat.addMessage("[SERVER]", "Mode: " + _sessionConfig.getModeString() + " | Frag Limit: " + std::to_string(_sessionConfig.fragLimit), Vec3(0.3f, 0.8f, 1.0f));
        _chat.addMessage("[SYSTEM]", "Hold [TAB] for scoreboard, press [Y] for chat, [R] to reload", Vec3(1.0f, 0.9f, 0.3f));

        if (_sessionConfig.enableBots && _sessionConfig.botCount > 0) {
            _aiManager.spawnBotsForMap(_currentMap.get(), _sessionConfig.botCount, _sessionConfig.mode);
            _hud.showCombatMessage("MATCH HOSTED: " + _sessionConfig.getModeString() + " WITH " + std::to_string(_sessionConfig.botCount) + " BOTS", 3.0f);
        } else if (isCampaignMode && _currentMap && (!_currentMap->triggers.empty() || !_currentMap->waypoints.empty())) {
            // Campaign / Story Mode only: spawn scripted corridor bot for Trigger & Waypoint sequences
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
        if (ScriptEngine::getHUDText("cod_intro_status") == "ACTIVE") {
            float dur = ScriptEngine::getHUDValue("cod_intro_duration");
            _codIntroDuration = (dur > 0.5f) ? dur : 4.5f;
            _codIntroTimer = 0.01f;
        } else {
            _codIntroTimer = 0.0f;
        }
        setCursorCaptured(true);
    }

    void disconnectActiveSession() {
        _hasActiveGame = false;
        _isConnecting = false;
        _loadingState.active = false;
        _devConsole.isPublicMultiplayerServer = false;
        _netClient.disconnect();
        _localServer.stop();
        _aiManager.clear();
        _pickups.clear();
        _interactiveSystem.clear();
        _particleSystem.clear();
        _decalSystem.clear();
        _isPlayerDead = false;
        loadSelectedMap("assets/maps/menu_background.lmap");
        _hasActiveGame = false;
        _inMenu = true;
        _menuScreen = MenuScreen::Main;
        setCursorCaptured(false);
    }

    void onFixedUpdate(float fixedDelta) override {
        if (_inMenu) return;

        // Apply World Timescale from console
        fixedDelta *= _devConsole.wrd_timescale;

        // Freeze movement if player is dead or typing in chat / console
        if (_isPlayerDead) {
            _velocity = { 0, 0, 0 };
            if (_netClient.isConnected()) {
                uint32_t netButtons = 0;
                if (Input::isKeyPressed(32) || Input::isKeyPressed(257)) netButtons |= NetButton_Jump;
                _netClient.update(fixedDelta, _camera.getPosition(), _velocity, _camera.getYaw(), _camera.getPitch(), netButtons, 0.0f, 0.0f, static_cast<uint8_t>(_weaponSystem.getActiveId()));
            }
            return;
        }
        if (_chat.isOpen || _devConsole.isOpen) {
            _velocity.x *= 0.8f;
            _velocity.z *= 0.8f;
            if (_netClient.isConnected()) {
                _netClient.update(fixedDelta, _camera.getPosition(), _velocity, _camera.getYaw(), _camera.getPitch(), 0, 0.0f, 0.0f, static_cast<uint8_t>(_weaponSystem.getActiveId()));
            }
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
                if (_bhopGraceTimer > 0.0f) {
                    _careerStats.bhopJumps++;
                    updateAchievements();
                }
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

        // Procedural Door animations, activator mechanisms and dependencies
        if (_currentMap) {
            for (auto& door : _currentMap->doors) {
                // Dependency check: if door depends on another trigger/power switch
                bool dependencySatisfied = true;
                if (!door.dependency.empty()) {
                    dependencySatisfied = false;
                    for (const auto& tr : _currentMap->triggers) {
                        if (tr.name == door.dependency && tr.triggered) {
                            dependencySatisfied = true;
                            break;
                        }
                    }
                    if (!dependencySatisfied) {
                        for (const auto& otherDoor : _currentMap->doors) {
                            if (otherDoor.name == door.dependency && otherDoor.isOpen) {
                                dependencySatisfied = true;
                                break;
                            }
                        }
                    }
                }

                if (!door.isLocked && dependencySatisfied && !door.requiresKeycard) {
                    float distSq = (pos - door.position).lengthSq();
                    float triggerRadiusSq = door.triggerRadius * door.triggerRadius;
                    if (door.activator == "proximity" || door.activator.empty()) {
                        door.isOpen = (distSq < triggerRadiusSq);
                    } else if (door.activator == "trigger_only") {
                        // Keeps its triggered open state; closes only if autoClose is true and player left radius
                        if (door.isOpen && door.autoClose && distSq >= triggerRadiusSq * 1.5f) {
                            door.isOpen = false;
                        }
                    } else if (door.activator == "manual") {
                        // Interactive key 'E' / use in proximity
                        if (distSq < triggerRadiusSq && Input::isKeyJustPressed(GLFW_KEY_E)) {
                            door.isOpen = !door.isOpen;
                        }
                    }
                } else if (!dependencySatisfied || door.isLocked) {
                    door.isOpen = false;
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

        // Always tick local Listen Server so Host responds to clients even while in menu, loading, console, or dead
        if (_localServer.isRunning()) {
            _localServer.tick(time.delta);
        }

        // Always progress pending client connection (works from JoinGame menu AND from developer console `connect`)
        if (_isConnecting) {
            _connectAnimTimer += time.delta;
            _netClient.update(time.delta, _camera.getPosition(), _velocity, _camera.getYaw(), _camera.getPitch(), 0, 0.0f, 0.0f, 0);
            if (_netClient.isConnected()) {
                _isConnecting = false;
                _connectStatusMsg = "CONNECTED TO SERVER! JOINING ARENA...";
                std::string srvMap = _netClient.getConnectedMapName();
                if (srvMap.empty()) srvMap = "assets/maps/facility_alpha.lmap";
                if (srvMap.find('/') == std::string::npos && srvMap.find('\\') == std::string::npos) {
                    srvMap = "assets/maps/" + srvMap;
                }
                _sessionConfig.mapPath = srvMap;
                _sessionConfig.mode = GameMode::FFA;
                _sessionConfig.enableBots = false; // Never spawn local bots on guest
                _sessionConfig.botCount = 0;
                _isLocalHost = _localServer.isRunning();
                _serverBrowser.addHistory("127.0.0.1", _sessionConfig.port, "Connected Arena Server", srvMap);
                if (_devConsole.isOpen) {
                    _devConsole.isOpen = false;
                    _devConsoleWasOpen = false;
                }
                beginLoadingSequence(_sessionConfig, true);
                return;
            } else if (_netClient.getState() == ClientConnectionState::TimedOut) {
                _isConnecting = false;
                _connectStatusMsg = "FAILED TO CONNECT: TIMEOUT! Check Port 27015 / UDP Punch or connect via Radmin VPN";
                _netClient.disconnect();
            }
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
            int winW = 1280, winH = 720;
            getWindowSize(winW, winH);
            double curX = 0.0, curY = 0.0;
            getCursorPos(curX, curY);
            if (curX == 0.0 && curY == 0.0) {
                curX = static_cast<double>(Input::getMouseX());
                curY = static_cast<double>(Input::getMouseY());
            }
            float mx = (winW > 0) ? (static_cast<float>(curX) * 1280.0f / static_cast<float>(winW)) : static_cast<float>(curX);
            float my = (winH > 0) ? (static_cast<float>(curY) * 720.0f / static_cast<float>(winH)) : static_cast<float>(curY);

            _devConsole.onMouseMove(mx, my);

            if (_devConsole.isDragging && !Input::isMouseButtonPressed(0)) {
                _devConsole.isDragging = false;
            }
            if (_devConsole.isResizing && !Input::isMouseButtonPressed(0)) {
                _devConsole.isResizing = false;
            }

            if (Input::isMouseButtonJustPressed(0)) {
                _devConsole.onMouseButton(0, 1, mx, my);
            } else if (Input::isMouseButtonJustReleased(0)) {
                _devConsole.onMouseButton(0, 0, mx, my);
            }

            if (Input::isMouseButtonJustPressed(1)) {
                _devConsole.onMouseButton(1, 1, mx, my);
            } else if (Input::isMouseButtonJustReleased(1)) {
                _devConsole.onMouseButton(1, 0, mx, my);
            }

            if (Input::scrollDelta != 0.0f) {
                _devConsole.onMouseScroll(Input::scrollDelta);
                Input::scrollDelta = 0.0f;
            }

            bool shift = Input::isKeyPressed(GLFW_KEY_LEFT_SHIFT) || Input::isKeyPressed(GLFW_KEY_RIGHT_SHIFT);
            bool ctrl = Input::isKeyPressed(GLFW_KEY_LEFT_CONTROL) || Input::isKeyPressed(GLFW_KEY_RIGHT_CONTROL);

            // Forward control keys directly to developer console
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
            if (_netClient.isConnected()) {
                _netClient.update(time.delta, _camera.getPosition(), _velocity, _camera.getYaw(), _camera.getPitch(), 0, 0.0f, 0.0f, static_cast<uint8_t>(_weaponSystem.getActiveId()));
            }
            int winW = 1280, winH = 720;
            getWindowSize(winW, winH);
            int fbW = 1280, fbH = 720;
            getFramebufferSize(fbW, fbH);

            // Real-time 3D cinematic background camera dolly & orbit around the assembly line
            _menuCamTime += time.delta * 0.12f;
            float r = 9.0f;
            float cx = std::sin(_menuCamTime) * r;
            float cz = std::cos(_menuCamTime * 0.65f) * r;
            float cy = 2.4f + std::sin(_menuCamTime * 0.45f) * 0.5f;
            _camera.setPosition(Vec3(cx, cy, cz));
            _camera.lookAt(Vec3(0.0f, 1.2f, 0.0f));

            // Mouse wheel scrolling for map lists in Singleplayer and HostGame
            if (Input::scrollDelta != 0.0f) {
                if (_menuScreen == MenuScreen::Singleplayer || _menuScreen == MenuScreen::HostGame) {
                    if (Input::scrollDelta < 0.0f) {
                        _mapScrollOffset = std::min(std::max(0, (int)_availableMaps.size() - 7), _mapScrollOffset + 1);
                    } else if (Input::scrollDelta > 0.0f) {
                        _mapScrollOffset = std::max(0, _mapScrollOffset - 1);
                    }
                }
                Input::scrollDelta = 0.0f;
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

            if (_menuScreen == MenuScreen::LoadingMap) {
                if (_loadingState.step(time.delta)) {
                    _loadingState.active = false;
                    startSession(_loadingState.pendingConfig);
                    if (_loadingState.isMultiplayer && _netClient.isConnected()) {
                        _chat.addMessage("[CLIENT]", "Connected to server " + _netClient.getServerAddressString() + "!", Vec3(0.2f, 0.9f, 0.35f));
                        _hud.showCombatMessage("CONNECTED TO MULTIPLAYER ARENA!", 3.0f);
                    }
                    return;
                }
                if (lmbClick) {
                    const float loadW = 720.0f;
                    const float loadH = 420.0f;
                    const float loadX = (1280.0f - loadW) * 0.5f;
                    const float loadY = (720.0f - loadH) * 0.5f;
                    // Cancel button
                    if (mx >= loadX + loadW - 150.0f && mx <= loadX + loadW - 20.0f && my >= loadY + loadH - 50.0f && my <= loadY + loadH - 16.0f) {
                        _loadingState.active = false;
                        _isConnecting = false;
                        _netClient.disconnect();
                        _menuScreen = MenuScreen::Main;
                        return;
                    }
                }
                return;
            }

            if (_menuScreen == MenuScreen::JoinGame) {
                _serverBrowser.update(time.delta);
                // F5 Refresh shortcut
                if (Input::isKeyPressed(294)) {
                    _serverBrowser.refresh();
                }
            }

            handleMenuInput(mx, my, lmbClick);
            return;
        }

        // Poll incoming network events (chat, authoritative damage, kills, projectiles, snapshots)
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

            if (_netClient.hasNewSnapshot()) {
                const auto& snap = _netClient.getLatestSnapshot();
                for (uint16_t i = 0; i < snap.playerCount; ++i) {
                    if (snap.players[i].clientId == _netClient.getClientId()) {
                        if (_isPlayerDead && snap.players[i].isAlive != 0 && _playerRespawnTimer <= 2.5f) {
                            _isPlayerDead = false;
                            _hud.health = snap.players[i].health > 0.0f ? snap.players[i].health : _scriptEngine.getPlayerMaxHealth();
                            _hud.suitArmor = snap.players[i].armor;
                            _camera.setPosition(snap.players[i].position);
                            _velocity = { 0, 0, 0 };
                            _netClient.getPrediction().reset();
                            _weaponSystem.reset();
                            for (const auto& wdef : _scriptEngine.getWeaponDefinitions()) {
                                _weaponSystem.applyScriptOverrides(wdef.id, wdef.damage, wdef.fireRate, wdef.clipSize, wdef.maxReserve, wdef.splashDamage, wdef.splashRadius);
                            }
                            syncHudWeapon();
                            _chat.addMessage("[SERVER]", "Player respawned at designated base!", Vec3(0.3f, 0.85f, 1.0f));
                            _hud.showCombatMessage("RESPAWNED - READY FOR COMBAT!", 2.5f);
                        } else if (!_isPlayerDead && snap.players[i].isAlive != 0) {
                            float errorThreshold = _isLocalHost ? 2.5f : 0.45f;
                            Vec3 correctedPos, correctedVel;
                            if (_netClient.getPrediction().reconcile(snap.lastProcessedCmd, snap.players[i].position, snap.players[i].velocity, correctedPos, correctedVel, errorThreshold)) {
                                _camera.setPosition(correctedPos);
                                _velocity = correctedVel;
                            }
                        }
                        break;
                    }
                }
                _netClient.consumeSnapshot();
            }
        }

        // Handle Match Intermission timer
        if (_isIntermission) {
            _intermissionTimer -= time.delta;
            if (_intermissionTimer <= 0.0f) {
                _isIntermission = false;
                _playerKills = 0;
                _playerDeaths = 0;
                _hud.frags = 0;
                if (_devConsole.onRestartRound) _devConsole.onRestartRound();
            }
        } else {
            checkMatchEndCondition();
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
                _netClient.getPrediction().reset();
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
                if (true) {
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
                if (!_qPressedLast) {
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
                    if (!_numPressedLast[k]) {
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

        if (!_inMenu && _codIntroTimer > 0.0f) {
            _codIntroTimer += time.delta;
            if (_codIntroTimer > _codIntroDuration) {
                _codIntroTimer = 0.0f;
            }
        }

        if (!_inMenu && _currentMap != nullptr) {
            _careerStats.totalPlaytimeSeconds += time.delta;
        }

        // Ambient drifting weather (Snow/Frost for Cryo maps, subtle industrial dust for others)
        if (!_inMenu && !_isPlayerDead) {
            bool isCryo = (_currentMap && _currentMap->metadata.name.find("Cryo") != std::string::npos);
            _particleSystem.spawnAmbientWeather(_camera.getPosition(), 2, isCryo);
        }

        // ==================== PLAYER COMBAT: 9 DISTINCT WEAPONS & FIRING MODES ====================
        bool isFireHeld = Input::isMouseButtonPressed(0);
        auto& activeWep = _weaponSystem.getActiveWeapon();
        const auto& def = activeWep.def;

        if (def.id == WeaponID::Minigun && isFireHeld && !_chat.isOpen && !_isPlayerDead) {
            _weaponSystem.addMinigunSpin(time.delta * 720.0f);
        }

        bool triggerFire = false;
        if (def.isAutomatic) {
            triggerFire = isFireHeld && (_weaponSystem.getFireCooldown() <= 0.0f);
        } else {
            triggerFire = isFireHeld && (!_fireLmbLast) && (_weaponSystem.getFireCooldown() <= 0.0f);
        }
        _fireLmbLast = isFireHeld;

        if (triggerFire && !_chat.isOpen && !_isPlayerDead) {
            _careerStats.shotsFired++;
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
                                        recordPlayerKill(hit.isHeadshot, false);
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
                                            recordPlayerKill(hit.isHeadshot, false);
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
                            recordPlayerKill(false, false);
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
                            recordPlayerKill(false, true);
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
            // Check Story / Gameplay Trigger Volumes against player position (Campaign / Story Mode only!)
            bool isCampaignMode = (_sessionConfig.mode == GameMode::Campaign) && !_netClient.isConnected() && !_localServer.isRunning();
            if (isCampaignMode) {
                auto firedTriggers = LabCollision::checkTriggerVolumes(_camera.getPosition(), *_currentMap);
                for (int trigIdx : firedTriggers) {
                    if (trigIdx >= 0 && trigIdx < (int)_currentMap->triggers.size()) {
                        auto& trig = _currentMap->triggers[trigIdx];
                        AIManager::executeTrigger(trig, *_currentMap, &_chat);
                        _aiManager.triggerStoryEventOnBot(trig.targetBot, trig.dialogText, trig.audioPath, *_currentMap);
                        if (!trig.dialogText.empty()) {
                            _hud.showCombatMessage(trig.dialogText, 4.5f);
                        }
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
        float armorAdded = 0.0f;
        int weaponUnlocked = -1;
        std::string pickupNotice;
        bool weaponRespawned = false;
        std::string customItem;
        _pickups.update(time.delta, _camera.getPosition(), ammoAdded, healthAdded, armorAdded, weaponUnlocked, pickupNotice, weaponRespawned, customItem);
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
            _chat.addMessage("[ARSENAL]", "An item has respawned on the arena pad!", Vec3(0.3f, 0.7f, 0.9f));
            AudioEngine::playSound(SoundID::WeaponSpawn);
        }
        if (armorAdded > 0.0f) {
            _hud.suitArmor = std::min(100.0f, _hud.suitArmor + armorAdded);
            _hud.showCombatMessage(pickupNotice, 2.0f);
            _chat.addMessage("[SUIT]", pickupNotice, Vec3(1.0f, 0.65f, 0.1f));
            _particleSystem.spawnBeamSparks(_camera.getPosition(), _camera.getPosition() + Vec3(0, 0.8f, 0), Vec3(1.0f, 0.65f, 0.1f), 12);
            AudioEngine::playSound(SoundID::PickupAmmo);
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
        if (!customItem.empty()) {
            _hud.showCombatMessage(pickupNotice, 2.0f);
            _chat.addMessage("[MOD]", pickupNotice, Vec3(0.8f, 0.3f, 1.0f));
            AudioEngine::playSound(SoundID::WeaponSpawn);
        }

        // Update active Bullet Tracers
        for (auto it = _tracers.begin(); it != _tracers.end(); ) {
            it->lifetime += time.delta;
            if (it->isExpired()) it = _tracers.erase(it);
            else ++it;
        }

        // Weapon Reload (R key)
        if (!_inMenu && (Input::isKeyPressed('R') || Input::isKeyPressed('r'))) {
            if (!_chat.isOpen && !_isPlayerDead) {
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
        if (!_inMenu && (Input::isKeyPressed('F') || Input::isKeyPressed('f'))) {
            if (!_chat.isOpen && !_isPlayerDead && !_fPressedLast) {
                _flashlight.toggle();
                AudioEngine::playSound(SoundID::FlashlightToggle);
                _hud.showCombatMessage(_flashlight.enabled ? "FLASHLIGHT: ACTIVATED" : "FLASHLIGHT: DEACTIVATED", 1.5f);
                _fPressedLast = true;
            }
        } else if (!_inMenu) {
            _fPressedLast = false;
        }

        // Weapon Inspect (V key)
        if (!_inMenu && (Input::isKeyPressed('V') || Input::isKeyPressed('v'))) {
            if (!_chat.isOpen && !_isPlayerDead && !_vPressedLast) {
                if (!_weaponAnimator.isReloading() && !_weaponAnimator.isInspecting()) {
                    _weaponAnimator.onInspect(2.4f);
                }
                _vPressedLast = true;
            }
        } else if (!_inMenu) {
            _vPressedLast = false;
        }

        // Update tactical flashlight beam kinematics
        _flashlight.update(_camera.getPosition(), _camera.getFront(), _camera.getRight(), _camera.getUp());

        // Procedural Weapon Sway and Bob update
        if (!_inMenu) {
            float horizontalSpeed = std::sqrt(_velocity.x * _velocity.x + _velocity.z * _velocity.z);
            _weaponAnimator.update(time.delta, Input::mouseDelta, _devConsole.cam_bob ? horizontalSpeed : 0.0f);
        }

        // Feed real dynamic player values to HUD
        if (_currentMap) _hud.mapName = _currentMap->metadata.name;
        _hud.update(time.delta);



        // F3 Debug Mode Toggle
        if (Input::isKeyPressed(292)) { // GLFW_KEY_F3
            if (!_f3PressedLast) {
                if (_netClient.isConnected() && _devConsole.sv_cheats == 0) {
                    _chat.addMessage("[CONSOLE]", "Cannot use F3 debug cheat on connected server without sv_cheats 1", Vec3(1.0f, 0.35f, 0.35f));
                } else {
                    _debugMode = !_debugMode;
                    _weaponSystem.unlockAll();
                    syncHudWeapon();
                    LabLog::info("Debug mode: " + std::string(_debugMode ? "ON" : "OFF") + " (All weapons unlocked)");
                    _chat.addMessage("[SYSTEM]", "Debug mode ON: All 9 weapons unlocked!", Vec3(0.3f, 0.95f, 0.5f));
                }
                _f3PressedLast = true;
            }
        } else {
            _f3PressedLast = false;
        }

        // Interactive In-World Systems & Usable Entities ([E] Key)
        if (!_inMenu && !_isPlayerDead) {
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

            if (useJustPressed && _currentMap) {
                RaycastHit botHit;
                if (_aiManager.testRaycast(_camera.getPosition(), _camera.getFront(), botHit) && botHit.distance < 4.5f) {
                    static const char* guardQuotes[] = {
                        "Stay sharp, soldier. Sector 14 is on red alert.",
                        "Command wants full perimeter lockdown. Keep your eyes open.",
                        "All clear here. Move to your assigned checkpoint.",
                        "Watch the ventilation shafts. Sensors picked up movement."
                    };
                    static size_t qIdx = 0;
                    std::string speech = guardQuotes[qIdx % 4];
                    qIdx++;
                    _aiManager.triggerStoryEventOnBot("Guard", speech, "", *_currentMap);
                    _hud.showCombatMessage(speech, 4.0f);
                    _chat.addMessage("[Guard]", speech, Vec3(0.3f, 0.9f, 1.0f));
                }
            }
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

    void refreshDiscoveredAddons() {
        _discoveredAddons.clear();

        // Built-in core game assets item
        AddonItem base;
        base.folderName = "assets (Core)";
        base.title = "Frozen-Life Core Content";
        base.version = "0.4.0";
        base.author = "Lab Engine Foundation";
        base.description = "Official base game assets, core maps, and character viewmodels.";
        base.enabled = true;
        base.mapCount = static_cast<int>(_availableMaps.size());
        _discoveredAddons.push_back(base);

        std::filesystem::path addonsDir("addons");
        if (!std::filesystem::exists(addonsDir)) {
            try { std::filesystem::create_directories(addonsDir); } catch (...) {}
        }

        auto manifests = VFS::discoverAddons(addonsDir);
        for (const auto& m : manifests) {
            AddonItem mod;
            mod.folderName = m.folderName;
            mod.title = m.title;
            mod.version = m.version;
            mod.author = m.author;
            mod.description = m.description;
            mod.enabled = m.enabled;
            mod.mapCount = m.mapCount;
            _discoveredAddons.push_back(mod);

            if (mod.enabled) {
                std::filesystem::path addonPath = addonsDir / mod.folderName;
                VFS::get().mountAddonLayer(addonPath);
                if (!m.startupScript.empty()) {
                    _scriptEngine.executeStartupHook((addonPath / m.startupScript).string());
                }
            }
        }
    }

    void initAchievements() {
        _achievements.clear();
        _achievements.push_back({
            "first_blood",
            "FIRST BLOOD",
            "Eliminate your first enemy combatant or bot in combat.",
            "[★]",
            1, 1, true, "2026-10-24",
            "assets/textures/achievements/ach_first_blood.png"
        });
        _achievements.push_back({
            "sharpshooter",
            "DEADEYE MARKSMAN",
            "Eliminate 10 enemies with precise headshots.",
            "[🎯]",
            10, 10, true, "2026-10-24",
            "assets/textures/achievements/ach_sharpshooter.png"
        });
        _achievements.push_back({
            "bhop_master",
            "BUNNYHOP MASTER",
            "Perform 15 consecutive fluid bunny hops using Source physics.",
            "[⚡]",
            15, 15, true, "2026-10-24",
            "assets/textures/achievements/ach_bhop_master.png"
        });
        _achievements.push_back({
            "demolition",
            "DEMOLITION EXPERT",
            "Detonate 5 explosive barrels with weapon fire or physics impacts.",
            "[☣]",
            5, 5, true, "2026-10-24",
            "assets/textures/achievements/ach_demolition.png"
        });
        _achievements.push_back({
            "cryo_survivor",
            "CRYO SURVIVOR",
            "Survive cryogenic lockdown and clear the primary facility sector.",
            "[🏆]",
            1, 1, true, "2026-10-24",
            "assets/textures/achievements/ach_cryo_survivor.png"
        });
        _achievements.push_back({
            "heavy_arsenal",
            "HEAVY ARSENAL",
            "Test fire all 9 experimental Lab weapons in combat.",
            "[⚔]",
            6, 9, false, "",
            "assets/textures/achievements/ach_heavy_arsenal.png"
        });
        _achievements.push_back({
            "mod_creator",
            "COMMUNITY PIONEER",
            "Load a custom map or addon package from the /addons directory.",
            "[⚙]",
            1, 1, (!_discoveredAddons.empty()), (!_discoveredAddons.empty() ? "2026-10-24" : ""),
            "assets/textures/achievements/ach_mod_creator.png"
        });
        _achievements.push_back({
            "untouchable",
            "UNTOUCHABLE",
            "Eliminate 5 consecutive enemies without taking health damage.",
            "[💀]",
            3, 5, false, "",
            "assets/textures/achievements/ach_untouchable.png"
        });
        updateAchievements();
    }

    void updateAchievements() {
        for (auto& a : _achievements) {
            if (a.id == "first_blood") {
                a.currentProgress = std::min(a.maxProgress, _careerStats.totalKills);
                if (a.currentProgress >= a.maxProgress) { a.unlocked = true; if (a.unlockDate.empty()) a.unlockDate = "2026-10-24"; }
            } else if (a.id == "sharpshooter") {
                a.currentProgress = std::min(a.maxProgress, _careerStats.headshots);
                if (a.currentProgress >= a.maxProgress) { a.unlocked = true; if (a.unlockDate.empty()) a.unlockDate = "2026-10-24"; }
            } else if (a.id == "bhop_master") {
                a.currentProgress = std::min(a.maxProgress, _careerStats.bhopJumps);
                if (a.currentProgress >= a.maxProgress) { a.unlocked = true; if (a.unlockDate.empty()) a.unlockDate = "2026-10-24"; }
            } else if (a.id == "demolition") {
                a.currentProgress = std::min(a.maxProgress, _careerStats.barrelsExploded);
                if (a.currentProgress >= a.maxProgress) { a.unlocked = true; if (a.unlockDate.empty()) a.unlockDate = "2026-10-24"; }
            } else if (a.id == "mod_creator") {
                if (!_discoveredAddons.empty()) {
                    a.currentProgress = 1;
                    a.unlocked = true;
                    if (a.unlockDate.empty()) a.unlockDate = "2026-10-24";
                }
            }
        }
    }

    void checkMatchEndCondition() {
        if (_isIntermission) return;
        int limit = _sessionConfig.fragLimit;
        if (limit <= 0) limit = 20;

        std::string winner = "";
        if (_playerKills >= limit) {
            winner = ProfileManager::get().getProfile().name;
        } else {
            for (const auto& bot : _aiManager.bots) {
                if (bot.kills >= limit) {
                    winner = bot.name;
                    break;
                }
            }
            if (winner.empty() && _netClient.isConnected()) {
                const auto& snap = _netClient.getLatestSnapshot();
                for (size_t i = 0; i < snap.playerCount; ++i) {
                    if (snap.players[i].kills >= limit) {
                        winner = (snap.players[i].name[0] != '\0') ? snap.players[i].name : ("Player_" + std::to_string(snap.players[i].clientId));
                        break;
                    }
                }
            }
        }

        if (!winner.empty()) {
            _isIntermission = true;
            _intermissionTimer = 5.0f;
            _intermissionWinner = winner;
            bool localWon = (winner == ProfileManager::get().getProfile().name);
            ProfileManager::get().recordMatch(localWon);
            std::string endMsg = "MATCH COMPLETED! WINNER: " + winner + " REACHED " + std::to_string(limit) + " FRAGS!";
            _hud.showCombatMessage(endMsg, 5.0f);
            _chat.addMessage("[SERVER]", endMsg, Vec3(1.0f, 0.85f, 0.2f));
            AudioEngine::playSound(localWon ? SoundID::AccessGranted : SoundID::AccessDenied);
        }
    }

    void recordPlayerKill(bool isHeadshot = false, bool isBarrel = false) {
        _playerKills++;
        _hud.frags = _playerKills;
        _careerStats.totalKills++;
        ProfileManager::get().addKill();
        if (isHeadshot) _careerStats.headshots++;
        if (isBarrel) _careerStats.barrelsExploded++;
        updateAchievements();
        checkMatchEndCondition();
    }

    void recordPlayerDeath() {
        _playerDeaths++;
        _careerStats.totalDeaths++;
        ProfileManager::get().addDeath();
        updateAchievements();
    }

    void handleMenuInput(float mx, float my, bool lmbClick) {
        // Keyboard shortcuts in menu
        if (Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
            if (!_escPressedLast) {
                if (_menuScreen != MenuScreen::Main) {
                    _menuScreen = MenuScreen::Main;
                } else if (_menuScreen == MenuScreen::Main && _hasActiveGame && _currentMap) {
                    _inMenu = false;
                    _fireLmbLast = true;
                    setCursorCaptured(true);
                }
                _escPressedLast = true;
            }
        } else {
            _escPressedLast = false;
        }

        if (false) {
            return;
        }

        const float winX = 150.0f;
        const float winY = 70.0f;
        const float winW = 980.0f;
        const float winH = 580.0f;

        if (_menuScreen == MenuScreen::Main) {
            if (lmbClick) {
                float curY = 160.0f;
                // Active gameplay session options: Resume & Disconnect
                if (_hasActiveGame && _currentMap) {
                    // Item 0: Resume
                    if (mx >= 60.0f && mx <= 420.0f && my >= curY && my <= curY + 36.0f) {
                        _inMenu = false;
                        _fireLmbLast = true;
                        setCursorCaptured(true);
                        return;
                    }
                    curY += 44.0f;

                    // Item 0b: Disconnect
                    if (mx >= 60.0f && mx <= 420.0f && my >= curY && my <= curY + 36.0f) {
                        disconnectActiveSession();
                        return;
                    }
                    curY += 44.0f;
                }
                // Item 1: New Game
                if (mx >= 60.0f && mx <= 420.0f && my >= curY && my <= curY + 36.0f) {
                    _menuScreen = MenuScreen::Singleplayer;
                    return;
                }
                curY += 44.0f;
                // Item 2: Find Servers
                if (mx >= 60.0f && mx <= 420.0f && my >= curY && my <= curY + 36.0f) {
                    _menuScreen = MenuScreen::JoinGame;
                    return;
                }
                curY += 44.0f;
                // Item 3: Create Server
                if (mx >= 60.0f && mx <= 420.0f && my >= curY && my <= curY + 36.0f) {
                    _menuScreen = MenuScreen::HostGame;
                    return;
                }
                curY += 44.0f;
                // Item 4: Player Profile
                if (mx >= 60.0f && mx <= 420.0f && my >= curY && my <= curY + 36.0f) {
                    const auto& prof = ProfileManager::get().getProfile();
                    _profileEditName = prof.name;
                    _profilePrimaryChoice = prof.primaryWeapon;
                    _profileSecondaryChoice = prof.secondaryWeapon;
                    _profileModelChoice = (prof.preferredModel == "worker" ? 1 : (prof.preferredModel == "scientist" ? 2 : 0));
                    _menuScreen = MenuScreen::Profile;
                    return;
                }
                curY += 44.0f;
                // Item 5: Addons
                if (mx >= 60.0f && mx <= 420.0f && my >= curY && my <= curY + 36.0f) {
                    refreshDiscoveredAddons();
                    _menuScreen = MenuScreen::Addons;
                    return;
                }
                curY += 44.0f;
                // Item 5: Achievements
                if (mx >= 60.0f && mx <= 420.0f && my >= curY && my <= curY + 36.0f) {
                    updateAchievements();
                    _menuScreen = MenuScreen::Achievements;
                    return;
                }
                curY += 44.0f;
                // Item 6: Options
                if (mx >= 60.0f && mx <= 420.0f && my >= curY && my <= curY + 36.0f) {
                    _menuScreen = MenuScreen::Options;
                    return;
                }
                curY += 44.0f;
                // Item 8: Quit
                if (mx >= 60.0f && mx <= 420.0f && my >= curY && my <= curY + 36.0f) {
                    stop();
                    return;
                }
            }
        }
        else if (_menuScreen == MenuScreen::Profile) {
            if (lmbClick) {
                // Top-right [X] close button or Bottom Close
                if ((mx >= winX + winW - 28.0f && mx <= winX + winW - 6.0f && my >= winY + 6.0f && my <= winY + 28.0f) ||
                    (mx >= winX + 20.0f && mx <= winX + 160.0f && my >= winY + 524.0f && my <= winY + 558.0f)) {
                    _menuScreen = MenuScreen::Main;
                    return;
                }
                // Save button
                if (mx >= winX + winW - 160.0f && mx <= winX + winW - 20.0f && my >= winY + 524.0f && my <= winY + 558.0f) {
                    auto& prof = ProfileManager::get().getProfile();
                    if (!_profileEditName.empty()) prof.name = _profileEditName;
                    prof.primaryWeapon = static_cast<uint8_t>(_profilePrimaryChoice);
                    prof.secondaryWeapon = static_cast<uint8_t>(_profileSecondaryChoice);
                    prof.preferredModel = (_profileModelChoice == 0 ? "guard" : (_profileModelChoice == 1 ? "worker" : "scientist"));
                    ProfileManager::get().saveToFile();
                    _menuScreen = MenuScreen::Main;
                    return;
                }
                // Primary weapon choices
                if (my >= winY + 105.0f && my <= winY + 137.0f) {
                    if (mx >= winX + 510.0f && mx <= winX + 640.0f) { _profilePrimaryChoice = 3; return; }
                    if (mx >= winX + 650.0f && mx <= winX + 780.0f) { _profilePrimaryChoice = 2; return; }
                    if (mx >= winX + 790.0f && mx <= winX + 920.0f) { _profilePrimaryChoice = 5; return; }
                }
                // Secondary weapon choices
                if (my >= winY + 180.0f && my <= winY + 212.0f) {
                    if (mx >= winX + 510.0f && mx <= winX + 640.0f) { _profileSecondaryChoice = 1; return; }
                    if (mx >= winX + 650.0f && mx <= winX + 780.0f) { _profileSecondaryChoice = 4; return; }
                }
                // Model choices
                if (my >= winY + 255.0f && my <= winY + 287.0f) {
                    if (mx >= winX + 510.0f && mx <= winX + 640.0f) { _profileModelChoice = 0; return; }
                    if (mx >= winX + 650.0f && mx <= winX + 780.0f) { _profileModelChoice = 1; return; }
                    if (mx >= winX + 790.0f && mx <= winX + 920.0f) { _profileModelChoice = 2; return; }
                }
            }
        }
        else if (_menuScreen == MenuScreen::Achievements) {
            if (lmbClick) {
                // Top-right [X] close button
                if (mx >= winX + winW - 28.0f && mx <= winX + winW - 6.0f && my >= winY + 6.0f && my <= winY + 28.0f) {
                    _menuScreen = MenuScreen::Main;
                    return;
                }
                // Tabs: [ ACHIEVEMENTS ], [ STATISTICS ]
                if (my >= winY + 42.0f && my <= winY + 68.0f) {
                    if (mx >= winX + 20.0f && mx <= winX + 160.0f) {
                        _achievementsActiveTab = 0;
                        return;
                    }
                    if (mx >= winX + 165.0f && mx <= winX + 305.0f) {
                        _achievementsActiveTab = 1;
                        return;
                    }
                }
                if (_achievementsActiveTab == 0) {
                    // Checkbox: [ ] Hide completed
                    if (mx >= winX + winW - 210.0f && mx <= winX + winW - 20.0f && my >= winY + 82.0f && my <= winY + 110.0f) {
                        _achievementsHideCompleted = !_achievementsHideCompleted;
                        return;
                    }
                }
                // Bottom Close button
                if (mx >= winX + 20.0f && mx <= winX + 160.0f && my >= winY + 524.0f && my <= winY + 558.0f) {
                    _menuScreen = MenuScreen::Main;
                    return;
                }
                // Bottom Reset button
                if (mx >= winX + 180.0f && mx <= winX + 320.0f && my >= winY + 524.0f && my <= winY + 558.0f) {
                    _careerStats = GameCareerStats{};
                    _careerStats.totalKills = 0;
                    _careerStats.totalDeaths = 0;
                    _careerStats.headshots = 0;
                    _careerStats.shotsFired = 0;
                    _careerStats.shotsHit = 0;
                    _careerStats.bhopJumps = 0;
                    _careerStats.barrelsExploded = 0;
                    _careerStats.mapsCompleted = 0;
                    _careerStats.totalPlaytimeSeconds = 0.0f;
                    for (auto& a : _achievements) {
                        a.currentProgress = 0;
                        a.unlocked = false;
                        a.unlockDate = "";
                    }
                    return;
                }
            }
        }
        else if (_menuScreen == MenuScreen::Singleplayer) {
            if (lmbClick) {
                // Top-right [X] close button
                if (mx >= winX + winW - 28.0f && mx <= winX + winW - 6.0f && my >= winY + 6.0f && my <= winY + 28.0f) {
                    _menuScreen = MenuScreen::Main;
                    return;
                }
                // Map rows selection with pagination
                int maxVis = 7;
                int totalMaps = (int)_availableMaps.size();
                int visCount = std::min(maxVis, totalMaps - _mapScrollOffset);
                for (int i = 0; i < visCount; ++i) {
                    int mapIdx = _mapScrollOffset + i;
                    float iy = winY + 80.0f + static_cast<float>(i) * 44.0f;
                    if (mx >= winX + 26.0f && mx <= winX + 454.0f && my >= iy && my <= iy + 38.0f) {
                        _selectedMapIndex = mapIdx;
                        _sessionConfig.mapPath = _availableMaps[mapIdx];
                        _sessionConfig.enableBots = false;
                        return;
                    }
                }
                // Scroll UP button
                if (mx >= winX + 26.0f && mx <= winX + 76.0f && my >= winY + 395.0f && my <= winY + 425.0f) {
                    _mapScrollOffset = std::max(0, _mapScrollOffset - 1);
                    return;
                }
                // Scroll DOWN button
                if (mx >= winX + 404.0f && mx <= winX + 454.0f && my >= winY + 395.0f && my <= winY + 425.0f) {
                    _mapScrollOffset = std::min(std::max(0, totalMaps - maxVis), _mapScrollOffset + 1);
                    return;
                }
                // Bottom Back button
                if (mx >= winX + 20.0f && mx <= winX + 160.0f && my >= winY + 524.0f && my <= winY + 558.0f) {
                    _menuScreen = MenuScreen::Main;
                    return;
                }
                // Bottom Browse button
                if (mx >= winX + 180.0f && mx <= winX + 330.0f && my >= winY + 524.0f && my <= winY + 558.0f) {
                    std::string picked = LabDialogs::openFileDialog(getWindow(), "Lab Scene Files (*.labscene)\0*.labscene\0Lab Map Files (*.labmap)\0*.labmap\0", "assets\\maps");
                    if (!picked.empty() && LabDialogs::isAllowedSceneFile(picked)) {
                        _sessionConfig.mapPath = toSceneDisplayPath(picked);
                        _sessionConfig.mode = GameMode::Campaign;
                        _sessionConfig.enableBots = false;
                        beginLoadingSequence(_sessionConfig, false);
                    }
                    return;
                }
                // Bottom Start button
                if (mx >= winX + winW - 160.0f && mx <= winX + winW - 20.0f && my >= winY + 524.0f && my <= winY + 558.0f) {
                    if (!_availableMaps.empty() && _selectedMapIndex < (int)_availableMaps.size()) {
                        _sessionConfig.mapPath = _availableMaps[_selectedMapIndex];
                        _sessionConfig.mode = GameMode::Campaign;
                        _sessionConfig.enableBots = false;
                        beginLoadingSequence(_sessionConfig, false);
                    }
                    return;
                }
            }
        }
        else if (_menuScreen == MenuScreen::HostGame) {
            if (lmbClick) {
                // Top-right [X] close button
                if (mx >= winX + winW - 28.0f && mx <= winX + winW - 6.0f && my >= winY + 6.0f && my <= winY + 28.0f) {
                    _menuScreen = MenuScreen::Main;
                    return;
                }
            }

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

            if (lmbClick) {
                // Left Panel: Map selection rows with pagination
                int maxVisHost = 7;
                int totalMapsHost = (int)_availableMaps.size();
                int visCountHost = std::min(maxVisHost, totalMapsHost - _mapScrollOffset);
                for (int i = 0; i < visCountHost; ++i) {
                    int mapIdx = _mapScrollOffset + i;
                    float iy = winY + 80.0f + static_cast<float>(i) * 44.0f;
                    if (mx >= winX + 26.0f && mx <= winX + 454.0f && my >= iy && my <= iy + 38.0f) {
                        _selectedMapIndex = mapIdx;
                        _sessionConfig.mapPath = _availableMaps[mapIdx];
                        return;
                    }
                }
                // Scroll UP button
                if (mx >= winX + 26.0f && mx <= winX + 76.0f && my >= winY + 395.0f && my <= winY + 425.0f) {
                    _mapScrollOffset = std::max(0, _mapScrollOffset - 1);
                    return;
                }
                // Scroll DOWN button
                if (mx >= winX + 404.0f && mx <= winX + 454.0f && my >= winY + 395.0f && my <= winY + 425.0f) {
                    _mapScrollOffset = std::min(std::max(0, totalMapsHost - maxVisHost), _mapScrollOffset + 1);
                    return;
                }

                // Left Panel: Browse button
                if (mx >= winX + 26.0f && mx <= winX + 454.0f && my >= winY + 450.0f && my <= winY + 484.0f) {
                    std::string picked = LabDialogs::openFileDialog(getWindow(), "Lab Scene Files (*.labscene)\0*.labscene\0Lab Map Files (*.labmap)\0*.labmap\0", "assets\\maps");
                    if (!picked.empty() && LabDialogs::isAllowedSceneFile(picked)) {
                        _sessionConfig.mapPath = toSceneDisplayPath(picked);
                    }
                    return;
                }

                // Network Mode: LAN vs P2P
                if (my >= winY + 80.0f && my <= winY + 110.0f) {
                    if (mx >= winX + 490.0f && mx <= winX + 715.0f) { _sessionConfig.networkMode = NetworkMode::LAN; return; }
                    if (mx >= winX + 725.0f && mx <= winX + 950.0f) { _sessionConfig.networkMode = NetworkMode::P2P; return; }
                }

                // Game Mode: FFA, DM, TDM
                if (my >= winY + 130.0f && my <= winY + 160.0f) {
                    if (mx >= winX + 490.0f && mx <= winX + 635.0f) { _sessionConfig.mode = GameMode::FFA; return; }
                    if (mx >= winX + 645.0f && mx <= winX + 790.0f) { _sessionConfig.mode = GameMode::DM; return; }
                    if (mx >= winX + 800.0f && mx <= winX + 950.0f) { _sessionConfig.mode = GameMode::TDM; return; }
                }

                // Combat Bots: Toggle
                if (mx >= winX + 490.0f && mx <= winX + 710.0f && my >= winY + 180.0f && my <= winY + 210.0f) {
                    _sessionConfig.enableBots = !_sessionConfig.enableBots;
                    return;
                }

                // Bot count [-] and [+]
                if (my >= winY + 180.0f && my <= winY + 210.0f) {
                    if (mx >= winX + 725.0f && mx <= winX + 765.0f) { _sessionConfig.botCount = std::max(0, _sessionConfig.botCount - 1); return; }
                    if (mx >= winX + 910.0f && mx <= winX + 950.0f) { _sessionConfig.botCount = std::min(8, _sessionConfig.botCount + 1); return; }
                }

                // Frag Limit [-] and [+]
                if (my >= winY + 230.0f && my <= winY + 260.0f) {
                    if (mx >= winX + 725.0f && mx <= winX + 765.0f) { _sessionConfig.fragLimit = std::max(5, _sessionConfig.fragLimit - 5); return; }
                    if (mx >= winX + 910.0f && mx <= winX + 950.0f) { _sessionConfig.fragLimit = std::min(100, _sessionConfig.fragLimit + 5); return; }
                }

                // Telemetry: Copy IP
                if (mx >= winX + 490.0f && mx <= winX + 710.0f && my >= winY + 280.0f && my <= winY + 310.0f) {
                    std::string inviteIP = _hostNATStatus.publicIP.empty() ? _hostNATStatus.localIP : _hostNATStatus.publicIP;
                    std::string inviteStr = inviteIP + ":" + std::to_string(_sessionConfig.port);
                    setClipboardText(inviteStr);
                    _punchStatusMsg = "Copied address: " + inviteStr;
                    return;
                }

                // Telemetry: Copy Room Code
                if (mx >= winX + 725.0f && mx <= winX + 835.0f && my >= winY + 280.0f && my <= winY + 310.0f) {
                    setClipboardText(_pnpRoomCode);
                    _punchStatusMsg = "Copied Room Code [" + _pnpRoomCode + "]!";
                    return;
                }

                // Telemetry: Generate New Code
                if (mx >= winX + 845.0f && mx <= winX + 950.0f && my >= winY + 280.0f && my <= winY + 310.0f) {
                    _pnpRoomCode = PnPBroker::generateRoomCode();
                    _sessionConfig.roomCode = _pnpRoomCode;
                    _punchStatusMsg = "New Room Code: [" + _pnpRoomCode + "]!";
                    return;
                }

                // Bottom Close button
                if (mx >= winX + 20.0f && mx <= winX + 160.0f && my >= winY + 524.0f && my <= winY + 558.0f) {
                    _menuScreen = MenuScreen::Main;
                    return;
                }

                // Bottom Start Server button
                if (mx >= winX + winW - 160.0f && mx <= winX + winW - 20.0f && my >= winY + 524.0f && my <= winY + 558.0f) {
                    ServerConfig srvCfg;
                    srvCfg.port = _sessionConfig.port;
                    srvCfg.roomCode = _pnpRoomCode;
                    srvCfg.serverName = (_sessionConfig.networkMode == NetworkMode::LAN) ? "Lab LAN Server" : "Lab P2P Server";
                    srvCfg.mapName = _sessionConfig.mapPath;
                    srvCfg.gameMode = (_sessionConfig.mode == GameMode::FFA ? "FFA" : (_sessionConfig.mode == GameMode::DM ? "DM" : "TDM"));
                    srvCfg.enableBots = _sessionConfig.enableBots;
                    srvCfg.botCount = _sessionConfig.botCount;
                    srvCfg.fragLimit = _sessionConfig.fragLimit;
                    srvCfg.maxPlayers = 16;
                    srvCfg.tickrate = 64;
                    srvCfg.networkMode = (_sessionConfig.networkMode == NetworkMode::LAN) ? NetMatchMode::LAN : NetMatchMode::P2P;
                    srvCfg.lanMode = (_sessionConfig.networkMode == NetworkMode::LAN);

                    srvCfg.saveToFile("assets/configs/server.cfg");

                    if (_sessionConfig.networkMode == NetworkMode::P2P) {
                        std::string pubIP = NATDiscovery::discoverPublicIP(srvCfg.port);
                        std::string lanIP = NATDiscovery::getLocalIP();
                        PnPBroker::publishRoom(_pnpRoomCode, pubIP, srvCfg.port, lanIP, srvCfg.port, srvCfg.mapName, srvCfg.gameMode);
                    }

                    _localServer.stop();
                    _isLocalHost = true;
                    _localServer.start(srvCfg);
                    _netClient.connect("127.0.0.1", srvCfg.port, ProfileManager::get().getProfile().name);
                    for (int i = 0; i < 4 && !_netClient.isConnected(); ++i) {
                        _localServer.tick(0.016f);
                        _netClient.update(0.016f, _camera.getPosition(), _velocity, _camera.getYaw(), _camera.getPitch(), 0, 0.0f, 0.0f, 0);
                    }
                    beginLoadingSequence(_sessionConfig, true);
                    return;
                }
            }
        }
        else if (_menuScreen == MenuScreen::JoinGame) {
            if (lmbClick) {
                // Top-right [X] close button
                if (mx >= winX + winW - 28.0f && mx <= winX + winW - 6.0f && my >= winY + 6.0f && my <= winY + 28.0f) {
                    _menuScreen = MenuScreen::Main;
                    return;
                }
            }

            // Keyboard input for direct connect IP or room code
            for (int k = 0; k < 512; ++k) {
                if (Input::keys[k] && !_lastMenuKeys[k]) {
                    if (_pnpJoinCodeActive) {
                        if (k == 259) {
                            if (!_pnpJoinCode.empty()) _pnpJoinCode.pop_back();
                        } else if (k == 'V' && (Input::isKeyPressed(GLFW_KEY_LEFT_CONTROL) || Input::isKeyPressed(GLFW_KEY_RIGHT_CONTROL))) {
                            std::string clip = getClipboardText();
                            if (!clip.empty()) _pnpJoinCode = clip;
                        } else if (k >= 32 && k <= 126) {
                            if (_pnpJoinCode.size() < 12) _pnpJoinCode.push_back(static_cast<char>(k));
                        }
                    } else if (_directConnectActive) {
                        if (k == 259) {
                            if (!_directConnectIP.empty()) _directConnectIP.pop_back();
                        } else if (k == 'V' && (Input::isKeyPressed(GLFW_KEY_LEFT_CONTROL) || Input::isKeyPressed(GLFW_KEY_RIGHT_CONTROL))) {
                            std::string clip = getClipboardText();
                            if (!clip.empty()) _directConnectIP = clip;
                        } else if (k >= 32 && k <= 126) {
                            if (_directConnectIP.size() < 45) _directConnectIP.push_back(static_cast<char>(k));
                        }
                    }
                }
                _lastMenuKeys[k] = Input::keys[k];
            }

            if (lmbClick) {
                // Server Browser Tabs: INTERNET, FAVORITES, HISTORY, LAN, FRIENDS
                if (my >= winY + 38.0f && my <= winY + 62.0f) {
                    for (int t = 0; t < 5; ++t) {
                        float tx = winX + 20.0f + static_cast<float>(t) * 118.0f;
                        if (mx >= tx && mx <= tx + 112.0f) {
                            _serverBrowserActiveTab = t;
                            _selectedServerIndex = 0;
                            return;
                        }
                    }
                }

                // Room Code field
                if (mx >= winX + 30.0f && mx <= winX + 230.0f && my >= winY + 68.0f && my <= winY + 96.0f) {
                    _pnpJoinCodeActive = true;
                    _directConnectActive = false;
                    return;
                }
                // Paste Code
                if (mx >= winX + 240.0f && mx <= winX + 320.0f && my >= winY + 68.0f && my <= winY + 96.0f) {
                    std::string clip = getClipboardText();
                    if (!clip.empty()) _pnpJoinCode = clip;
                    return;
                }
                // Join Code
                if (mx >= winX + 330.0f && mx <= winX + 440.0f && my >= winY + 68.0f && my <= winY + 96.0f) {
                    if (_pnpJoinCode.empty()) {
                        _connectStatusMsg = "Enter 4-digit Room Code first!";
                        return;
                    }
                    _isConnecting = true;
                    _connectAnimTimer = 0.0f;
                    _connectStatusMsg = "CONNECTING VIA ROOM [" + _pnpJoinCode + "]...";

                    PnPRoomInfo room;
                    if (PnPBroker::fetchRoom(_pnpJoinCode, room) && room.valid) {
                        std::string myPub = NATDiscovery::discoverPublicIP(27016);
                        std::string targetIP = room.publicIP;
                        uint16_t targetPort = room.publicPort;

                        if (!room.lanIP.empty() && (myPub == room.publicIP || room.publicIP == "127.0.0.1" || room.publicIP.empty())) {
                            targetIP = room.lanIP;
                            targetPort = room.lanPort;
                        } else {
                            PnPBroker::registerGuestPunch(_pnpJoinCode, myPub, 27016);
                            NATDiscovery::punchHole(room.publicIP, room.publicPort, 27016);
                        }
                        _directConnectIP = targetIP + ":" + std::to_string(targetPort);
                        _netClient.connect(targetIP, targetPort, ProfileManager::get().getProfile().name);
                    } else {
                        _isConnecting = false;
                        _connectStatusMsg = "ROOM [" + _pnpJoinCode + "] NOT FOUND!";
                    }
                    return;
                }

                // Copy My IP
                if (mx >= winX + winW - 130.0f && mx <= winX + winW - 40.0f && my >= winY + 68.0f && my <= winY + 96.0f) {
                    std::string myIP = (_guestNATStatus.publicIP.empty() ? "127.0.0.1" : _guestNATStatus.publicIP) + ":27016";
                    setClipboardText(myIP);
                    _connectStatusMsg = "Copied your IP (" + myIP + ") to clipboard!";
                    return;
                }

                // Server list rows
                auto servers = _serverBrowser.getFilteredServers(_serverBrowserActiveTab, _serverFilterMap, _serverFilterMaxPing, _serverFilterNotFull, _serverFilterHasPlayers);
                for (size_t i = 0; i < servers.size() && i < 6; ++i) {
                    float rowY = winY + 134.0f + static_cast<float>(i) * 40.0f;
                    if (mx >= winX + 22.0f && mx <= winX + winW - 22.0f && my >= rowY && my <= rowY + 36.0f) {
                        _selectedServerIndex = static_cast<int>(i);
                        _directConnectIP = servers[i].ip + ":" + std::to_string(servers[i].port);
                        return;
                    }
                }

                // Bottom Filter Controls (winY + 432 .. winY + 462)
                if (my >= winY + 432.0f && my <= winY + 462.0f) {
                    // [x] Not Full
                    if (mx >= winX + 20.0f && mx <= winX + 160.0f) {
                        _serverFilterNotFull = !_serverFilterNotFull;
                        return;
                    }
                    // [x] Has Players
                    if (mx >= winX + 170.0f && mx <= winX + 320.0f) {
                        _serverFilterHasPlayers = !_serverFilterHasPlayers;
                        return;
                    }
                    // Max Ping cycle (Any -> <100 -> <150 -> <250)
                    if (mx >= winX + 330.0f && mx <= winX + 490.0f) {
                        if (_serverFilterMaxPing == 0) _serverFilterMaxPing = 100;
                        else if (_serverFilterMaxPing == 100) _serverFilterMaxPing = 150;
                        else if (_serverFilterMaxPing == 150) _serverFilterMaxPing = 250;
                        else _serverFilterMaxPing = 0;
                        return;
                    }
                    // Add Fav button
                    if (mx >= winX + winW - 140.0f && mx <= winX + winW - 20.0f) {
                        std::string favIP = "127.0.0.1";
                        uint16_t favPort = DEFAULT_SERVER_PORT;
                        if (!_directConnectIP.empty()) {
                            size_t colon = _directConnectIP.find(':');
                            if (colon != std::string::npos) {
                                favIP = _directConnectIP.substr(0, colon);
                                try { favPort = static_cast<uint16_t>(std::stoi(_directConnectIP.substr(colon + 1))); } catch (...) {}
                            } else {
                                favIP = _directConnectIP;
                            }
                        }
                        _serverBrowser.addFavorite(favIP, favPort);
                        _connectStatusMsg = "Toggled Favorite: " + favIP + ":" + std::to_string(favPort);
                        return;
                    }
                }

                // Direct IP field
                if (mx >= winX + 20.0f && mx <= winX + 620.0f && my >= winY + 472.0f && my <= winY + 504.0f) {
                    _directConnectActive = true;
                    _pnpJoinCodeActive = false;
                    return;
                }
                // Direct IP Paste
                if (mx >= winX + 630.0f && mx <= winX + 710.0f && my >= winY + 472.0f && my <= winY + 504.0f) {
                    std::string clip = getClipboardText();
                    if (!clip.empty()) _directConnectIP = clip;
                    return;
                }
                // Direct IP Clear
                if (mx >= winX + 720.0f && mx <= winX + 800.0f && my >= winY + 472.0f && my <= winY + 504.0f) {
                    _directConnectIP.clear();
                    return;
                }

                // Bottom Close button
                if (mx >= winX + 20.0f && mx <= winX + 160.0f && my >= winY + 524.0f && my <= winY + 558.0f) {
                    if (_isConnecting) {
                        _isConnecting = false;
                        _netClient.disconnect();
                        _connectStatusMsg = "";
                    }
                    _menuScreen = MenuScreen::Main;
                    return;
                }
                // Bottom Refresh button
                if (mx >= winX + winW - 320.0f && mx <= winX + winW - 180.0f && my >= winY + 524.0f && my <= winY + 558.0f) {
                    _serverBrowser.refresh();
                    return;
                }
                // Bottom Connect button
                if (mx >= winX + winW - 160.0f && mx <= winX + winW - 20.0f && my >= winY + 524.0f && my <= winY + 558.0f) {
                    if (_isConnecting) {
                        _isConnecting = false;
                        _netClient.disconnect();
                        _connectStatusMsg = "Connection cancelled.";
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
                    _netClient.connect(targetIP, targetPort, ProfileManager::get().getProfile().name);
                    return;
                }
            }
        }
        else if (_menuScreen == MenuScreen::Addons) {
            if (lmbClick) {
                // Top-right [X] close button
                if (mx >= winX + winW - 28.0f && mx <= winX + winW - 6.0f && my >= winY + 6.0f && my <= winY + 28.0f) {
                    _menuScreen = MenuScreen::Main;
                    return;
                }
                // Category tabs on left (winX + 26 .. winX + 234, winY + 80 + c * 40)
                for (int c = 0; c < 5; ++c) {
                    float cy = winY + 80.0f + static_cast<float>(c) * 40.0f;
                    if (mx >= winX + 26.0f && mx <= winX + 234.0f && my >= cy && my <= cy + 34.0f) {
                        _selectedAddonCategory = c;
                        return;
                    }
                }
                // Checkbox on addon cards
                for (size_t i = 0; i < _discoveredAddons.size() && i < 5; ++i) {
                    float cardY = winY + 80.0f + static_cast<float>(i) * 78.0f;
                    float cbX = winX + 270.0f + 580.0f;
                    float cbY = cardY + 20.0f;
                    if (mx >= cbX && mx <= cbX + 85.0f && my >= cbY && my <= cbY + 28.0f) {
                        _discoveredAddons[i].enabled = !_discoveredAddons[i].enabled;
                        std::filesystem::path addonPath = std::filesystem::path("addons") / _discoveredAddons[i].folderName;
                        if (_discoveredAddons[i].enabled) {
                            VFS::get().mountAddonLayer(addonPath);
                            if (std::filesystem::exists(addonPath / "scripts" / "startup.lua")) {
                                _scriptEngine.executeStartupHook((addonPath / "scripts" / "startup.lua").string());
                            }
                        } else {
                            VFS::get().unmountAddonLayer(addonPath);
                            if (_discoveredAddons[i].folderName == "cod_intro") {
                                ScriptEngine::setHUDText("cod_intro_status", "DISABLED");
                            }
                        }
                        return;
                    }
                }
                // Bottom Close button
                if (mx >= winX + 20.0f && mx <= winX + 160.0f && my >= winY + 524.0f && my <= winY + 558.0f) {
                    _menuScreen = MenuScreen::Main;
                    return;
                }
                // Bottom Refresh button
                if (mx >= winX + 180.0f && mx <= winX + 320.0f && my >= winY + 524.0f && my <= winY + 558.0f) {
                    refreshDiscoveredAddons();
                    return;
                }
            }
        }
        else if (_menuScreen == MenuScreen::Options) {
            const float optX = 230.0f;
            const float optY = 90.0f;
            const float optW = 820.0f;
            const float optH = 540.0f;

            if (lmbClick) {
                // Top-right [X] close button
                if (mx >= optX + optW - 28.0f && mx <= optX + optW - 6.0f && my >= optY + 6.0f && my <= optY + 28.0f) {
                    _menuScreen = MenuScreen::Main;
                    return;
                }
                // Tabs: [ VIDEO ], [ AUDIO ], [ CONTROLS ]
                if (my >= optY + 42.0f && my <= optY + 68.0f) {
                    if (mx >= optX + 20.0f && mx <= optX + 140.0f) { _optionsActiveTab = 0; return; }
                    if (mx >= optX + 145.0f && mx <= optX + 265.0f) { _optionsActiveTab = 1; return; }
                    if (mx >= optX + 270.0f && mx <= optX + 390.0f) { _optionsActiveTab = 2; return; }
                }

                if (_optionsActiveTab == 0) { // Video
                    // Shadows toggle
                    if (mx >= optX + 320.0f && mx <= optX + 460.0f && my >= optY + 98.0f && my <= optY + 128.0f) {
                        _optShadows = !_optShadows;
                        _devConsole.r_shadows = _optShadows;
                        return;
                    }
                    // Shadow Res 1024 / 2048
                    if (my >= optY + 148.0f && my <= optY + 178.0f) {
                        if (mx >= optX + 320.0f && mx <= optX + 410.0f) { _optShadowRes = 1024; _devConsole.r_shadow_res = 1024; return; }
                        if (mx >= optX + 420.0f && mx <= optX + 510.0f) { _optShadowRes = 2048; _devConsole.r_shadow_res = 2048; return; }
                    }
                    // FOV 75 / 90 / 105
                    if (my >= optY + 198.0f && my <= optY + 228.0f) {
                        if (mx >= optX + 320.0f && mx <= optX + 380.0f) { _optFov = 75; _camera.fieldOfView = 75.0f; _camera.syncProperties(); return; }
                        if (mx >= optX + 390.0f && mx <= optX + 450.0f) { _optFov = 90; _camera.fieldOfView = 90.0f; _camera.syncProperties(); return; }
                        if (mx >= optX + 460.0f && mx <= optX + 520.0f) { _optFov = 105; _camera.fieldOfView = 105.0f; _camera.syncProperties(); return; }
                    }
                } else if (_optionsActiveTab == 1) { // Audio
                    if (my >= optY + 98.0f && my <= optY + 128.0f) {
                        if (mx >= optX + 320.0f && mx <= optX + 360.0f) { _optMasterVolume = std::max(0.0f, _optMasterVolume - 0.1f); return; }
                        if (mx >= optX + 460.0f && mx <= optX + 500.0f) { _optMasterVolume = std::min(1.0f, _optMasterVolume + 0.1f); return; }
                    }
                } else if (_optionsActiveTab == 2) { // Controls
                    if (my >= optY + 98.0f && my <= optY + 128.0f) {
                        if (mx >= optX + 320.0f && mx <= optX + 360.0f) { _optMouseSens = std::max(0.02f, _optMouseSens - 0.02f); return; }
                        if (mx >= optX + 460.0f && mx <= optX + 500.0f) { _optMouseSens = std::min(0.50f, _optMouseSens + 0.02f); return; }
                    }
                    if (mx >= optX + 320.0f && mx <= optX + 460.0f && my >= optY + 148.0f && my <= optY + 178.0f) {
                        _optInvertY = !_optInvertY;
                        return;
                    }
                }

                // Bottom Cancel / OK buttons
                if (mx >= optX + 20.0f && mx <= optX + 160.0f && my >= optY + 484.0f && my <= optY + 518.0f) {
                    _menuScreen = MenuScreen::Main;
                    return;
                }
                if (mx >= optX + optW - 160.0f && mx <= optX + optW - 20.0f && my >= optY + 484.0f && my <= optY + 518.0f) {
                    _menuScreen = MenuScreen::Main;
                    return;
                }
            }
        }
        else if (_menuScreen == MenuScreen::MultiSelect) {
            if (lmbClick) {
                // Card 1: Host Game
                if (mx >= winX + 40.0f && mx <= winX + 460.0f && my >= winY + 70.0f && my <= winY + 450.0f) {
                    _menuScreen = MenuScreen::HostGame;
                    return;
                }
                // Card 2: Join Game
                if (mx >= winX + 520.0f && mx <= winX + 940.0f && my >= winY + 70.0f && my <= winY + 450.0f) {
                    _menuScreen = MenuScreen::JoinGame;
                    return;
                }
                // Bottom Back button
                if (mx >= winX + 40.0f && mx <= winX + 180.0f && my >= winY + 500.0f && my <= winY + 534.0f) {
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
        // Framebuffer size
        (void)fbW; (void)fbH;



        int w = 1280, h = 720;
        Renderer::beginUI(w, h);

        // Backdrop
        if (_currentMap != nullptr) {
            Renderer::drawRect(0, 0, (float)w, (float)h, { 0.0f, 0.0f, 0.0f }, 0.52f);
        } else {
            Renderer::drawRect(0, 0, (float)w, (float)h, { 0.04f, 0.05f, 0.07f }, 1.0f);
            Renderer::drawRect(0, 0, (float)w, 80.0f, { 0.02f, 0.03f, 0.04f }, 0.70f);
            Renderer::drawRect(0, (float)h - 50.0f, (float)w, 50.0f, { 0.02f, 0.03f, 0.04f }, 0.70f);
        }

        // Shared Lambda drawing helpers for authentic Valve Source VGUI2 dialogs
        auto drawVguiWindow = [](float x, float y, float winWidth, float winHeight, const std::string& title, bool isCloseHovered) {
            Renderer::drawRect(x, y, winWidth, winHeight, Vec3(0.11f, 0.13f, 0.16f), 0.96f);
            Renderer::drawRect(x, y, winWidth, 1.0f, Vec3(0.28f, 0.32f, 0.38f), 0.85f);
            Renderer::drawRect(x, y, 1.0f, winHeight, Vec3(0.28f, 0.32f, 0.38f), 0.85f);
            Renderer::drawRect(x + winWidth - 1.0f, y, 1.0f, winHeight, Vec3(0.06f, 0.07f, 0.09f), 0.85f);
            Renderer::drawRect(x, y + winHeight - 1.0f, winWidth, 1.0f, Vec3(0.06f, 0.07f, 0.09f), 0.85f);

            Renderer::drawRect(x + 1.0f, y + 1.0f, winWidth - 2.0f, 32.0f, Vec3(0.15f, 0.17f, 0.21f), 0.98f);
            Renderer::drawRect(x, y + 33.0f, winWidth, 1.0f, Vec3(0.06f, 0.07f, 0.08f), 0.90f);
            LabFont::drawText(x + 14.0f, y + 8.0f, title, 1.75f, Vec3(0.92f, 0.94f, 0.97f), LabFontType::GeoSans);

            float closeX = x + winWidth - 26.0f;
            float closeY = y + 5.0f;
            Vec3 closeBg = isCloseHovered ? Vec3(0.55f, 0.15f, 0.15f) : Vec3(0.18f, 0.20f, 0.24f);
            Renderer::drawRect(closeX, closeY, 20.0f, 20.0f, closeBg);
            Renderer::drawRect(closeX, closeY, 20.0f, 1.0f, Vec3(0.30f, 0.34f, 0.40f));
            LabFont::drawText(closeX + 5.0f, closeY + 3.0f, "X", 1.40f, Vec3(0.95f, 0.95f, 0.95f), LabFontType::GeoSans);
        };

        auto drawVguiWell = [](float x, float y, float wellW, float wellH, const std::string& header) {
            Renderer::drawRect(x, y, wellW, wellH, Vec3(0.07f, 0.08f, 0.10f), 0.95f);
            Renderer::drawRect(x, y, wellW, 1.0f, Vec3(0.04f, 0.05f, 0.06f));
            Renderer::drawRect(x, y, 1.0f, wellH, Vec3(0.04f, 0.05f, 0.06f));
            Renderer::drawRect(x + wellW - 1.0f, y, 1.0f, wellH, Vec3(0.20f, 0.23f, 0.28f));
            Renderer::drawRect(x, y + wellH - 1.0f, wellW, 1.0f, Vec3(0.20f, 0.23f, 0.28f));

            if (!header.empty()) {
                Renderer::drawRect(x + 1.0f, y + 1.0f, wellW - 2.0f, 24.0f, Vec3(0.12f, 0.14f, 0.17f));
                Renderer::drawRect(x, y + 25.0f, wellW, 1.0f, Vec3(0.05f, 0.06f, 0.07f));
                LabFont::drawText(x + 10.0f, y + 5.0f, header, 1.45f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
            }
        };

        auto drawVguiButton = [](float x, float y, float btnW, float btnH, const std::string& text, bool isHovered, bool isPrimary) {
            Vec3 bg = isPrimary
                ? (isHovered ? Vec3(0.20f, 0.26f, 0.34f) : Vec3(0.15f, 0.20f, 0.26f))
                : (isHovered ? Vec3(0.22f, 0.25f, 0.30f) : Vec3(0.16f, 0.18f, 0.22f));
            Vec3 border = isPrimary
                ? (isHovered ? Vec3(0.35f, 0.45f, 0.58f) : Vec3(0.25f, 0.33f, 0.42f))
                : (isHovered ? Vec3(0.32f, 0.36f, 0.44f) : Vec3(0.24f, 0.28f, 0.34f));
            Vec3 txtCol = isPrimary
                ? (isHovered ? Vec3(1.0f, 1.0f, 1.0f) : Vec3(1.0f, 0.84f, 0.24f))
                : (isHovered ? Vec3(1.0f, 1.0f, 1.0f) : Vec3(0.78f, 0.81f, 0.85f));

            Renderer::drawRect(x, y, btnW, btnH, bg);
            Renderer::drawRect(x, y, btnW, 1.0f, border);
            Renderer::drawRect(x, y, 1.0f, btnH, border);
            Renderer::drawRect(x + btnW - 1.0f, y, 1.0f, btnH, border);
            Renderer::drawRect(x, y + btnH - 1.0f, btnW, 1.0f, border);

            if (isPrimary) {
                Renderer::drawRect(x + 2.0f, y + btnH - 2.0f, btnW - 4.0f, 2.0f, Vec3(1.0f, 0.72f, 0.15f));
            }

            float textX = x + 16.0f;
            float textY = y + (btnH - 16.0f) * 0.5f - 1.0f;
            LabFont::drawText(textX, textY, text, 1.55f, txtCol, LabFontType::GeoSans);
        };

        auto drawVguiProgressBar = [](float x, float y, float barW, float barH, float fraction, const Vec3& fillColor) {
            fraction = std::clamp(fraction, 0.0f, 1.0f);
            Renderer::drawRect(x, y, barW, barH, Vec3(0.05f, 0.06f, 0.07f));
            Renderer::drawRect(x, y, barW, 1.0f, Vec3(0.18f, 0.21f, 0.26f));
            Renderer::drawRect(x, y, 1.0f, barH, Vec3(0.18f, 0.21f, 0.26f));
            Renderer::drawRect(x + barW - 1.0f, y, 1.0f, barH, Vec3(0.18f, 0.21f, 0.26f));
            Renderer::drawRect(x, y + barH - 1.0f, barW, 1.0f, Vec3(0.18f, 0.21f, 0.26f));

            float fillW = (barW - 2.0f) * fraction;
            if (fillW > 0.0f) {
                Renderer::drawRect(x + 1.0f, y + 1.0f, fillW, barH - 2.0f, fillColor);
                for (float tick = 10.0f; tick < fillW; tick += 10.0f) {
                    Renderer::drawRect(x + 1.0f + tick, y + 1.0f, 1.0f, barH - 2.0f, Vec3(0.05f, 0.06f, 0.07f), 0.6f);
                }
            }
        };

        const float winX = 150.0f;
        const float winY = 70.0f;
        const float winW = 980.0f;
        const float winH = 580.0f;

        if (_menuScreen == MenuScreen::Main) {
            // Top-left Game Title
            LabFont::drawText(72.0f, 54.0f, "FROZEN-LIFE", 3.2f, Vec3(0.95f, 0.97f, 1.0f), LabFontType::GeoSans);
            LabFont::drawText(74.0f, 92.0f, "LAB ENGINE // SOURCE FPS FOUNDATION", 1.40f, Vec3(0.46f, 0.62f, 0.80f), LabFontType::GeoSans);
            Renderer::drawRect(74.0f, 114.0f, 280.0f, 1.5f, Vec3(0.20f, 0.35f, 0.55f), 0.65f);

            // Left Navigation Menu Stack
            struct MainMenuItem {
                std::string title;
                float y;
                bool active;
            };

            std::vector<MainMenuItem> items;
            float curY = 160.0f;
            if (_hasActiveGame && _currentMap != nullptr) {
                items.push_back({ "RESUME", curY, true });
                curY += 44.0f;
                items.push_back({ "DISCONNECT", curY, true });
                curY += 44.0f;
            }
            items.push_back({ "NEW GAME", curY, true });
            curY += 44.0f;
            items.push_back({ "FIND SERVERS", curY, true });
            curY += 44.0f;
            items.push_back({ "CREATE SERVER", curY, true });
            curY += 44.0f;
            items.push_back({ "PLAYER PROFILE", curY, true });
            curY += 44.0f;
            items.push_back({ "ADDONS", curY, true });
            curY += 44.0f;
            items.push_back({ "ACHIEVEMENTS", curY, true });
            curY += 44.0f;
            items.push_back({ "OPTIONS", curY, true });
            curY += 44.0f;
            items.push_back({ "QUIT", curY, true });

            for (const auto& it : items) {
                bool isHovered = it.active && (_menuMouseX >= 60.0f && _menuMouseX <= 420.0f && _menuMouseY >= it.y && _menuMouseY <= it.y + 36.0f);
                Vec3 txtCol = it.active ? (isHovered ? Vec3(1.0f, 0.84f, 0.22f) : Vec3(0.80f, 0.84f, 0.88f)) : Vec3(0.40f, 0.44f, 0.48f);

                if (isHovered) {
                    Renderer::drawRect(54.0f, it.y + 2.0f, 3.5f, 24.0f, Vec3(1.0f, 0.80f, 0.18f), 0.95f);
                }
                LabFont::drawText(72.0f, it.y + 4.0f, it.title, 2.2f, txtCol, LabFontType::GeoSans);
            }

            // Right side session telemetry card (only shown during active gameplay session)
            if (_hasActiveGame && _currentMap) {
                drawVguiWell(720.0f, 160.0f, 480.0f, 220.0f, "CURRENT ACTIVE SESSION");
                LabFont::drawText(744.0f, 200.0f, "MAP:    " + _currentMap->metadata.name, 1.6f, Vec3(0.85f, 0.90f, 0.95f), LabFontType::GeoSans);
                LabFont::drawText(744.0f, 230.0f, "MODE:   " + _sessionConfig.getModeString(), 1.6f, Vec3(0.85f, 0.90f, 0.95f), LabFontType::GeoSans);
                LabFont::drawText(744.0f, 260.0f, "HOST:   Localhost / 27015", 1.6f, Vec3(0.85f, 0.90f, 0.95f), LabFontType::GeoSans);
                LabFont::drawText(744.0f, 290.0f, "STATUS: Session running in background", 1.45f, Vec3(0.35f, 0.85f, 0.55f), LabFontType::System);
            }

            std::string footerText = _hasActiveGame ? "[ESC] RESUME   |   [~] CONSOLE   |   [TAB] SCOREBOARD" : "[~] CONSOLE";
            LabFont::drawText(72.0f, 672.0f, footerText, 1.40f, Vec3(0.48f, 0.55f, 0.65f), LabFontType::GeoSans);
        }
        else if (_menuScreen == MenuScreen::Profile) {
            bool isCloseHov = (_menuMouseX >= winX + winW - 28.0f && _menuMouseX <= winX + winW - 6.0f && _menuMouseY >= winY + 6.0f && _menuMouseY <= winY + 28.0f);
            drawVguiWindow(winX, winY, winW, winH, "OPERATIVE PROFILE & LOADOUT", isCloseHov);

            auto& prof = ProfileManager::get().getProfile();
            // Left Well: Profile & Tactical Stats
            drawVguiWell(winX + 20.0f, winY + 46.0f, 450.0f, 455.0f, "IDENTITY & CAREER STATS");
            LabFont::drawText(winX + 40.0f, winY + 85.0f, "OPERATIVE CODENAME", 1.45f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
            drawVguiWell(winX + 40.0f, winY + 105.0f, 410.0f, 36.0f, "");
            LabFont::drawText(winX + 52.0f, winY + 114.0f, _profileEditName.empty() ? prof.name : _profileEditName, 1.6f, Vec3(1, 1, 1), LabFontType::GeoSans);

            LabFont::drawText(winX + 40.0f, winY + 160.0f, "RANK & SECURITY CLEARANCE", 1.45f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
            std::string rankStr = prof.getRankTitle() + " (Level " + std::to_string(prof.level) + ")";
            LabFont::drawText(winX + 40.0f, winY + 185.0f, rankStr, 1.7f, Vec3(1.0f, 0.84f, 0.22f), LabFontType::GeoSans);
            float xpFrac = static_cast<float>(prof.xp % 1000) / 1000.0f;
            drawVguiProgressBar(winX + 40.0f, winY + 215.0f, 410.0f, 16.0f, xpFrac, Vec3(0.83f, 0.61f, 0.16f));
            LabFont::drawText(winX + 40.0f, winY + 236.0f, "XP: " + std::to_string(prof.xp % 1000) + " / 1000", 1.35f, Vec3(0.6f, 0.65f, 0.75f), LabFontType::GeoSans);

            // Career telemetry grid
            Renderer::drawRect(winX + 40.0f, winY + 265.0f, 410.0f, 30.0f, Vec3(0.10f, 0.12f, 0.16f));
            LabFont::drawText(winX + 50.0f, winY + 272.0f, "CAREER KILLS", 1.35f, Vec3(0.7f, 0.75f, 0.8f), LabFontType::GeoSans);
            LabFont::drawText(winX + 340.0f, winY + 272.0f, std::to_string(prof.kills), 1.45f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(winX + 40.0f, winY + 300.0f, 410.0f, 30.0f, Vec3(0.12f, 0.14f, 0.18f));
            LabFont::drawText(winX + 50.0f, winY + 307.0f, "CAREER DEATHS", 1.35f, Vec3(0.7f, 0.75f, 0.8f), LabFontType::GeoSans);
            LabFont::drawText(winX + 340.0f, winY + 307.0f, std::to_string(prof.deaths), 1.45f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(winX + 40.0f, winY + 335.0f, 410.0f, 30.0f, Vec3(0.10f, 0.12f, 0.16f));
            LabFont::drawText(winX + 50.0f, winY + 342.0f, "KILL / DEATH RATIO", 1.35f, Vec3(0.7f, 0.75f, 0.8f), LabFontType::GeoSans);
            char kdBuf[32]; snprintf(kdBuf, sizeof(kdBuf), "%.2f", prof.getKD());
            LabFont::drawText(winX + 340.0f, winY + 342.0f, kdBuf, 1.45f, Vec3(0.3f, 0.9f, 0.45f), LabFontType::GeoSans);

            Renderer::drawRect(winX + 40.0f, winY + 370.0f, 410.0f, 30.0f, Vec3(0.12f, 0.14f, 0.18f));
            LabFont::drawText(winX + 50.0f, winY + 377.0f, "MATCHES WON / PLAYED", 1.35f, Vec3(0.7f, 0.75f, 0.8f), LabFontType::GeoSans);
            LabFont::drawText(winX + 340.0f, winY + 377.0f, std::to_string(prof.wins) + " / " + std::to_string(prof.matchesPlayed), 1.45f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(winX + 40.0f, winY + 405.0f, 410.0f, 30.0f, Vec3(0.10f, 0.12f, 0.16f));
            LabFont::drawText(winX + 50.0f, winY + 412.0f, "WIN RATE", 1.35f, Vec3(0.7f, 0.75f, 0.8f), LabFontType::GeoSans);
            char wrBuf[32]; snprintf(wrBuf, sizeof(wrBuf), "%.1f %%", prof.getWinRate());
            LabFont::drawText(winX + 340.0f, winY + 412.0f, wrBuf, 1.45f, Vec3(1.0f, 0.84f, 0.22f), LabFontType::GeoSans);

            // Right Well: Tactical DM Loadout Configuration
            drawVguiWell(winX + 490.0f, winY + 46.0f, 470.0f, 455.0f, "TACTICAL LOADOUT (DM / TDM)");
            LabFont::drawText(winX + 510.0f, winY + 85.0f, "PRIMARY WEAPON", 1.45f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
            drawVguiButton(winX + 510.0f, winY + 105.0f, 130.0f, 32.0f, "Rifle", false, _profilePrimaryChoice == 3);
            drawVguiButton(winX + 650.0f, winY + 105.0f, 130.0f, 32.0f, "Shotgun", false, _profilePrimaryChoice == 2);
            drawVguiButton(winX + 790.0f, winY + 105.0f, 130.0f, 32.0f, "Crossbow", false, _profilePrimaryChoice == 5);

            LabFont::drawText(winX + 510.0f, winY + 160.0f, "SECONDARY SIDEARM", 1.45f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
            drawVguiButton(winX + 510.0f, winY + 180.0f, 130.0f, 32.0f, "Pistol", false, _profileSecondaryChoice == 1);
            drawVguiButton(winX + 650.0f, winY + 180.0f, 130.0f, 32.0f, "Revolver", false, _profileSecondaryChoice == 4);

            LabFont::drawText(winX + 510.0f, winY + 235.0f, "TACTICAL OPERATIVE MODEL", 1.45f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
            drawVguiButton(winX + 510.0f, winY + 255.0f, 130.0f, 32.0f, "Guard", false, _profileModelChoice == 0);
            drawVguiButton(winX + 650.0f, winY + 255.0f, 130.0f, 32.0f, "Worker", false, _profileModelChoice == 1);
            drawVguiButton(winX + 790.0f, winY + 255.0f, 130.0f, 32.0f, "Scientist", false, _profileModelChoice == 2);

            LabFont::drawText(winX + 510.0f, winY + 310.0f, "CONFIG PREVIEW", 1.45f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
            drawVguiWell(winX + 510.0f, winY + 330.0f, 430.0f, 150.0f, "");
            LabFont::drawText(winX + 524.0f, winY + 350.0f, "Steam ID: " + prof.steamId, 1.40f, Vec3(0.6f, 0.85f, 1.0f), LabFontType::GeoSans);
            LabFont::drawText(winX + 524.0f, winY + 380.0f, "Save Location: assets/configs/user_profile.json", 1.35f, Vec3(0.7f, 0.7f, 0.7f), LabFontType::GeoSans);
            LabFont::drawText(winX + 524.0f, winY + 410.0f, "Status: Local profile active & synchronized", 1.35f, Vec3(0.35f, 0.9f, 0.45f), LabFontType::GeoSans);

            // Bottom Buttons
            bool isCloseBtnHov = (_menuMouseX >= winX + 20.0f && _menuMouseX <= winX + 160.0f && _menuMouseY >= winY + 524.0f && _menuMouseY <= winY + 558.0f);
            drawVguiButton(winX + 20.0f, winY + 524.0f, 140.0f, 34.0f, "Close", isCloseBtnHov, false);

            bool isSaveHov = (_menuMouseX >= winX + winW - 160.0f && _menuMouseX <= winX + winW - 20.0f && _menuMouseY >= winY + 524.0f && _menuMouseY <= winY + 558.0f);
            drawVguiButton(winX + winW - 160.0f, winY + 524.0f, 140.0f, 34.0f, "Save", isSaveHov, true);
        }
        else if (_menuScreen == MenuScreen::Achievements) {
            bool isCloseHov = (_menuMouseX >= winX + winW - 28.0f && _menuMouseX <= winX + winW - 6.0f && _menuMouseY >= winY + 6.0f && _menuMouseY <= winY + 28.0f);
            drawVguiWindow(winX, winY, winW, winH, "ACHIEVEMENTS & STATISTICS", isCloseHov);

            // Tabs
            const char* tabs[2] = { "ACHIEVEMENTS", "STATISTICS" };
            for (int t = 0; t < 2; ++t) {
                float tx = winX + 20.0f + static_cast<float>(t) * 145.0f;
                bool isSel = (_achievementsActiveTab == t);
                bool isHov = (_menuMouseX >= tx && _menuMouseX <= tx + 140.0f && _menuMouseY >= winY + 42.0f && _menuMouseY <= winY + 68.0f);
                Vec3 tabBg = isSel ? Vec3(0.14f, 0.16f, 0.20f) : (isHov ? Vec3(0.12f, 0.14f, 0.17f) : Vec3(0.09f, 0.10f, 0.12f));
                Vec3 txtCol = isSel ? Vec3(1.0f, 0.83f, 0.22f) : (isHov ? Vec3(0.85f, 0.88f, 0.92f) : Vec3(0.50f, 0.54f, 0.58f));
                Renderer::drawRect(tx, winY + 42.0f, 140.0f, 26.0f, tabBg);
                if (isSel) Renderer::drawRect(tx, winY + 66.0f, 140.0f, 2.0f, Vec3(1.0f, 0.70f, 0.0f));
                LabFont::drawText(tx + 16.0f, winY + 47.0f, tabs[t], 1.45f, txtCol, LabFontType::GeoSans);
            }

            if (_achievementsActiveTab == 0) {
                // Summary progress bar
                int unlockedCount = (int)std::count_if(_achievements.begin(), _achievements.end(), [](const auto& a){ return a.unlocked; });
                int totalCount = (int)_achievements.size();
                float progRatio = totalCount > 0 ? ((float)unlockedCount / (float)totalCount) : 0.0f;
                int percent = (int)(progRatio * 100.0f);

                drawVguiWell(winX + 20.0f, winY + 76.0f, winW - 40.0f, 42.0f, "");
                std::string progStr = std::to_string(unlockedCount) + " of " + std::to_string(totalCount) + " Achievements Unlocked (" + std::to_string(percent) + "%)";
                LabFont::drawText(winX + 32.0f, winY + 88.0f, progStr, 1.55f, Vec3(1.0f, 0.84f, 0.22f), LabFontType::GeoSans);
                drawVguiProgressBar(winX + 370.0f, winY + 89.0f, 360.0f, 16.0f, progRatio, Vec3(0.83f, 0.61f, 0.16f));

                // Checkbox: [ ] Hide completed
                bool isCbHov = (_menuMouseX >= winX + winW - 210.0f && _menuMouseX <= winX + winW - 20.0f && _menuMouseY >= winY + 82.0f && _menuMouseY <= winY + 110.0f);
                std::string cbText = _achievementsHideCompleted ? "[x] Hide Unlocked" : "[ ] Hide Unlocked";
                LabFont::drawText(winX + winW - 205.0f, winY + 88.0f, cbText, 1.40f, isCbHov ? Vec3(1, 1, 1) : Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);

                // Grid of 8 achievements (2 columns x 4 rows)
                drawVguiWell(winX + 20.0f, winY + 124.0f, winW - 40.0f, 388.0f, "");

                int cardIdx = 0;
                for (size_t i = 0; i < _achievements.size(); ++i) {
                    const auto& a = _achievements[i];
                    if (_achievementsHideCompleted && a.unlocked) continue;

                    int col = cardIdx % 2;
                    int row = cardIdx / 2;
                    cardIdx++;
                    if (row >= 4) break;

                    float cx = winX + 30.0f + static_cast<float>(col) * 470.0f;
                    float cy = winY + 134.0f + static_cast<float>(row) * 92.0f;

                    // Card background
                    Renderer::drawRect(cx, cy, 455.0f, 84.0f, a.unlocked ? Vec3(0.12f, 0.15f, 0.19f) : Vec3(0.09f, 0.10f, 0.12f));
                    Renderer::drawRect(cx, cy, 455.0f, 1.0f, a.unlocked ? Vec3(0.25f, 0.32f, 0.42f) : Vec3(0.16f, 0.18f, 0.22f));
                    Renderer::drawRect(cx, cy, 1.0f, 84.0f, a.unlocked ? Vec3(0.25f, 0.32f, 0.42f) : Vec3(0.16f, 0.18f, 0.22f));
                    Renderer::drawRect(cx + 454.0f, cy, 1.0f, 84.0f, Vec3(0.05f, 0.06f, 0.07f));
                    Renderer::drawRect(cx, cy + 83.0f, 455.0f, 1.0f, Vec3(0.05f, 0.06f, 0.07f));

                    // Badge box
                    Renderer::drawRect(cx + 10.0f, cy + 10.0f, 44.0f, 44.0f, a.unlocked ? Vec3(0.18f, 0.24f, 0.32f) : Vec3(0.07f, 0.08f, 0.10f));
                    Renderer::drawRect(cx + 10.0f, cy + 10.0f, 44.0f, 1.0f, a.unlocked ? Vec3(1.0f, 0.72f, 0.15f) : Vec3(0.20f, 0.22f, 0.26f));
                    Renderer::drawRect(cx + 10.0f, cy + 10.0f, 1.0f, 44.0f, a.unlocked ? Vec3(1.0f, 0.72f, 0.15f) : Vec3(0.20f, 0.22f, 0.26f));
                    Texture* badgeTex = a.iconTexture.empty() ? nullptr : getTexture(a.iconTexture);
                    if (badgeTex) {
                        Vec3 badgeTint = a.unlocked ? Vec3(1.0f, 0.85f, 0.25f) : Vec3(0.40f, 0.44f, 0.50f);
                        float badgeAlpha = a.unlocked ? 1.0f : 0.40f;
                        Renderer::drawTextureRect(cx + 14.0f, cy + 14.0f, 36.0f, 36.0f, *badgeTex, badgeTint, badgeAlpha);
                    } else {
                        LabFont::drawText(cx + 18.0f, cy + 20.0f, a.icon, 1.60f, a.unlocked ? Vec3(1.0f, 0.84f, 0.22f) : Vec3(0.40f, 0.44f, 0.50f), LabFontType::GeoSans);
                    }

                    // Title & Description
                    LabFont::drawText(cx + 64.0f, cy + 10.0f, a.title, 1.55f, a.unlocked ? Vec3(0.95f, 0.98f, 1.0f) : Vec3(0.70f, 0.74f, 0.80f), LabFontType::GeoSans);
                    LabFont::drawText(cx + 64.0f, cy + 30.0f, a.description, 1.25f, Vec3(0.55f, 0.60f, 0.68f), LabFontType::System);

                    // Mini Progress bar
                    float frac = a.maxProgress > 0 ? ((float)a.currentProgress / (float)a.maxProgress) : 0.0f;
                    drawVguiProgressBar(cx + 64.0f, cy + 58.0f, 230.0f, 12.0f, frac, a.unlocked ? Vec3(0.42f, 0.66f, 0.20f) : Vec3(0.70f, 0.52f, 0.15f));
                    std::string progCount = std::to_string(a.currentProgress) + " / " + std::to_string(a.maxProgress);
                    LabFont::drawText(cx + 302.0f, cy + 57.0f, progCount, 1.25f, Vec3(0.60f, 0.65f, 0.72f), LabFontType::GeoSans);

                    // Status Badge
                    if (a.unlocked) {
                        LabFont::drawText(cx + 355.0f, cy + 10.0f, "[ UNLOCKED ]", 1.35f, Vec3(1.0f, 0.80f, 0.20f), LabFontType::GeoSans);
                    } else {
                        LabFont::drawText(cx + 368.0f, cy + 10.0f, "[ LOCKED ]", 1.35f, Vec3(0.40f, 0.44f, 0.50f), LabFontType::GeoSans);
                    }
                }
            } else {
                // Tab 1: Career Statistics
                drawVguiWell(winX + 20.0f, winY + 76.0f, winW - 40.0f, 436.0f, "CAREER GAMEPLAY STATISTICS");

                struct StatRow {
                    std::string label;
                    std::string value;
                };

                int kdRatioInt = _careerStats.totalDeaths > 0 ? (int)((_careerStats.totalKills * 100) / _careerStats.totalDeaths) : (_careerStats.totalKills * 100);
                std::string kdStr = std::to_string(kdRatioInt / 100) + "." + std::to_string((kdRatioInt % 100) < 10 ? 0 : 0) + std::to_string(kdRatioInt % 100);
                int hsPct = _careerStats.totalKills > 0 ? ((_careerStats.headshots * 100) / _careerStats.totalKills) : 0;
                int accPct = _careerStats.shotsFired > 0 ? ((_careerStats.shotsHit * 100) / _careerStats.shotsFired) : 0;
                int playMinutes = (int)(_careerStats.totalPlaytimeSeconds / 60.0f);

                std::vector<StatRow> leftStats = {
                    { "TOTAL KILLS / FRAGS", std::to_string(_careerStats.totalKills) },
                    { "TOTAL DEATHS", std::to_string(_careerStats.totalDeaths) },
                    { "KILL / DEATH RATIO", kdStr },
                    { "HEADSHOT ELIMINATIONS", std::to_string(_careerStats.headshots) },
                    { "HEADSHOT ACCURACY RATIO", std::to_string(hsPct) + " %" }
                };

                std::vector<StatRow> rightStats = {
                    { "TOTAL SHOTS FIRED", std::to_string(_careerStats.shotsFired) },
                    { "OVERALL WEAPON ACCURACY", std::to_string(accPct) + " %" },
                    { "BUNNYHOP JUMPS EXECUTED", std::to_string(_careerStats.bhopJumps) },
                    { "EXPLOSIVE BARRELS DETONATED", std::to_string(_careerStats.barrelsExploded) },
                    { "RECORDED PLAYTIME", std::to_string(playMinutes) + " minutes" }
                };

                for (size_t r = 0; r < leftStats.size(); ++r) {
                    float ry = winY + 115.0f + static_cast<float>(r) * 56.0f;
                    Renderer::drawRect(winX + 40.0f, ry, 430.0f, 44.0f, (r % 2 == 0) ? Vec3(0.09f, 0.11f, 0.14f) : Vec3(0.12f, 0.14f, 0.18f));
                    LabFont::drawText(winX + 54.0f, ry + 12.0f, leftStats[r].label, 1.45f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
                    LabFont::drawText(winX + 380.0f, ry + 12.0f, leftStats[r].value, 1.55f, Vec3(1.0f, 0.84f, 0.22f), LabFontType::GeoSans);
                }

                for (size_t r = 0; r < rightStats.size(); ++r) {
                    float ry = winY + 115.0f + static_cast<float>(r) * 56.0f;
                    Renderer::drawRect(winX + 510.0f, ry, 430.0f, 44.0f, (r % 2 == 0) ? Vec3(0.09f, 0.11f, 0.14f) : Vec3(0.12f, 0.14f, 0.18f));
                    LabFont::drawText(winX + 524.0f, ry + 12.0f, rightStats[r].label, 1.45f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
                    LabFont::drawText(winX + 850.0f, ry + 12.0f, rightStats[r].value, 1.55f, Vec3(1.0f, 0.84f, 0.22f), LabFontType::GeoSans);
                }
            }

            // Bottom action buttons
            bool isCloseBtnHov = (_menuMouseX >= winX + 20.0f && _menuMouseX <= winX + 160.0f && _menuMouseY >= winY + 524.0f && _menuMouseY <= winY + 558.0f);
            drawVguiButton(winX + 20.0f, winY + 524.0f, 140.0f, 34.0f, "Close", isCloseBtnHov, false);

            bool isResetHov = (_menuMouseX >= winX + 180.0f && _menuMouseX <= winX + 320.0f && _menuMouseY >= winY + 524.0f && _menuMouseY <= winY + 558.0f);
            drawVguiButton(winX + 180.0f, winY + 524.0f, 140.0f, 34.0f, "Reset Stats", isResetHov, false);
        }
        else if (_menuScreen == MenuScreen::Singleplayer) {
            bool isCloseHov = (_menuMouseX >= winX + winW - 28.0f && _menuMouseX <= winX + winW - 6.0f && _menuMouseY >= winY + 6.0f && _menuMouseY <= winY + 28.0f);
            drawVguiWindow(winX, winY, winW, winH, "NEW GAME", isCloseHov);

            // Left Well: Map Selection
            drawVguiWell(winX + 20.0f, winY + 46.0f, 440.0f, 455.0f, "CHAPTER & SECTOR SELECTION");

            int maxVis = 7;
            int totalMaps = (int)_availableMaps.size();
            _mapScrollOffset = std::clamp(_mapScrollOffset, 0, std::max(0, totalMaps - maxVis));
            int visCount = std::min(maxVis, totalMaps - _mapScrollOffset);

            for (int i = 0; i < visCount; ++i) {
                int mapIdx = _mapScrollOffset + i;
                float iy = winY + 80.0f + static_cast<float>(i) * 44.0f;
                bool isSel = (mapIdx == _selectedMapIndex);
                bool isHov = (_menuMouseX >= winX + 26.0f && _menuMouseX <= winX + 454.0f && _menuMouseY >= iy && _menuMouseY <= iy + 38.0f);
                Vec3 rowBg = isSel ? Vec3(0.18f, 0.24f, 0.32f) : (isHov ? Vec3(0.12f, 0.15f, 0.19f) : Vec3(0.08f, 0.09f, 0.11f));
                Renderer::drawRect(winX + 26.0f, iy, 428.0f, 38.0f, rowBg);
                if (isSel) {
                    Renderer::drawRect(winX + 26.0f, iy, 3.5f, 38.0f, Vec3(1.0f, 0.72f, 0.15f));
                }
                LabFont::drawText(winX + 38.0f, iy + 10.0f, getMapTitle(_availableMaps[mapIdx]), 1.55f, isSel ? Vec3(1, 1, 1) : Vec3(0.78f, 0.82f, 0.88f), LabFontType::GeoSans);
            }

            // Scroll / Navigation Bar
            if (totalMaps > maxVis) {
                bool isUpHov = (_menuMouseX >= winX + 26.0f && _menuMouseX <= winX + 76.0f && _menuMouseY >= winY + 395.0f && _menuMouseY <= winY + 425.0f);
                bool isDnHov = (_menuMouseX >= winX + 404.0f && _menuMouseX <= winX + 454.0f && _menuMouseY >= winY + 395.0f && _menuMouseY <= winY + 425.0f);
                drawVguiButton(winX + 26.0f, winY + 395.0f, 50.0f, 30.0f, "UP", isUpHov, false);
                std::string pageStr = std::to_string(_mapScrollOffset + 1) + " - " + std::to_string(_mapScrollOffset + visCount) + " of " + std::to_string(totalMaps);
                LabFont::drawText(winX + 180.0f, winY + 403.0f, pageStr, 1.45f, Vec3(0.65f, 0.70f, 0.78f), LabFontType::GeoSans);
                drawVguiButton(winX + 404.0f, winY + 395.0f, 50.0f, 30.0f, "DN", isDnHov, false);
            }

            // Right Well: Map Information
            drawVguiWell(winX + 480.0f, winY + 46.0f, 480.0f, 455.0f, "MISSION BRIEFING");

            std::string curMapName = (_selectedMapIndex < (int)_availableMaps.size()) ? toSceneDisplayPath(_availableMaps[_selectedMapIndex]) : "Cryo Facility";
            LabFont::drawText(winX + 504.0f, winY + 84.0f, curMapName, 2.2f, Vec3(1.0f, 0.84f, 0.22f), LabFontType::GeoSans);
            LabFont::drawText(winX + 504.0f, winY + 124.0f, "TYPE:     Singleplayer Solo Exploration & Combat", 1.45f, Vec3(0.80f, 0.85f, 0.90f), LabFontType::GeoSans);
            std::string mapPathStr = (_selectedMapIndex < (int)_availableMaps.size()) ? _availableMaps[_selectedMapIndex] : "";
            LabFont::drawText(winX + 504.0f, winY + 154.0f, "PATH:     " + mapPathStr, 1.30f, Vec3(0.55f, 0.65f, 0.75f), LabFontType::System);
            LabFont::drawText(winX + 504.0f, winY + 184.0f, "ENGINE:   OpenGL 4.5+ Core DSA / Blinn-Phong", 1.40f, Vec3(0.35f, 0.85f, 0.55f), LabFontType::System);
            LabFont::drawText(winX + 504.0f, winY + 214.0f, "PHYSICS:  Source 64Hz Bhop, triggers & dynamic collisions", 1.40f, Vec3(0.35f, 0.85f, 0.95f), LabFontType::System);

            // Bottom Buttons
            bool isBackHov = (_menuMouseX >= winX + 20.0f && _menuMouseX <= winX + 160.0f && _menuMouseY >= winY + 524.0f && _menuMouseY <= winY + 558.0f);
            drawVguiButton(winX + 20.0f, winY + 524.0f, 140.0f, 34.0f, "Back", isBackHov, false);

            bool isBrowseHov = (_menuMouseX >= winX + 180.0f && _menuMouseX <= winX + 330.0f && _menuMouseY >= winY + 524.0f && _menuMouseY <= winY + 558.0f);
            drawVguiButton(winX + 180.0f, winY + 524.0f, 150.0f, 34.0f, "Browse...", isBrowseHov, false);

            bool isStartHov = (_menuMouseX >= winX + winW - 160.0f && _menuMouseX <= winX + winW - 20.0f && _menuMouseY >= winY + 524.0f && _menuMouseY <= winY + 558.0f);
            drawVguiButton(winX + winW - 160.0f, winY + 524.0f, 140.0f, 34.0f, "Start", isStartHov, true);
        }
        else if (_menuScreen == MenuScreen::HostGame) {
            bool isCloseHov = (_menuMouseX >= winX + winW - 28.0f && _menuMouseX <= winX + winW - 6.0f && _menuMouseY >= winY + 6.0f && _menuMouseY <= winY + 28.0f);
            drawVguiWindow(winX, winY, winW, winH, "CREATE SERVER", isCloseHov);

            // Left Well: Map Selection
            drawVguiWell(winX + 20.0f, winY + 46.0f, 440.0f, 455.0f, "SELECT MAP");

            int maxVisHost = 7;
            int totalMapsHost = (int)_availableMaps.size();
            _mapScrollOffset = std::clamp(_mapScrollOffset, 0, std::max(0, totalMapsHost - maxVisHost));
            int visCountHost = std::min(maxVisHost, totalMapsHost - _mapScrollOffset);

            for (int i = 0; i < visCountHost; ++i) {
                int mapIdx = _mapScrollOffset + i;
                float iy = winY + 80.0f + static_cast<float>(i) * 44.0f;
                bool isSel = (_sessionConfig.mapPath == _availableMaps[mapIdx]);
                bool isHov = (_menuMouseX >= winX + 26.0f && _menuMouseX <= winX + 454.0f && _menuMouseY >= iy && _menuMouseY <= iy + 38.0f);
                Vec3 rowBg = isSel ? Vec3(0.18f, 0.24f, 0.32f) : (isHov ? Vec3(0.12f, 0.15f, 0.19f) : Vec3(0.08f, 0.09f, 0.11f));
                Renderer::drawRect(winX + 26.0f, iy, 428.0f, 38.0f, rowBg);
                if (isSel) Renderer::drawRect(winX + 26.0f, iy, 3.5f, 38.0f, Vec3(1.0f, 0.72f, 0.15f));
                LabFont::drawText(winX + 38.0f, iy + 10.0f, getMapTitle(_availableMaps[mapIdx]), 1.55f, isSel ? Vec3(1, 1, 1) : Vec3(0.78f, 0.82f, 0.88f), LabFontType::GeoSans);
            }

            if (totalMapsHost > maxVisHost) {
                bool isUpHov = (_menuMouseX >= winX + 26.0f && _menuMouseX <= winX + 76.0f && _menuMouseY >= winY + 395.0f && _menuMouseY <= winY + 425.0f);
                bool isDnHov = (_menuMouseX >= winX + 404.0f && _menuMouseX <= winX + 454.0f && _menuMouseY >= winY + 395.0f && _menuMouseY <= winY + 425.0f);
                drawVguiButton(winX + 26.0f, winY + 395.0f, 50.0f, 30.0f, "UP", isUpHov, false);
                std::string pageStr = std::to_string(_mapScrollOffset + 1) + " - " + std::to_string(_mapScrollOffset + visCountHost) + " of " + std::to_string(totalMapsHost);
                LabFont::drawText(winX + 180.0f, winY + 403.0f, pageStr, 1.45f, Vec3(0.65f, 0.70f, 0.78f), LabFontType::GeoSans);
                drawVguiButton(winX + 404.0f, winY + 395.0f, 50.0f, 30.0f, "DN", isDnHov, false);
            }

            bool isBrowseHov = (_menuMouseX >= winX + 26.0f && _menuMouseX <= winX + 454.0f && _menuMouseY >= winY + 450.0f && _menuMouseY <= winY + 484.0f);
            drawVguiButton(winX + 26.0f, winY + 450.0f, 428.0f, 34.0f, "Browse Disk Map...", isBrowseHov, false);

            // Right Well: Rules & Bots
            drawVguiWell(winX + 480.0f, winY + 46.0f, 480.0f, 455.0f, "SERVER SETTINGS");

            // Network Mode
            LabFont::drawText(winX + 490.0f, winY + 62.0f, "NETWORK MODE", 1.35f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
            bool isLAN = (_sessionConfig.networkMode == NetworkMode::LAN);
            drawVguiButton(winX + 490.0f, winY + 80.0f, 225.0f, 30.0f, "LAN Mode", false, isLAN);
            drawVguiButton(winX + 725.0f, winY + 80.0f, 225.0f, 30.0f, "P2P / Internet", false, !isLAN);

            // Game Mode
            LabFont::drawText(winX + 490.0f, winY + 116.0f, "GAME MODE", 1.35f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
            drawVguiButton(winX + 490.0f, winY + 130.0f, 145.0f, 30.0f, "FFA", false, _sessionConfig.mode == GameMode::FFA);
            drawVguiButton(winX + 645.0f, winY + 130.0f, 145.0f, 30.0f, "DM", false, _sessionConfig.mode == GameMode::DM);
            drawVguiButton(winX + 800.0f, winY + 130.0f, 150.0f, 30.0f, "TDM", false, _sessionConfig.mode == GameMode::TDM);

            // Combat Bots
            LabFont::drawText(winX + 490.0f, winY + 166.0f, "AI COMBAT SQUAD", 1.35f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
            drawVguiButton(winX + 490.0f, winY + 180.0f, 220.0f, 30.0f, _sessionConfig.enableBots ? "Bots: Enabled" : "Bots: Disabled", false, _sessionConfig.enableBots);
            drawVguiButton(winX + 725.0f, winY + 180.0f, 40.0f, 30.0f, "-", false, false);
            std::string botCntStr = std::to_string(_sessionConfig.botCount) + " Bots";
            LabFont::drawText(winX + 780.0f, winY + 186.0f, botCntStr, 1.50f, Vec3(1, 1, 1), LabFontType::GeoSans);
            drawVguiButton(winX + 910.0f, winY + 180.0f, 40.0f, 30.0f, "+", false, false);

            // Frag Limit
            LabFont::drawText(winX + 490.0f, winY + 216.0f, "MATCH LIMITS", 1.35f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
            drawVguiButton(winX + 725.0f, winY + 230.0f, 40.0f, 30.0f, "-", false, false);
            std::string fragStr = std::to_string(_sessionConfig.fragLimit) + " Frags";
            LabFont::drawText(winX + 780.0f, winY + 236.0f, fragStr, 1.50f, Vec3(1, 1, 1), LabFontType::GeoSans);
            drawVguiButton(winX + 910.0f, winY + 230.0f, 40.0f, 30.0f, "+", false, false);

            // Telemetry
            drawVguiButton(winX + 490.0f, winY + 280.0f, 220.0f, 30.0f, "Copy Host IP", false, false);
            drawVguiButton(winX + 725.0f, winY + 280.0f, 110.0f, 30.0f, "Copy Code", false, false);
            drawVguiButton(winX + 845.0f, winY + 280.0f, 105.0f, 30.0f, "New Code", false, false);

            if (!_punchStatusMsg.empty()) {
                LabFont::drawText(winX + 490.0f, winY + 330.0f, _punchStatusMsg, 1.25f, Vec3(0.35f, 0.95f, 0.50f), LabFontType::System);
            }

            // Bottom Buttons
            bool isCloseBtnHov = (_menuMouseX >= winX + 20.0f && _menuMouseX <= winX + 160.0f && _menuMouseY >= winY + 524.0f && _menuMouseY <= winY + 558.0f);
            drawVguiButton(winX + 20.0f, winY + 524.0f, 140.0f, 34.0f, "Close", isCloseBtnHov, false);

            bool isStartBtnHov = (_menuMouseX >= winX + winW - 160.0f && _menuMouseX <= winX + winW - 20.0f && _menuMouseY >= winY + 524.0f && _menuMouseY <= winY + 558.0f);
            drawVguiButton(winX + winW - 160.0f, winY + 524.0f, 140.0f, 34.0f, "Start", isStartBtnHov, true);
        }
        else if (_menuScreen == MenuScreen::JoinGame) {
            bool isCloseHov = (_menuMouseX >= winX + winW - 28.0f && _menuMouseX <= winX + winW - 6.0f && _menuMouseY >= winY + 6.0f && _menuMouseY <= winY + 28.0f);
            drawVguiWindow(winX, winY, winW, winH, "FIND SERVERS", isCloseHov);

            // Top Source VGUI2 Server Browser Tabs: INTERNET, FAVORITES, HISTORY, LAN, FRIENDS
            const char* srvTabs[5] = { "INTERNET", "FAVORITES", "HISTORY", "LAN", "FRIENDS" };
            for (int t = 0; t < 5; ++t) {
                float tx = winX + 20.0f + static_cast<float>(t) * 118.0f;
                bool isSel = (_serverBrowserActiveTab == t);
                bool isHov = (_menuMouseX >= tx && _menuMouseX <= tx + 112.0f && _menuMouseY >= winY + 38.0f && _menuMouseY <= winY + 62.0f);
                Vec3 tabBg = isSel ? Vec3(0.14f, 0.16f, 0.20f) : (isHov ? Vec3(0.12f, 0.14f, 0.17f) : Vec3(0.09f, 0.10f, 0.12f));
                Vec3 txtCol = isSel ? Vec3(1.0f, 0.83f, 0.22f) : (isHov ? Vec3(0.85f, 0.88f, 0.92f) : Vec3(0.50f, 0.54f, 0.58f));
                Renderer::drawRect(tx, winY + 38.0f, 112.0f, 24.0f, tabBg);
                if (isSel) Renderer::drawRect(tx, winY + 60.0f, 112.0f, 2.0f, Vec3(1.0f, 0.70f, 0.0f));
                LabFont::drawText(tx + 12.0f, winY + 42.0f, srvTabs[t], 1.35f, txtCol, LabFontType::GeoSans);
            }

            // Quick Join Bar
            drawVguiWell(winX + 20.0f, winY + 64.0f, winW - 40.0f, 38.0f, "");
            std::string codeDisplay = "ROOM CODE: " + _pnpJoinCode + (_pnpJoinCodeActive ? "_" : "");
            Renderer::drawRect(winX + 30.0f, winY + 68.0f, 200.0f, 28.0f, _pnpJoinCodeActive ? Vec3(0.14f, 0.18f, 0.24f) : Vec3(0.09f, 0.10f, 0.13f));
            LabFont::drawText(winX + 40.0f, winY + 74.0f, codeDisplay, 1.40f, Vec3(1.0f, 0.84f, 0.22f), LabFontType::GeoSans);

            drawVguiButton(winX + 240.0f, winY + 68.0f, 80.0f, 28.0f, "Paste", false, false);
            drawVguiButton(winX + 330.0f, winY + 68.0f, 110.0f, 28.0f, "Join Code", false, true);
            drawVguiButton(winX + winW - 130.0f, winY + 68.0f, 90.0f, 28.0f, "Copy IP", false, false);

            if (!_connectStatusMsg.empty()) {
                LabFont::drawText(winX + 460.0f, winY + 74.0f, _connectStatusMsg, 1.20f, Vec3(0.35f, 0.85f, 0.95f), LabFontType::System);
            }

            // Server List Table Header
            Renderer::drawRect(winX + 20.0f, winY + 108.0f, winW - 40.0f, 24.0f, Vec3(0.12f, 0.14f, 0.18f));
            LabFont::drawText(winX + 35.0f, winY + 112.0f, "SERVER NAME", 1.35f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
            LabFont::drawText(winX + 340.0f, winY + 112.0f, "GAME", 1.35f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
            LabFont::drawText(winX + 480.0f, winY + 112.0f, "MAP", 1.35f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
            LabFont::drawText(winX + 660.0f, winY + 112.0f, "MODE", 1.35f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
            LabFont::drawText(winX + 760.0f, winY + 112.0f, "PLAYERS", 1.35f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
            LabFont::drawText(winX + 870.0f, winY + 112.0f, "PING", 1.35f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);

            // Table Content Well
            drawVguiWell(winX + 20.0f, winY + 132.0f, winW - 40.0f, 294.0f, "");

            auto servers = _serverBrowser.getFilteredServers(_serverBrowserActiveTab, _serverFilterMap, _serverFilterMaxPing, _serverFilterNotFull, _serverFilterHasPlayers);
            if (servers.empty()) {
                std::string emptyMsg = (_serverBrowserActiveTab == 1) ? "No favorite servers saved yet. Select a server and click [Add Fav]." :
                                       (_serverBrowserActiveTab == 2) ? "No recent servers in connection history." :
                                       "No servers matching active filters. Use Room Code above or Direct IP below.";
                LabFont::drawText(winX + 40.0f, winY + 160.0f, emptyMsg, 1.45f, Vec3(0.55f, 0.60f, 0.68f), LabFontType::GeoSans);
            } else {
                for (size_t i = 0; i < servers.size() && i < 6; ++i) {
                    float rowY = winY + 134.0f + static_cast<float>(i) * 40.0f;
                    bool isSel = ((int)i == _selectedServerIndex);
                    bool isHov = (_menuMouseX >= winX + 22.0f && _menuMouseX <= winX + winW - 22.0f && _menuMouseY >= rowY && _menuMouseY <= rowY + 36.0f);
                    Vec3 rBg = isSel ? Vec3(0.18f, 0.24f, 0.32f) : (isHov ? Vec3(0.12f, 0.15f, 0.19f) : ((i % 2 == 0) ? Vec3(0.08f, 0.09f, 0.11f) : Vec3(0.10f, 0.11f, 0.14f)));
                    Renderer::drawRect(winX + 22.0f, rowY, winW - 44.0f, 36.0f, rBg);
                    if (isSel) Renderer::drawRect(winX + 22.0f, rowY, 3.5f, 36.0f, Vec3(1.0f, 0.72f, 0.15f));

                    std::string srvLabel = (servers[i].isFavorite ? "[*] " : "") + servers[i].name;
                    LabFont::drawText(winX + 35.0f, rowY + 10.0f, srvLabel, 1.45f, isSel ? Vec3(1, 1, 1) : Vec3(0.85f, 0.88f, 0.92f), LabFontType::GeoSans);
                    LabFont::drawText(winX + 340.0f, rowY + 10.0f, servers[i].game, 1.40f, Vec3(0.65f, 0.72f, 0.80f), LabFontType::GeoSans);
                    LabFont::drawText(winX + 480.0f, rowY + 10.0f, servers[i].map, 1.45f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
                    LabFont::drawText(winX + 660.0f, rowY + 10.0f, servers[i].mode, 1.45f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
                    std::string plyStr = std::to_string(servers[i].playerCount) + " / " + std::to_string(servers[i].maxPlayers);
                    LabFont::drawText(winX + 760.0f, rowY + 10.0f, plyStr, 1.45f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::GeoSans);
                    LabFont::drawText(winX + 870.0f, rowY + 10.0f, std::to_string(servers[i].pingMs) + "ms", 1.45f, Vec3(0.35f, 0.85f, 0.55f), LabFontType::GeoSans);
                }
            }

            // Bottom Filter Bar (Source VGUI2 Filter Row)
            drawVguiButton(winX + 20.0f, winY + 432.0f, 140.0f, 30.0f, _serverFilterNotFull ? "[x] Not Full" : "[ ] Not Full", false, _serverFilterNotFull);
            drawVguiButton(winX + 170.0f, winY + 432.0f, 150.0f, 30.0f, _serverFilterHasPlayers ? "[x] Has Players" : "[ ] Has Players", false, _serverFilterHasPlayers);
            std::string pingFilterLbl = (_serverFilterMaxPing == 0) ? "Ping: Any" : ("Ping < " + std::to_string(_serverFilterMaxPing));
            drawVguiButton(winX + 330.0f, winY + 432.0f, 160.0f, 30.0f, pingFilterLbl, false, _serverFilterMaxPing > 0);
            drawVguiButton(winX + winW - 140.0f, winY + 432.0f, 120.0f, 30.0f, "Add Fav", false, false);

            // Direct IP Connect Bar
            std::string directDisplay = "DIRECT IP: " + _directConnectIP + (_directConnectActive ? "_" : "");
            Renderer::drawRect(winX + 20.0f, winY + 472.0f, 600.0f, 32.0f, _directConnectActive ? Vec3(0.14f, 0.18f, 0.24f) : Vec3(0.09f, 0.10f, 0.13f));
            LabFont::drawText(winX + 32.0f, winY + 480.0f, directDisplay, 1.45f, Vec3(1.0f, 0.84f, 0.22f), LabFontType::GeoSans);
            drawVguiButton(winX + 630.0f, winY + 472.0f, 80.0f, 32.0f, "Paste", false, false);
            drawVguiButton(winX + 720.0f, winY + 472.0f, 80.0f, 32.0f, "Clear", false, false);

            // Bottom Buttons
            bool isCloseBtnHov = (_menuMouseX >= winX + 20.0f && _menuMouseX <= winX + 160.0f && _menuMouseY >= winY + 524.0f && _menuMouseY <= winY + 558.0f);
            drawVguiButton(winX + 20.0f, winY + 524.0f, 140.0f, 34.0f, "Close", isCloseBtnHov, false);

            bool isRefreshHov = (_menuMouseX >= winX + winW - 320.0f && _menuMouseX <= winX + winW - 180.0f && _menuMouseY >= winY + 524.0f && _menuMouseY <= winY + 558.0f);
            drawVguiButton(winX + winW - 320.0f, winY + 524.0f, 140.0f, 34.0f, "Refresh", isRefreshHov, false);

            bool isConnectHov = (_menuMouseX >= winX + winW - 160.0f && _menuMouseX <= winX + winW - 20.0f && _menuMouseY >= winY + 524.0f && _menuMouseY <= winY + 558.0f);
            drawVguiButton(winX + winW - 160.0f, winY + 524.0f, 140.0f, 34.0f, _isConnecting ? "Cancel" : "Connect", isConnectHov, true);
        }
        else if (_menuScreen == MenuScreen::LoadingMap) {
            const float loadW = 720.0f;
            const float loadH = 420.0f;
            const float loadX = (1280.0f - loadW) * 0.5f;
            const float loadY = (720.0f - loadH) * 0.5f;

            drawVguiWindow(loadX, loadY, loadW, loadH, "LOADING SECTOR // RESOURCE SYNCHRONIZATION", false);
            drawVguiWell(loadX + 20.0f, loadY + 48.0f, loadW - 40.0f, 300.0f, "FASTDL & WORLD INITIALIZATION");

            LabFont::drawText(loadX + 38.0f, loadY + 86.0f, "TARGET SECTOR: " + _loadingState.pendingConfig.mapPath, 1.55f, Vec3(1.0f, 0.84f, 0.22f), LabFontType::GeoSans);
            LabFont::drawText(loadX + 38.0f, loadY + 114.0f, "GAMEPLAY MODE: " + _loadingState.pendingConfig.getModeString(), 1.40f, Vec3(0.75f, 0.82f, 0.90f), LabFontType::GeoSans);

            const char* stages[5] = {
                "1. Establishing network handshake & server tick synchronization...",
                "2. FastDL: Verifying & downloading missing .lmap / .lpak packages...",
                "3. Parsing BSP world geometry, lightmaps & static props...",
                "4. Precaching 70-bone Guard skeletal rig & 3D spatial audio...",
                "5. Sending client info & spawning local player entity..."
            };
            for (int s = 0; s < 5; ++s) {
                float sy = loadY + 148.0f + static_cast<float>(s) * 24.0f;
                bool done = (_loadingState.stageIndex > s);
                bool active = (_loadingState.stageIndex == s);
                Vec3 col = done ? Vec3(0.35f, 0.88f, 0.50f) : (active ? Vec3(1.0f, 0.84f, 0.22f) : Vec3(0.45f, 0.50f, 0.56f));
                std::string prefix = done ? "[OK] " : (active ? "[>>] " : "[..] ");
                LabFont::drawText(loadX + 42.0f, sy, prefix + stages[s], 1.30f, col, LabFontType::System);
            }

            // FastDL Transfer Bar
            float dlFrac = (_loadingState.fastDlTotalBytes > 0)
                ? (static_cast<float>(_loadingState.fastDlBytesDownloaded) / static_cast<float>(_loadingState.fastDlTotalBytes))
                : 1.0f;
            std::string dlText = "FastDL Package: " + _loadingState.fastDlCurrentFile + " (" +
                                 std::to_string(_loadingState.fastDlBytesDownloaded / 1024) + " KB / " +
                                 std::to_string(_loadingState.fastDlTotalBytes / 1024) + " KB)";
            LabFont::drawText(loadX + 38.0f, loadY + 276.0f, dlText, 1.25f, Vec3(0.65f, 0.75f, 0.88f), LabFontType::System);
            drawVguiProgressBar(loadX + 38.0f, loadY + 296.0f, loadW - 76.0f, 14.0f, dlFrac, Vec3(0.25f, 0.72f, 0.95f));

            // Overall Segmented Source Orange Progress Bar
            LabFont::drawText(loadX + 20.0f, loadY + loadH - 44.0f, _loadingState.statusText, 1.35f, Vec3(0.85f, 0.88f, 0.92f), LabFontType::GeoSans);
            drawVguiProgressBar(loadX + 38.0f, loadY + 322.0f, loadW - 76.0f, 18.0f, _loadingState.progress, Vec3(0.92f, 0.68f, 0.16f));

            bool isCancelHov = (_menuMouseX >= loadX + loadW - 150.0f && _menuMouseX <= loadX + loadW - 20.0f && _menuMouseY >= loadY + loadH - 50.0f && _menuMouseY <= loadY + loadH - 16.0f);
            drawVguiButton(loadX + loadW - 150.0f, loadY + loadH - 50.0f, 130.0f, 34.0f, "Cancel", isCancelHov, false);
        }
        else if (_menuScreen == MenuScreen::Addons) {
            bool isCloseHov = (_menuMouseX >= winX + winW - 28.0f && _menuMouseX <= winX + winW - 6.0f && _menuMouseY >= winY + 6.0f && _menuMouseY <= winY + 28.0f);
            drawVguiWindow(winX, winY, winW, winH, "ADDONS MANAGER", isCloseHov);

            // Left Well: Categories
            drawVguiWell(winX + 20.0f, winY + 46.0f, 220.0f, 455.0f, "CATEGORIES");

            const char* cats[5] = { "ALL PACKAGES", "MAPS & LEVELS", "MODELS & SKINS", "LUA SCRIPTS", "UI HUD PACKS" };
            for (int c = 0; c < 5; ++c) {
                float cy = winY + 80.0f + static_cast<float>(c) * 40.0f;
                bool isSel = (c == _selectedAddonCategory);
                bool isHov = (_menuMouseX >= winX + 26.0f && _menuMouseX <= winX + 234.0f && _menuMouseY >= cy && _menuMouseY <= cy + 34.0f);
                Vec3 rBg = isSel ? Vec3(0.18f, 0.24f, 0.32f) : (isHov ? Vec3(0.12f, 0.15f, 0.19f) : Vec3(0.08f, 0.09f, 0.11f));
                Renderer::drawRect(winX + 26.0f, cy, 208.0f, 34.0f, rBg);
                if (isSel) Renderer::drawRect(winX + 26.0f, cy, 3.5f, 34.0f, Vec3(1.0f, 0.72f, 0.15f));
                LabFont::drawText(winX + 36.0f, cy + 9.0f, cats[c], 1.45f, isSel ? Vec3(1, 1, 1) : Vec3(0.75f, 0.80f, 0.86f), LabFontType::GeoSans);
            }

            // Right Well: Addons List
            drawVguiWell(winX + 260.0f, winY + 46.0f, 700.0f, 455.0f, "INSTALLED PACKAGES");

            if (_discoveredAddons.empty()) {
                LabFont::drawText(winX + 280.0f, winY + 90.0f, "No addons detected in /addons directory.", 1.55f, Vec3(0.55f, 0.60f, 0.68f), LabFontType::GeoSans);
                LabFont::drawText(winX + 280.0f, winY + 120.0f, "Drop maps, models, scripts or skins into /addons/my_mod to load them!", 1.35f, Vec3(0.40f, 0.45f, 0.52f), LabFontType::System);
            } else {
                for (size_t i = 0; i < _discoveredAddons.size() && i < 5; ++i) {
                    float cardY = winY + 80.0f + static_cast<float>(i) * 78.0f;
                    const auto& mod = _discoveredAddons[i];

                    Renderer::drawRect(winX + 270.0f, cardY, 680.0f, 70.0f, mod.enabled ? Vec3(0.11f, 0.13f, 0.17f) : Vec3(0.08f, 0.09f, 0.11f));
                    Renderer::drawRect(winX + 270.0f, cardY, 680.0f, 1.0f, Vec3(0.18f, 0.22f, 0.28f));
                    Renderer::drawRect(winX + 270.0f, cardY, 1.0f, 70.0f, Vec3(0.18f, 0.22f, 0.28f));

                    LabFont::drawText(winX + 284.0f, cardY + 10.0f, mod.title + " v" + mod.version, 1.60f, mod.enabled ? Vec3(1.0f, 0.84f, 0.22f) : Vec3(0.60f, 0.64f, 0.70f), LabFontType::GeoSans);
                    std::string sub = "Folder: /addons/" + mod.folderName + " | Author: " + mod.author;
                    LabFont::drawText(winX + 284.0f, cardY + 30.0f, sub, 1.25f, Vec3(0.50f, 0.55f, 0.62f), LabFontType::System);
                    LabFont::drawText(winX + 284.0f, cardY + 48.0f, mod.description, 1.30f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::System);

                    // Checkbox
                    float cbX = winX + 270.0f + 580.0f;
                    float cbY = cardY + 20.0f;
                    drawVguiButton(cbX, cbY, 85.0f, 28.0f, mod.enabled ? "[v] Enabled" : "[ ] Disabled", false, mod.enabled);
                }
            }

            // Bottom Buttons
            bool isCloseBtnHov = (_menuMouseX >= winX + 20.0f && _menuMouseX <= winX + 160.0f && _menuMouseY >= winY + 524.0f && _menuMouseY <= winY + 558.0f);
            drawVguiButton(winX + 20.0f, winY + 524.0f, 140.0f, 34.0f, "Close", isCloseBtnHov, false);

            bool isRefreshHov = (_menuMouseX >= winX + 180.0f && _menuMouseX <= winX + 320.0f && _menuMouseY >= winY + 524.0f && _menuMouseY <= winY + 558.0f);
            drawVguiButton(winX + 180.0f, winY + 524.0f, 140.0f, 34.0f, "Refresh", isRefreshHov, false);
        }
        else if (_menuScreen == MenuScreen::Options) {
            const float optX = 230.0f;
            const float optY = 90.0f;
            const float optW = 820.0f;
            const float optH = 540.0f;

            bool isCloseHov = (_menuMouseX >= optX + optW - 28.0f && _menuMouseX <= optX + optW - 6.0f && _menuMouseY >= optY + 6.0f && _menuMouseY <= optY + 28.0f);
            drawVguiWindow(optX, optY, optW, optH, "OPTIONS", isCloseHov);

            // Tabs
            const char* optTabs[3] = { "VIDEO", "AUDIO", "CONTROLS" };
            for (int t = 0; t < 3; ++t) {
                float tx = optX + 20.0f + static_cast<float>(t) * 125.0f;
                bool isSel = (_optionsActiveTab == t);
                bool isHov = (_menuMouseX >= tx && _menuMouseX <= tx + 120.0f && _menuMouseY >= optY + 42.0f && _menuMouseY <= optY + 68.0f);
                Vec3 tabBg = isSel ? Vec3(0.14f, 0.16f, 0.20f) : (isHov ? Vec3(0.12f, 0.14f, 0.17f) : Vec3(0.09f, 0.10f, 0.12f));
                Vec3 txtCol = isSel ? Vec3(1.0f, 0.83f, 0.22f) : (isHov ? Vec3(0.85f, 0.88f, 0.92f) : Vec3(0.50f, 0.54f, 0.58f));
                Renderer::drawRect(tx, optY + 42.0f, 120.0f, 26.0f, tabBg);
                if (isSel) Renderer::drawRect(tx, optY + 66.0f, 120.0f, 2.0f, Vec3(1.0f, 0.70f, 0.0f));
                LabFont::drawText(tx + 16.0f, optY + 47.0f, optTabs[t], 1.45f, txtCol, LabFontType::GeoSans);
            }

            // Container Well
            drawVguiWell(optX + 20.0f, optY + 74.0f, optW - 40.0f, 396.0f, "");

            if (_optionsActiveTab == 0) { // Video
                LabFont::drawText(optX + 44.0f, optY + 104.0f, "DYNAMIC SHADOWS", 1.55f, Vec3(0.85f, 0.88f, 0.94f), LabFontType::GeoSans);
                drawVguiButton(optX + 320.0f, optY + 98.0f, 140.0f, 30.0f, _optShadows ? "Enabled" : "Disabled", false, _optShadows);

                LabFont::drawText(optX + 44.0f, optY + 154.0f, "SHADOW MAP RESOLUTION", 1.55f, Vec3(0.85f, 0.88f, 0.94f), LabFontType::GeoSans);
                drawVguiButton(optX + 320.0f, optY + 148.0f, 90.0f, 30.0f, "1024", false, _optShadowRes == 1024);
                drawVguiButton(optX + 420.0f, optY + 148.0f, 90.0f, 30.0f, "2048", false, _optShadowRes == 2048);

                LabFont::drawText(optX + 44.0f, optY + 204.0f, "FIELD OF VIEW (FOV)", 1.55f, Vec3(0.85f, 0.88f, 0.94f), LabFontType::GeoSans);
                drawVguiButton(optX + 320.0f, optY + 198.0f, 60.0f, 30.0f, "75", false, _optFov == 75);
                drawVguiButton(optX + 390.0f, optY + 198.0f, 60.0f, 30.0f, "90", false, _optFov == 90);
                drawVguiButton(optX + 460.0f, optY + 198.0f, 60.0f, 30.0f, "105", false, _optFov == 105);
            } else if (_optionsActiveTab == 1) { // Audio
                LabFont::drawText(optX + 44.0f, optY + 104.0f, "MASTER VOLUME", 1.55f, Vec3(0.85f, 0.88f, 0.94f), LabFontType::GeoSans);
                drawVguiButton(optX + 320.0f, optY + 98.0f, 40.0f, 30.0f, "-", false, false);
                std::string volStr = std::to_string((int)(_optMasterVolume * 100.0f)) + " %";
                LabFont::drawText(optX + 380.0f, optY + 104.0f, volStr, 1.55f, Vec3(1, 1, 1), LabFontType::GeoSans);
                drawVguiButton(optX + 460.0f, optY + 98.0f, 40.0f, 30.0f, "+", false, false);
            } else if (_optionsActiveTab == 2) { // Controls
                LabFont::drawText(optX + 44.0f, optY + 104.0f, "MOUSE SENSITIVITY", 1.55f, Vec3(0.85f, 0.88f, 0.94f), LabFontType::GeoSans);
                drawVguiButton(optX + 320.0f, optY + 98.0f, 40.0f, 30.0f, "-", false, false);
                char sensBuf[32];
                snprintf(sensBuf, sizeof(sensBuf), "%.2f", _optMouseSens);
                LabFont::drawText(optX + 380.0f, optY + 104.0f, sensBuf, 1.55f, Vec3(1, 1, 1), LabFontType::GeoSans);
                drawVguiButton(optX + 460.0f, optY + 98.0f, 40.0f, 30.0f, "+", false, false);

                LabFont::drawText(optX + 44.0f, optY + 154.0f, "INVERT VERTICAL AXIS", 1.55f, Vec3(0.85f, 0.88f, 0.94f), LabFontType::GeoSans);
                drawVguiButton(optX + 320.0f, optY + 148.0f, 140.0f, 30.0f, _optInvertY ? "Enabled" : "Disabled", false, _optInvertY);

                LabFont::drawText(optX + 44.0f, optY + 210.0f, "KEYBOARD SHORTCUTS:", 1.45f, Vec3(1.0f, 0.84f, 0.22f), LabFontType::GeoSans);
                LabFont::drawText(optX + 44.0f, optY + 236.0f, "WASD - Movement               |  LMB - Primary Weapon Fire", 1.35f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::System);
                LabFont::drawText(optX + 44.0f, optY + 258.0f, "SPACE - Jump / Source Bhop    |  RMB - Phys Cannon / Iron Sights", 1.35f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::System);
                LabFont::drawText(optX + 44.0f, optY + 280.0f, "R - Reload Weapon             |  F - Tactical Flashlight", 1.35f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::System);
                LabFont::drawText(optX + 44.0f, optY + 302.0f, "~ - Developer Console         |  F2 - Hammer 3D Editor", 1.35f, Vec3(0.70f, 0.75f, 0.82f), LabFontType::System);
            }

            // Bottom Buttons
            bool isCancelHov = (_menuMouseX >= optX + 20.0f && _menuMouseX <= optX + 160.0f && _menuMouseY >= optY + 484.0f && _menuMouseY <= optY + 518.0f);
            drawVguiButton(optX + 20.0f, optY + 484.0f, 140.0f, 34.0f, "Cancel", isCancelHov, false);

            bool isOkHov = (_menuMouseX >= optX + optW - 160.0f && _menuMouseX <= optX + optW - 20.0f && _menuMouseY >= optY + 484.0f && _menuMouseY <= optY + 518.0f);
            drawVguiButton(optX + optW - 160.0f, optY + 484.0f, 140.0f, 34.0f, "OK", isOkHov, true);
        }
        else if (_menuScreen == MenuScreen::MultiSelect) {
            bool isCloseHov = (_menuMouseX >= winX + winW - 28.0f && _menuMouseX <= winX + winW - 6.0f && _menuMouseY >= winY + 6.0f && _menuMouseY <= winY + 28.0f);
            drawVguiWindow(winX, winY, winW, winH, "MULTIPLAYER", isCloseHov);

            // Card 1: Create Server
            drawVguiWell(winX + 40.0f, winY + 70.0f, 420.0f, 380.0f, "CREATE SERVER");
            LabFont::drawText(winX + 60.0f, winY + 120.0f, "Host a custom LAN or P2P session.", 1.55f, Vec3(0.85f, 0.90f, 0.95f), LabFontType::GeoSans);
            LabFont::drawText(winX + 60.0f, winY + 150.0f, "Configure combat bot squad count and match rules.", 1.40f, Vec3(0.65f, 0.70f, 0.78f), LabFontType::GeoSans);
            drawVguiButton(winX + 60.0f, winY + 360.0f, 380.0f, 38.0f, "Configure & Host", false, true);

            // Card 2: Find Servers
            drawVguiWell(winX + 520.0f, winY + 70.0f, 420.0f, 380.0f, "FIND SERVERS");
            LabFont::drawText(winX + 540.0f, winY + 120.0f, "Browse servers running on LAN network.", 1.55f, Vec3(0.85f, 0.90f, 0.95f), LabFontType::GeoSans);
            LabFont::drawText(winX + 540.0f, winY + 150.0f, "Connect with 4-digit Room Code or Direct IP.", 1.40f, Vec3(0.65f, 0.70f, 0.78f), LabFontType::GeoSans);
            drawVguiButton(winX + 540.0f, winY + 360.0f, 380.0f, 38.0f, "Open Browser", false, true);

            // Bottom Back button
            bool isBackHov = (_menuMouseX >= winX + 40.0f && _menuMouseX <= winX + 180.0f && _menuMouseY >= winY + 500.0f && _menuMouseY <= winY + 534.0f);
            drawVguiButton(winX + 40.0f, winY + 500.0f, 140.0f, 34.0f, "Back", isBackHov, false);
        }

        Renderer::endUI();
    }


    void drawUI() {
        int w = 1280, h = 720;
        if (_devConsole.ui_scale > 0.1f && std::abs(_devConsole.ui_scale - 1.0f) > 0.01f) {
            w = static_cast<int>(1280.0f / _devConsole.ui_scale);
            h = static_cast<int>(720.0f / _devConsole.ui_scale);
        }

        if (_inMenu) {
            Renderer::beginUI(w, h);
            Renderer::drawRect(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), Vec3(0.02f, 0.03f, 0.06f), 0.42f);
            Renderer::endUI();
            drawMenu();
            if (_devConsole.isOpen) {
                _devConsole.render(w, h);
            }
            return;
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

        if (_isIntermission) {
            // Render Match End Intermission Banner & Scoreboard
            Renderer::beginUI(w, h);
            Renderer::drawRect(0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), Vec3(0.0f, 0.0f, 0.0f), 0.75f);
            Renderer::drawRect(0.0f, static_cast<float>(h) * 0.10f, static_cast<float>(w), 70.0f, Vec3(0.08f, 0.12f, 0.18f), 0.90f);
            Renderer::drawRect(0.0f, static_cast<float>(h) * 0.10f, static_cast<float>(w), 2.0f, Vec3(1.0f, 0.80f, 0.20f));
            Renderer::drawRect(0.0f, static_cast<float>(h) * 0.10f + 68.0f, static_cast<float>(w), 2.0f, Vec3(1.0f, 0.80f, 0.20f));

            std::string winBanner = "MATCH OVER // WINNER: " + _intermissionWinner;
            LabFont::drawText(static_cast<float>(w) * 0.5f - 240.0f, static_cast<float>(h) * 0.10f + 16.0f, winBanner, 2.4f, Vec3(1.0f, 0.85f, 0.25f), LabFontType::GeoSans);
            std::string countdown = "NEXT ROUND COMMENCING IN " + std::to_string(static_cast<int>(std::ceil(_intermissionTimer))) + " SECONDS";
            LabFont::drawText(static_cast<float>(w) * 0.5f - 180.0f, static_cast<float>(h) * 0.10f + 44.0f, countdown, 1.4f, Vec3(0.7f, 0.8f, 0.9f), LabFontType::GeoSans);
            Renderer::endUI();
        }

        if (_isPlayerDead) {
            _hud.renderDeathScreen(w, h, _playerRespawnTimer);
        } else {
            _hud.render(w, h);
            _interactiveSystem.renderHUD(w, h);

            // Call of Duty Style Tactical Entrance Typewriter Overlay (powered by addons/cod_intro/scripts/startup.lua)
            if (_codIntroTimer > 0.0f && _codIntroTimer <= _codIntroDuration) {
                std::string l1 = ScriptEngine::getHUDText("cod_intro_line1");
                std::string l2 = ScriptEngine::getHUDText("cod_intro_line2");
                std::string l3 = ScriptEngine::getHUDText("cod_intro_line3");
                if (!l1.empty()) {
                    Renderer::beginUI(w, h);
                    float alpha = 1.0f;
                    if (_codIntroTimer > _codIntroDuration - 0.8f) {
                        alpha = std::clamp((_codIntroDuration - _codIntroTimer) / 0.8f, 0.0f, 1.0f);
                    }
                    auto typewriterSlice = [](const std::string& full, float elapsed, float startDelay) -> std::string {
                        if (elapsed <= startDelay) return "";
                        size_t chars = static_cast<size_t>((elapsed - startDelay) * 32.0f);
                        if (chars >= full.size()) return full;
                        return full.substr(0, chars) + "_";
                    };
                    float baseX = 48.0f;
                    float baseY = static_cast<float>(h) - 215.0f;
                    Renderer::drawRect(baseX - 10.0f, baseY - 8.0f, 520.0f, 78.0f, Vec3(0.02f, 0.05f, 0.04f), 0.65f * alpha);
                    Renderer::drawRect(baseX - 10.0f, baseY - 8.0f, 3.0f, 78.0f, Vec3(0.35f, 0.95f, 0.45f), 0.90f * alpha);
                    LabFont::drawText(baseX, baseY, typewriterSlice(l1, _codIntroTimer, 0.0f), 1.45f, Vec3(0.45f, 1.0f, 0.55f) * alpha, LabFontType::GeoSans);
                    LabFont::drawText(baseX, baseY + 24.0f, typewriterSlice(l2, _codIntroTimer, 0.45f), 1.35f, Vec3(0.75f, 0.95f, 0.80f) * alpha, LabFontType::System);
                    LabFont::drawText(baseX, baseY + 46.0f, typewriterSlice(l3, _codIntroTimer, 0.95f), 1.35f, Vec3(0.75f, 0.95f, 0.80f) * alpha, LabFontType::System);
                    Renderer::endUI();
                }
            }
        }
        _chat.render(w, h);

        // Scoreboard (Hold or toggle TAB)
        if (Input::isKeyPressed(258)) { // GLFW_KEY_TAB
            std::vector<ScoreboardEntry> entries;

            const auto& myProf = ProfileManager::get().getProfile();
            ScoreboardEntry pe;
            pe.name = "[YOU] " + myProf.name;
            pe.kills = _playerKills;
            pe.deaths = _playerDeaths;
            pe.ping = _netClient.isConnected() ? (std::to_string(static_cast<int>(std::round(_netClient.getRTT() * 1000.0f))) + "ms") : "0ms";
            pe.isBot = false;
            pe.isLocalPlayer = true;
            pe.isAlive = !_isPlayerDead;
            pe.status = _isPlayerDead ? "DEAD (" + std::to_string(std::max(0, (int)std::ceil(_playerRespawnTimer))) + "s)" : "ALIVE";
            pe.team = (_sessionConfig.mode == GameMode::TDM) ? "Blue (Alpha)" : "Free-For-All";
            entries.push_back(pe);

            // Populate all remote networked players from latest server snapshot
            if (_netClient.isConnected()) {
                const auto& snap = _netClient.getLatestSnapshot();
                for (size_t i = 0; i < snap.playerCount; ++i) {
                    const auto& p = snap.players[i];
                    if (p.clientId == _netClient.getClientId()) continue;
                    if (p.clientId == 0) continue;

                    ScoreboardEntry re;
                    re.name = (p.name[0] != '\0') ? p.name : ("Player_" + std::to_string(p.clientId));
                    re.kills = p.kills;
                    re.deaths = p.deaths;
                    re.ping = std::to_string(p.pingMs) + "ms";
                    re.isBot = false;
                    re.isLocalPlayer = false;
                    re.isAlive = (p.isAlive != 0);
                    re.status = (p.isAlive != 0) ? "ALIVE" : "DEAD";
                    re.team = (_sessionConfig.mode == GameMode::TDM) ? ((p.team == 0) ? "Red (Bravo)" : "Blue (Alpha)") : "Free-For-All";
                    entries.push_back(re);
                }
            }

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

        if (_inMenu && _currentMap == nullptr) {
            loadSelectedMap("assets/maps/menu_background.labscene");
            _hasActiveGame = false;
            _inMenu = true;
            setCursorCaptured(false);
        }

        if (_inMenu && _currentMap == nullptr) {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            drawUI();
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
        if (_currentMap) {
            Renderer::setSunLight(
                _currentMap->metadata.sunDir,
                _currentMap->metadata.sunColor,
                _devConsole.wrd_light_ambient
            );
        }

        // Atmospheric Environment: apply map environment or live developer console toggles
        if (_currentMap && _currentMap->env.fogEnabled) {
            Renderer::setFog(true, _currentMap->env.fogColor, _currentMap->env.fogStart, _currentMap->env.fogEnd);
        } else if (_devConsole.wrd_fog) {
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

        // Render other connected multiplayer players from network snapshot using 70-bone Guard model & per-player Animator
        if (_netClient.isConnected()) {
            const auto& snap = _netClient.getLatestSnapshot();
            for (uint16_t i = 0; i < snap.playerCount; ++i) {
                const auto& p = snap.players[i];
                if (p.clientId == _netClient.getClientId() || !p.isAlive) continue;

                Vec3 pFeet = p.position - Vec3(0.0f, 1.70f, 0.0f);
                if (_aiManager.skinnedMesh && _aiManager.skeleton) {
                    Animator& rAnim = _remotePlayerAnimators[p.clientId];
                    if (!rAnim.getSkeleton() && !_aiManager.animations.empty()) {
                        rAnim.setSkeleton(_aiManager.skeleton);
                        for (const auto& clip : _aiManager.animations) {
                            rAnim.addClip(clip);
                        }
                        rAnim.playAnimation("Idle", true);
                    }
                    float rSpeed = std::sqrt(p.velocity.x * p.velocity.x + p.velocity.z * p.velocity.z);
                    if (rSpeed > 5.5f) {
                        rAnim.playAnimation("Run Forward", true, 0.15f);
                    } else if (rSpeed > 0.3f) {
                        rAnim.playAnimation("Walking", true, 0.15f);
                    } else {
                        rAnim.playAnimation("Idle", true, 0.2f);
                    }
                    rAnim.update(0.016f);

                    float pScale = _aiManager.skinnedMesh->getBaseScale(1.85f);
                    float yOff = -_aiManager.skinnedMesh->getMinBounds().y * pScale;
                    float radYaw = p.yaw * 3.14159265f / 180.0f;
                    Vec3 fwd(std::cos(radYaw), 0.0f, std::sin(radYaw));
                    float modelFacingYaw = std::atan2(fwd.x, fwd.z) * 180.0f / 3.14159265f;
                    Mat4 pModel = Mat4::translate(pFeet + Vec3(0.0f, yOff, 0.0f)) *
                                  Mat4::rotate(modelFacingYaw * 3.14159265f / 180.0f, Vec3(0, 1, 0)) *
                                  Mat4::scale(Vec3(pScale, pScale, pScale));
                    Renderer::drawSkinnedMesh(*_aiManager.skinnedMesh, pModel,
                                              rAnim.getSkinMatrices(),
                                              Vec3(1.0f, 1.0f, 1.0f), _aiManager.guardTexture.get(), true);

                    int wIdx = static_cast<int>(p.activeWeapon);
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
                            Mat4 weaponSocket = rAnim.getSocketTransform("Socket_Weapon", pModel,
                                makeTransform(socketPos, socketRot, weaponScale));
                            Renderer::drawMesh(*wMesh, weaponSocket, Vec3(0.92f, 0.92f, 0.95f), wTex, true);
                        }
                    }
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
                Vec3 pColor = (_sessionConfig.mode == GameMode::TDM) ? Vec3(0.2f, 0.6f, 1.0f) : Vec3(1.0f, 1.0f, 1.0f);
                Renderer::drawSkinnedMesh(*_aiManager.skinnedMesh, pModel,
                                          _playerAnimator.getSkinMatrices(),
                                          pColor, _aiManager.guardTexture.get(), true);

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

        // Viewmodel (only drawn during active in-game FPP gameplay)
        if (!_inMenu && !_devConsole.cam_thirdperson) {
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
        const std::vector<std::string> searchPaths = {
            "./LabServer",
            "build-linux/LabServer",
            "build/LabServer",
            "bin/LabServer",
            "../build-linux/LabServer"
        };
        std::string foundExe;
        for (const auto& p : searchPaths) {
            if (std::filesystem::exists(p)) {
                foundExe = p;
                break;
            }
        }
        if (foundExe.empty()) {
            outError = "LabServer binary not found";
            return false;
        }
        std::string modeFlag = (cfg.networkMode == NetMatchMode::P2P) ? " -p2p" : " -lan";
        std::string cmd = foundExe + " -config assets/configs/server.cfg -port " + std::to_string(cfg.port) + modeFlag + " &";
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
    int _serverBrowserActiveTab = 0;
    std::string _serverFilterMap = "";
    int _serverFilterMaxPing = 0;
    bool _serverFilterNotFull = false;
    bool _serverFilterHasPlayers = false;
    LoadingSequenceState _loadingState;
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
    bool _hasActiveGame = false;
    std::vector<std::string> _availableMaps;
    int _selectedMapIndex = 0;
    bool _escPressedLast = false;
    bool _oPressedLast = false;
    bool _menuLmbLast = false;
    bool _lastMenuKeys[512] = { false };

    // Achievements & Career Statistics (CS:Source & HL2 VGUI Style)
    struct Achievement {
        std::string id;
        std::string title;
        std::string description;
        std::string icon;
        int currentProgress = 0;
        int maxProgress = 1;
        bool unlocked = false;
        std::string unlockDate = "";
        std::string iconTexture = "";
    };

    struct GameCareerStats {
        int totalKills = 38;
        int totalDeaths = 14;
        int headshots = 12;
        int shotsFired = 342;
        int shotsHit = 158;
        int bhopJumps = 24;
        int barrelsExploded = 6;
        int mapsCompleted = 3;
        float totalPlaytimeSeconds = 4320.0f;
    };

    std::vector<Achievement> _achievements;
    GameCareerStats _careerStats;
    int _achievementsActiveTab = 0; // 0 = Achievements, 1 = Statistics
    bool _achievementsHideCompleted = false;
    int _mapScrollOffset = 0;
    float _menuCamTime = 0.0f;

    // Addon Management State (Source Engine / GMod /addons/ model)
    struct AddonItem {
        std::string folderName;
        std::string title;
        std::string version;
        std::string author;
        std::string description;
        bool enabled = true;
        int mapCount = 0;
    };
    std::vector<AddonItem> _discoveredAddons;
    int _selectedAddonCategory = 0; // 0 = All, 1 = Maps, 2 = Models, 3 = Scripts, 4 = Packages
    int _selectedAddonIndex = 0;

    // Options Dialog State (Source Engine VGUI Style)
    int _optionsActiveTab = 0; // 0 = Video, 1 = Audio, 2 = Controls
    float _optMasterVolume = 1.0f;
    float _optMouseSens = 0.12f;
    int _optShadowRes = 1024;
    bool _optShadows = true;
    int _optFov = 90;
    bool _optInvertY = false;

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

    // Match Intermission & Frag Limit
    bool _isIntermission = false;
    float _intermissionTimer = 0.0f;
    std::string _intermissionWinner = "";
    std::string _profileEditName = "";
    int _profilePrimaryChoice = 3;
    int _profileSecondaryChoice = 1;
    int _profileModelChoice = 0; // 0=guard, 1=worker, 2=scientist

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
    std::unordered_map<uint32_t, Animator> _remotePlayerAnimators;
    float _playerShootTimer = 0.0f;
    PhysicsWorld _physicsWorld;
    std::vector<BulletTracer> _tracers;
    float _muzzleFlashTime;
    float _footstepTimer = 0.0f;

    // Hammer Editor & HUD

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

    // Startup Splash Screen & Addon Intro State
    bool _showSplash = true;
    bool _splashActive = true;
    float _splashTimer = 0.0f;
    float _splashDuration = 2.5f;
    float _codIntroTimer = 0.0f;
    float _codIntroDuration = 4.5f;
    std::string _initialDirectMap;

    static FrozenLife* s_instance;
};

FrozenLife* FrozenLife::s_instance = nullptr;

int main(int argc, char* argv[]) {
    bool showSplash = true;
    std::string directMap = "";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--create-project" || arg == "-create-project" || arg == "--create-game" || arg == "-create-game") {
            if (i + 1 < argc) {
                std::string projName = argv[++i];
                std::string msg;
                if (Lab::CreateStandaloneGameProject(projName, msg)) {
                    std::cout << msg << std::endl;
                    return 0;
                } else {
                    std::cerr << msg << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: Missing project name! Usage: game.exe --create-project <name>" << std::endl;
                return 1;
            }
        } else if (arg == "--create-addon" || arg == "-create-addon" || arg == "--create-mod" || arg == "-create-mod") {
            if (i + 1 < argc) {
                std::string modName = argv[++i];
                std::string msg;
                if (Lab::CreateAssetAddon(modName, msg)) {
                    std::cout << msg << std::endl;
                    return 0;
                } else {
                    std::cerr << msg << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Error: Missing addon name! Usage: game.exe --create-addon <name>" << std::endl;
                return 1;
            }
        } else if (arg == "--nosplash" || arg == "-nosplash") {
            showSplash = false;
        } else if (arg == "-game" || arg == "--game") {
            if (i + 1 < argc) {
                std::string addonArg = argv[++i];
                VFS::get().mountAddonLayer(addonArg);
            }
        } else if (arg == "+map" || arg == "--map" || arg == "-map") {
            if (i + 1 < argc) {
                directMap = argv[++i];
                showSplash = false;
            }
        } else if (!arg.empty() && arg[0] != '-' && arg[0] != '+' && directMap.empty()) {
            directMap = arg;
            showSplash = false;
        }
    }
    FrozenLife game(showSplash, directMap);
    game.run();
    return 0;
}
