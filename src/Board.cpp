#include "Board.h"
#include <algorithm>

void Board::Init() {
    pieces.clear();

    uint8_t nextId = 0;
    auto add = [&](PieceType t, PieceColor c, int x, int y) {
        pieces.push_back(std::make_unique<Piece>(t, c, GridPos{x, y}));
        pieces.back()->id = nextId++;
    };

    // White — back rank at y=0, pawns at y=1
    add(PieceType::ROOK,   PieceColor::Light, 0, 0);
    add(PieceType::KNIGHT, PieceColor::Light, 1, 0);
    add(PieceType::BISHOP, PieceColor::Light, 2, 0);
    add(PieceType::QUEEN,  PieceColor::Light, 3, 0);
    add(PieceType::KING,   PieceColor::Light, 4, 0);
    add(PieceType::BISHOP, PieceColor::Light, 5, 0);
    add(PieceType::KNIGHT, PieceColor::Light, 6, 0);
    add(PieceType::ROOK,   PieceColor::Light, 7, 0);
    for (int x = 0; x < 8; x++) add(PieceType::PAWN, PieceColor::Light, x, 1);

    // Black — back rank at y=7, pawns at y=6
    add(PieceType::ROOK,   PieceColor::Dark, 0, 7);
    add(PieceType::KNIGHT, PieceColor::Dark, 1, 7);
    add(PieceType::BISHOP, PieceColor::Dark, 2, 7);
    add(PieceType::QUEEN,  PieceColor::Dark, 3, 7);
    add(PieceType::KING,   PieceColor::Dark, 4, 7);
    add(PieceType::BISHOP, PieceColor::Dark, 5, 7);
    add(PieceType::KNIGHT, PieceColor::Dark, 6, 7);
    add(PieceType::ROOK,   PieceColor::Dark, 7, 7);
    for (int x = 0; x < 8; x++) add(PieceType::PAWN, PieceColor::Dark, x, 6);
}

Piece* Board::GetPieceById(uint8_t id) const {
    for (auto& p : pieces)
        if (p->id == id) return p.get();
    return nullptr;
}

Piece* Board::GetPieceAt(GridPos pos) const {
    for (auto& p : pieces)
        if (!p->isDead && p->gridPos == pos) return p.get();
    return nullptr;
}

Piece* Board::GetPieceAtOrTarget(GridPos pos) const {
    for (auto& p : pieces)
        if (!p->isDead && (p->gridPos == pos || p->targetGrid == pos)) return p.get();
    return nullptr;
}

void Board::AddSlideMoves(const Piece* piece, std::vector<GridPos>& out, int dx, int dz) const {
    GridPos cur = { piece->gridPos.x + dx, piece->gridPos.y + dz };
    while (cur.x >= 0 && cur.x < 8 && cur.y >= 0 && cur.y < 8) {
        Piece* there = GetPieceAt(cur);
        if (there) {
            if (there->color != piece->color) out.push_back(cur);
            break;
        }
        out.push_back(cur);
        cur.x += dx;
        cur.y += dz;
    }
}

std::vector<GridPos> Board::GetValidMoves(const Piece* piece) const {
    std::vector<GridPos> moves;
    if (!piece || piece->isDead || piece->isMoving || piece->isJumping) return moves;

    int x = piece->gridPos.x;
    int y = piece->gridPos.y;

    auto tryStep = [&](int tx, int ty) {
        if (tx < 0 || tx >= 8 || ty < 0 || ty >= 8) return;
        GridPos t = {tx, ty};
        Piece* there = GetPieceAt(t);
        if (!there || there->color != piece->color) moves.push_back(t);
    };

    switch (piece->type) {
        case PieceType::PAWN: {
            int dir = (piece->color == PieceColor::Light) ? 1 : -1;
            GridPos fwd = {x, y + dir};
            if (fwd.y >= 0 && fwd.y < 8 && !GetPieceAt(fwd)) {
                moves.push_back(fwd);
                if (!piece->hasMoved) {
                    GridPos fwd2 = {x, y + 2*dir};
                    if (!GetPieceAt(fwd2)) moves.push_back(fwd2);
                }
            }
            for (int s : {-1, 1}) {
                GridPos diag = {x + s, y + dir};
                if (diag.x >= 0 && diag.x < 8 && diag.y >= 0 && diag.y < 8) {
                    Piece* there = GetPieceAt(diag);
                    if (there && there->color != piece->color) moves.push_back(diag);
                }
            }
            break;
        }
        case PieceType::ROOK:
            AddSlideMoves(piece, moves,  1,  0);
            AddSlideMoves(piece, moves, -1,  0);
            AddSlideMoves(piece, moves,  0,  1);
            AddSlideMoves(piece, moves,  0, -1);
            break;
        case PieceType::BISHOP:
            AddSlideMoves(piece, moves,  1,  1);
            AddSlideMoves(piece, moves,  1, -1);
            AddSlideMoves(piece, moves, -1,  1);
            AddSlideMoves(piece, moves, -1, -1);
            break;
        case PieceType::QUEEN:
            AddSlideMoves(piece, moves,  1,  0);
            AddSlideMoves(piece, moves, -1,  0);
            AddSlideMoves(piece, moves,  0,  1);
            AddSlideMoves(piece, moves,  0, -1);
            AddSlideMoves(piece, moves,  1,  1);
            AddSlideMoves(piece, moves,  1, -1);
            AddSlideMoves(piece, moves, -1,  1);
            AddSlideMoves(piece, moves, -1, -1);
            break;
        case PieceType::KNIGHT: {
            int offs[8][2] = {{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};
            for (auto& o : offs) tryStep(x + o[0], y + o[1]);
            break;
        }
        case PieceType::KING:
            for (int dx = -1; dx <= 1; dx++)
                for (int dz = -1; dz <= 1; dz++)
                    if (dx || dz) tryStep(x + dx, y + dz);
            break;
    }
    return moves;
}

void Board::PurgeDead() {
    pieces.erase(
        std::remove_if(pieces.begin(), pieces.end(), [](const auto& p){ return p->isDead; }),
        pieces.end()
    );
}
