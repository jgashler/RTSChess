#pragma once
#include "Piece.h"
#include <vector>
#include <memory>

class Board {
public:
    std::vector<std::unique_ptr<Piece>> pieces;

    void Init();

    Piece* GetPieceAt(GridPos pos) const;         // by current gridPos
    Piece* GetPieceAtOrTarget(GridPos pos) const; // includes pieces en-route
    Piece* GetPieceById(uint8_t id) const;        // by stable network id

    std::vector<GridPos> GetValidMoves(const Piece* piece) const;

    void PurgeDead();

private:
    void AddSlideMoves(const Piece* p, std::vector<GridPos>& out, int dx, int dz) const;
};
