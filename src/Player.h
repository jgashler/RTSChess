#pragma once
#include "Piece.h"
#include <algorithm>

class Player {
public:
    PieceColor color;
    float mana;

    static constexpr float MAX_MANA   = 10.0f;
    static constexpr float MANA_REGEN = 0.5f; // per second

    Player() : color(PieceColor::Light), mana(MAX_MANA) {}
    Player(PieceColor c) : color(c), mana(MAX_MANA) {}

    void Update(float dt) {
        mana = std::min(MAX_MANA, mana + MANA_REGEN * dt);
    }

    bool CanAfford(int cost) const { return mana >= (float)cost; }
    void Spend(int cost)           { mana -= (float)cost; }
};
