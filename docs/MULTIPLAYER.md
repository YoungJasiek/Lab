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

## 3. Dedicated Server (`LabServer.exe`)

The dedicated server runs headless as a console application:
- **Port:** Configurable UDP port (default: `27015` or `27019`).
- **Tickrate:** Fixed $64\text{ Hz}$ tick simulation loop ($15.625\text{ ms}$ per tick).
- **Map Loading:** Parses `.labmap` geometry into solid collision boxes for authoritative wall collision and hitscan verification.
- **Command Line Arguments:**
  ```powershell
  .\build\Release\LabServer.exe --port 27015 --map facility_alpha.labmap --mode ffa --maxplayers 16
  ```

---

## 4. LAN Server Browser & Discovery

Clients discover local servers via UDP broadcast:
- Client broadcasts a discovery ping packet on the local subnet.
- Active servers respond with server name, current map, active game mode, player count, and max slots.
- The client UI lists available servers with real-time round-trip ping measurements.
