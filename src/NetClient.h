#pragma once
#include "Packets.h"
#include <functional>
#include <cstdint>

// ENet types forward-declared — keeps enet.h (and windows.h) out of this header.
struct _ENetHost;
struct _ENetPeer;
typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

class NetClient {
public:
    using StateCallback = std::function<void(const GameStatePacket&)>;

    bool Connect(const char* address, uint16_t port = 7777);
    void Poll();
    void SetStateCallback(StateCallback cb) { onState = std::move(cb); }
    void SendMoveRequest(uint8_t pieceId, int destX, int destY);

    bool IsConnected() const;
    void Disconnect();
    ~NetClient() { Disconnect(); }

private:
    ENetHost*     host    = nullptr;
    ENetPeer*     peer    = nullptr;
    StateCallback onState;
};
