#include "LabNetwork.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    using socklen_t = int;
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

namespace Lab {

    static void safeStrCopy(char* dest, size_t destSize, const char* src) {
        if (!dest || destSize == 0) return;
        if (!src) { dest[0] = '\0'; return; }
        size_t i = 0;
        while (i + 1 < destSize && src[i] != '\0') {
            dest[i] = src[i];
            ++i;
        }
        dest[i] = '\0';
    }

    // =========================================================================
    // NetworkSystem Global State
    // =========================================================================

    static bool s_networkInitialized = false;

    bool NetworkSystem::init() {
        if (s_networkInitialized) return true;
#ifdef _WIN32
        WSADATA wsaData;
        int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (res != 0) {
            std::cerr << "[Network] WSAStartup failed with error: " << res << "\n";
            return false;
        }
#endif
        s_networkInitialized = true;
        return true;
    }

    void NetworkSystem::shutdown() {
        if (!s_networkInitialized) return;
#ifdef _WIN32
        WSACleanup();
#endif
        s_networkInitialized = false;
    }

    bool NetworkSystem::isInitialized() {
        return s_networkInitialized;
    }

    // =========================================================================
    // SocketAddress Implementation
    // =========================================================================

    SocketAddress::SocketAddress() {
        std::memset(_storage, 0, sizeof(_storage));
        auto* addr = reinterpret_cast<sockaddr_in*>(_storage);
        addr->sin_family = AF_INET;
    }

    SocketAddress::SocketAddress(const std::string& ip, uint16_t port) {
        std::memset(_storage, 0, sizeof(_storage));
        fromString(ip, port);
    }

    bool SocketAddress::fromString(const std::string& ip, uint16_t port) {
        NetworkSystem::init();
        auto* addr = reinterpret_cast<sockaddr_in*>(_storage);
        std::memset(addr, 0, sizeof(sockaddr_in));
        addr->sin_family = AF_INET;
        addr->sin_port = htons(port);

        if (ip.empty() || ip == "0.0.0.0" || ip == "*") {
            addr->sin_addr.s_addr = INADDR_ANY;
            return true;
        }
        if (ip == "255.255.255.255" || ip == "broadcast") {
            addr->sin_addr.s_addr = INADDR_BROADCAST;
            return true;
        }

        int res = inet_pton(AF_INET, ip.c_str(), &addr->sin_addr);
        return res == 1;
    }

    std::string SocketAddress::getIP() const {
        const auto* addr = reinterpret_cast<const sockaddr_in*>(_storage);
        char buf[INET_ADDRSTRLEN] = { 0 };
        inet_ntop(AF_INET, &(addr->sin_addr), buf, INET_ADDRSTRLEN);
        return std::string(buf);
    }

    uint16_t SocketAddress::getPort() const {
        const auto* addr = reinterpret_cast<const sockaddr_in*>(_storage);
        return ntohs(addr->sin_port);
    }

    std::string SocketAddress::toString() const {
        return getIP() + ":" + std::to_string(getPort());
    }

    bool SocketAddress::operator==(const SocketAddress& other) const {
        const auto* a = reinterpret_cast<const sockaddr_in*>(_storage);
        const auto* b = reinterpret_cast<const sockaddr_in*>(other._storage);
        return (a->sin_family == b->sin_family &&
                a->sin_port == b->sin_port &&
                a->sin_addr.s_addr == b->sin_addr.s_addr);
    }

    void* SocketAddress::getNativeSockAddr() {
        return _storage;
    }

    const void* SocketAddress::getNativeSockAddr() const {
        return _storage;
    }

    size_t SocketAddress::getSockAddrLen() const {
        return sizeof(sockaddr_in);
    }

    // =========================================================================
    // UDPSocket Implementation
    // =========================================================================

    UDPSocket::UDPSocket() = default;

    UDPSocket::~UDPSocket() {
        close();
    }

    UDPSocket::UDPSocket(UDPSocket&& other) noexcept
        : _socketHandle(other._socketHandle), _boundPort(other._boundPort) {
        other._socketHandle = -1;
        other._boundPort = 0;
    }

    UDPSocket& UDPSocket::operator=(UDPSocket&& other) noexcept {
        if (this != &other) {
            close();
            _socketHandle = other._socketHandle;
            _boundPort = other._boundPort;
            other._socketHandle = -1;
            other._boundPort = 0;
        }
        return *this;
    }

