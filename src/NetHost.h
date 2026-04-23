#pragma once
#include <enet/enet.h>
#include "Packets.h"

class Game;

class NetHost {
public:
    // Returns true on success. Call once before the game loop.
    bool Init(uint16_t port = 7777);

    // Call every frame: accepts new connections and dispatches move requests.
    void Poll(Game& game);

    // Send authoritative game state to the connected client.
    void BroadcastState(const GameStatePacket& pkt);

    bool HasClient()    const { return peer != nullptr; }
    bool IsListening()  const { return host != nullptr; }

    void Shutdown();
    ~NetHost() { Shutdown(); }

private:
    ENetHost* host = nullptr;
    ENetPeer* peer = nullptr;   // single client (1v1)
};
