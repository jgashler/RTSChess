#include "Piece.h"
#include "raymath.h"
#include <cmath>

Piece::Piece(PieceType t, PieceColor c, GridPos pos)
    : type(t), color(c), gridPos(pos), targetGrid(pos)
{
    worldPos = GridToWorld(pos);
}

int Piece::ManaCost() const {
    switch (type) {
        case PieceType::PAWN:   return 1;
        case PieceType::KNIGHT: return 3;
        case PieceType::BISHOP: return 3;
        case PieceType::KING:   return 3;
        case PieceType::ROOK:   return 5;
        case PieceType::QUEEN:  return 7;
    }
    return 1;
}

const char* Piece::TypeName() const {
    switch (type) {
        case PieceType::PAWN:   return "Pawn";
        case PieceType::ROOK:   return "Rook";
        case PieceType::KNIGHT: return "Knight";
        case PieceType::BISHOP: return "Bishop";
        case PieceType::QUEEN:  return "Queen";
        case PieceType::KING:   return "King";
    }
    return "?";
}

void Piece::StartMove(GridPos dest) {
    targetGrid = dest;
    isMoving   = true;
    Vector3 target = GridToWorld(dest);
    float dx = target.x - worldPos.x;
    float dz = target.z - worldPos.z;
    float len = sqrtf(dx*dx + dz*dz);
    if (len > 0.001f) moveDir = { dx/len, 0.0f, dz/len };
}

void Piece::StartJump(GridPos dest) {
    targetGrid  = dest;
    isJumping   = true;
    isMoving    = false;
    justLanded  = false;
    jumpTimer   = 0.0f;
    jumpStart   = worldPos;
    jumpEnd     = GridToWorld(dest);
}

void Piece::Update(float dt) {
    justLanded  = false;
    justArrived = false;

    if (isJumping) {
        jumpTimer += dt;
        float t = jumpTimer / JUMP_DURATION;
        if (t >= 1.0f) {
            t = 1.0f;
            isJumping  = false;
            justLanded = true;
            gridPos    = targetGrid;
            worldPos   = GridToWorld(gridPos);
        } else {
            worldPos.x = jumpStart.x + (jumpEnd.x - jumpStart.x) * t;
            worldPos.z = jumpStart.z + (jumpEnd.z - jumpStart.z) * t;
            worldPos.y = JUMP_HEIGHT * sinf(t * PI);
        }
        return;
    }

    if (isMoving) {
        Vector3 target = GridToWorld(targetGrid);
        float dx   = target.x - worldPos.x;
        float dz   = target.z - worldPos.z;
        float dist = sqrtf(dx*dx + dz*dz);
        if (dist <= MOVE_SPEED * dt) {
            worldPos    = target;
            gridPos     = targetGrid;
            isMoving    = false;
            justArrived = true;
            moveDir     = {0,0,0};
        } else {
            worldPos.x += moveDir.x * MOVE_SPEED * dt;
            worldPos.z += moveDir.z * MOVE_SPEED * dt;
        }
    }
}

Vector3 Piece::GetHitboxPos() const {
    return {
        worldPos.x + moveDir.x * HURTBOX_RADIUS,
        worldPos.y,
        worldPos.z + moveDir.z * HURTBOX_RADIUS
    };
}
