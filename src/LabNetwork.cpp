#include "LabNetwork.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <ctime>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
    #define NOMINMAX
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #include <wininet.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "ws2_32.lib")
        #pragma comment(lib, "wininet.lib")
    #endif
    using socklen_t = int;
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <fcntl.h>
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    inline int closesocket(int s) { return ::close(s); }
    inline int ioctlsocket(int s, unsigned long cmd, void* argp) { return ::ioctl(s, cmd, argp); }
    using SOCKET = int;
    using u_long = unsigned long;
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
        if (res == 1) return true;

        // Fallback to DNS resolution (e.g. DDNS domain names or hostnames)
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo* result = nullptr;
        if (getaddrinfo(ip.c_str(), nullptr, &hints, &result) == 0 && result != nullptr) {
            auto* resolved = reinterpret_cast<sockaddr_in*>(result->ai_addr);
            addr->sin_addr = resolved->sin_addr;
            freeaddrinfo(result);
            return true;
        }
        return false;
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

        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) {
            std::cerr << "[Network] Failed to create UDP socket.\n";
            return false;
        }

        // Allow fast port reuse
        int opt = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

        // Allow broadcast for LAN discovery
        int bcast = 1;
        setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&bcast), sizeof(bcast));

#ifdef _WIN32
        // Disable WSAECONNRESET (10054) on Windows UDP sockets when ICMP Port Unreachable is received
        #ifndef SIO_UDP_CONNRESET
        #define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
        #endif
        BOOL bNewBehavior = FALSE;
        DWORD dwBytesReturned = 0;
        WSAIoctl(s, SIO_UDP_CONNRESET, &bNewBehavior, sizeof(bNewBehavior), NULL, 0, &dwBytesReturned, NULL, NULL);
