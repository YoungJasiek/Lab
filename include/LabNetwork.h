#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <deque>
#include "LabMath.h"
#include "LabCollision.h"

namespace Lab {

    // Network protocol constants
    constexpr uint32_t NET_MAGIC = 0x4C414231; // "LAB1"
    constexpr uint16_t NET_PROTOCOL_VERSION = 1;
    constexpr uint16_t DEFAULT_SERVER_PORT = 27015;
    constexpr size_t MAX_PACKET_SIZE = 1400;
    constexpr int MAX_NETWORK_PLAYERS = 16;

    // --- Message Types ---
    enum class NetMsgType : uint8_t {
        None = 0,
        ConnectRequest = 1,
        ConnectResponse = 2,
        Disconnect = 3,
        Ping = 4,
        Pong = 5,
        UserCmd = 6,
        ServerSnapshot = 7,
        ChatMessage = 8,
        ServerQuery = 9,
        ServerInfo = 10
    };

    // --- Compact Binary Protocol Structures ---
#pragma pack(push, 1)
    struct NetHeader {
        uint32_t magic = NET_MAGIC;
        uint16_t version = NET_PROTOCOL_VERSION;
        uint8_t type = static_cast<uint8_t>(NetMsgType::None);
        uint32_t sequence = 0;
        uint32_t ack = 0;
    };

    struct NetMsgConnectRequest {
        NetHeader header;
        char playerName[32] = { 0 };
    };

    struct NetMsgConnectResponse {
        NetHeader header;
        uint32_t assignedClientId = 0;
        uint16_t tickrate = 64;
        char mapName[32] = { 0 };
    };

    struct NetMsgPingPong {
        NetHeader header;
        float timestamp = 0.0f;
    };

    // Buttons bitmask
    enum NetButtonFlags : uint32_t {
        NetButton_Jump       = (1 << 0),
        NetButton_Fire       = (1 << 1),
        NetButton_Reload     = (1 << 2),
        NetButton_Sprint     = (1 << 3),
        NetButton_Flashlight = (1 << 4)
    };

    struct NetUserCmd {
        NetHeader header;
        uint32_t cmdNumber = 0;
        float forwardMove = 0.0f; // -1.0 to 1.0
        float sideMove = 0.0f;    // -1.0 to 1.0
        float yaw = 0.0f;
        float pitch = 0.0f;
        uint32_t buttons = 0;
        float deltaTime = 0.015625f;
    };

    struct NetPlayerState {
        uint32_t clientId = 0;
        Vec3 position = { 0, 0, 0 };
        Vec3 velocity = { 0, 0, 0 };
        float yaw = 0.0f;
        float pitch = 0.0f;
        float health = 150.0f;
        float armor = 50.0f;
        uint8_t activeWeapon = 0;
        uint8_t isAlive = 1;
        uint16_t kills = 0;
        uint16_t deaths = 0;
        char name[32] = { 0 };
    };

    struct NetServerSnapshot {
        NetHeader header;
        uint32_t serverTick = 0;
        uint32_t lastProcessedCmd = 0; // The latest cmdNumber processed for the receiving client
        uint16_t playerCount = 0;
        NetPlayerState players[MAX_NETWORK_PLAYERS];
    };

    struct NetChatMessage {
        NetHeader header;
        char sender[32] = { 0 };
        char text[128] = { 0 };
    };

    struct NetMsgServerQuery {
        NetHeader header;
    };

    struct NetMsgServerInfo {
        NetHeader header;
        char serverName[64] = { 0 };
        char mapName[32] = { 0 };
        char gameMode[16] = { 0 };
        uint16_t playerCount = 0;
        uint16_t maxPlayers = 16;
        uint16_t pingMs = 0;
    };
#pragma pack(pop)

    // --- Socket Address Abstraction ---
    class SocketAddress {
    public:
        SocketAddress();
        SocketAddress(const std::string& ip, uint16_t port);

        bool fromString(const std::string& ip, uint16_t port);
        std::string getIP() const;
        uint16_t getPort() const;
        std::string toString() const;

