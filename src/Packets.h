#pragma once
#include <cstdint>

// ─────────────────────────────────────────────
//  Wire protocol — all structs are packed so
//  sizeof() equals the byte count on the wire.
// ─────────────────────────────────────────────

enum class PacketType : uint8_t {
    MOVE_REQUEST  = 0,   // client → host
    GAME_STATE    = 1,   // host   → both
    RESTART_REQ   = 2,   // client → host: request game restart
};

// Bit flags packed into PieceNetState::flags
constexpr uint8_t NET_FLAG_MOVING    = 1 << 0;
constexpr uint8_t NET_FLAG_JUMPING   = 1 << 1;
constexpr uint8_t NET_FLAG_DEAD      = 1 << 2;
constexpr uint8_t NET_FLAG_HAS_MOVED = 1 << 3;
constexpr uint8_t NET_FLAG_LANDED    = 1 << 4;
constexpr uint8_t NET_FLAG_ARRIVED   = 1 << 5;  // sliding piece just reached its dest

#pragma pack(push, 1)

// ── client → host: ask host to restart ───────
struct RestartRequestPacket {
    PacketType type = PacketType::RESTART_REQ;
};

// ── client → host ─────────────────────────────
struct MoveRequestPacket {
    PacketType type   = PacketType::MOVE_REQUEST;
    uint8_t    pieceId;       // stable piece id (0–31)
    int8_t     destX, destY;
};

// ── per-piece snapshot (20 bytes) ────────────
struct PieceNetState {
    float   worldX, worldY, worldZ;   // smooth animated position
    int8_t  gridX,   gridY;           // logical grid position
    int8_t  targetX, targetY;         // destination grid
    uint8_t type;                     // PieceType cast to uint8_t
    uint8_t color;                    // PieceColor cast to uint8_t
    uint8_t flags;                    // NET_FLAG_* bitmask
    uint8_t id;                       // stable id assigned at board init
};

// ── server → both clients ────────────────────
struct GameStatePacket {
    PacketType    type       = PacketType::GAME_STATE;
    uint8_t       gameState;    // GameState enum cast to uint8_t
    uint8_t       resetGen;     // incremented each time the host restarts; client mirrors
    float         whiteMana;
    float         blackMana;
    uint8_t       pieceCount;   // always 32 (dead pieces included, flag set)
    PieceNetState pieces[32];
};

#pragma pack(pop)