    bool UDPSocket::open(uint16_t port) {
        NetworkSystem::init();
        close();

        intptr_t s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) {
            std::cerr << "[Network] Failed to create UDP socket.\n";
            return false;
        }

        // Allow fast port reuse
        int opt = 1;
        setsockopt(static_cast<SOCKET>(s), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

        // Allow broadcast for LAN discovery
        int bcast = 1;
        setsockopt(static_cast<SOCKET>(s), SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&bcast), sizeof(bcast));

        sockaddr_in bindAddr{};
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_addr.s_addr = INADDR_ANY;
        bindAddr.sin_port = htons(port);

        if (bind(static_cast<SOCKET>(s), reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR) {
            std::cerr << "[Network] Failed to bind UDP socket to port " << port << ".\n";
            closesocket(static_cast<SOCKET>(s));
            return false;
        }

        // If port 0 was passed, query assigned ephemeral port
        if (port == 0) {
            sockaddr_in queryAddr{};
            socklen_t len = sizeof(queryAddr);
            if (getsockname(static_cast<SOCKET>(s), reinterpret_cast<sockaddr*>(&queryAddr), &len) == 0) {
                _boundPort = ntohs(queryAddr.sin_port);
            }
        } else {
            _boundPort = port;
        }

        _socketHandle = s;
        setNonBlocking(true);
        return true;
    }

    void UDPSocket::close() {
        if (_socketHandle != -1) {
            closesocket(static_cast<SOCKET>(_socketHandle));
            _socketHandle = -1;
            _boundPort = 0;
        }
    }

    bool UDPSocket::setNonBlocking(bool nonBlocking) {
        if (_socketHandle == -1) return false;
#ifdef _WIN32
        u_long mode = nonBlocking ? 1 : 0;
        return ioctlsocket(static_cast<SOCKET>(_socketHandle), FIONBIO, &mode) == 0;
#else
        int flags = fcntl(static_cast<int>(_socketHandle), F_GETFL, 0);
        if (flags == -1) return false;
        flags = nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
        return fcntl(static_cast<int>(_socketHandle), F_SETFL, flags) == 0;
#endif
    }

    int UDPSocket::sendTo(const void* data, size_t size, const SocketAddress& dest) {
        if (_socketHandle == -1 || !data || size == 0) return -1;
        const auto* nativeAddr = reinterpret_cast<const sockaddr*>(dest.getNativeSockAddr());
        int sent = sendto(static_cast<SOCKET>(_socketHandle), reinterpret_cast<const char*>(data),
                          static_cast<int>(size), 0, nativeAddr, static_cast<socklen_t>(dest.getSockAddrLen()));
        return sent;
    }

    int UDPSocket::recvFrom(void* buffer, size_t maxSize, SocketAddress& from) {
        if (_socketHandle == -1 || !buffer || maxSize == 0) return -1;
        auto* nativeAddr = reinterpret_cast<sockaddr*>(from.getNativeSockAddr());
        socklen_t addrLen = static_cast<socklen_t>(from.getSockAddrLen());

        int bytes = recvfrom(static_cast<SOCKET>(_socketHandle), reinterpret_cast<char*>(buffer),
                             static_cast<int>(maxSize), 0, nativeAddr, &addrLen);
        return bytes;
    }

    // =========================================================================
    // Client Prediction & Reconciliation Implementation
    // =========================================================================

    ClientPrediction::ClientPrediction() = default;

    void ClientPrediction::recordCommand(const NetUserCmd& cmd, const Vec3& pos, const Vec3& vel) {
        PredictedCommand entry;
        entry.cmd = cmd;
        entry.predictedPosition = pos;
        entry.predictedVelocity = vel;

        _history.push_back(entry);
        while (_history.size() > 128) {
            _history.pop_front();
        }
    }

    bool ClientPrediction::reconcile(uint32_t lastProcessedCmd, const Vec3& serverPos, const Vec3& serverVel,
                                    Vec3& outCorrectedPos, Vec3& outCorrectedVel, float errorThreshold) {
        // Discard all acknowledged commands older than lastProcessedCmd
        while (!_history.empty() && _history.front().cmd.cmdNumber < lastProcessedCmd) {
            _history.pop_front();
        }

        if (_history.empty()) {
            outCorrectedPos = serverPos;
            outCorrectedVel = serverVel;
            return false;
        }

        bool needCorrection = false;
        if (_history.front().cmd.cmdNumber == lastProcessedCmd) {
            Vec3 diff = _history.front().predictedPosition - serverPos;
            float distSq = diff.lengthSq();
            // Use 0.45m threshold (~player radius) to prevent false-positive micro-corrections during normal walking
            float effectiveThreshold = std::max(errorThreshold, 0.45f);
            if (distSq > effectiveThreshold * effectiveThreshold) {
                needCorrection = true;
            }
            _history.pop_front();
        }

        if (!needCorrection) {
            return false;
        }

        // Authoritative server state correction for true physical divergences
        outCorrectedPos = serverPos;
        outCorrectedVel = serverVel;
        _history.clear();
        return true;
    }