        bool operator==(const SocketAddress& other) const;

        void* getNativeSockAddr();
        const void* getNativeSockAddr() const;
        size_t getSockAddrLen() const;

    private:
        uint8_t _storage[28] = { 0 }; // Large enough for sockaddr_in and alignment
    };

    // --- UDP Socket Wrapper ---
    class UDPSocket {
    public:
        UDPSocket();
        ~UDPSocket();

        // Non-copyable, movable (RAII)
        UDPSocket(const UDPSocket&) = delete;
        UDPSocket& operator=(const UDPSocket&) = delete;
        UDPSocket(UDPSocket&& other) noexcept;
        UDPSocket& operator=(UDPSocket&& other) noexcept;

        bool open(uint16_t port = 0);
        void close();
        bool setNonBlocking(bool nonBlocking);

        int sendTo(const void* data, size_t size, const SocketAddress& dest);
        int recvFrom(void* buffer, size_t maxSize, SocketAddress& from);

        bool isOpen() const { return _socketHandle != -1; }
        uint16_t getBoundPort() const { return _boundPort; }

    private:
        intptr_t _socketHandle = -1;
        uint16_t _boundPort = 0;
    };

    // --- Global Network Subsystem (WSAStartup / WSACleanup) ---
    class NetworkSystem {
    public:
        static bool init();
        static void shutdown();
        static bool isInitialized();
    };

    // --- Client Prediction & Reconciliation Subsystem ---
    struct PredictedCommand {
        NetUserCmd cmd;
        Vec3 predictedPosition;
        Vec3 predictedVelocity;
    };

    class ClientPrediction {
    public:
        ClientPrediction();

        // Record a locally simulated command
        void recordCommand(const NetUserCmd& cmd, const Vec3& pos, const Vec3& vel);

        // Reconcile with authoritative server snapshot
        // If predicted state diverges beyond errorThreshold, snaps and replays unacknowledged commands
        bool reconcile(uint32_t lastProcessedCmd, const Vec3& serverPos, const Vec3& serverVel,
                       Vec3& outCorrectedPos, Vec3& outCorrectedVel, float errorThreshold = 0.05f);

        void reset();
        size_t getPendingCount() const { return _history.size(); }

    private:
        std::deque<PredictedCommand> _history;
        uint32_t _nextCmdNumber = 1;
    };

    // --- Server Architecture & Configuration ---
    enum class HostArchitecture : uint8_t {
        Dedicated = 0,
        ListenLAN = 1
    };

    struct ServerConfig {
        uint16_t port = DEFAULT_SERVER_PORT;
        std::string serverName = "Lab Dedicated Arena [LAN]";
        std::string mapName = "facility_alpha.labmap";
        std::string gameMode = "FFA";
        uint16_t maxPlayers = 16;
        uint16_t tickrate = 64;
        int fragLimit = 25;
        int timeLimitMinutes = 10;
        bool enableBots = true;
        int botCount = 4;
        int botDifficulty = 1;
        bool lanMode = true;
        std::string password = "";
        HostArchitecture hostType = HostArchitecture::Dedicated;

        bool loadFromFile(const std::string& filepath = "server.cfg");
        bool saveToFile(const std::string& filepath = "server.cfg") const;
    };

    // --- Dedicated Headless & Listen Server Engine ---
    struct ConnectedClient {
        uint32_t clientId = 0;
        SocketAddress address;
        std::string name;
        NetPlayerState state;
        float lastPacketTime = 0.0f;
        uint32_t lastProcessedCmd = 0;
        float ping = 0.0f;
        bool isGrounded = true;
    };

    class DedicatedServer {
    public:
        DedicatedServer();
        ~DedicatedServer();

        bool start(const ServerConfig& config);
        bool start(uint16_t port = DEFAULT_SERVER_PORT, const std::string& mapName = "facility_alpha.labmap");
        void stop();
        void tick(float dt);

