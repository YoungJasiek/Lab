---
docid: LH01
name: Authoritative Multiplayer Architecture
related_docs: [LA01, LE01, LF01]
tags: [network, multiplayer, udp, replication, dedicated-server, ecs, source-architecture]
---

# 🌐 Authoritative Multiplayer Architecture & Generic ECS Networking

The **Lab Engine Multiplayer Subsystem** (`LabNetwork`, `server.dll`, `client.dll`, `LabServer.exe`) implements a high-performance authoritative client-server architecture over connectionless **UDP sockets**, engineered for low-latency networked gameplay, client prediction, and data-driven generic ECS state replication.

---

## 1. Network Topology & Source-Style Modular Architecture

Following the architectural separation pioneered by Valve's Source Engine, gameplay networking is decomposed into discrete modular dynamic libraries:

```
[ client.dll (Client Presentation) ]               [ server.dll (Authoritative Logic) ]
  ├── Input Sampling & Prediction                     ├── 64-128Hz Fixed-Tick World Simulation
  ├── Snapshot Buffer & Interpolation                 ├── Authoritative Entity Spawning / Despawning
  └── Presentation Sync                               └── Delta Snapshot Broadcast
          │                                                       │
          └───────────── UDP Sockets (LabNetwork) ────────────────┘
```

- **`server.dll` (`IServerSubsystem`):** Encapsulates authoritative server simulation, physics ticking, game mode rules, client connection validation, and network entity management. Operates identically in headless mode (`LabServer.exe`) or when running embedded inside `Lab.exe`.
- **`client.dll` (`IClientSubsystem`):** Encapsulates local prediction, client-side entity smoothing, interpolation buffers, and user input transmission without GPU context coupling.

### Packet Protocol & Generic Message Types
Alongside classic connection messages, the modern network layer utilizes generic ECS replication messages:
- **`ConnectRequest` / `ConnectAccept`:** Handshake exchanging player nickname, assigned `clientId`, and map/scene hash verification.
- **`NetMsgType::EntitySpawn`:** Instantiates an entity with `NetworkIdentityComponent` across all clients with initial transform, owner ID, and prefab name.
- **`NetMsgType::EntityDestroy`:** Authoritatively despawns an entity by network ID across connected clients.
- **`NetMsgType::EntitySync`:** Continuous binary transform replication (`position`, `rotation`, `velocity`) with sequence numbers and interpolation timestamps.
- **`NetMsgType::ClientInput`:** Generic player input state packet transmitted at the client's framerate/tickrate.
- **`NetMsgType::RPC`:** Remote procedure call targeting a specific entity ID, method index, and arbitrary payload buffer.
- **`ChatMessagePacket`:** Text messages broadcast to the in-game chat log.
- **`DisconnectPacket`:** Clean disconnection notification.

---

## 2. Generic ECS Network Components (`LabComponents.h`)

To eliminate hardcoded multiplayer game modes and specific player assumptions:

### `NetworkIdentityComponent`
```cpp
struct NetworkIdentityComponent {
    uint32_t netId{0};              // Unique cluster-wide network entity ID
    uint32_t ownerClientId{0};      // ID of the client with input authority
    std::string prefabName;        // Prefab asset path for client instantiation
    bool isServerAuthoritative{true};
};
```

### `NetworkTransformComponent`
```cpp
struct NetworkTransformComponent {
    Vec3 targetPosition{0.0f, 0.0f, 0.0f};
    Vec3 targetVelocity{0.0f, 0.0f, 0.0f};
    Quat targetRotation{0.0f, 0.0f, 0.0f, 1.0f};
    float interpolationSpeed{15.0f};
    float teleportThreshold{5.0f}; // Snap instantly if discrepancy exceeds limit
    uint32_t lastServerTick{0};
};
```
The client continuously interpolates between incoming snapshots, eliminating jitter while preventing high-speed position desynchronization.

---

## 3. Client-Side Prediction & Server Reconciliation

To eliminate perceived input latency on the local client without sacrificing server authority:

1. **Prediction:** When the player presses movement keys, the client immediately executes physics and moves locally, storing the unacknowledged inputs in a circular history buffer.
2. **Snapshot Arrival:** When the server snapshot arrives, the client compares its historical predicted position at that server tick against the authoritative server position.
3. **Reconciliation:** If the spatial discrepancy exceeds a small threshold ($> 0.05\text{ m}$):
   - The client snaps its position to the server's authoritative state.
   - Replays all pending unacknowledged inputs from that tick forward to the present.
   - The player experiences zero local input lag while preventing speedhacks and wall-glitches.

---

## 4. Dedicated Server (`LabServer.exe` & Linux `LabServer`)

The dedicated server runs completely headless without requiring an OpenGL context, audio drivers, or windowing system:
- **Platform Parity:** Runs identically on Linux (Ubuntu, Debian, Arch) and Windows.
- **Port:** Configurable UDP port (default: `27015`).
- **Tickrate:** Configurable tickrate ($64\text{ Hz}$ or $128\text{ Hz}$).
- **Map Loading:** Parses `.labmap` geometry into solid collision boxes for authoritative wall collision and hitscan verification.
- **Configuration File (`server.cfg`):** The server automatically reads `server.cfg` (or custom `-config <path>`).
- **Command Line Overrides:** Command-line parameters override values in `server.cfg`.

### `server.cfg` Reference Format:
```ini
# ===================================================================
# LAB MULTIPLAYER SERVER CONFIGURATION (server.cfg)
# Supports both Linux Headless and Windows Dedicated Servers
# ===================================================================

[Server]
port = 27015
server_name = Lab Dedicated Server [Linux/Windows]
map = assets/maps/facility_alpha.labmap
tickrate = 64
max_clients = 16

[Match]
round_limit = 10
time_limit = 600

[Network]
lan_mode = 1
host_type = dedicated
password = 
```

### Linux Dedicated Server Quickstart:
```bash
# Compile headless server on Linux
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target LabServer

# Run dedicated server with server.cfg
./build/LabServer

# Or run with custom config / overrides
./build/LabServer -config my_custom.cfg -port 27020 -map assets/maps/facility_alpha.labmap
```

---

## 5. Hosting from `Lab.exe`: Dedicated vs. LAN / P2P

When configuring multiplayer in the `Lab.exe` GUI:
1. **DEDICATED (`LabServer.exe`):**
   - Spawns an external dedicated `LabServer.exe` console process in the background.
   - Writes the user match settings to `server.cfg`.
   - `Lab.exe` connects to `127.0.0.1:<port>` as an authoritative client.
   - Clean lifecycle management: terminating `Lab.exe` terminates the spawned dedicated server cleanly without orphaned processes.
2. **LAN / P2P (LISTEN SERVER):**
   - Runs an embedded UDP listen server directly inside `Lab.exe`.
   - Broadcasts discovery queries across the local subnet (`255.255.255.255`).
   - Nearby players on the same Wi-Fi/LAN can find and join the match with 1-click in the Server Browser.

---

## 6. LAN Server Browser & Discovery

Clients discover local servers via UDP broadcast:
- Client broadcasts a discovery query packet on the local subnet (`255.255.255.255`).
- Active servers respond with server name, current map, active game mode, player count, and max slots.
- The client UI lists available servers with real-time round-trip ping measurements.
