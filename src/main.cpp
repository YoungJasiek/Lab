#include "Lab.h"
#include "LabFont.h"
#include "LabDialogs.h"
#include <iostream>
#include <algorithm>
#include <memory>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <cmath>

using namespace Lab;

class FrozenLife : public Engine {
public:
    FrozenLife()
        : Engine("Frozen-Life: Lab FPS", 1280, 720),
          _camera(75.0f, 16.0f / 9.0f, 0.01f, 1000.0f),
          _velocity{ 0, 0, 0 },
          _isGrounded(false),
          _isJumping(false),
          _bobTime(0.0f),
          _muzzleFlashTime(0.0f) {
    }

    void onInit() override {
        LabLog::info("Frozen-Life Init: Modular .LABMAP and GUI System...");
        Renderer::init();

        s_instance = this;
        glfwSetCharCallback(getWindow(), [](GLFWwindow* /*w*/, unsigned int codepoint) {
            if (s_instance && s_instance->_chat.isOpen) {
                s_instance->_chat.onChar(codepoint);
            }
        });

        // Discover available maps in assets/maps/
        scanMapFiles();

        // Preload Core Textures
        std::vector<std::string> coreTextures = {
            "concrete_wall.bmp", "floor_tiles.bmp", "cryo_ice.bmp",
            "hazard_stripes.bmp", "metal_hull.bmp", "snow_frost.bmp",
            "floor_lab.bmp", "wall_concrete.bmp", "brick_wall.bmp",
            "weapon_pipe.bmp", "weapon_pistol.bmp", "weapon_shotgun.bmp",
            "weapon_m4a4s.bmp", "weapon_sg553.bmp", "weapon_minigun.bmp",
            "weapon_plasma.bmp", "weapon_railgun.bmp", "weapon_rpg.bmp"
        };
        for (const auto& texName : coreTextures) {
            getTexture(texName);
        }

        // Initialize Weapon System & Particles
        _weaponSystem.init();
        _particleSystem.init();

        // Check ONCE at startup which weapons have custom STL models and textures
        _cachedWeaponMeshes.resize(9, nullptr);
        _cachedWeaponTextures.resize(9, nullptr);
        int customSTLCount = 0;
        for (int i = 0; i < 9; ++i) {
            const auto& wDef = _weaponSystem.getWeapon((WeaponID)i).def;
            _cachedWeaponMeshes[i] = getMesh(wDef.modelFile);
            std::string resolvedTex = Renderer::resolveModelTexture(wDef.modelFile, wDef.textureFile);
            _cachedWeaponTextures[i] = getTexture(resolvedTex);
            if (_cachedWeaponMeshes[i]) customSTLCount++;
        }
        std::cout << "[Weapons] Checked 9 weapons at startup: " << customSTLCount << " custom STL, " 
                  << (9 - customSTLCount) << " built-in procedural.\n";

        syncHudWeapon();

        // Initialize 3D Spatial Audio Engine (miniaudio)
        AudioEngine::init(true);

        // Start in Main Menu
        _inMenu = true;
        _menuScreen = MenuScreen::Main;
        glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
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

    Mesh* getMesh(const std::string& path) {
        if (path.empty()) return nullptr;
        auto it = _meshes.find(path);
        if (it != _meshes.end()) return it->second.get();
        if (_missingMeshes.contains(path)) return nullptr;

        Mesh* m = Mesh::loadSTL(path);
        if (m) {
            Mesh* ptr = m;
            _meshes[path] = std::unique_ptr<Mesh>(m);
            return ptr;
        }
        _missingMeshes.insert(path);
        return nullptr;
    }

    void scanMapFiles() {
        _availableMaps.clear();
        std::vector<std::string> searchDirs = { "assets/maps", "../assets/maps", "../../assets/maps" };
        for (const auto& dir : searchDirs) {
            if (std::filesystem::exists(dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    if (entry.path().extension() == ".labmap") {
                        _availableMaps.push_back(entry.path().string());
                    }
                }
                if (!_availableMaps.empty()) break;
            }
        }
        if (_availableMaps.empty()) {
            _availableMaps.push_back("assets/maps/facility_alpha.labmap");
            _availableMaps.push_back("assets/maps/cryo_outpost.labmap");
        }
    }

    void loadSelectedMap(const std::string& mapPath) {
        LabLog::info("Loading Map: " + mapPath);
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
                if (!_meshes.contains(prop.modelPath)) {
                    Mesh* m = Mesh::loadSTL(prop.modelPath);
                    if (m) _meshes[prop.modelPath] = std::unique_ptr<Mesh>(m);
                }
                if (!prop.texturePath.empty()) {
                    getTexture(prop.texturePath);
                }
            }

            // Preload brush textures
            for (const auto& brush : _currentMap->brushes) {
                if (!brush.texturePath.empty()) {
                    getTexture(brush.texturePath);
                }
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

            // Ensure map is registered in list and selected
            bool found = false;
            for (int i = 0; i < (int)_availableMaps.size(); ++i) {
                if (_availableMaps[i] == mapPath) {
                    _selectedMapIndex = i;
                    found = true;
                    break;
                }
            }
            if (!found) {
                _availableMaps.push_back(mapPath);
                _selectedMapIndex = (int)_availableMaps.size() - 1;
            }

            // Switch to gameplay mode and lock cursor
            _inMenu = false;
            glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            LabLog::info("Map successfully loaded and entered: " + mapPath);
        } else {
            LabLog::error("Failed to load map: " + mapPath);
        }
    }