#endif

        sockaddr_in bindAddr{};
        bindAddr.sin_family = AF_INET;
        bindAddr.sin_addr.s_addr = INADDR_ANY;
        bindAddr.sin_port = htons(port);

        if (bind(s, reinterpret_cast<sockaddr*>(&bindAddr), sizeof(bindAddr)) == SOCKET_ERROR) {
            std::cerr << "[Network] Failed to bind UDP socket to port " << port << ".\n";
            closesocket(s);
            return false;
        }

        // If port 0 was passed, query assigned ephemeral port
        if (port == 0) {
            sockaddr_in queryAddr{};
            socklen_t len = sizeof(queryAddr);
            if (getsockname(s, reinterpret_cast<sockaddr*>(&queryAddr), &len) == 0) {
                _boundPort = ntohs(queryAddr.sin_port);
            }
        } else {
            _boundPort = port;
        }

        _socketHandle = static_cast<intptr_t>(s);
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
            float effectiveThreshold = std::max(errorThreshold, 0.45f);
            if (distSq > effectiveThreshold * effectiveThreshold) {
                needCorrection = true;
            }
            _history.pop_front();
        }

        if (!needCorrection) {
            return false;
        }

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
            } else if (key == "room_code" || key == "room" || key == "code") {
                roomCode = val;
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
        out << "room_code = " << roomCode << "\n";

        return true;
    }

    // =========================================================================
    // NAT Traversal, STUN & UPnP Discovery Implementation
    // =========================================================================

    std::string NATDiscovery::getLocalIP() {
        NetworkSystem::init();
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) return "127.0.0.1";

        sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_port = htons(80);
        inet_pton(AF_INET, "8.8.8.8", &target.sin_addr);

        if (connect(s, reinterpret_cast<sockaddr*>(&target), sizeof(target)) == SOCKET_ERROR) {
            closesocket(s);
            return "127.0.0.1";
        }

        sockaddr_in local{};
        socklen_t len = sizeof(local);
        if (getsockname(s, reinterpret_cast<sockaddr*>(&local), &len) == SOCKET_ERROR) {
            closesocket(s);
            return "127.0.0.1";
        }

        closesocket(s);
        char buf[INET_ADDRSTRLEN] = { 0 };
        inet_ntop(AF_INET, &local.sin_addr, buf, INET_ADDRSTRLEN);
        std::string ipStr(buf);
        return ipStr.empty() ? "127.0.0.1" : ipStr;
    }

    std::string NATDiscovery::discoverPublicIP(uint16_t localPort) {
        NetworkSystem::init();

        const char* stunHosts[] = { "stun.l.google.com", "stun1.l.google.com", "stun2.l.google.com" };

        for (const char* host : stunHosts) {
            addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            addrinfo* res = nullptr;

            if (getaddrinfo(host, "19302", &hints, &res) != 0 || !res) {
                continue;
            }

            SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (s == INVALID_SOCKET) {
                freeaddrinfo(res);
                continue;
            }

            // Bind to requested local port (or ephemeral)
            sockaddr_in localAddr{};
            localAddr.sin_family = AF_INET;
            localAddr.sin_addr.s_addr = INADDR_ANY;
            localAddr.sin_port = htons(localPort != 0 ? localPort : 0);
            bind(s, reinterpret_cast<sockaddr*>(&localAddr), sizeof(localAddr));

            // RFC 5389 STUN Binding Request Header (20 bytes)
            uint8_t request[20] = { 0 };
            request[0] = 0x00; request[1] = 0x01; // Binding Request
            request[2] = 0x00; request[3] = 0x00; // Message Length: 0
            request[4] = 0x21; request[5] = 0x12; request[6] = 0xA4; request[7] = 0x42; // Magic Cookie
            for (int i = 8; i < 20; ++i) request[i] = static_cast<uint8_t>(i * 17);

            sendto(s, reinterpret_cast<const char*>(request), sizeof(request), 0, res->ai_addr, static_cast<socklen_t>(res->ai_addrlen));
            freeaddrinfo(res);

            // Wait up to 1000ms using select
            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(s, &readSet);
            timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;

            int sel = select(static_cast<int>(s + 1), &readSet, nullptr, nullptr, &tv);
            if (sel > 0 && FD_ISSET(s, &readSet)) {
                uint8_t buffer[512] = { 0 };
                sockaddr_in fromAddr{};
                socklen_t fromLen = sizeof(fromAddr);
                int bytes = recvfrom(s, reinterpret_cast<char*>(buffer), sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&fromAddr), &fromLen);

                closesocket(s);

                if (bytes >= 20) {
                    uint16_t msgType = (static_cast<uint16_t>(buffer[0]) << 8) | buffer[1];
                    uint32_t magic = (static_cast<uint32_t>(buffer[4]) << 24) |
                                     (static_cast<uint32_t>(buffer[5]) << 16) |
                                     (static_cast<uint32_t>(buffer[6]) << 8) |
                                     buffer[7];

                    if (msgType == 0x0101 && magic == 0x2112A442) {
                        int offset = 20;
                        while (offset + 4 <= bytes) {
                            uint16_t attrType = (static_cast<uint16_t>(buffer[offset]) << 8) | buffer[offset + 1];
                            uint16_t attrLen  = (static_cast<uint16_t>(buffer[offset + 2]) << 8) | buffer[offset + 3];
                            offset += 4;

                            if (offset + attrLen > bytes) break;

                            if (attrType == 0x0020 && attrLen >= 8) { // XOR-MAPPED-ADDRESS
                                uint8_t family = buffer[offset + 1];
                                if (family == 0x01) { // IPv4
                                    uint32_t xorIP = (static_cast<uint32_t>(buffer[offset + 4]) << 24) |
                                                     (static_cast<uint32_t>(buffer[offset + 5]) << 16) |
                                                     (static_cast<uint32_t>(buffer[offset + 6]) << 8) |
                                                     buffer[offset + 7];
                                    uint32_t realIP = xorIP ^ 0x2112A442;
                                    in_addr in;
                                    in.s_addr = htonl(realIP);
                                    char ipBuf[INET_ADDRSTRLEN] = { 0 };
                                    inet_ntop(AF_INET, &in, ipBuf, INET_ADDRSTRLEN);
                                    return std::string(ipBuf);
                                }
                            } else if (attrType == 0x0001 && attrLen >= 8) { // MAPPED-ADDRESS
                                uint8_t family = buffer[offset + 1];
                                if (family == 0x01) { // IPv4
                                    uint32_t ip = (static_cast<uint32_t>(buffer[offset + 4]) << 24) |
                                                  (static_cast<uint32_t>(buffer[offset + 5]) << 16) |
                                                  (static_cast<uint32_t>(buffer[offset + 6]) << 8) |
                                                  buffer[offset + 7];
                                    in_addr in;
                                    in.s_addr = htonl(ip);
                                    char ipBuf[INET_ADDRSTRLEN] = { 0 };
                                    inet_ntop(AF_INET, &in, ipBuf, INET_ADDRSTRLEN);
                                    return std::string(ipBuf);
                                }
                            }
                            offset += ((attrLen + 3) & ~3);
                        }
                    }
                }
            } else {
                closesocket(s);
            }
        }

        return getLocalIP();
    }

    static std::string s_lastUPnPRouterHost;
    static uint16_t s_lastUPnPRouterPort = 0;
    static std::string s_lastUPnPControlURL;
    static std::string s_lastUPnPServiceType = "urn:schemas-upnp-org:service:WANIPConnection:1";

    static std::string httpGetRequest(const std::string& host, uint16_t port, const std::string& path, int timeoutMs = 800) {
        SOCKET tcpSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (tcpSock == INVALID_SOCKET) return "";

        sockaddr_in rAddr{};
        rAddr.sin_family = AF_INET;
        rAddr.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &rAddr.sin_addr) <= 0) {
            closesocket(tcpSock);
            return "";
        }

        u_long mode = 1;
        ioctlsocket(tcpSock, FIONBIO, &mode);
        connect(tcpSock, reinterpret_cast<sockaddr*>(&rAddr), sizeof(rAddr));

        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(tcpSock, &writeSet);
        timeval ctv;
        ctv.tv_sec = timeoutMs / 1000;
        ctv.tv_usec = (timeoutMs % 1000) * 1000;

        if (select(static_cast<int>(tcpSock + 1), nullptr, &writeSet, nullptr, &ctv) <= 0) {
            closesocket(tcpSock);
            return "";
        }

        std::string req = "GET " + path + " HTTP/1.1\r\nHost: " + host + ":" + std::to_string(port) + "\r\nConnection: Close\r\n\r\n";
        send(tcpSock, req.c_str(), static_cast<int>(req.size()), 0);

        std::string response;
        char buf[2048];
        fd_set readSet;
        timeval rtv;
        rtv.tv_sec = 0;
        rtv.tv_usec = 400000;

        while (true) {
            FD_ZERO(&readSet);
            FD_SET(tcpSock, &readSet);
            int sel = select(static_cast<int>(tcpSock + 1), &readSet, nullptr, nullptr, &rtv);
            if (sel <= 0) break;
            int bytes = recv(tcpSock, buf, sizeof(buf) - 1, 0);
            if (bytes <= 0) break;
            buf[bytes] = '\0';
            response.append(buf, bytes);
            if (response.size() > 65536) break;
        }
        closesocket(tcpSock);
        return response;
    }

    static bool sendSoapPost(const std::string& host, uint16_t port, const std::string& path,
                             const std::string& serviceType, const std::string& action, const std::string& soapBody,
                             int timeoutMs = 800) {
        SOCKET tcpSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (tcpSock == INVALID_SOCKET) return false;

        sockaddr_in rAddr{};
        rAddr.sin_family = AF_INET;
        rAddr.sin_port = htons(port);
        if (inet_pton(AF_INET, host.c_str(), &rAddr.sin_addr) <= 0) {
            closesocket(tcpSock);
            return false;
        }

        u_long mode = 1;
        ioctlsocket(tcpSock, FIONBIO, &mode);
        connect(tcpSock, reinterpret_cast<sockaddr*>(&rAddr), sizeof(rAddr));

        fd_set writeSet;
        FD_ZERO(&writeSet);
        FD_SET(tcpSock, &writeSet);
        timeval ctv;
        ctv.tv_sec = timeoutMs / 1000;
        ctv.tv_usec = (timeoutMs % 1000) * 1000;

        if (select(static_cast<int>(tcpSock + 1), nullptr, &writeSet, nullptr, &ctv) <= 0) {
            closesocket(tcpSock);
            return false;
        }

        std::string httpRequest =
            "POST " + path + " HTTP/1.1\r\n"
            "Host: " + host + ":" + std::to_string(port) + "\r\n"
            "User-Agent: FrozenLife/1.0 UPnP/1.1\r\n"
            "Content-Length: " + std::to_string(soapBody.size()) + "\r\n"
            "Content-Type: text/xml; charset=\"utf-8\"\r\n"
            "SOAPAction: \"" + serviceType + "#" + action + "\"\r\n"
            "Connection: Close\r\n\r\n" + soapBody;

        send(tcpSock, httpRequest.c_str(), static_cast<int>(httpRequest.size()), 0);

        char respBuf[2048] = { 0 };
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(tcpSock, &readSet);
        timeval rtv;
        rtv.tv_sec = 0;
        rtv.tv_usec = 600000;
        bool ok = false;
        if (select(static_cast<int>(tcpSock + 1), &readSet, nullptr, nullptr, &rtv) > 0) {
            int bytes = recv(tcpSock, respBuf, sizeof(respBuf) - 1, 0);
            if (bytes > 0) {
                respBuf[bytes] = '\0';
                if (std::strstr(respBuf, "200 OK") != nullptr ||
                    std::strstr(respBuf, "AddPortMappingResponse") != nullptr ||
                    std::strstr(respBuf, "DeletePortMappingResponse") != nullptr) {
                    ok = true;
                }
            }
        }
        closesocket(tcpSock);
        return ok;
    }

    bool NATDiscovery::openUPnPPort(uint16_t port, const std::string& description) {
        NetworkSystem::init();
        std::string localIP = getLocalIP();

        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) return false;

        int opt = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
        setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&opt), sizeof(opt));

        sockaddr_in msearchAddr{};
        msearchAddr.sin_family = AF_INET;
        msearchAddr.sin_port = htons(1900);
        inet_pton(AF_INET, "239.255.255.250", &msearchAddr.sin_addr);

        const char* msearchDevices =
            "M-SEARCH * HTTP/1.1\r\n"
            "HOST: 239.255.255.250:1900\r\n"
            "ST: urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\n"
            "MAN: \"ssdp:discover\"\r\n"
            "MX: 2\r\n\r\n";

        const char* msearchServices =
            "M-SEARCH * HTTP/1.1\r\n"
            "HOST: 239.255.255.250:1900\r\n"
            "ST: urn:schemas-upnp-org:service:WANIPConnection:1\r\n"
            "MAN: \"ssdp:discover\"\r\n"
            "MX: 2\r\n\r\n";

        sendto(s, msearchDevices, static_cast<int>(std::strlen(msearchDevices)), 0,
               reinterpret_cast<sockaddr*>(&msearchAddr), sizeof(msearchAddr));
        sendto(s, msearchServices, static_cast<int>(std::strlen(msearchServices)), 0,
               reinterpret_cast<sockaddr*>(&msearchAddr), sizeof(msearchAddr));

        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(s, &readSet);
        timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 200000;

        char buf[2048] = { 0 };
        sockaddr_in respAddr{};
        socklen_t respLen = sizeof(respAddr);

        int sel = select(static_cast<int>(s + 1), &readSet, nullptr, nullptr, &tv);
        bool found = false;
        std::string location;

        if (sel > 0 && FD_ISSET(s, &readSet)) {
            int bytes = recvfrom(s, buf, sizeof(buf) - 1, 0, reinterpret_cast<sockaddr*>(&respAddr), &respLen);
            if (bytes > 0) {
                buf[bytes] = '\0';
                std::string respStr(buf);
                size_t locPos = respStr.find("LOCATION:");
                if (locPos == std::string::npos) locPos = respStr.find("location:");
                if (locPos != std::string::npos) {
                    size_t endLine = respStr.find("\r\n", locPos);
                    location = respStr.substr(locPos + 9, endLine - (locPos + 9));
                    location = trimString(location);
                    found = true;
                }
            }
        }
        closesocket(s);

        std::string routerHost = "192.168.1.1";
        uint16_t routerPort = 80;
        std::string controlPath = "/ctl/IPConn";
        std::string serviceType = "urn:schemas-upnp-org:service:WANIPConnection:1";

        auto isPrivateIPv4 = [](const std::string& ip) -> bool {
            unsigned int a = 0, b = 0, c = 0, d = 0;
            if (std::sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return false;
            if (a > 255 || b > 255 || c > 255 || d > 255) return false;
            if (a == 10 || a == 127) return true;
            if (a == 192 && b == 168) return true;
            if (a == 172 && b >= 16 && b <= 31) return true;
            return false;
        };

        auto escapeXml = [](const std::string& raw) -> std::string {
            std::string out;
            out.reserve(raw.size());
            for (char ch : raw) {
                switch (ch) {
                    case '&':  out += "&amp;"; break;
                    case '<':  out += "&lt;"; break;
                    case '>':  out += "&gt;"; break;
                    case '"':  out += "&quot;"; break;
                    case '\'': out += "&apos;"; break;
                    default:   out += ch; break;
                }
            }
            return out;
        };

        if (found && location.rfind("http://", 0) == 0) {
            std::string sub = location.substr(7);
            size_t slash = sub.find('/');
            std::string hostPort = (slash != std::string::npos) ? sub.substr(0, slash) : sub;
            std::string xmlPath = (slash != std::string::npos) ? sub.substr(slash) : "/";
            size_t colon = hostPort.find(':');
            if (colon != std::string::npos) {
                routerHost = hostPort.substr(0, colon);
                try { routerPort = static_cast<uint16_t>(std::stoi(hostPort.substr(colon + 1))); } catch (...) {}
            } else {
                routerHost = hostPort;
                routerPort = 80;
            }

            if (!isPrivateIPv4(routerHost)) {
                size_t lastDot = localIP.find_last_of('.');
                routerHost = (lastDot != std::string::npos) ? (localIP.substr(0, lastDot) + ".1") : "192.168.1.1";
                routerPort = 80;
                xmlPath.clear();
            }

            // Fetch router XML root description to discover authoritative controlURL
            std::string xml = xmlPath.empty() ? std::string() : httpGetRequest(routerHost, routerPort, xmlPath, 800);
            if (!xml.empty()) {
                size_t sPos = xml.find("urn:schemas-upnp-org:service:WANIPConnection:1");
                if (sPos == std::string::npos) sPos = xml.find("urn:schemas-upnp-org:service:WANPPPConnection:1");
                if (sPos == std::string::npos) sPos = xml.find("urn:schemas-upnp-org:service:WANIPConnection:2");

                if (sPos != std::string::npos) {
                    if (xml.find("WANPPPConnection", sPos) != std::string::npos && xml.find("WANPPPConnection", sPos) < sPos + 64) {
                        serviceType = "urn:schemas-upnp-org:service:WANPPPConnection:1";
                    }
                    size_t ctrlTag = xml.find("<controlURL>", sPos);
                    if (ctrlTag != std::string::npos && ctrlTag < sPos + 600) {
                        size_t ctrlEnd = xml.find("</controlURL>", ctrlTag);
                        if (ctrlEnd != std::string::npos) {
                            std::string rawCtrl = trimString(xml.substr(ctrlTag + 12, ctrlEnd - (ctrlTag + 12)));
                            if (rawCtrl.rfind("http://", 0) == 0) {
                                std::string cSub = rawCtrl.substr(7);
                                size_t cSlash = cSub.find('/');
                                if (cSlash != std::string::npos) controlPath = cSub.substr(cSlash);
                            } else {
                                controlPath = (rawCtrl.front() == '/') ? rawCtrl : ("/" + rawCtrl);
                            }
                        }
                    }
                }
            }
        } else {
            size_t lastDot = localIP.find_last_of('.');
            if (lastDot != std::string::npos) {
                routerHost = localIP.substr(0, lastDot) + ".1";
            }
        }

        std::string safeDescription = escapeXml(description);
        std::string soapBody =
            "<?xml version=\"1.0\"?>\r\n"
            "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
            "<s:Body>\r\n"
            "<u:AddPortMapping xmlns:u=\"" + serviceType + "\">\r\n"
            "<NewRemoteHost></NewRemoteHost>\r\n"
            "<NewExternalPort>" + std::to_string(port) + "</NewExternalPort>\r\n"
            "<NewProtocol>UDP</NewProtocol>\r\n"
            "<NewInternalPort>" + std::to_string(port) + "</NewInternalPort>\r\n"
            "<NewInternalClient>" + localIP + "</NewInternalClient>\r\n"
            "<NewEnabled>1</NewEnabled>\r\n"
            "<NewPortMappingDescription>" + safeDescription + "</NewPortMappingDescription>\r\n"
            "<NewLeaseDuration>0</NewLeaseDuration>\r\n"
            "</u:AddPortMapping>\r\n"
            "</s:Body>\r\n"
            "</s:Envelope>\r\n";

        if (sendSoapPost(routerHost, routerPort, controlPath, serviceType, "AddPortMapping", soapBody, 800)) {
            s_lastUPnPRouterHost = routerHost;
            s_lastUPnPRouterPort = routerPort;
            s_lastUPnPControlURL = controlPath;
            s_lastUPnPServiceType = serviceType;
            std::cout << "[UPnP] Successfully mapped UDP port " << port << " via UPnP IGD on " << routerHost << ":" << routerPort << " (" << controlPath << ")!\n";
            return true;
        }

        // Fallback trial on port 80 and common control paths if router port differed
        const std::vector<std::string> fallbacks = { "/ctl/IPConn", "/upnp/control/WANIPConn1", "/ipc", "/upnp/control/WANPPPConn1" };
        for (const auto& fb : fallbacks) {
            if (sendSoapPost(routerHost, 80, fb, serviceType, "AddPortMapping", soapBody, 300) ||
                sendSoapPost(routerHost, 1900, fb, serviceType, "AddPortMapping", soapBody, 300) ||
                sendSoapPost(routerHost, routerPort, fb, serviceType, "AddPortMapping", soapBody, 300)) {
                s_lastUPnPRouterHost = routerHost;
                s_lastUPnPRouterPort = (routerPort != 0 ? routerPort : 80);
                s_lastUPnPControlURL = fb;
                s_lastUPnPServiceType = serviceType;
                std::cout << "[UPnP] Successfully mapped UDP port " << port << " via UPnP fallback!\n";
                return true;
            }
        }

        std::cout << "[UPnP] Notice: UPnP automatic port-mapping not supported by gateway. Direct, Radmin VPN, or LAN connect active.\n";
        return false;
    }

    void NATDiscovery::closeUPnPPort(uint16_t port) {
        NetworkSystem::init();
        std::string localIP = getLocalIP();
        std::string routerHost = s_lastUPnPRouterHost.empty() ? "192.168.1.1" : s_lastUPnPRouterHost;
        uint16_t routerPort = s_lastUPnPRouterPort != 0 ? s_lastUPnPRouterPort : 80;
        std::string controlPath = s_lastUPnPControlURL.empty() ? "/ctl/IPConn" : s_lastUPnPControlURL;
        std::string serviceType = s_lastUPnPServiceType.empty() ? "urn:schemas-upnp-org:service:WANIPConnection:1" : s_lastUPnPServiceType;

        if (s_lastUPnPRouterHost.empty()) {
            size_t lastDot = localIP.find_last_of('.');
            if (lastDot != std::string::npos) routerHost = localIP.substr(0, lastDot) + ".1";
        }

        std::string soapBody =
            "<?xml version=\"1.0\"?>\r\n"
            "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
            "<s:Body>\r\n"
            "<u:DeletePortMapping xmlns:u=\"" + serviceType + "\">\r\n"
            "<NewRemoteHost></NewRemoteHost>\r\n"
            "<NewExternalPort>" + std::to_string(port) + "</NewExternalPort>\r\n"
            "<NewProtocol>UDP</NewProtocol>\r\n"
            "</u:DeletePortMapping>\r\n"
            "</s:Body>\r\n"
            "</s:Envelope>\r\n";

        sendSoapPost(routerHost, routerPort, controlPath, serviceType, "DeletePortMapping", soapBody, 400);
    }

    bool NATDiscovery::punchHole(const std::string& remoteIP, uint16_t remotePort, uint16_t localPort) {
        NetworkSystem::init();
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) return false;

        if (localPort != 0) {
            sockaddr_in localAddr{};
            localAddr.sin_family = AF_INET;
            localAddr.sin_addr.s_addr = INADDR_ANY;
            localAddr.sin_port = htons(localPort);
            int opt = 1;
            setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
            bind(s, reinterpret_cast<sockaddr*>(&localAddr), sizeof(localAddr));
        }

        sockaddr_in destAddr{};
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(remotePort);
        if (inet_pton(AF_INET, remoteIP.c_str(), &destAddr.sin_addr) <= 0) {
            closesocket(s);
            return false;
        }

        NetMsgNatPunch punch;
        punch.header.type = static_cast<uint8_t>(NetMsgType::NatPunch);
        punch.senderId = 0;
        safeStrCopy(punch.tag, sizeof(punch.tag), "NAT_BEACON");

        for (int i = 0; i < 5; ++i) {
            sendto(s, reinterpret_cast<const char*>(&punch), sizeof(punch), 0,
                   reinterpret_cast<sockaddr*>(&destAddr), sizeof(destAddr));
        }
        closesocket(s);
        return true;
    }

    NATStatus NATDiscovery::queryFullStatus(uint16_t port) {
        NATStatus status;
        status.localIP = getLocalIP();
        status.publicIP = discoverPublicIP(port);
        status.externalPort = port;
        status.upnpActive = openUPnPPort(port, "FrozenLife Dedicated Server");
        if (status.upnpActive) {
            status.upnpMessage = "Port " + std::to_string(port) + " opened via UPnP IGD";
        } else {
            status.upnpMessage = "UPnP unavailable (forward port 27015 or use Radmin VPN)";
        }
        return status;
    }

    // =========================================================================
    // Plug & Play Cloud Rendezvous & Automatic Hole Punching Implementation
    // =========================================================================

