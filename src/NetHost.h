#pragma once
#include "Packets.h"
#include <functional>
#include <cstdint>

// ENet types forward-declared so enet.h is NOT included here.
// Including enet.h in a header pulls in <windows.h> which conflicts
// with raylib.h macros (Rectangle, DrawText, CloseWindow, etc.).
struct _ENetHost;
struct _ENetPeer;
typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

class NetHost {
public:
    // Called by move callback when a client move request arrives
    using MoveCallback = std::function<void(uint8_t pieceId, int destX, int destY)>;

    // One-time ENet library init/shutdown — call from main(), not from here
    static bool GlobalInit();
    static void GlobalShutdown();

    bool Init(uint16_t port = 7777);
    void Poll();                                          // process incoming packets
    void SetMoveCallback(MoveCallback cb) { onMove = std::move(cb); }
    void BroadcastState(const GameStatePacket& pkt);

    bool HasClient()   const { return peer != nullptr; }
    bool IsListening() const { return host != nullptr; }

    void Shutdown();
    ~NetHost() { Shutdown(); }

private:
    ENetHost*    host   = nullptr;
    ENetPeer*    peer   = nullptr;
    MoveCallback onMove;
};
