# 🌐 Authoritative Multiplayer Architecture

The **Lab Engine Multiplayer Subsystem** (`LabNetwork`, `LabServer.exe`) implements an authoritative client-server architecture over connectionless **UDP sockets**, engineered for competitive low-latency deathmatch play.

---

## 1. Network Topology & Protocol

The network architecture is strictly authoritative:

```
[ Client 1 ] ----( ClientInputPackets @ 64Hz )----> [ Dedicated Server ]
[ Client 2 ] ----( ClientInputPackets @ 64Hz )----> [  LabServer.exe  ]
                                                           |
                                                  Tick Simulation (64Hz)
                                                           |
[ Client 1 ] <---( NetServerSnapshot @ 32-64Hz )<---------+
[ Client 2 ] <---( NetServerSnapshot @ 32-64Hz )<---------+
```

### Packet Protocol
- **`ConnectRequest` / `ConnectAccept`:** Handshake exchanging player nickname, assigned `clientId`, and map verification.
- **`ClientInputPacket`:** Sent every client frame containing tick number, view angles (Yaw, Pitch), input movement flags (`W`, `A`, `S`, `D`, `Jump`, `Crouch`, `Fire`), and weapon selection.
- **`NetServerSnapshot`:** Compact delta snapshot broadcast to all connected clients containing server tick, player entities (position, velocity, health, armor, alive flag, weapon), active tracers, and combat events.
- **`ChatMessagePacket`:** Text messages broadcast to the in-game chat log.
- **`DisconnectPacket`:** Clean disconnection notification.

---

## 2. Client-Side Prediction & Server Reconciliation

To eliminate perceived input latency on the local client without sacrificing server authority:

1. **Prediction:** When the player presses movement keys, the client immediately executes physics and moves locally, storing the unacknowledged inputs in a circular history buffer.
2. **Snapshot Arrival:** When the server snapshot arrives, the client compares its historical predicted position at that server tick against the authoritative server position.
3. **Reconciliation:** If the spatial discrepancy exceeds a small threshold ($> 0.05\text{ m}$):
   - The client snaps its position to the server's authoritative state.
   - Replays all pending unacknowledged inputs from that tick forward to the present.
   - The player experiences zero local input lag while preventing speedhacks and wall-glitches.

---

---

## 3. Dedicated Server (`LabServer.exe` & Linux `LabServer`)

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
# FROZEN-LIFE : LAB MULTIPLAYER SERVER CONFIGURATION (server.cfg)
# Supports both Linux Headless and Windows Dedicated Servers
# ===================================================================

[Server]
port = 27015
server_name = Lab Dedicated Arena [Linux/Windows]
map = assets/maps/facility_alpha.labmap
game_mode = FFA
max_players = 16
tickrate = 64

[Match]
frag_limit = 25
time_limit = 10
enable_bots = 1
bot_count = 4
bot_difficulty = 1

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

## 4. Hosting from `Lab.exe`: Dedicated vs. LAN / P2P

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

## 5. LAN Server Browser & Discovery

Clients discover local servers via UDP broadcast:
- Client broadcasts a discovery query packet on the local subnet (`255.255.255.255`).
- Active servers respond with server name, current map, active game mode, player count, and max slots.
- The client UI lists available servers with real-time round-trip ping measurements.