    void startSession(const GameSessionConfig& config) {
        _sessionConfig = config;
        loadSelectedMap(_sessionConfig.mapPath);

        _hud.mapName = _currentMap ? _currentMap->metadata.name : "Sector";
        _hud.gameModeName = _sessionConfig.getModeString();
        _hud.frags = 0;
        _hud.health = _hud.maxHealth;
        _hud.suitArmor = 50.0f;
        _weaponSystem.reset();
        syncHudWeapon();
        _particleSystem.clear();
        _tracers.clear();
        _pickups.clear();
        _chat.history.clear();
        _playerKills = 0;
        _playerDeaths = 0;
        _isPlayerDead = false;
        _playerRespawnTimer = 0.0f;

        _chat.addMessage("[SERVER]", "Welcome to Frozen-Life :: " + (_currentMap ? _currentMap->metadata.name : "Sector"), Vec3(0.3f, 0.8f, 1.0f));
        _chat.addMessage("[SERVER]", "Mode: " + _sessionConfig.getModeString() + " | Frag Limit: " + std::to_string(_sessionConfig.fragLimit), Vec3(0.3f, 0.8f, 1.0f));
        _chat.addMessage("[SYSTEM]", "Hold [TAB] for scoreboard, press [Y] for chat, [R] to reload", Vec3(1.0f, 0.9f, 0.3f));

        if (_sessionConfig.enableBots && _sessionConfig.botCount > 0) {
            _aiManager.spawnBotsForMap(_currentMap.get(), _sessionConfig.botCount, _sessionConfig.mode);
            _hud.showCombatMessage("MATCH HOSTED: " + _sessionConfig.getModeString() + " WITH " + std::to_string(_sessionConfig.botCount) + " BOTS", 3.0f);
        } else {
            _aiManager.clear();
            _hud.showCombatMessage("MATCH STARTED: BOTS DISABLED (SOLO)", 3.0f);
        }
    }

    void onFixedUpdate(float fixedDelta) override {
        if (_inMenu) return;

        // Freeze movement if player is dead or typing in chat
        if (_isPlayerDead) {
            _velocity = { 0, 0, 0 };
            return;
        }
        if (_chat.isOpen) {
            _velocity.x *= 0.8f;
            _velocity.z *= 0.8f;
            return;
        }

        // Physics tick rate (64 ticks per second)
        bool isSprinting = Input::isKeyPressed(340); // Left Shift
        float speed = isSprinting ? 8.5f : 4.5f;
        Vec3 inputDir = { 0, 0, 0 };

        Vec3 front = _camera.getFront();
        front.y = 0;
        front = front.normalized();

        Vec3 right = _camera.getRight();
        right.y = 0;
        right = right.normalized();

        if (Input::isKeyPressed('W') || Input::isKeyPressed('w')) inputDir += front;
        if (Input::isKeyPressed('S') || Input::isKeyPressed('s')) inputDir -= front;
        if (Input::isKeyPressed('A') || Input::isKeyPressed('a')) inputDir -= right;
        if (Input::isKeyPressed('D') || Input::isKeyPressed('d')) inputDir += right;

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

        if (inputDir.lengthSq() > 0) {
            inputDir = inputDir.normalized();
            _velocity.x = inputDir.x * speed;
            _velocity.z = inputDir.z * speed;
            _bobTime += fixedDelta * (speed * 2.0f);
        } else {
            _velocity.x *= 0.85f;
            _velocity.z *= 0.85f;
        }

        // Jump physics (Spacebar = 32)
        if (Input::isKeyPressed(32) && _isGrounded) {
            _velocity.y = 5.0f;
            _isGrounded = false;
        }

        // Gravity
        _velocity.y -= 12.0f * fixedDelta;

        // ==================== AABB MOVE & SLIDE WALL COLLISION ====================
        Vec3 pos = _camera.getPosition();
        if (_currentMap) {
            auto obstacles = LabCollision::getMapSolidBoxes(*_currentMap);
            LabCollision::moveAndSlide(pos, _velocity, _isGrounded, fixedDelta, obstacles);
        } else {
            pos += _velocity * fixedDelta;
            if (pos.y < 1.70f) {
                pos.y = 1.70f;
                _velocity.y = 0.0f;
                _isGrounded = true;
            }
        }
        _camera.setPosition(pos);

        // Procedural Door animations and distance triggers
        if (_currentMap) {
            for (auto& door : _currentMap->doors) {
                float distSq = (pos - door.position).lengthSq();
                float triggerRadiusSq = door.triggerRadius * door.triggerRadius;
                door.isOpen = (distSq < triggerRadiusSq);

                if (door.isOpen && door.currentProgress < 1.0f) {
                    door.currentProgress = std::min(1.0f, door.currentProgress + fixedDelta * door.openSpeed);
                } else if (!door.isOpen && door.currentProgress > 0.0f) {
                    door.currentProgress = std::max(0.0f, door.currentProgress - fixedDelta * door.openSpeed);
                }
            }
        }
    }