        bool isRunning() const { return _running; }
        uint16_t getPort() const { return _port; }
        size_t getClientCount() const { return _clients.size(); }
        uint32_t getServerTick() const { return _serverTick; }
        const std::string& getServerName() const { return _serverName; }
        void setServerName(const std::string& name) { _serverName = name; _config.serverName = name; }
        const std::string& getMapName() const { return _mapName; }
        const std::string& getGameMode() const { return _gameMode; }
        void setGameMode(const std::string& mode) { _gameMode = mode; _config.gameMode = mode; }
        const ServerConfig& getConfig() const { return _config; }
        void setConfig(const ServerConfig& cfg) { _config = cfg; }
        uint16_t getTickrate() const { return _config.tickrate > 0 ? _config.tickrate : 64; }

        // Send a chat message to all connected clients
        void broadcastChatMessage(const std::string& sender, const std::string& text);

    private:
        UDPSocket _socket;
        ServerConfig _config;
        uint16_t _port = DEFAULT_SERVER_PORT;
        std::string _serverName = "Lab Dedicated Arena [LAN]";
        std::string _gameMode = "FFA";
        std::string _mapName;
        bool _running = false;
        uint32_t _serverTick = 0;
        uint32_t _nextClientId = 1;
        float _timeAccumulator = 0.0f;
        float _serverTime = 0.0f;

        std::vector<ConnectedClient> _clients;
        std::vector<CollisionBox> _solidBoxes;
        Vec3 _spawnPosition{ 0.0f, 1.80f, 0.0f };
        float _spawnYaw = 0.0f;

        void processIncomingPackets();
        void simulateWorld(float dt);
        void broadcastSnapshot();
        ConnectedClient* findClient(const SocketAddress& addr);
    };

    // --- Network Client Subsystem (Integrated into FrozenLife game client) ---
    class NetworkClient {
    public:
        NetworkClient();
        ~NetworkClient();

        bool connect(const std::string& ip, uint16_t port, const std::string& playerName);
        void disconnect();
        void update(float dt, const Vec3& localPos, const Vec3& localVel, float yaw, float pitch, uint32_t buttons, float forwardMove = 0.0f, float sideMove = 0.0f);

        bool isConnected() const { return _connected; }
        uint32_t getClientId() const { return _assignedClientId; }
        float getRTT() const { return _rtt; }
        const NetServerSnapshot& getLatestSnapshot() const { return _latestSnapshot; }
        bool hasNewSnapshot() const { return _hasNewSnapshot; }
        void consumeSnapshot() { _hasNewSnapshot = false; }

        ClientPrediction& getPrediction() { return _prediction; }

    private:
        UDPSocket _socket;
        SocketAddress _serverAddr;
        std::string _playerName;
        bool _connected = false;
        uint32_t _assignedClientId = 0;
        float _rtt = 0.0f;
        float _pingTimer = 0.0f;
        uint32_t _currentCmdSeq = 1;

        ClientPrediction _prediction;
        NetServerSnapshot _latestSnapshot;
        bool _hasNewSnapshot = false;

        void processIncomingPackets();
    };

    // --- Dynamic LAN Server Discovery Browser ---
    struct DiscoveredServer {
        std::string ip = "127.0.0.1";
        uint16_t port = DEFAULT_SERVER_PORT;
        std::string name = "Lab Server";
        std::string map = "facility_alpha.labmap";
        std::string mode = "FFA";
        int playerCount = 0;
        int maxPlayers = 16;
        int pingMs = 5;
        float lastSeen = 0.0f;
    };

    class ServerBrowser {
    public:
        ServerBrowser();
        ~ServerBrowser();

        bool start();
        void stop();
        void refresh();
        void update(float dt);

        const std::vector<DiscoveredServer>& getServers() const { return _servers; }
        bool isScanning() const { return _scanTimer < 2.5f; }
        void sendQueryTo(const std::string& ip, uint16_t port);

    private:
        UDPSocket _socket;
        std::vector<DiscoveredServer> _servers;
        float _scanTimer = 0.0f;
        float _queryTimer = 0.0f;
        float _currentTime = 0.0f;
        bool _initialized = false;
    };

} // namespace Lab