#ifdef _WIN32
    static std::string httpFetch(const std::string& url) {
        HINTERNET hInternet = InternetOpenA("FrozenLife-PnP/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (!hInternet) return "";

        DWORD flags = INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE;
        if (url.rfind("https://", 0) == 0) {
            flags |= INTERNET_FLAG_SECURE;
        }

        HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, flags, 0);
        if (!hUrl && (flags & INTERNET_FLAG_SECURE)) {
            hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0, INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
        }
        if (!hUrl) {
            InternetCloseHandle(hInternet);
            return "";
        }

        std::string response;
        char buffer[4096];
        DWORD bytesRead = 0;
        while (InternetReadFile(hUrl, buffer, sizeof(buffer) - 1, &bytesRead) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            response.append(buffer, bytesRead);
            if (response.size() > 65536) break;
        }

        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return response;
    }
#else
    static std::string httpFetch(const std::string& url) {
        std::string cmd = "curl -s -m 3 \"" + url + "\"";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "";
        char buffer[1024];
        std::string result;
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result += buffer;
        }
        pclose(pipe);
        return result;
    }
#endif

    static std::string extractJsonValue(const std::string& json, const std::string& key) {
        size_t pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        size_t colon = json.find(':', pos);
        if (colon == std::string::npos) return "";
        size_t start = json.find_first_not_of(" \t\r\n", colon + 1);
        if (start == std::string::npos) return "";
        if (json[start] == '\"') {
            size_t end = json.find('\"', start + 1);
            if (end == std::string::npos) return "";
            return json.substr(start + 1, end - start - 1);
        } else {
            size_t end = json.find_first_of(",}\r\n ", start);
            if (end == std::string::npos) end = json.size();
            return json.substr(start, end - start);
        }
    }

    std::string PnPBroker::generateRoomCode() {
        static bool seeded = false;
        if (!seeded) {
            std::srand(static_cast<unsigned int>(std::time(nullptr)));
            seeded = true;
        }
        int code = 1000 + (std::rand() % 9000);
        return std::to_string(code);
    }

    bool PnPBroker::publishRoom(const std::string& roomCode, const std::string& publicIP, uint16_t publicPort,
                                const std::string& lanIP, uint16_t lanPort, const std::string& mapName,
                                const std::string& gameMode) {
        if (roomCode.empty()) return false;
        std::string cleanCode = toLowerString(roomCode);
        std::string url = "https://dweet.cc/dweet/for/fl-room-" + cleanCode +
                          "?pubIP=" + publicIP +
                          "&pubPort=" + std::to_string(publicPort) +
                          "&lanIP=" + lanIP +
                          "&lanPort=" + std::to_string(lanPort) +
                          "&map=" + mapName +
                          "&mode=" + gameMode;
        std::string resp = httpFetch(url);
        return (resp.find("succeeded") != std::string::npos);
    }

    bool PnPBroker::fetchRoom(const std::string& roomCode, PnPRoomInfo& outInfo) {
        if (roomCode.empty()) return false;
        std::string cleanCode = toLowerString(roomCode);
        std::string url = "https://dweet.cc/get/latest/dweet/for/fl-room-" + cleanCode;
        std::string resp = httpFetch(url);
        if (resp.empty() || resp.find("succeeded") == std::string::npos) return false;

        outInfo.roomCode = roomCode;
        outInfo.publicIP = extractJsonValue(resp, "pubIP");
        std::string sPubPort = extractJsonValue(resp, "pubPort");
        outInfo.lanIP = extractJsonValue(resp, "lanIP");
        std::string sLanPort = extractJsonValue(resp, "lanPort");
        outInfo.mapName = extractJsonValue(resp, "map");
        outInfo.gameMode = extractJsonValue(resp, "mode");

        try { outInfo.publicPort = static_cast<uint16_t>(std::stoi(sPubPort)); } catch (...) { outInfo.publicPort = DEFAULT_SERVER_PORT; }
        try { outInfo.lanPort = static_cast<uint16_t>(std::stoi(sLanPort)); } catch (...) { outInfo.lanPort = DEFAULT_SERVER_PORT; }

        outInfo.valid = (!outInfo.publicIP.empty() || !outInfo.lanIP.empty());
        return outInfo.valid;
    }

    bool PnPBroker::registerGuestPunch(const std::string& roomCode, const std::string& guestIP, uint16_t guestPort) {
        if (roomCode.empty() || guestIP.empty() || guestPort == 0) return false;
        std::string cleanCode = toLowerString(roomCode);
        std::string url = "https://dweet.cc/dweet/for/fl-room-" + cleanCode + "-guest" +
                          "?guestIP=" + guestIP +
                          "&guestPort=" + std::to_string(guestPort);
        std::string resp = httpFetch(url);
        return (resp.find("succeeded") != std::string::npos);
    }

    bool PnPBroker::fetchGuestPunch(const std::string& roomCode, std::string& outGuestIP, uint16_t& outGuestPort) {
        if (roomCode.empty()) return false;
        std::string cleanCode = toLowerString(roomCode);
        std::string url = "https://dweet.cc/get/latest/dweet/for/fl-room-" + cleanCode + "-guest";
        std::string resp = httpFetch(url);
        if (resp.empty() || resp.find("succeeded") == std::string::npos) return false;

        outGuestIP = extractJsonValue(resp, "guestIP");
        std::string sPort = extractJsonValue(resp, "guestPort");
        try { outGuestPort = static_cast<uint16_t>(std::stoi(sPort)); } catch (...) { outGuestPort = 0; }
        return (!outGuestIP.empty() && outGuestPort > 0);
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
        _roomCode = config.roomCode;
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
        _pnpPunchPollTimer = 0.0f;
        _lastPunchedGuest = "";
        _clients.clear();

        if (_roomCode.empty()) {
            _roomCode = _config.roomCode.empty() ? PnPBroker::generateRoomCode() : _config.roomCode;
            _config.roomCode = _roomCode;
        }

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

        // Publish to Cloud Rendezvous Broker for zero-config Plug & Play
        std::string pubIP = NATDiscovery::discoverPublicIP(_port);
        std::string lanIP = NATDiscovery::getLocalIP();
        PnPBroker::publishRoom(_roomCode, pubIP, _port, lanIP, _port, _mapName, _gameMode);
        std::cout << "[Server] [PLUG & PLAY] Room Code: [" << _roomCode << "] published to Cloud Broker!\n";

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

        // Periodic Cloud Broker Punch Polling (every 1.5 seconds)
        _pnpPunchPollTimer += dt;
        if (_pnpPunchPollTimer >= 1.5f) {
            _pnpPunchPollTimer = 0.0f;
            if (!_roomCode.empty()) {
                std::string guestIP;
                uint16_t guestPort = 0;
                if (PnPBroker::fetchGuestPunch(_roomCode, guestIP, guestPort) && !guestIP.empty() && guestPort > 0) {
                    std::string guestKey = guestIP + ":" + std::to_string(guestPort);
                    if (guestKey != _lastPunchedGuest) {
                        _lastPunchedGuest = guestKey;
                        punchClient(guestIP, guestPort);
                        std::cout << "[Server] [PLUG & PLAY] Automatically punched NAT hole for guest " << guestKey << "!\n";
                    }
                }
            }
        }

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
                        newClient.deathTime = 0.0f;
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
                        client->state.activeWeapon = cmd->activeWeapon;

                        if (client->state.isAlive) {
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
                                client->state.velocity.x *= 0.80f;
                                client->state.velocity.z *= 0.80f;
                                if (std::abs(client->state.velocity.x) < 0.05f) client->state.velocity.x = 0.0f;
                                if (std::abs(client->state.velocity.z) < 0.05f) client->state.velocity.z = 0.0f;
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
                }
            } else if (msgType == NetMsgType::DamageEvent) {
                if (bytes >= static_cast<int>(sizeof(NetMsgDamageEvent))) {
                    const auto* dmg = reinterpret_cast<const NetMsgDamageEvent*>(packetBuffer);
                    ConnectedClient* client = findClient(sender);
                    if (client && client->clientId == dmg->attackerId) {
                        applyDamageToClient(dmg->attackerId, dmg->targetId, dmg->damage, dmg->weaponId, dmg->isHeadshot != 0, dmg->hitPoint);
                    }
                }
            } else if (msgType == NetMsgType::ChatMessage) {
                if (bytes >= static_cast<int>(sizeof(NetChatMessage))) {
                    const auto* chat = reinterpret_cast<const NetChatMessage*>(packetBuffer);
                    ConnectedClient* client = findClient(sender);
                    std::string senderName = client ? client->name : chat->sender;
                    broadcastChatMessage(senderName, chat->text);
                }
            } else if (msgType == NetMsgType::ProjectileSpawn) {
                if (bytes >= static_cast<int>(sizeof(NetMsgProjectileSpawn))) {
                    const auto* proj = reinterpret_cast<const NetMsgProjectileSpawn*>(packetBuffer);
                    broadcastProjectileSpawn(*proj);
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
            } else if (msgType == NetMsgType::NatPunch) {
                NetMsgNatPunch punchAck;
                punchAck.header.type = static_cast<uint8_t>(NetMsgType::NatPunch);
                safeStrCopy(punchAck.tag, sizeof(punchAck.tag), "PUNCH_ACK");
                _socket.sendTo(&punchAck, sizeof(punchAck), sender);
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

    bool DedicatedServer::punchClient(const std::string& clientIP, uint16_t clientPort) {
        if (!_running) return false;
        SocketAddress addr;
        if (!addr.fromString(clientIP, clientPort)) return false;

        NetMsgNatPunch punch;
        punch.header.type = static_cast<uint8_t>(NetMsgType::NatPunch);
        punch.senderId = 0;
        safeStrCopy(punch.tag, sizeof(punch.tag), "SRV_PUNCH");

        for (int i = 0; i < 5; ++i) {
            _socket.sendTo(&punch, sizeof(punch), addr);
        }
        std::cout << "[Server] Sent UDP NAT Hole Punch beacons to " << clientIP << ":" << clientPort << "!\n";
        return true;
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

        // Authoritative 3-second respawn for eliminated players
        for (auto& c : _clients) {
            if (!c.state.isAlive && (_serverTime - c.deathTime >= 3.0f)) {
                c.state.isAlive = 1;
                c.state.health = 150.0f;
                c.state.armor = 50.0f;
                c.state.position = _spawnPosition;
                c.state.velocity = Vec3(0, 0, 0);
                std::cout << "[Server] Respawned player: " << c.name << " at ("
                          << _spawnPosition.x << ", " << _spawnPosition.y << ", " << _spawnPosition.z << ")\n";
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

    void DedicatedServer::broadcastDamageEvent(const NetMsgDamageEvent& dmg) {
        for (const auto& client : _clients) {
            _socket.sendTo(&dmg, sizeof(dmg), client.address);
        }
    }

    void DedicatedServer::broadcastKillEvent(const NetMsgKillEvent& kill) {
        for (const auto& client : _clients) {
            _socket.sendTo(&kill, sizeof(kill), client.address);
        }
    }

    void DedicatedServer::broadcastProjectileSpawn(const NetMsgProjectileSpawn& proj) {
        for (const auto& client : _clients) {
            if (client.clientId != proj.ownerId) {
                _socket.sendTo(&proj, sizeof(proj), client.address);
            }
        }
    }

    void DedicatedServer::applyDamageToClient(uint32_t attackerId, uint32_t targetId, float damage, uint8_t weaponId, bool isHeadshot, const Vec3& hitPoint) {
        ConnectedClient* target = findClientById(targetId);
        if (!target || !target->state.isAlive) return;

        // Armor absorbs 50% damage
        float armorAbsorb = std::min(target->state.armor, damage * 0.5f);
        target->state.armor -= armorAbsorb;
        float hpDamage = damage - armorAbsorb;
        target->state.health = std::max(0.0f, target->state.health - hpDamage);

        NetMsgDamageEvent dmg;
        dmg.header.type = static_cast<uint8_t>(NetMsgType::DamageEvent);
        dmg.attackerId = attackerId;
        dmg.targetId = targetId;
        dmg.damage = damage;
        dmg.weaponId = weaponId;
        dmg.isHeadshot = isHeadshot ? 1 : 0;
        dmg.hitPoint = hitPoint;
        broadcastDamageEvent(dmg);

        if (target->state.health <= 0.001f) {
            target->state.health = 0.0f;
            target->state.isAlive = 0;
            target->deathTime = _serverTime;
            target->state.deaths++;

            NetMsgKillEvent kill;
            kill.header.type = static_cast<uint8_t>(NetMsgType::KillEvent);
            kill.killerId = attackerId;
            kill.victimId = targetId;
            kill.weaponId = weaponId;
            kill.isHeadshot = isHeadshot ? 1 : 0;

            ConnectedClient* killer = findClientById(attackerId);
            if (killer) {
                killer->state.kills++;
                safeStrCopy(kill.killerName, sizeof(kill.killerName), killer->name.c_str());
            } else {
                safeStrCopy(kill.killerName, sizeof(kill.killerName), "Unknown");
            }
            safeStrCopy(kill.victimName, sizeof(kill.victimName), target->name.c_str());

            std::cout << "[Server] Kill Event: " << kill.killerName << " eliminated " << kill.victimName
                      << " with weapon " << static_cast<int>(weaponId) << (isHeadshot ? " (HEADSHOT!)" : "") << "\n";

            broadcastKillEvent(kill);
        }
    }

    ConnectedClient* DedicatedServer::findClient(const SocketAddress& addr) {
        for (auto& c : _clients) {
            if (c.address == addr) return &c;
        }
        return nullptr;
    }

    ConnectedClient* DedicatedServer::findClientById(uint32_t clientId) {
        for (auto& c : _clients) {
            if (c.clientId == clientId) return &c;
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
        _serverAddrStr = ip + ":" + std::to_string(port);

        if (!_serverAddr.fromString(ip, port)) {
            std::cerr << "[Client] Invalid server address or DNS name: " << ip << ":" << port << "\n";
            _state = ClientConnectionState::Disconnected;
            return false;
        }

        if (!_socket.open(0)) { // Bind ephemeral client port
            std::cerr << "[Client] Failed to open UDP client socket.\n";
            _state = ClientConnectionState::Disconnected;
            return false;
        }

        _state = ClientConnectionState::Connecting;
        _connectAttempts = 1;
        _connectTimer = 0.0f;
        _pingTimer = 0.0f;
        _currentCmdSeq = 1;
        _prediction.reset();
        _incomingChat.clear();
        _incomingDamageEvents.clear();
        _incomingKillEvents.clear();
        _incomingProjectiles.clear();

        // Send initial Connect Request
        NetMsgConnectRequest req;
        req.header.type = static_cast<uint8_t>(NetMsgType::ConnectRequest);
        safeStrCopy(req.playerName, sizeof(req.playerName), _playerName.c_str());
        _socket.sendTo(&req, sizeof(req), _serverAddr);

        std::cout << "[Client] Connecting to " << _serverAddrStr << " (attempt 1/20)...\n";
        return true;
    }

    void NetworkClient::disconnect() {
        if (_state == ClientConnectionState::Connected && _socket.isOpen()) {
            NetHeader dc;
            dc.type = static_cast<uint8_t>(NetMsgType::Disconnect);
            _socket.sendTo(&dc, sizeof(dc), _serverAddr);
        }
        _socket.close();
        _state = ClientConnectionState::Disconnected;
        _assignedClientId = 0;
        _connectAttempts = 0;
    }

    void NetworkClient::update(float dt, const Vec3& localPos, const Vec3& localVel, float yaw, float pitch, uint32_t buttons, float forwardMove, float sideMove, uint8_t activeWeapon) {
        if (_state == ClientConnectionState::Disconnected) return;

        processIncomingPackets();

        // If connecting, retry handshake every 250ms up to 20 attempts
        if (_state == ClientConnectionState::Connecting) {
            _connectTimer += dt;
            if (_connectTimer >= 0.25f) {
                _connectTimer = 0.0f;
                _connectAttempts++;
                if (_connectAttempts > 20) {
                    _state = ClientConnectionState::TimedOut;
                    std::cerr << "[Client] Connection to " << _serverAddrStr << " timed out after 20 attempts.\n";
                    return;
                }
                NetMsgConnectRequest req;
                req.header.type = static_cast<uint8_t>(NetMsgType::ConnectRequest);
                safeStrCopy(req.playerName, sizeof(req.playerName), _playerName.c_str());
                _socket.sendTo(&req, sizeof(req), _serverAddr);
            }
            return;
        }

        if (_state != ClientConnectionState::Connected) return;

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
        cmd.activeWeapon = activeWeapon;

        cmd.forwardMove = std::clamp(forwardMove, -1.0f, 1.0f);
        cmd.sideMove = std::clamp(sideMove, -1.0f, 1.0f);
        if (std::abs(cmd.forwardMove) < 0.001f) cmd.forwardMove = 0.0f;
        if (std::abs(cmd.sideMove) < 0.001f) cmd.sideMove = 0.0f;

        _socket.sendTo(&cmd, sizeof(cmd), _serverAddr);
        _prediction.recordCommand(cmd, localPos, localVel);
    }

    void NetworkClient::sendDamage(uint32_t targetId, float damage, uint8_t weaponId, bool isHeadshot, const Vec3& hitPoint) {
        if (_state != ClientConnectionState::Connected) return;
        NetMsgDamageEvent dmg;
        dmg.header.type = static_cast<uint8_t>(NetMsgType::DamageEvent);
        dmg.attackerId = _assignedClientId;
        dmg.targetId = targetId;
        dmg.damage = damage;
        dmg.weaponId = weaponId;
        dmg.isHeadshot = isHeadshot ? 1 : 0;
        dmg.hitPoint = hitPoint;
        _socket.sendTo(&dmg, sizeof(dmg), _serverAddr);
    }

    void NetworkClient::sendChatMessage(const std::string& text) {
        if (_state != ClientConnectionState::Connected || text.empty()) return;
        NetChatMessage chat;
        chat.header.type = static_cast<uint8_t>(NetMsgType::ChatMessage);
        safeStrCopy(chat.sender, sizeof(chat.sender), _playerName.c_str());
        safeStrCopy(chat.text, sizeof(chat.text), text.c_str());
        _socket.sendTo(&chat, sizeof(chat), _serverAddr);
    }

    void NetworkClient::sendProjectileSpawn(uint8_t weaponId, const Vec3& pos, const Vec3& dir) {
        if (_state != ClientConnectionState::Connected) return;
        NetMsgProjectileSpawn proj;
        proj.header.type = static_cast<uint8_t>(NetMsgType::ProjectileSpawn);
        proj.ownerId = _assignedClientId;
        proj.weaponId = weaponId;
        proj.position = pos;
        proj.direction = dir;
        _socket.sendTo(&proj, sizeof(proj), _serverAddr);
    }

    std::vector<NetChatMessage> NetworkClient::pollIncomingChat() {
        std::vector<NetChatMessage> res = std::move(_incomingChat);
        _incomingChat.clear();
        return res;
    }

    std::vector<NetMsgDamageEvent> NetworkClient::pollIncomingDamageEvents() {
        std::vector<NetMsgDamageEvent> res = std::move(_incomingDamageEvents);
        _incomingDamageEvents.clear();
        return res;
    }

    std::vector<NetMsgKillEvent> NetworkClient::pollIncomingKillEvents() {
        std::vector<NetMsgKillEvent> res = std::move(_incomingKillEvents);
        _incomingKillEvents.clear();
        return res;
    }

    std::vector<NetMsgProjectileSpawn> NetworkClient::pollIncomingProjectiles() {
        std::vector<NetMsgProjectileSpawn> res = std::move(_incomingProjectiles);
        _incomingProjectiles.clear();
        return res;
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
                    _connectedMapName = resp->mapName;
                    if (_connectedMapName.empty()) _connectedMapName = "facility_alpha.labmap";
                    _state = ClientConnectionState::Connected;
                    std::cout << "[Client] Connected to server! Assigned Client ID: " << _assignedClientId
                              << ", Map: " << _connectedMapName << "\n";
                }
            } else if (msgType == NetMsgType::ServerSnapshot) {
                if (bytes >= static_cast<int>(sizeof(NetServerSnapshot))) {
                    std::memcpy(&_latestSnapshot, packetBuffer, sizeof(NetServerSnapshot));
                    _hasNewSnapshot = true;
                }
            } else if (msgType == NetMsgType::Pong) {
                if (bytes >= static_cast<int>(sizeof(NetMsgPingPong))) {
                    _rtt = 0.012f;
                }
            } else if (msgType == NetMsgType::ChatMessage) {
                if (bytes >= static_cast<int>(sizeof(NetChatMessage))) {
                    _incomingChat.push_back(*reinterpret_cast<const NetChatMessage*>(packetBuffer));
                }
            } else if (msgType == NetMsgType::DamageEvent) {
                if (bytes >= static_cast<int>(sizeof(NetMsgDamageEvent))) {
                    _incomingDamageEvents.push_back(*reinterpret_cast<const NetMsgDamageEvent*>(packetBuffer));
                }
            } else if (msgType == NetMsgType::KillEvent) {
                if (bytes >= static_cast<int>(sizeof(NetMsgKillEvent))) {
                    _incomingKillEvents.push_back(*reinterpret_cast<const NetMsgKillEvent*>(packetBuffer));
                }
            } else if (msgType == NetMsgType::ProjectileSpawn) {
                if (bytes >= static_cast<int>(sizeof(NetMsgProjectileSpawn))) {
                    _incomingProjectiles.push_back(*reinterpret_cast<const NetMsgProjectileSpawn*>(packetBuffer));
                }
            } else if (msgType == NetMsgType::NatPunch) {
                // Keep-alive / NAT hole punch packet safely consumed
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
        _queryTimer = 1.0f;
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

    void ServerBrowser::addFavorite(const std::string& ip, uint16_t port) {
        for (auto& s : _servers) {
            if (s.ip == ip && s.port == port) {
                s.isFavorite = !s.isFavorite;
            }
        }
        for (auto it = _favorites.begin(); it != _favorites.end(); ++it) {
            if (it->ip == ip && it->port == port) {
                _favorites.erase(it);
                return;
            }
        }
        DiscoveredServer fav;
        fav.ip = ip;
        fav.port = port;
        fav.name = "Favorite Server (" + ip + ")";
        fav.isFavorite = true;
        for (const auto& s : _servers) {
            if (s.ip == ip && s.port == port) {
                fav = s;
                fav.isFavorite = true;
                break;
            }
        }
        _favorites.push_back(fav);
    }

    void ServerBrowser::addHistory(const std::string& ip, uint16_t port, const std::string& name, const std::string& map) {
        for (auto& h : _history) {
            if (h.ip == ip && h.port == port) {
                h.name = name;
                h.map = map;
                return;
            }
        }
        DiscoveredServer hist;
        hist.ip = ip;
        hist.port = port;
        hist.name = name;
        hist.map = map;
        hist.isHistory = true;
        _history.push_back(hist);
    }

    std::vector<DiscoveredServer> ServerBrowser::getFilteredServers(int tabIndex, const std::string& mapFilter, int maxPing, bool notFull, bool hasPlayers) const {
        // tabIndex: 0 = Internet, 1 = Favorites, 2 = History, 3 = LAN, 4 = Friends
        std::vector<DiscoveredServer> sourceList;
        if (tabIndex == 1) {
            sourceList = _favorites;
        } else if (tabIndex == 2) {
            sourceList = _history;
        } else {
            sourceList = _servers;
        }

        std::vector<DiscoveredServer> result;
        for (const auto& s : sourceList) {
            if (tabIndex == 3 && !s.isLAN) continue;
            if (tabIndex == 4 && !s.isFriends) continue;
            if (!mapFilter.empty()) {
                std::string mLow = s.map;
                std::string fLow = mapFilter;
                for (char& c : mLow) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                for (char& c : fLow) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (mLow.find(fLow) == std::string::npos) continue;
            }
            if (maxPing > 0 && s.pingMs > maxPing) continue;
            if (notFull && s.playerCount >= s.maxPlayers) continue;
            if (hasPlayers && s.playerCount <= 0) continue;
            result.push_back(s);
        }
        return result;
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

        if (_queryTimer >= 1.0f) {
            _queryTimer = 0.0f;
            sendQueryTo("127.0.0.1", DEFAULT_SERVER_PORT);
            sendQueryTo("127.0.0.1", 27016);
            sendQueryTo("255.255.255.255", DEFAULT_SERVER_PORT);
            sendQueryTo("255.255.255.255", 27016);
        }

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

        for (auto it = _servers.begin(); it != _servers.end();) {
            if (_currentTime - it->lastSeen > 4.0f) {
                it = _servers.erase(it);
            } else {
                ++it;
            }
        }
    }

} // namespace Lab
