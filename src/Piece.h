#pragma once
#include "raylib.h"

enum class PieceType  { PAWN, ROOK, KNIGHT, BISHOP, QUEEN, KING };
enum class PieceColor { Light, Dark };  // NOT White/Black — raylib macros those

struct GridPos {
    int x, y;
    bool operator==(const GridPos& o) const { return x == o.x && y == o.y; }
    bool operator!=(const GridPos& o) const { return !(*this == o); }
};

inline Vector3 GridToWorld(GridPos g) {
    return { g.x + 0.5f, 0.0f, g.y + 0.5f };
}

class Piece {
public:
    PieceType  type;
    PieceColor color;
    uint8_t    id = 0;  // stable index assigned at board init (0–31)

    GridPos gridPos;    // logical position (updated on arrival)
    GridPos targetGrid; // where it's heading

    Vector3 worldPos;   // smooth animated position
    bool isMoving  = false;
    bool isJumping = false; // knight arc
    bool justLanded = false;
    bool isDead    = false;
    bool hasMoved  = false; // pawn first-move flag

    static constexpr float MOVE_SPEED     = 1.8f;  // world units / sec
    static constexpr float JUMP_DURATION  = 0.65f; // seconds
    static constexpr float JUMP_HEIGHT    = 2.8f;
    static constexpr float HITBOX_RADIUS  = 0.22f;
    static constexpr float HURTBOX_RADIUS = 0.38f;

    Piece(PieceType t, PieceColor c, GridPos pos);

    void StartMove(GridPos dest);
    void StartJump(GridPos dest);
    void Update(float dt);

    int         ManaCost() const;
    const char* TypeName() const;

    // Leading-edge hitbox position (only meaningful when isMoving && !isJumping)
    Vector3 GetHitboxPos() const;

private:
    Vector3 moveDir    = {0,0,0};
    float   jumpTimer  = 0.0f;
    Vector3 jumpStart  = {0,0,0};
    Vector3 jumpEnd    = {0,0,0};
};