    void onUpdate(const Time& time) override {
        if (_inMenu) {
            float scaleX = 1280.0f / (float)std::max(1, getWidth());
            float scaleY = 720.0f / (float)std::max(1, getHeight());
            float mx = Input::mousePos.x * scaleX;
            float my = Input::mousePos.y * scaleY;
            bool lmbJustPressed = false;

            if (Input::isMouseButtonPressed(0)) {
                if (!_menuLmbLast) {
                    lmbJustPressed = true;
                    _menuLmbLast = true;
                }
            } else {
                _menuLmbLast = false;
            }

            handleMenuInput(mx, my, lmbJustPressed);
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
                _playerRespawnTimer = 0.0f;
                _hud.health = _hud.maxHealth;
                _hud.suitArmor = 50.0f;
                _weaponSystem.reset();
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

        // Gameplay camera orientation update (only when not typing in chat)
        if (!_chat.isOpen) {
            _camera.update(Input::mouseDelta);
        }

        // Return to menu with M or ESC key (only when chat is closed)
        if (!_chat.isOpen && (Input::isKeyPressed(GLFW_KEY_M) || Input::isKeyPressed(GLFW_KEY_ESCAPE))) {
            _inMenu = true;
            _menuScreen = MenuScreen::Main;
            glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
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
                _weaponAnimator.onFire();
                _weaponAnimator.recoilSpring.addImpulse(Vec3(0.0f, def.recoilPitch, def.recoilKick));

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
                } else if (def.isMelee) {
                    // Melee sweep: Pipe (2.6m sweep)
                    RaycastHit hit;
                    hit.distance = def.range;
                    _aiManager.testRaycast(rayOrigin, forward, hit);
                    if (hit.hit && hit.distance <= def.range && hit.tag == EntityTag::Bot) {
                        AudioEngine::playSound(SoundID::PipeHit);
                        for (auto& bot : _aiManager.bots) {
                            if (bot.id == hit.entityIndex && bot.isAlive()) {
                                float dmg = hit.isHeadshot ? (def.damage * def.headshotMultiplier) : def.damage;
                                bool killed = bot.takeDamage(dmg, hit.isHeadshot);
                                _hud.triggerHitmarker(hit.isHeadshot);

                                // Melee blood impact
                                _particleSystem.spawnBlood(hit.point, -forward, hit.isHeadshot);

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
                                    WeaponID dropWep = (WeaponID)(2 + (rand() % 7));
                                    _pickups.spawnPickup(PickupType::WeaponDrop, bot.position + Vec3(-0.35f, 0.35f, 0.2f), 30, (int)dropWep);
                                }
                                break;
                            }
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

                        // 1. Test AI Combat Bots
                        _aiManager.testRaycast(rayOrigin, spreadDir, hit);

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
                            if (hit.tag == EntityTag::Bot) {
                                // Crimson blood splatter spray & mist
                                _particleSystem.spawnBlood(hit.point, -spreadDir, hit.isHeadshot);

                                for (auto& bot : _aiManager.bots) {
                                    if (bot.id == hit.entityIndex && bot.isAlive()) {
                                        float dmg = hit.isHeadshot ? (def.damage * def.headshotMultiplier) : def.damage;
                                        bool killed = bot.takeDamage(dmg, hit.isHeadshot);
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
                                            WeaponID dropWep = (WeaponID)(2 + (rand() % 7));
                                            _pickups.spawnPickup(PickupType::WeaponDrop, bot.position + Vec3(-0.35f, 0.35f, 0.2f), 30, (int)dropWep);
                                            _chat.addMessage(bot.name, "Critical damage! Unit offline...", Vec3(0.9f, 0.45f, 0.45f));
                                        }
                                        break;
                                    }
                                }
                            } else if (hit.tag == EntityTag::World) {
                                // Concrete dust puff + spark spray + debris
                                _particleSystem.spawnImpact(hit.point, hit.normal, SurfaceType::Concrete);
                            } else if (hit.tag == EntityTag::Door) {
                                // Metal ricochet spark spray + metallic shrapnel
                                _particleSystem.spawnImpact(hit.point, hit.normal, SurfaceType::Metal);
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
                AudioEngine::playSound3D(SoundID::RPGExplosion, hitPos);
                float radiusSq = proj.splashRadius * proj.splashRadius;

                for (auto& bot : _aiManager.bots) {
                    if (!bot.isAlive()) continue;
                    float dSq = (bot.position - hitPos).lengthSq();
                    if (dSq <= radiusSq) {
                        float dist = std::sqrt(dSq);
                        float factor = 1.0f - (dist / proj.splashRadius);
                        float dmg = proj.damage + proj.splashDamage * factor;
                        bool killed = bot.takeDamage(dmg, false);
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
                            WeaponID dropWep = (WeaponID)(2 + (rand() % 7));
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

        // ==================== COMBAT AI BOTS UPDATE & RETALIATION ====================
        float botDamageToPlayer = 0.0f;
        if (_currentMap) {
            int pTeam = (_sessionConfig.mode == GameMode::TDM) ? 1 : -1;
            _aiManager.update(time.delta, _camera.getPosition(), !_isPlayerDead, pTeam, *_currentMap, _tracers, botDamageToPlayer, &_pickups, &_chat);
        }

        if (botDamageToPlayer > 0.0f) {
            AudioEngine::playSound(SoundID::PlayerHurt);
            _hud.triggerDamageFlash();
            if (_hud.suitArmor > 0.0f) {
                float absorb = std::min(_hud.suitArmor, botDamageToPlayer * 0.7f);
                _hud.suitArmor -= absorb;
                _hud.health -= (botDamageToPlayer - absorb);
            } else {
                _hud.health -= botDamageToPlayer;
            }

            if (_hud.health <= 0.0f) {
                _hud.health = 0.0f;
                _isPlayerDead = true;
                _playerDeaths++;
                _playerRespawnTimer = 4.0f;
                _velocity = { 0, 0, 0 };
                _chat.addMessage("[SERVER]", "Player was eliminated by Combat Synth!", Vec3(1.0f, 0.3f, 0.3f));
                _hud.showCombatMessage("YOU WERE ELIMINATED! PRESS [SPACE] TO RESPAWN", 3.5f);
            }
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
                if (curWep.canReload()) {
                    curWep.reload();
                    syncHudWeapon();
                    _weaponAnimator.recoilSpring.addImpulse(Vec3(0.0f, -0.05f, 0.05f));
                    AudioEngine::playSound(SoundID::Reload);
                }
            }
        }

        // Procedural Weapon Sway and Bob update
        float horizontalSpeed = std::sqrt(_velocity.x * _velocity.x + _velocity.z * _velocity.z);
        _weaponAnimator.update(time.delta, Input::mouseDelta, horizontalSpeed);

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

            // E key: Place object (Brush or Door)
            if (Input::isKeyPressed('E') || Input::isKeyPressed('e')) {
                if (!_ePressedLast) {
                    if (_hammerEditor.currentTool == EditorTool::CreateBrush) {
                        _hammerEditor.placeBrush(*_currentMap);
                    } else if (_hammerEditor.currentTool == EditorTool::CreateDoor) {
                        _hammerEditor.placeDoor(*_currentMap);
                    }
                    _ePressedLast = true;
                }
            } else {
                _ePressedLast = false;
            }

            // R key: Toggle Tool (Brush <-> Door)
            if (Input::isKeyPressed('R') || Input::isKeyPressed('r')) {
                if (!_rPressedLast) {
                    _hammerEditor.currentTool = (_hammerEditor.currentTool == EditorTool::CreateBrush) ? EditorTool::CreateDoor : EditorTool::CreateBrush;
                    _rPressedLast = true;
                }
            } else {
                _rPressedLast = false;
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

            // Backspace / Delete: Delete last placed brush
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
                } else if (_menuScreen == MenuScreen::MultiSelect || _menuScreen == MenuScreen::Singleplayer) {
                    _menuScreen = MenuScreen::Main;
                } else if (_menuScreen == MenuScreen::Main && _currentMap) {
                    _inMenu = false;
                    glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                }
                _escPressedLast = true;
            }
        } else {
            _escPressedLast = false;
        }

        if (_menuScreen == MenuScreen::Main) {
            if (lmbClick) {
                // Button 0: Campaign / Singleplayer (x: 120..420, y: 200..248)
                if (mx >= 120.0f && mx <= 420.0f && my >= 200.0f && my <= 248.0f) {
                    _menuScreen = MenuScreen::Singleplayer;
                    return;
                }
                // Button 1: Multiplayer (x: 120..420, y: 268..316)
                if (mx >= 120.0f && mx <= 420.0f && my >= 268.0f && my <= 316.0f) {
                    _menuScreen = MenuScreen::MultiSelect;
                    return;
                }
                // Button 2: Resume Mission (x: 120..420, y: 336..384)
                if (_currentMap && mx >= 120.0f && mx <= 420.0f && my >= 336.0f && my <= 384.0f) {
                    _inMenu = false;
                    glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    return;
                }
                // Button 3: Quit Game (x: 120..420, y: 404..452)
                if (mx >= 120.0f && mx <= 420.0f && my >= 404.0f && my <= 452.0f) {
                    stop();
                    return;
                }
            }
        }
        else if (_menuScreen == MenuScreen::MultiSelect) {
            if (lmbClick) {
                // Card 1: Host Game / Create Server (x: 140..620, y: 200..400)
                if (mx >= 140.0f && mx <= 620.0f && my >= 200.0f && my <= 400.0f) {
                    _menuScreen = MenuScreen::HostGame;
                    return;
                }
                // Card 2: Find Servers / Join Game (x: 660..1140, y: 200..400)
                if (mx >= 660.0f && mx <= 1140.0f && my >= 200.0f && my <= 400.0f) {
                    _menuScreen = MenuScreen::JoinGame;
                    return;
                }
                // Back Button (x: 140..380, y: 580..628)
                if (mx >= 140.0f && mx <= 380.0f && my >= 580.0f && my <= 628.0f) {
                    _menuScreen = MenuScreen::Main;
                    return;
                }
            }
        }
        else if (_menuScreen == MenuScreen::HostGame) {
            // O key: Open native Windows File Dialog for map
            if (Input::isKeyPressed('O') || Input::isKeyPressed('o')) {
                if (!_oPressedLast) {
                    std::string picked = LabDialogs::openFileDialog(getWindow(), "Lab Map Files (*.labmap)\0*.labmap\0All Files (*.*)\0*.*\0", "assets\\maps");
                    if (!picked.empty()) {
                        _sessionConfig.mapPath = picked;
                    }
                    _oPressedLast = true;
                }
            } else {
                _oPressedLast = false;
            }

            if (lmbClick) {
                // Left Panel: Map items (x: 120..520, y: 170 + i * 50)
                for (int i = 0; i < (int)_availableMaps.size(); ++i) {
                    float iy = 170.0f + i * 50.0f;
                    if (mx >= 120.0f && mx <= 520.0f && my >= iy && my <= iy + 42.0f) {
                        _selectedMapIndex = i;
                        _sessionConfig.mapPath = _availableMaps[i];
                        return;
                    }
                }

                // Open from disk button (x: 120..520, y: 490..532)
                if (mx >= 120.0f && mx <= 520.0f && my >= 490.0f && my <= 532.0f) {
                    std::string picked = LabDialogs::openFileDialog(getWindow(), "Lab Map Files (*.labmap)\0*.labmap\0All Files (*.*)\0*.*\0", "assets\\maps");
                    if (!picked.empty()) {
                        _sessionConfig.mapPath = picked;
                    }
                    return;
                }

                // Game Mode buttons: FFA (580..740), DM (760..920), TDM (940..1100) at y: 190..232
                if (my >= 190.0f && my <= 232.0f) {
                    if (mx >= 580.0f && mx <= 740.0f) { _sessionConfig.mode = GameMode::FFA; return; }
                    if (mx >= 760.0f && mx <= 920.0f) { _sessionConfig.mode = GameMode::DM; return; }
                    if (mx >= 940.0f && mx <= 1100.0f) { _sessionConfig.mode = GameMode::TDM; return; }
                }

                // Bots toggle button (x: 580..920, y: 280..322)
                if (mx >= 580.0f && mx <= 920.0f && my >= 280.0f && my <= 322.0f) {
                    _sessionConfig.enableBots = !_sessionConfig.enableBots;
                    return;
                }

                // Bot count [-] (580..625) and [+] (785..830) at y: 370..412
                if (my >= 370.0f && my <= 412.0f) {
                    if (mx >= 580.0f && mx <= 625.0f) { _sessionConfig.botCount = std::max(0, _sessionConfig.botCount - 1); return; }
                    if (mx >= 785.0f && mx <= 830.0f) { _sessionConfig.botCount = std::min(8, _sessionConfig.botCount + 1); return; }
                }

                // Frag Limit [-] (580..625) and [+] (785..830) at y: 460..502
                if (my >= 460.0f && my <= 502.0f) {
                    if (mx >= 580.0f && mx <= 625.0f) { _sessionConfig.fragLimit = std::max(5, _sessionConfig.fragLimit - 5); return; }
                    if (mx >= 785.0f && mx <= 830.0f) { _sessionConfig.fragLimit = std::min(100, _sessionConfig.fragLimit + 5); return; }
                }

                // Bottom Back button (x: 100..280, y: 570..618)
                if (mx >= 100.0f && mx <= 280.0f && my >= 570.0f && my <= 618.0f) {
                    _menuScreen = MenuScreen::MultiSelect;
                    return;
                }

                // Bottom Start Server button (x: 820..1180, y: 570..618)
                if (mx >= 820.0f && mx <= 1180.0f && my >= 570.0f && my <= 618.0f) {
                    startSession(_sessionConfig);
                    return;
                }
            }
        }
        else if (_menuScreen == MenuScreen::JoinGame) {
            if (lmbClick) {
                // Server Row 0 (x: 120..1160, y: 170..215)
                if (mx >= 120.0f && mx <= 1160.0f && my >= 170.0f && my <= 215.0f) {
                    _sessionConfig.mapPath = "assets/maps/facility_alpha.labmap";
                    _sessionConfig.mode = GameMode::FFA;
                    _sessionConfig.enableBots = true;
                    _sessionConfig.botCount = 2;
                    startSession(_sessionConfig);
                    return;
                }
                // Server Row 1 (x: 120..1160, y: 225..270)
                if (mx >= 120.0f && mx <= 1160.0f && my >= 225.0f && my <= 270.0f) {
                    _sessionConfig.mapPath = "assets/maps/cryo_outpost.labmap";
                    _sessionConfig.mode = GameMode::TDM;
                    _sessionConfig.enableBots = true;
                    _sessionConfig.botCount = 4;
                    startSession(_sessionConfig);
                    return;
                }
                // Direct Connect button (x: 840..1160, y: 560..608)
                if (mx >= 840.0f && mx <= 1160.0f && my >= 560.0f && my <= 608.0f) {
                    startSession(_sessionConfig);
                    return;
                }
                // Back button (x: 100..280, y: 560..608)
                if (mx >= 100.0f && mx <= 280.0f && my >= 560.0f && my <= 608.0f) {
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
                    std::string picked = LabDialogs::openFileDialog(getWindow(), "Lab Map Files (*.labmap)\0*.labmap\0All Files (*.*)\0*.*\0", "assets\\maps");
                    if (!picked.empty()) {
                        _sessionConfig.mapPath = picked;
                        _sessionConfig.enableBots = false;
                        startSession(_sessionConfig);
                    }
                    return;
                }
                // Resume (x: 680..920, y: 560..608)
                if (_currentMap && mx >= 680.0f && mx <= 920.0f && my >= 560.0f && my <= 608.0f) {
                    _inMenu = false;
                    glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
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
        Texture* tex = (curIdx >= 0 && curIdx < (int)_cachedWeaponTextures.size()) ? _cachedWeaponTextures[curIdx] : nullptr;
        Mesh* stlMesh = (curIdx >= 0 && curIdx < (int)_cachedWeaponMeshes.size()) ? _cachedWeaponMeshes[curIdx] : nullptr;
        _weaponSystem.renderViewModel(_camera, _weaponAnimator, tex, stlMesh, _muzzleFlashTime);
    }

    void drawMenu() {
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
                { "CAMPAIGN / MISSIONS", "Explore maps solo without hostile combat bot squads", 200.0f, true },
                { "MULTIPLAYER (HOST / JOIN)", "Host a custom LAN match with bots, game modes (FFA/DM/TDM) & rules", 268.0f, true },
                { "RESUME MISSION [ESC]", "Return to current active gameplay session", 336.0f, (_currentMap != nullptr) },
                { "QUIT GAME", "Exit to desktop", 404.0f, true }
            };

            for (const auto& it : items) {
                Vec3 bgCol = it.active ? Vec3(0.18f, 0.28f, 0.40f) : Vec3(0.12f, 0.14f, 0.18f);
                Vec3 txtCol = it.active ? Vec3(1, 1, 1) : Vec3(0.4f, 0.4f, 0.45f);

                Renderer::drawRect(120.0f, it.y, 380.0f, 48.0f, bgCol);
                Renderer::drawRect(120.0f, it.y, 4.0f, 48.0f, Vec3(0.2f, 0.75f, 0.95f));
                LabFont::drawText(140.0f, it.y + 14.0f, it.title, 2.0f, txtCol, LabFontType::GeoSans);
            }

            // Right side status panel
            Renderer::drawRect(540.0f, 200.0f, 620.0f, 252.0f, Vec3(0.08f, 0.10f, 0.14f));
            Renderer::drawRect(540.0f, 200.0f, 620.0f, 1.0f, Vec3(0.2f, 0.3f, 0.4f));
            LabFont::drawText(560.0f, 220.0f, "ENGINE SUBSYSTEMS STATUS", 2.0f, Vec3(0.95f, 0.75f, 0.1f), LabFontType::GeoSans);
            LabFont::drawText(560.0f, 260.0f, "> Rendering: OpenGL 4.5 Core (Direct State Access + Source Engine UV)", 1.6f, Vec3(0.8f, 0.85f, 0.9f), LabFontType::GeoSans);
            LabFont::drawText(560.0f, 290.0f, "> Optimization: 6-Plane Frustum Culling ('To czego oko nie widzi...')", 1.6f, Vec3(0.8f, 0.85f, 0.9f), LabFontType::GeoSans);
            LabFont::drawText(560.0f, 320.0f, "> Combat: Hitscan Raycast with Headshot Multiplier + 3D Tracers", 1.6f, Vec3(0.8f, 0.85f, 0.9f), LabFontType::GeoSans);
            LabFont::drawText(560.0f, 350.0f, "> AI Squad: Tactical Patrol & Combat State Machine (LoS + Bursts)", 1.6f, Vec3(0.8f, 0.85f, 0.9f), LabFontType::GeoSans);
            LabFont::drawText(560.0f, 380.0f, "> Multiplayer: Host Match with FFA / DM / TDM Rules & Custom Bots", 1.6f, Vec3(0.8f, 0.85f, 0.9f), LabFontType::GeoSans);

            LabFont::drawText(120.0f, 625.0f, "USE MOUSE TO CLICK MENU OPTIONS | F2: HAMMER EDITOR | F9: RUN", 1.4f, Vec3(0.45f, 0.55f, 0.65f), LabFontType::GeoSans);
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
                LabFont::drawText(130.0f, iy + 12.0f, _availableMaps[i], 1.7f, isSel ? Vec3(1, 1, 1) : Vec3(0.7f, 0.75f, 0.8f), LabFontType::GeoSans);
            }

            Renderer::drawRect(115.0f, 485.0f, 410.0f, 42.0f, Vec3(0.20f, 0.32f, 0.48f));
            LabFont::drawText(170.0f, 498.0f, "OPEN MAP FROM DISK... [O]", 1.6f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // Right Column: Rules & Bot Settings Box
            Renderer::drawRect(560.0f, 120.0f, 620.0f, 425.0f, Vec3(0.08f, 0.10f, 0.14f));
            Renderer::drawRect(560.0f, 120.0f, 620.0f, 32.0f, Vec3(0.18f, 0.35f, 0.55f));
            LabFont::drawText(580.0f, 128.0f, "2. SERVER & MATCH RULES", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // Game Mode Buttons
            LabFont::drawText(580.0f, 165.0f, "GAME MODE:", 1.8f, Vec3(0.85f, 0.88f, 0.95f), LabFontType::GeoSans);
            bool isFFA = (_sessionConfig.mode == GameMode::FFA);
            bool isDM  = (_sessionConfig.mode == GameMode::DM);
            bool isTDM = (_sessionConfig.mode == GameMode::TDM);

            Renderer::drawRect(580.0f, 190.0f, 160.0f, 42.0f, isFFA ? Vec3(0.18f, 0.65f, 0.45f) : Vec3(0.14f, 0.18f, 0.24f));
            if (isFFA) Renderer::drawRect(580.0f, 190.0f, 160.0f, 2.0f, Vec3(0.98f, 0.78f, 0.08f));
            LabFont::drawText(620.0f, 202.0f, "FFA (All)", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(760.0f, 190.0f, 160.0f, 42.0f, isDM ? Vec3(0.18f, 0.65f, 0.45f) : Vec3(0.14f, 0.18f, 0.24f));
            if (isDM) Renderer::drawRect(760.0f, 190.0f, 160.0f, 2.0f, Vec3(0.98f, 0.78f, 0.08f));
            LabFont::drawText(800.0f, 202.0f, "Deathmatch", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(940.0f, 190.0f, 160.0f, 42.0f, isTDM ? Vec3(0.18f, 0.65f, 0.45f) : Vec3(0.14f, 0.18f, 0.24f));
            if (isTDM) Renderer::drawRect(940.0f, 190.0f, 160.0f, 2.0f, Vec3(0.98f, 0.78f, 0.08f));
            LabFont::drawText(980.0f, 202.0f, "Team DM", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // Bots Toggle Button
            LabFont::drawText(580.0f, 255.0f, "COMBAT AI BOTS:", 1.8f, Vec3(0.85f, 0.88f, 0.95f), LabFontType::GeoSans);
            Vec3 botBtnBg = _sessionConfig.enableBots ? Vec3(0.18f, 0.65f, 0.35f) : Vec3(0.24f, 0.26f, 0.30f);
            std::string botBtnText = _sessionConfig.enableBots ? "BOTS: ENABLED (ON)" : "BOTS: DISABLED (OFF)";
            Renderer::drawRect(580.0f, 280.0f, 340.0f, 42.0f, botBtnBg);
            LabFont::drawText(620.0f, 292.0f, botBtnText, 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // Bot Count Adjuster
            LabFont::drawText(580.0f, 345.0f, "BOT COUNT (0 - 8):", 1.8f, Vec3(0.85f, 0.88f, 0.95f), LabFontType::GeoSans);
            Renderer::drawRect(580.0f, 370.0f, 45.0f, 42.0f, Vec3(0.22f, 0.28f, 0.38f));
            LabFont::drawText(598.0f, 380.0f, "-", 2.4f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(635.0f, 370.0f, 140.0f, 42.0f, Vec3(0.12f, 0.15f, 0.20f));
            std::string botCountStr = std::to_string(_sessionConfig.botCount) + " BOTS";
            LabFont::drawText(660.0f, 382.0f, botCountStr, 2.0f, Vec3(0.95f, 0.85f, 0.2f), LabFontType::GeoSans);

            Renderer::drawRect(785.0f, 370.0f, 45.0f, 42.0f, Vec3(0.22f, 0.28f, 0.38f));
            LabFont::drawText(802.0f, 380.0f, "+", 2.4f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // Frag Limit Adjuster
            LabFont::drawText(580.0f, 435.0f, "FRAG LIMIT:", 1.8f, Vec3(0.85f, 0.88f, 0.95f), LabFontType::GeoSans);
            Renderer::drawRect(580.0f, 460.0f, 45.0f, 42.0f, Vec3(0.22f, 0.28f, 0.38f));
            LabFont::drawText(598.0f, 470.0f, "-", 2.4f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(635.0f, 460.0f, 140.0f, 42.0f, Vec3(0.12f, 0.15f, 0.20f));
            std::string fragLimitStr = std::to_string(_sessionConfig.fragLimit) + " KILLS";
            LabFont::drawText(660.0f, 472.0f, fragLimitStr, 2.0f, Vec3(0.3f, 0.85f, 1.0f), LabFontType::GeoSans);

            Renderer::drawRect(785.0f, 460.0f, 45.0f, 42.0f, Vec3(0.22f, 0.28f, 0.38f));
            LabFont::drawText(802.0f, 470.0f, "+", 2.4f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // Bottom Buttons: Back and Launch
            Renderer::drawRect(100.0f, 570.0f, 200.0f, 48.0f, Vec3(0.20f, 0.25f, 0.35f));
            LabFont::drawText(150.0f, 586.0f, "< BACK", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(820.0f, 570.0f, 360.0f, 48.0f, Vec3(0.18f, 0.65f, 0.35f));
            Renderer::drawRect(820.0f, 570.0f, 360.0f, 2.0f, Vec3(0.98f, 0.78f, 0.08f));
            LabFont::drawText(860.0f, 586.0f, "START SERVER / LAUNCH MATCH", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);
        }
        else if (_menuScreen == MenuScreen::JoinGame) {
            // Server Browser Frame
            Renderer::drawRect(80.0f, 60.0f, 1120.0f, 600.0f, { 0.1f, 0.13f, 0.18f });
            Renderer::drawRect(82.0f, 62.0f, 1116.0f, 44.0f, { 0.15f, 0.22f, 0.32f });
            Renderer::drawRect(82.0f, 106.0f, 1116.0f, 4.0f, { 0.2f, 0.75f, 0.95f });

            LabFont::drawText(100.0f, 74.0f, "MULTIPLAYER : LOCAL LAN SERVER BROWSER", 2.4f, Vec3(0.95f, 0.98f, 1.0f), LabFontType::GeoSans);

            // Table Header
            Renderer::drawRect(110.0f, 130.0f, 1060.0f, 32.0f, Vec3(0.15f, 0.25f, 0.38f));
            LabFont::drawText(130.0f, 138.0f, "SERVER NAME", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);
            LabFont::drawText(500.0f, 138.0f, "MAP", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);
            LabFont::drawText(720.0f, 138.0f, "MODE", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);
            LabFont::drawText(880.0f, 138.0f, "PLAYERS", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);
            LabFont::drawText(1030.0f, 138.0f, "PING", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);

            // Server Rows
            Renderer::drawRect(110.0f, 170.0f, 1060.0f, 44.0f, Vec3(0.18f, 0.45f, 0.75f));
            LabFont::drawText(130.0f, 184.0f, "Research Complex Alpha Arena", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);
            LabFont::drawText(500.0f, 184.0f, "facility_alpha.labmap", 1.8f, Vec3(0.85f, 0.9f, 1.0f), LabFontType::GeoSans);
            LabFont::drawText(720.0f, 184.0f, "FFA", 1.8f, Vec3(0.95f, 0.85f, 0.2f), LabFontType::GeoSans);
            LabFont::drawText(880.0f, 184.0f, "1 / 8", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);
            LabFont::drawText(1030.0f, 184.0f, "4 ms", 1.8f, Vec3(0.2f, 0.9f, 0.3f), LabFontType::GeoSans);

            Renderer::drawRect(110.0f, 222.0f, 1060.0f, 44.0f, Vec3(0.12f, 0.16f, 0.22f));
            LabFont::drawText(130.0f, 236.0f, "Cryo Outpost Team Fortress", 1.8f, Vec3(0.8f, 0.85f, 0.9f), LabFontType::GeoSans);
            LabFont::drawText(500.0f, 236.0f, "cryo_outpost.labmap", 1.8f, Vec3(0.7f, 0.75f, 0.8f), LabFontType::GeoSans);
            LabFont::drawText(720.0f, 236.0f, "TDM", 1.8f, Vec3(0.3f, 0.85f, 1.0f), LabFontType::GeoSans);
            LabFont::drawText(880.0f, 236.0f, "4 / 8", 1.8f, Vec3(0.8f, 0.85f, 0.9f), LabFontType::GeoSans);
            LabFont::drawText(1030.0f, 236.0f, "12 ms", 1.8f, Vec3(0.2f, 0.9f, 0.3f), LabFontType::GeoSans);

            // Direct Connect field
            Renderer::drawRect(110.0f, 470.0f, 600.0f, 44.0f, Vec3(0.08f, 0.10f, 0.14f));
            LabFont::drawText(130.0f, 482.0f, "DIRECT CONNECT IP:  127.0.0.1:27015", 1.8f, Vec3(0.3f, 0.85f, 1.0f), LabFontType::GeoSans);

            // Bottom Buttons
            Renderer::drawRect(110.0f, 560.0f, 200.0f, 48.0f, Vec3(0.20f, 0.25f, 0.35f));
            LabFont::drawText(160.0f, 576.0f, "< BACK", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);

            Renderer::drawRect(860.0f, 560.0f, 310.0f, 48.0f, Vec3(0.20f, 0.65f, 0.42f));
            LabFont::drawText(900.0f, 576.0f, "CONNECT TO SERVER", 1.8f, Vec3(1, 1, 1), LabFontType::GeoSans);
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

                std::string mapDisplay = _availableMaps[i];
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

        if (!_hammerEditor.active) {
            if (_isPlayerDead) {
                _hud.renderDeathScreen(w, h, _playerRespawnTimer);
            } else {
                _hud.render(w, h);
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
    }

    void onRender() override {
        if (_inMenu) {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            drawMenu();
            return;
        }

        Renderer::beginFrame(_camera);

        if (_currentMap) {
            // Render map brushes with Frustum Culling & Source Tri-Planar UV scaling
            for (const auto& b : _currentMap->brushes) {
                Vec3 halfSize = b.size * 0.5f;
                Vec3 bMin = b.position - halfSize;
                Vec3 bMax = b.position + halfSize;
                if (!_camera.isInFrustum(bMin, bMax)) continue;

                Texture* tex = b.texturePath.empty() ? nullptr : getTexture(b.texturePath);
                Renderer::drawCube(b.position, b.size, b.color, tex, true, b.uvScale, b.uvMode);
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

        // Render 3D World Pickups (Ammo crates, Medkits, and 1-min Weapon Spawn Pads)
        _pickups.render(_cachedWeaponMeshes, _cachedWeaponTextures);

        // Render 3D Bullet Tracers (Source / Half-Life 2 style luminous beams)
        for (const auto& tr : _tracers) {
            Vec3 diff = tr.end - tr.start;
            float len = diff.length();
            if (len < 0.05f) continue;
            Vec3 mid = tr.start + diff * 0.5f;
            float yaw = std::atan2(diff.x, diff.z) * 180.0f / 3.14159265f;
            float pitch = -std::asin(std::clamp(diff.y / len, -1.0f, 1.0f)) * 180.0f / 3.14159265f;
            Renderer::drawCube(mid, Vec3(pitch, yaw, 0.0f), Vec3(tr.thickness, tr.thickness, len), tr.color, nullptr, false);
        }

        // Render Active Projectiles (RPG Rockets & Plasma Orbs)
        for (const auto& proj : _weaponSystem.getProjectiles()) {
            if (!proj.active) continue;
            float pSize = (proj.weaponId == WeaponID::RPG) ? 0.22f : 0.16f;
            Renderer::drawCube(proj.position, Vec3(0, 0, 0), Vec3(pSize, pSize, pSize * 2.0f), proj.color, nullptr, false);
        }

        // Render 3D Particle System (Sparks, blood, smoke, fire, frost)
        _particleSystem.render(_camera);

        // Lab Hammer Editor 3D Ghost/Grid Overlay
        _hammerEditor.draw3DOverlay();

        // Viewmodel (only drawn when not in Hammer Editor mode)
        if (!_hammerEditor.active) {
            drawWeapon();
        }

        drawUI();

        Renderer::endFrame();
    }

    void onShutdown() override {
        AudioEngine::shutdown();
        _particleSystem.shutdown();
        Renderer::shutdown();
    }

private:
    Camera _camera;
    ParticleSystem _particleSystem;
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
    bool _inMenu = true;
    std::vector<std::string> _availableMaps;
    int _selectedMapIndex = 0;
    bool _escPressedLast = false;
    bool _oPressedLast = false;
    bool _menuLmbLast = false;

    // Movement state
    Vec3 _velocity;
    bool _isGrounded;
    bool _isJumping;
    float _bobTime;

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
    WeaponAnimator _weaponAnimator;
    AIManager _aiManager;
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

    // Debug mode
    bool _debugMode = false;
    bool _wireframeMode = false;
    bool _f3PressedLast = false;
    bool _f1PressedLast = false;

    static FrozenLife* s_instance;
};

FrozenLife* FrozenLife::s_instance = nullptr;

int main() {
    FrozenLife game;
    game.run();
    return 0;
}