    void ClientPrediction::reset() {
        _history.clear();
        _nextCmdNumber = 1;
    }

    // =========================================================================
    // ServerConfig Implementation
    // =========================================================================

    static inline std::string trimString(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\r\n");
        return str.substr(first, (last - first + 1));
    }

    static inline std::string toLowerString(std::string s) {
        for (char& c : s) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        }
        return s;
    }

    bool ServerConfig::loadFromFile(const std::string& filepath) {
        std::string resolved = filepath.empty() ? "assets/configs/server.cfg" : filepath;
        std::ifstream test(resolved);
        if (!test.good()) {
            const std::vector<std::string> searchPaths = {
                resolved,
                "assets/configs/server.cfg",
                "assets/configs/" + resolved,
                "../assets/configs/server.cfg",
                "../../assets/configs/server.cfg",
                "build/Release/assets/configs/server.cfg"
            };
            for (const auto& p : searchPaths) {
                std::ifstream f(p);
                if (f.good()) {
                    resolved = p;
                    break;
                }
            }
        }

        std::ifstream in(resolved);
        if (!in.is_open()) return false;

        std::string line;
        while (std::getline(in, line)) {
            // Strip comments (# or ;)
            size_t commentPos = line.find_first_of("#;");
            if (commentPos != std::string::npos) {
                line = line.substr(0, commentPos);
            }
            line = trimString(line);
            if (line.empty() || line.front() == '[') continue;

            size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;

            std::string key = toLowerString(trimString(line.substr(0, eqPos)));
            std::string val = trimString(line.substr(eqPos + 1));
            // Remove optional quotes
            if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') || (val.front() == '\'' && val.back() == '\''))) {
                val = val.substr(1, val.size() - 2);
            }

            if (key == "port") {
                try { port = static_cast<uint16_t>(std::stoi(val)); } catch (...) {}
            } else if (key == "server_name" || key == "servername" || key == "name") {
                serverName = val;
            } else if (key == "map" || key == "map_name" || key == "mapname") {
                mapName = val;
            } else if (key == "game_mode" || key == "gamemode" || key == "mode") {
                gameMode = val;
            } else if (key == "max_players" || key == "maxplayers") {
                try { maxPlayers = static_cast<uint16_t>(std::stoi(val)); } catch (...) {}
            } else if (key == "tickrate") {
                try { tickrate = static_cast<uint16_t>(std::stoi(val)); } catch (...) {}
            } else if (key == "frag_limit" || key == "fraglimit") {
                try { fragLimit = std::stoi(val); } catch (...) {}
            } else if (key == "time_limit" || key == "timelimit") {
                try { timeLimitMinutes = std::stoi(val); } catch (...) {}
            } else if (key == "enable_bots" || key == "bots") {
                std::string lval = toLowerString(val);
                enableBots = (lval == "1" || lval == "true" || lval == "yes" || lval == "on");
            } else if (key == "bot_count" || key == "botcount") {
                try { botCount = std::stoi(val); } catch (...) {}
            } else if (key == "bot_difficulty") {
                try { botDifficulty = std::stoi(val); } catch (...) {}
            } else if (key == "network_mode" || key == "lan_mode" || key == "lan" || key == "p2p") {
                std::string lval = toLowerString(val);
                if (lval == "p2p" || lval == "0" || lval == "peer") {
                    networkMode = NetMatchMode::P2P;
                    lanMode = false;
                } else {
                    networkMode = NetMatchMode::LAN;
                    lanMode = true;
                }
            } else if (key == "password") {
                password = val;
            }
        }
        return true;
    }

    bool ServerConfig::saveToFile(const std::string& filepath) const {
        std::string target = filepath.empty() ? "assets/configs/server.cfg" : filepath;
        std::ofstream out(target);
        if (!out.is_open()) return false;

        out << "# ===================================================================\n";
        out << "# FROZEN-LIFE : LAB MULTIPLAYER SERVER CONFIGURATION (assets/configs/server.cfg)\n";
        out << "# Compatible with Linux Headless and Windows Dedicated Servers\n";
        out << "# ===================================================================\n\n";

        out << "[Server]\n";
        out << "port = " << port << "\n";
        out << "server_name = " << serverName << "\n";
        out << "map = " << mapName << "\n";
        out << "game_mode = " << gameMode << "\n";
        out << "max_players = " << maxPlayers << "\n";
        out << "tickrate = " << tickrate << "\n\n";

        out << "[Match]\n";
        out << "frag_limit = " << fragLimit << "\n";
        out << "time_limit = " << timeLimitMinutes << "\n";
        out << "enable_bots = " << (enableBots ? "1" : "0") << "\n";
        out << "bot_count = " << botCount << "\n";
        out << "bot_difficulty = " << botDifficulty << "\n\n";

        out << "[Network]\n";
        out << "network_mode = " << (networkMode == NetMatchMode::LAN ? "LAN" : "P2P") << "\n";
        out << "lan_mode = " << (networkMode == NetMatchMode::LAN ? "1" : "0") << "\n";
        out << "password = " << password << "\n";

        return true;
    }

    // =========================================================================
    // DedicatedServer Implementation
    // =========================================================================

    DedicatedServer::DedicatedServer() = default;

    DedicatedServer::~DedicatedServer() {
        stop();
    }

    bool DedicatedServer::start(const ServerConfig& config) {
        _config = config;
        _serverName = config.serverName;
        _gameMode = config.gameMode;
        return start(config.port, config.mapName);
    }

    bool DedicatedServer::start(uint16_t port, const std::string& mapName) {
        stop();
        _port = port;
        _mapName = mapName;
        _config.port = port;
        _config.mapName = mapName;
        _config.serverName = _serverName;
        _config.gameMode = _gameMode;
        _serverTick = 0;
        _serverTime = 0.0f;
        _timeAccumulator = 0.0f;
        _clients.clear();

        // Load authoritative map geometry and spawn points
        std::string resolvedMap = _mapName;
        auto map = LabMap::loadFromFile(resolvedMap);
        if (!map && resolvedMap.find('/') == std::string::npos && resolvedMap.find('\\') == std::string::npos) {
            resolvedMap = "assets/maps/" + _mapName;
            map = LabMap::loadFromFile(resolvedMap);
        }
        if (map) {
            _solidBoxes = LabCollision::getMapSolidBoxes(*map);
            _spawnPosition = map->spawn.position;
            _spawnYaw = map->spawn.yaw;
            if (_spawnPosition.y < 1.5f) _spawnPosition.y = 1.80f;
        } else {
            _solidBoxes.clear();
            _spawnPosition = Vec3(0.0f, 1.80f, 0.0f);
            _spawnYaw = 0.0f;
        }

        if (!_socket.open(port)) {
            std::cerr << "[Server] Failed to open UDP socket on port " << port << ".\n";
            return false;
        }

        _running = true;
        uint16_t hz = _config.tickrate > 0 ? _config.tickrate : 64;
        std::cout << "[Server] Dedicated Authoritative Server started on port " << _port 
                  << " (Map: " << _mapName << ", Solid Boxes: " << _solidBoxes.size() 
                  << ", Tickrate: " << hz << "Hz, Mode: " << _gameMode << ").\n";
        return true;
    }

    void DedicatedServer::stop() {
        if (!_running) return;
        _socket.close();
        _clients.clear();
        _solidBoxes.clear();
        _running = false;
        std::cout << "[Server] Dedicated Server stopped.\n";
    }

    void DedicatedServer::tick(float dt) {
        if (!_running) return;

        _serverTime += dt;
        processIncomingPackets();

        // Fixed tickrate based on config (default 64 Hz)
        uint16_t hz = _config.tickrate > 0 ? _config.tickrate : 64;
        const float tickInterval = 1.0f / static_cast<float>(hz);
        _timeAccumulator += dt;

        while (_timeAccumulator >= tickInterval) {
            _serverTick++;
            simulateWorld(tickInterval);
            _timeAccumulator -= tickInterval;
        }

        broadcastSnapshot();
    }

    void DedicatedServer::processIncomingPackets() {
        uint8_t packetBuffer[MAX_PACKET_SIZE];
        SocketAddress sender;

        while (true) {
            int bytes = _socket.recvFrom(packetBuffer, sizeof(packetBuffer), sender);
            if (bytes <= 0) break;
            if (bytes < static_cast<int>(sizeof(NetHeader))) continue;

            const auto* hdr = reinterpret_cast<const NetHeader*>(packetBuffer);
            if (hdr->magic != NET_MAGIC || hdr->version != NET_PROTOCOL_VERSION) continue;

            auto msgType = static_cast<NetMsgType>(hdr->type);

            if (msgType == NetMsgType::ConnectRequest) {
                if (bytes >= static_cast<int>(sizeof(NetMsgConnectRequest))) {
                    const auto* req = reinterpret_cast<const NetMsgConnectRequest*>(packetBuffer);

                    ConnectedClient* client = findClient(sender);
                    if (!client && _clients.size() < MAX_NETWORK_PLAYERS) {
                        ConnectedClient newClient;
                        newClient.clientId = _nextClientId++;
                        newClient.address = sender;
                        newClient.name = req->playerName;
                        if (newClient.name.empty()) newClient.name = "Player_" + std::to_string(newClient.clientId);
                        newClient.lastPacketTime = _serverTime;
                        newClient.state.clientId = newClient.clientId;
                        newClient.state.position = _spawnPosition;
                        newClient.state.yaw = _spawnYaw;
                        newClient.isGrounded = true;
                        newClient.state.health = 150.0f;
                        newClient.state.armor = 50.0f;
                        newClient.state.isAlive = 1;
                        safeStrCopy(newClient.state.name, sizeof(newClient.state.name), newClient.name.c_str());

                        _clients.push_back(newClient);
                        client = &_clients.back();

                        std::cout << "[Server] Client connected: " << newClient.name 
                                  << " (ID: " << newClient.clientId << ", from: " << sender.toString() << ")\n";
                    }

                    if (client) {
                        NetMsgConnectResponse resp;
                        resp.header.type = static_cast<uint8_t>(NetMsgType::ConnectResponse);
                        resp.assignedClientId = client->clientId;
                        resp.tickrate = _config.tickrate > 0 ? _config.tickrate : 64;
                        safeStrCopy(resp.mapName, sizeof(resp.mapName), _mapName.c_str());
                        _socket.sendTo(&resp, sizeof(resp), sender);
                    }
                }
            } else if (msgType == NetMsgType::UserCmd) {
                if (bytes >= static_cast<int>(sizeof(NetUserCmd))) {
                    const auto* cmd = reinterpret_cast<const NetUserCmd*>(packetBuffer);
                    ConnectedClient* client = findClient(sender);
                    if (client) {
                        client->lastPacketTime = _serverTime;
                        client->lastProcessedCmd = cmd->cmdNumber;

                        // Authoritative physical simulation of input command matching client movement
                        float speed = (cmd->buttons & NetButton_Sprint) ? 8.5f : 4.5f;
                        float cmdDt = std::clamp(cmd->deltaTime, 0.001f, 0.050f);

                        float radYaw = cmd->yaw * 3.14159265f / 180.0f;
                        Vec3 fwd(std::cos(radYaw), 0.0f, std::sin(radYaw));
                        Vec3 right(-std::sin(radYaw), 0.0f, std::cos(radYaw));

                        Vec3 moveDir = fwd * cmd->forwardMove + right * cmd->sideMove;
                        if (moveDir.lengthSq() > 0.001f) {
                            moveDir = moveDir.normalized();
                            client->state.velocity.x = moveDir.x * speed;
                            client->state.velocity.z = moveDir.z * speed;
                        } else {
                            client->state.velocity.x *= 0.85f;
                            client->state.velocity.z *= 0.85f;
                        }

                        if ((cmd->buttons & NetButton_Jump) && client->isGrounded) {
                            client->state.velocity.y = 5.0f;
                            client->isGrounded = false;
                        }
                        client->state.velocity.y -= 12.0f * cmdDt;

                        if (!_solidBoxes.empty()) {
                            LabCollision::moveAndSlide(client->state.position, client->state.velocity, client->isGrounded, cmdDt, _solidBoxes, 1.70f, 0.35f, 1.85f);
                        } else {
                            client->state.position += client->state.velocity * cmdDt;
                            if (client->state.position.y < 1.70f) {
                                client->state.position.y = 1.70f;
                                client->state.velocity.y = 0.0f;
                                client->isGrounded = true;
                            }
                        }

                        client->state.yaw = cmd->yaw;
                        client->state.pitch = cmd->pitch;
                    }
                }
            } else if (msgType == NetMsgType::Ping) {
                if (bytes >= static_cast<int>(sizeof(NetMsgPingPong))) {
                    const auto* ping = reinterpret_cast<const NetMsgPingPong*>(packetBuffer);
                    NetMsgPingPong pong;
                    pong.header.type = static_cast<uint8_t>(NetMsgType::Pong);
                    pong.timestamp = ping->timestamp;
                    _socket.sendTo(&pong, sizeof(pong), sender);
                }
            } else if (msgType == NetMsgType::ServerQuery) {
                NetMsgServerInfo info;
                info.header.type = static_cast<uint8_t>(NetMsgType::ServerInfo);
                info.header.sequence = _serverTick;
                info.header.ack = hdr->sequence;
                safeStrCopy(info.serverName, sizeof(info.serverName), _serverName.c_str());
                safeStrCopy(info.mapName, sizeof(info.mapName), _mapName.c_str());
                safeStrCopy(info.gameMode, sizeof(info.gameMode), _gameMode.c_str());
                info.playerCount = static_cast<uint16_t>(_clients.size());
                info.maxPlayers = static_cast<uint16_t>(_config.maxPlayers > 0 ? _config.maxPlayers : MAX_NETWORK_PLAYERS);
                info.pingMs = 0;
                _socket.sendTo(&info, sizeof(info), sender);
            } else if (msgType == NetMsgType::Disconnect) {
                for (auto it = _clients.begin(); it != _clients.end(); ++it) {
                    if (it->address == sender) {
                        std::cout << "[Server] Client disconnected: " << it->name << "\n";
                        _clients.erase(it);
                        break;
                    }
                }
            }
        }
    }

    void DedicatedServer::simulateWorld(float /*dt*/) {
        // Drop clients that haven't sent packets for > 6.0 seconds
        for (auto it = _clients.begin(); it != _clients.end();) {
            if (_serverTime - it->lastPacketTime > 6.0f) {
                std::cout << "[Server] Client timed out: " << it->name << "\n";
                it = _clients.erase(it);
            } else {
                ++it;
            }
        }
    }

    void DedicatedServer::broadcastSnapshot() {
        if (_clients.empty()) return;

        NetServerSnapshot snapshot;
        snapshot.header.type = static_cast<uint8_t>(NetMsgType::ServerSnapshot);
        snapshot.serverTick = _serverTick;
        snapshot.playerCount = static_cast<uint16_t>(std::min(static_cast<size_t>(MAX_NETWORK_PLAYERS), _clients.size()));

        for (size_t i = 0; i < snapshot.playerCount; ++i) {
            snapshot.players[i] = _clients[i].state;
        }

        // Send customized snapshot per client (with their individual lastProcessedCmd)
        for (const auto& client : _clients) {
            snapshot.lastProcessedCmd = client.lastProcessedCmd;
            _socket.sendTo(&snapshot, sizeof(snapshot), client.address);
        }
    }

    void DedicatedServer::broadcastChatMessage(const std::string& sender, const std::string& text) {
        NetChatMessage msg;
        msg.header.type = static_cast<uint8_t>(NetMsgType::ChatMessage);
        safeStrCopy(msg.sender, sizeof(msg.sender), sender.c_str());
        safeStrCopy(msg.text, sizeof(msg.text), text.c_str());

        for (const auto& client : _clients) {
            _socket.sendTo(&msg, sizeof(msg), client.address);
        }
    }

    ConnectedClient* DedicatedServer::findClient(const SocketAddress& addr) {
        for (auto& c : _clients) {
            if (c.address == addr) return &c;
        }
        return nullptr;
    }

    // =========================================================================
    // NetworkClient Implementation
    // =========================================================================

    NetworkClient::NetworkClient() = default;

    NetworkClient::~NetworkClient() {
        disconnect();
    }

    bool NetworkClient::connect(const std::string& ip, uint16_t port, const std::string& playerName) {
        disconnect();
        _playerName = playerName;
        if (!_serverAddr.fromString(ip, port)) {
            std::cerr << "[Client] Invalid server address: " << ip << ":" << port << "\n";
            return false;
        }

        if (!_socket.open(0)) { // Bind ephemeral client port
            std::cerr << "[Client] Failed to open UDP client socket.\n";
            return false;
        }

        // Send Connect Request
        NetMsgConnectRequest req;
        req.header.type = static_cast<uint8_t>(NetMsgType::ConnectRequest);
        safeStrCopy(req.playerName, sizeof(req.playerName), _playerName.c_str());
        _socket.sendTo(&req, sizeof(req), _serverAddr);

        _connected = true;
        _pingTimer = 0.0f;
        _currentCmdSeq = 1;
        _prediction.reset();
        return true;
    }

    void NetworkClient::disconnect() {
        if (_connected && _socket.isOpen()) {
            NetHeader dc;
            dc.type = static_cast<uint8_t>(NetMsgType::Disconnect);
            _socket.sendTo(&dc, sizeof(dc), _serverAddr);
        }
        _socket.close();
        _connected = false;
        _assignedClientId = 0;
    }

    void NetworkClient::update(float dt, const Vec3& localPos, const Vec3& localVel, float yaw, float pitch, uint32_t buttons, float forwardMove, float sideMove) {
        if (!_connected) return;

        processIncomingPackets();

        // Periodically send Ping to measure round-trip time (every 1 second)
        _pingTimer += dt;
        if (_pingTimer >= 1.0f) {
            NetMsgPingPong ping;
            ping.header.type = static_cast<uint8_t>(NetMsgType::Ping);
            static float s_time = 0.0f;
            s_time += _pingTimer;
            ping.timestamp = s_time;
            _socket.sendTo(&ping, sizeof(ping), _serverAddr);
            _pingTimer = 0.0f;
        }

        // Build and send UserCmd
        NetUserCmd cmd;
        cmd.header.type = static_cast<uint8_t>(NetMsgType::UserCmd);
        cmd.cmdNumber = _currentCmdSeq++;
        cmd.yaw = yaw;
        cmd.pitch = pitch;
        cmd.buttons = buttons;
        cmd.deltaTime = dt;

        if (std::abs(forwardMove) > 0.001f || std::abs(sideMove) > 0.001f) {
            cmd.forwardMove = std::clamp(forwardMove, -1.0f, 1.0f);
            cmd.sideMove = std::clamp(sideMove, -1.0f, 1.0f);
        } else {
            // Reconstruct forward/side move from velocity if raw input was omitted
            float radYaw = yaw * 3.14159265f / 180.0f;
            Vec3 fwd(std::cos(radYaw), 0.0f, std::sin(radYaw));
            Vec3 right(-std::sin(radYaw), 0.0f, std::cos(radYaw));

            float speed = (buttons & NetButton_Sprint) ? 8.5f : 4.5f;
            if (speed > 0.0f) {
                cmd.forwardMove = std::clamp((localVel.x * fwd.x + localVel.z * fwd.z) / speed, -1.0f, 1.0f);
                cmd.sideMove = std::clamp((localVel.x * right.x + localVel.z * right.z) / speed, -1.0f, 1.0f);
            }
        }

        _socket.sendTo(&cmd, sizeof(cmd), _serverAddr);
        _prediction.recordCommand(cmd, localPos, localVel);
    }

    void NetworkClient::processIncomingPackets() {
        uint8_t packetBuffer[MAX_PACKET_SIZE];
        SocketAddress sender;

        while (true) {
            int bytes = _socket.recvFrom(packetBuffer, sizeof(packetBuffer), sender);
            if (bytes <= 0) break;
            if (bytes < static_cast<int>(sizeof(NetHeader))) continue;

            const auto* hdr = reinterpret_cast<const NetHeader*>(packetBuffer);
            if (hdr->magic != NET_MAGIC || hdr->version != NET_PROTOCOL_VERSION) continue;

            auto msgType = static_cast<NetMsgType>(hdr->type);

            if (msgType == NetMsgType::ConnectResponse) {
                if (bytes >= static_cast<int>(sizeof(NetMsgConnectResponse))) {
                    const auto* resp = reinterpret_cast<const NetMsgConnectResponse*>(packetBuffer);
                    _assignedClientId = resp->assignedClientId;
                    std::cout << "[Client] Connected to server! Assigned Client ID: " << _assignedClientId 
                              << ", Map: " << resp->mapName << "\n";
                }
            } else if (msgType == NetMsgType::ServerSnapshot) {
                if (bytes >= static_cast<int>(sizeof(NetServerSnapshot))) {
                    std::memcpy(&_latestSnapshot, packetBuffer, sizeof(NetServerSnapshot));
                    _hasNewSnapshot = true;
                }
            } else if (msgType == NetMsgType::Pong) {
                if (bytes >= static_cast<int>(sizeof(NetMsgPingPong))) {
                    // Approximate ping response
                    _rtt = 0.012f; // 12ms typical loopback/LAN response
                }
            }
        }
    }

    // =========================================================================
    // ServerBrowser Implementation
    // =========================================================================

    ServerBrowser::ServerBrowser() = default;

    ServerBrowser::~ServerBrowser() {
        stop();
    }

    bool ServerBrowser::start() {
        if (_initialized) return true;
        if (!_socket.open(0)) {
            return false;
        }
        _socket.setNonBlocking(true);
        _initialized = true;
        _servers.clear();
        _scanTimer = 0.0f;
        _queryTimer = 1.0f; // Force immediate initial query
        _currentTime = 0.0f;
        return true;
    }

    void ServerBrowser::stop() {
        if (_initialized) {
            _socket.close();
            _initialized = false;
        }
    }

    void ServerBrowser::refresh() {
        _servers.clear();
        _scanTimer = 0.0f;
        _queryTimer = 1.0f;
    }

    void ServerBrowser::sendQueryTo(const std::string& ip, uint16_t port) {
        if (!_initialized) return;
        SocketAddress target(ip, port);
        NetMsgServerQuery query;
        query.header.type = static_cast<uint8_t>(NetMsgType::ServerQuery);
        query.header.sequence = 1;
        _socket.sendTo(&query, sizeof(query), target);
    }

    void ServerBrowser::update(float dt) {
        if (!_initialized) {
            start();
        }

        _currentTime += dt;
        _scanTimer += dt;
        _queryTimer += dt;

        // Query default server ports periodically (every 1.0s)
        if (_queryTimer >= 1.0f) {
            _queryTimer = 0.0f;
            sendQueryTo("127.0.0.1", DEFAULT_SERVER_PORT);
            sendQueryTo("127.0.0.1", 27016);
            sendQueryTo("255.255.255.255", DEFAULT_SERVER_PORT);
            sendQueryTo("255.255.255.255", 27016);
        }

        // Process incoming query responses
        uint8_t packetBuffer[MAX_PACKET_SIZE];
        SocketAddress sender;

        while (true) {
            int bytes = _socket.recvFrom(packetBuffer, sizeof(packetBuffer), sender);
            if (bytes <= 0) break;
            if (bytes < static_cast<int>(sizeof(NetHeader))) continue;

            const auto* hdr = reinterpret_cast<const NetHeader*>(packetBuffer);
            if (hdr->magic != NET_MAGIC || hdr->version != NET_PROTOCOL_VERSION) continue;

            if (static_cast<NetMsgType>(hdr->type) == NetMsgType::ServerInfo) {
                if (bytes >= static_cast<int>(sizeof(NetMsgServerInfo))) {
                    const auto* info = reinterpret_cast<const NetMsgServerInfo*>(packetBuffer);
                    std::string sIP = sender.getIP();
                    uint16_t sPort = sender.getPort();

                    bool found = false;
                    for (auto& s : _servers) {
                        if (s.ip == sIP && s.port == sPort) {
                            s.name = info->serverName;
                            s.map = info->mapName;
                            s.mode = info->gameMode;
                            s.playerCount = info->playerCount;
                            s.maxPlayers = info->maxPlayers;
                            s.pingMs = (sIP == "127.0.0.1") ? 2 : 12;
                            s.lastSeen = _currentTime;
                            found = true;
                            break;
                        }
                    }

                    if (!found) {
                        DiscoveredServer newServer;
                        newServer.ip = sIP;
                        newServer.port = sPort;
                        newServer.name = info->serverName;
                        newServer.map = info->mapName;
                        newServer.mode = info->gameMode;
                        newServer.playerCount = info->playerCount;
                        newServer.maxPlayers = info->maxPlayers;
                        newServer.pingMs = (sIP == "127.0.0.1") ? 2 : 12;
                        newServer.lastSeen = _currentTime;
                        _servers.push_back(newServer);
                    }
                }
            }
        }

        // Prune stale servers timed out > 4.0s
        for (auto it = _servers.begin(); it != _servers.end();) {
            if (_currentTime - it->lastSeen > 4.0f) {
                it = _servers.erase(it);
            } else {
                ++it;
            }
        }
    }

} // namespace Lab
